/* drivers/gpu/drm/msm/sharp/drm_det.c  (Display Driver)
 *
 * Copyright (C) 2017 SHARP CORPORATION
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/fb.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/iopoll.h>
#include <soc/qcom/sh_smem.h>
#ifdef CONFIG_SHARP_SHTERM
#include "misc/shterm_k.h"
#endif /* CONFIG_SHARP_SHTERM */
#include "../dsi-staging/dsi_display.h"
#include "../dsi-staging/dsi_hw.h"
#include "../dsi-staging/dsi_ctrl_reg.h"
#include "msm_drm_context.h"
#include "drm_cmn.h"
#include "../sde/sde_connector.h"

#define DETIN_NAME	"detin"
#define MIPIERR_NAME	"mipierr"
#define DISPONCHK_NAME	"disponchk"

#define DETIN_ERR_COUNT	5
#define DETIN_ERR_ARRAY	(DETIN_ERR_COUNT-1)
#define DETIN_ERR_TIME	10000

#ifdef CONFIG_ARCH_JOHNNY
#define RETRY_RECOVERY_CNT 5
#else /* CONFIG_ARCH_JOHNNY */
#define RETRY_RECOVERY_CNT 3
#endif /* CONFIG_ARCH_JOHNNY */

/* A wake_lock prevents the system from entering suspend or other low power
 * states when active. If the type is set to WAKE_LOCK_SUSPEND, the wake_lock
 * prevents a full system suspend.
 */
enum {
	WAKE_LOCK_SUSPEND, /* Prevent suspend */
	WAKE_LOCK_TYPE_COUNT
};

struct wake_lock {
	struct wakeup_source ws;
};

static inline void wake_lock_init(struct wake_lock *lock, int type,
				  const char *name)
{
	wakeup_source_init(&lock->ws, name);
}

static inline void wake_lock_destroy(struct wake_lock *lock)
{
	wakeup_source_trash(&lock->ws);
}

static inline void wake_lock(struct wake_lock *lock)
{
	__pm_stay_awake(&lock->ws);
}

static inline void wake_lock_timeout(struct wake_lock *lock, long timeout)
{
	__pm_wakeup_event(&lock->ws, jiffies_to_msecs(timeout));
}

static inline void wake_unlock(struct wake_lock *lock)
{
	__pm_relax(&lock->ws);
}

static inline int wake_lock_active(struct wake_lock *lock)
{
	return lock->ws.active;
}

struct drm_det_irq {
	bool enable;
	char *name;
	int irq;
	int gpio;
	int trigger;
	ktime_t err_time[DETIN_ERR_ARRAY];
	struct wake_lock wakelock;
	struct workqueue_struct *workque;
	struct work_struct work;
	void (*workqueue_handler)(struct work_struct *work);
};

static struct {
	bool panel_on;
	int recovery_cnt;
	bool retry_over;
	struct drm_det_irq detin;
	struct drm_det_irq mipierr;
	struct dsi_display *display;
	struct workqueue_struct *delayedwkq;
	struct delayed_work delayedwk;
	struct wake_lock dispon_wakelock;
	unsigned char disp_on_status;
} drm_det_ctx = {0};

static int drm_det_is_panel_connected(void);
static int drm_det_irq_init
	(struct platform_device *pdev, struct drm_det_irq *ptr);
static void drm_det_remove(void);
static irqreturn_t drm_det_isr(int irq_num, void *data);
static int drm_det_set_irq(struct drm_det_irq *ptr, bool enable);
static void drm_det_resume(void);
static void drm_det_suspend(void);
static void drm_det_request_recovery(void);
static int drm_det_power_mode_chk(struct dsi_display *pdisp);
static int drm_det_mipierr_probe(struct platform_device *pdev);
static void drm_det_mipierr_workqueue_handler(struct work_struct *work);
static int drm_det_mipierr_port_chk(void);
static int drm_det_detin_probe(struct platform_device *pdev);
static void drm_det_detin_workqueue_handler(struct work_struct *work);
static int drm_det_detin_port_chk(void);
static void drm_det_dispon_recovery_work(struct work_struct *work);
#ifdef CONFIG_SHARP_SHTERM
static void drm_det_shterm_send_event(int event);
#endif /* CONFIG_SHARP_SHTERM */
static int drm_det_panel_dead(struct dsi_display *display);
static int drm_det_event_notify(struct drm_encoder  *drm_enc);
static int drm_det_dispon_recovery(void);
static int drm_det_disponchk_sub(struct dsi_display *display);
static int drm_det_chk_panel_on_sub(void);

int drm_det_init(struct dsi_display *display)
{
	int ret = 0;
	sharp_smem_common_type *smem = NULL;
	struct shdisp_boot_context *shdisp_boot_ctx = NULL;

	if (!display) {
		pr_err("%s: Invalid input data\n", __func__);
		return ret;
	}

	if (drm_det_ctx.display) {
		pr_warn("%s:already initialized", __func__);
		return 0;
	}

	drm_det_ctx.display = display;
	drm_det_ctx.recovery_cnt = 0;
	drm_det_ctx.retry_over = false;
	drm_det_ctx.delayedwkq
		= create_singlethread_workqueue(DISPONCHK_NAME);
	INIT_DELAYED_WORK(&drm_det_ctx.delayedwk,
		drm_det_dispon_recovery_work);

	wake_lock_init(&drm_det_ctx.dispon_wakelock,
		WAKE_LOCK_SUSPEND, DISPONCHK_NAME);

	smem = sh_smem_get_common_address();
	if (smem == NULL) {
		ret = 0;
		pr_err("%s: failed to "
			"sh_smem_get_common_address()\n",__func__);
	} else {
		shdisp_boot_ctx = (struct shdisp_boot_context*)
						smem->shdisp_data_buf;
		if (shdisp_boot_ctx == NULL) {
			pr_err("%s: shdisp_boot_context is NULL ",__func__);
		} else {
			drm_det_ctx.disp_on_status =
				shdisp_boot_ctx->disp_on_status;
			pr_info("%s: disp_on_status(from XBL)=%d panel_connected=%d\n",
				__func__, drm_det_ctx.disp_on_status,shdisp_boot_ctx->panel_connected);
		}
	}
	pr_debug("%s:out. ret=%d, disp_on_status=%d\n",
		__func__, ret, drm_det_ctx.disp_on_status);
	return ret;
}

int drm_det_post_panel_on(void)
{
	drm_det_resume();
	return 0;
}

int drm_det_disponchk(struct dsi_display *display)
{
	if (drm_det_is_panel_connected()) {
		return drm_det_disponchk_sub(display);
	}
	return 0;
}

int drm_det_pre_panel_off(void)
{
	struct dsi_display *dsi_display = drm_det_ctx.display;
	struct dsi_panel *panel = dsi_display->panel;

	drm_det_suspend();
	if (drm_det_ctx.retry_over) {
		dsi_panel_set_backlight(panel, 0);
	}
	return 0;
}

bool drm_det_is_retry_over(void)
{
	bool flg = drm_det_ctx.retry_over;
	return (flg);
}

int drm_det_chk_panel_on(void)
{
	if (drm_det_is_panel_connected()) {
		return drm_det_chk_panel_on_sub();
	}
	return 0;
}

static int drm_det_disponchk_sub(struct dsi_display *display)
{
	int ret = 0;
	pr_debug("%s:in \n", __func__);

	if (!display) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ret = drm_det_power_mode_chk(display);
	switch (ret) {
	case -EIO:
		drm_det_ctx.disp_on_status = DRM_PANEL_DISPONCHK_READERR;
		break;
	case -EINVAL:
		drm_det_ctx.disp_on_status = DRM_PANEL_DISPONCHK_STATERR;
		break;
	default:
		drm_det_ctx.disp_on_status = DRM_PANEL_DISPONCHK_SUCCESS;
		break;
	}

	pr_debug("%s:out. ret=%d, disp_on_status=%d\n",
		__func__, ret, drm_det_ctx.disp_on_status);

	return ret;
}


static int drm_det_chk_panel_on_sub(void)
{
	int ret = 0;
	unsigned char disp_on_status = drm_det_ctx.disp_on_status;

	pr_debug("%s:in , disp_on_status=%d\n", __func__, disp_on_status);

	if (disp_on_status == DRM_PANEL_DISPONCHK_READERR) {
		ret = -EIO;
#ifdef CONFIG_SHARP_SHTERM
		drm_det_shterm_send_event(SHBATTLOG_EVENT_DISP_ESD_READ_ERROR);
#endif /* CONFIG_SHARP_SHTERM */
	} else if (disp_on_status == DRM_PANEL_DISPONCHK_STATERR) {
		ret = -EINVAL;
#ifdef CONFIG_SHARP_SHTERM
		drm_det_shterm_send_event(SHBATTLOG_EVENT_DISP_ESD_DISPOFF);
#endif /* CONFIG_SHARP_SHTERM */
	} else {
		ret = 0;
	}

	if (drm_det_detin_port_chk()) {
#ifdef CONFIG_SHARP_SHTERM
		drm_det_shterm_send_event(SHBATTLOG_EVENT_DISP_ESD_SLPIN);
#endif /* CONFIG_SHARP_SHTERM */
		pr_err("%s:det port is low.\n", __func__);
		ret = -EIO;
	}
	if (drm_det_mipierr_port_chk()) {
#ifdef CONFIG_SHARP_SHTERM
		drm_det_shterm_send_event(SHBATTLOG_EVENT_DISP_ESD_MIPI_ERROR);
#endif /* CONFIG_SHARP_SHTERM */
		pr_err("%s:mipi error port is high.\n", __func__);
		ret = -EIO;
	}

	if (ret){
		if (drm_det_ctx.recovery_cnt >= RETRY_RECOVERY_CNT) {
			pr_err("%s:recovery retry over. recovery_cnt=%d\n",
				__func__, drm_det_ctx.recovery_cnt);
			drm_det_ctx.retry_over = true;
		}
		drm_det_ctx.recovery_cnt++;
		drm_det_dispon_recovery();
	} else {
		drm_det_ctx.recovery_cnt = 0;
		drm_det_ctx.retry_over = false;
	}

	pr_debug("%s:out. ret=%d, recovery_cnt=%d\n",
		__func__, ret, drm_det_ctx.recovery_cnt);

	return ret;
}

static int drm_det_is_panel_connected(void)
{
	struct shdisp_boot_context *shdisp_boot_ctx = NULL;
	sharp_smem_common_type *smem = sh_smem_get_common_address();
	if (smem == NULL) {
		pr_err("%s: failed to "
			"sh_smem_get_common_address()\n",__func__);
		return 1;
	}
	shdisp_boot_ctx= (struct shdisp_boot_context*)
						smem->shdisp_data_buf;
	return shdisp_boot_ctx->panel_connected;
}

static int drm_det_dispon_recovery(void)
{
	int ret = 0;
	pr_debug("%s: in\n", __func__);
	if (!drm_det_ctx.delayedwkq) {
		pr_err("%s:failed to create_singlethread_workqueue().\n",
			__func__);
		return -EFAULT;
	}
	wake_lock(&drm_det_ctx.dispon_wakelock);
	ret = queue_delayed_work(
		drm_det_ctx.delayedwkq,
		&drm_det_ctx.delayedwk,
		msecs_to_jiffies(100));
	if (ret == 0) {
		wake_unlock(&drm_det_ctx.dispon_wakelock);
		pr_err("%s:failed to queue_work(). ret=%d\n", __func__, ret);
	}
	pr_debug("%s: out ret=%d\n", __func__, ret);
	return 0;
}

static void drm_det_dispon_recovery_work(struct work_struct *work)
{
	pr_debug("%s: in\n", __func__);
	drm_det_request_recovery();
	wake_unlock(&drm_det_ctx.dispon_wakelock);
}

#ifdef CONFIG_SHARP_SHTERM
static void drm_det_shterm_send_event(int event)
{
	shbattlog_info_t info;
	memset(&info, 0, sizeof(info));

	info.event_num = event;
	shterm_k_set_event(&info);
}
#endif /* CONFIG_SHARP_SHTERM */

static int drm_det_power_mode_chk(struct dsi_display *pdisp)
{
	int rc = 0;
	char expect_val[] = {0x9C};
	unsigned char rbuf[] = {0x00};
	char pwrmode_addr[] = {0x0A};
	rc = drm_cmn_panel_dcs_read(pdisp, pwrmode_addr[0],
					sizeof(pwrmode_addr), rbuf);
	pr_info("%s: read rc = %d, rbuf=0x%02X\n",
		__func__, rc, rbuf[0]);
	if (rc) {
		pr_err("%s: failed to read "
			"power-mode rc=%d\n", __func__,rc);
		rc = -EIO;
	// } else if (memcmp(expect_val, rbuf, sizeof(rbuf))) {
	} else if (rbuf[0] != expect_val[0]) {
		pr_err("%s: disp on chk ng. "
			"rbuf[0x%02X], expect_val[0x%02X]\n",
			__func__, rbuf[0], expect_val[0]);
		rc = -EINVAL;
	}
	return rc;
}

static void drm_det_remove(void)
{
	memset(&drm_det_ctx, 0x00, sizeof(drm_det_ctx));

	pr_debug("%s: out\n", __func__);
}

static void drm_det_request_recovery(void)
{
	struct dsi_display *display = NULL;
	struct drm_connector *conn = NULL;
	struct sde_connector *c_conn = NULL;

	display = msm_drm_get_dsi_displey();
	if (display) {
		conn = display->drm_conn;
		if (conn) {
			c_conn = to_sde_connector(conn);
			if (c_conn)
				c_conn->panel_dead = true;
		}
	}
	drm_det_panel_dead(drm_det_ctx.display);
	pr_warn("%s: recovery_cnt=%d\n", __func__, drm_det_ctx.recovery_cnt);
}

void drm_det_esd_chk_ng(void)
{
#ifdef CONFIG_SHARP_SHTERM
	drm_det_shterm_send_event(SHBATTLOG_EVENT_DISP_ESD_CHK_NG);
#endif /* CONFIG_SHARP_SHTERM */
	drm_det_request_recovery();
}

static int drm_det_panel_dead(struct dsi_display *display)
{
	int rc = 0;
	struct drm_device *dev;
	struct drm_encoder *encoder;

	if (!display) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	dev = display->drm_dev;
	if (!dev) {
		pr_err("%s: no drm_dev\n", __func__);
		return -EINVAL;
	}

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (!encoder || !encoder->dev || !encoder->dev->dev_private ||
		!encoder->crtc || (encoder->encoder_type != DRM_MODE_ENCODER_DSI)) {
			continue;
		}
		drm_det_event_notify(encoder);
		break;
	}

	return  rc;
}

static int drm_det_event_notify(struct drm_encoder *drm_enc)
{
	int rc = 0;
	bool panel_dead = true;
	struct drm_event event;

	if (!drm_enc) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	/* panel dead event notify */
	if (drm_det_ctx.retry_over) {
		event.type = DRM_EVENT_PANEL_OFF;
	} else {
		event.type = DRM_EVENT_PANEL_DEAD;
	}
	event.length = sizeof(u32);
	msm_mode_object_event_notify(&drm_enc->base,
		drm_enc->dev, &event, (u8*)&panel_dead, true);

	return  rc;
}

static int drm_det_irq_init(struct platform_device *pdev, struct drm_det_irq *ptr)
{
	struct resource *pr = NULL;

	if (!pdev || !ptr) {
		pr_err("%s:parameter error.", __func__);
		return -EPERM;
	}
	ptr->gpio = of_get_named_gpio(pdev->dev.of_node, ptr->name, 0);
	if (!gpio_is_valid(ptr->gpio)) {
		pr_err("%s:failed to get named gpio().\n", ptr->name);
		return -EFAULT;
	}
	pr = platform_get_resource_byname(pdev, IORESOURCE_IRQ, ptr->name);
	if (pr != NULL) {
		ptr->irq = pr->start;
	} else if (0 <= gpio_to_irq(ptr->gpio)) {
		ptr->irq = gpio_to_irq(ptr->gpio);
	} else {
		pr_err("%s:failed to get irq().\n", ptr->name);
		return -EFAULT;
	}

	ptr->workque = create_singlethread_workqueue(ptr->name);
	if (!ptr->workque) {
		pr_err("%s:failed to create_singlethread_workqueue().\n", ptr->name);
		return -EFAULT;
	}
	if (!ptr->workqueue_handler) {
		pr_err("%s:workqueue_handler parameter error.\n", ptr->name);
		return -EPERM;
	}
	INIT_WORK(&ptr->work, ptr->workqueue_handler);
	wake_lock_init(&ptr->wakelock, WAKE_LOCK_SUSPEND, ptr->name);
	pr_debug("%s:gpio=%d irq=%d\n", __func__, ptr->gpio, ptr->irq);
	memset(ptr->err_time, 0, sizeof(ptr->err_time));
	return 0;
}

static irqreturn_t drm_det_isr(int irq_num, void *data)
{
	int ret;
	struct drm_det_irq *ptr;

	pr_debug("%s: irq_num=%d data=0x%p\n", __func__, irq_num, data);
	disable_irq_nosync(irq_num);

	if (!data) {
		pr_err("invalid isr parameter.\n");
		goto exit;
	}
	ptr = (struct drm_det_irq *)data;

	if (!ptr->workque) {
		pr_err("invalid work queue.\n");
		goto exit;
	}
	if (!drm_det_ctx.panel_on) {
		pr_debug("display OFF, will be exited.\n");
		goto exit;
	}
	wake_lock(&ptr->wakelock);
	ret = queue_work(ptr->workque, &ptr->work);
	if (ret == 0) {
		wake_unlock(&ptr->wakelock);
		pr_err("%s:failed to queue_work(). ret=%d\n",
			__func__, ret);
	}
	pr_debug("%s: out\n", __func__);
exit:
	return IRQ_HANDLED;
}

static int drm_det_set_irq(struct drm_det_irq *ptr, bool enable)
{
	int ret = 0;

	pr_debug("%s: data=0x%p, enable=%d\n", __func__, ptr, enable);
	if (ptr == NULL) {
		pr_err("%s:parameter error.\n", __func__);
		return -EPERM;
	}
	if (!ptr->enable) {
		pr_debug("%s:not irq configuration.\n", __func__);
		return -ENODEV;
	}
	if (enable) {
		ret = request_irq(ptr->irq, drm_det_isr,
					ptr->trigger, ptr->name, ptr);
		if (ret) {
			pr_err("%s:failed to request_irq().(ret=%d)\n"
				, ptr->name, ret);
		}
	} else {
		disable_irq(ptr->irq);
		free_irq(ptr->irq, ptr);
	}
	return ret;
}

static void drm_det_resume(void)
{
	pr_debug("%s: in\n", __func__);

	drm_det_ctx.panel_on = true;
	drm_det_set_irq(&drm_det_ctx.mipierr, true);
	drm_det_set_irq(&drm_det_ctx.detin, true);
}

static void drm_det_suspend(void)
{
	pr_debug("%s: in\n", __func__);
	if (cancel_delayed_work_sync(&drm_det_ctx.delayedwk) == true) {
		pr_debug("%s: cancel_delayed_work done.\n", __func__);
		wake_unlock(&drm_det_ctx.dispon_wakelock);
	}
	if (drm_det_ctx.panel_on) {
		drm_det_ctx.panel_on = false;
		drm_det_set_irq(&drm_det_ctx.detin, false);
		drm_det_set_irq(&drm_det_ctx.mipierr, false);
	}
}

static int drm_det_mipierr_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_debug("%s: in\n", __func__);
	if (pdev == NULL) {
		pr_err("%s:pdev parameter error.\n", __func__);
		return -EPERM;
	}
	drm_det_ctx.mipierr.name = MIPIERR_NAME;
	drm_det_ctx.mipierr.trigger = IRQF_TRIGGER_HIGH;
	drm_det_ctx.mipierr.workqueue_handler
		= drm_det_mipierr_workqueue_handler;
	ret = drm_det_irq_init(pdev, &drm_det_ctx.mipierr);
	if (!ret) {
		drm_det_ctx.mipierr.enable = true;
	} else {
		pr_warn("%s:mipi_err port no config.\n", __func__);
		drm_det_ctx.mipierr.enable = false;
	}
	pr_debug("%s: out\n", __func__);
	return 0;
}

static int drm_det_mipierr_port_chk(void)
{
	if (!drm_det_ctx.mipierr.enable) {
		pr_debug("%s:not gpio configuration.\n", __func__);
		return 0;
	}
	if (gpio_get_value(drm_det_ctx.mipierr.gpio)) {
		return -EIO;
	}
	return 0;
}

static int drm_det_dsi_cmd_bta_sw_trigger(struct dsi_ctrl *dsi_ctrl)
{
	u32 status;
	int timeout_us = 35000;
	struct dsi_ctrl_hw *ctrl = NULL;

	pr_debug("%s: in\n", __func__);

	if (dsi_ctrl == NULL) {
		pr_err("%s: dsi_ctrl is NULL.\n", __func__);
		return -EINVAL;
	}

	ctrl = &dsi_ctrl->hw;
	/* CMD_MODE_BTA_SW_TRIGGER */
	DSI_W32(ctrl, DSI_CMD_MODE_BTA_SW_TRIGGER, 0x00000001);
	wmb();

	/* Check for CMD_MODE_DMA_BUSY */
	if (readl_poll_timeout(((ctrl->base) + DSI_STATUS),
				status, ((status & 0x0010) == 0),
				0, timeout_us)) {
		pr_info("%s: timeout. status=0x%08x\n", __func__, status);
		return -EIO;
	}

	pr_debug("%s: out status=0x%08x\n", __func__, status);

	return 0;
}

static int drm_det_rosetta_mipierr_clear(struct dsi_display *display)
{
	int ret = 0;

	pr_debug("%s: in\n", __func__);

	if (!display) {
		pr_err("%s: Invalid input data\n", __func__);
		return ret;
	}

	drm_det_dsi_cmd_bta_sw_trigger(display->ctrl[0].ctrl);
	if (display->ctrl[1].ctrl) {
		drm_det_dsi_cmd_bta_sw_trigger(display->ctrl[1].ctrl);
	}

	pr_debug("%s:end ret=%d\n", __func__, ret);
	return 0;
}

int drm_det_mipierr_clear(struct dsi_display *display)
{
	int ret = 0;

	if (!display) {
		pr_err("%s: Invalid input data\n", __func__);
		return ret;
	}

	ret = drm_det_rosetta_mipierr_clear(display);

	return ret;
}

void drm_det_mipierr_irq_clear(void)
{
	cancel_work(&drm_det_ctx.mipierr.work);
	drm_det_mipierr_clear(drm_det_ctx.display);
}

int drm_det_check_mipierr_gpio(void)
{
	int ret = 0;
	ret = gpio_get_value(drm_det_ctx.mipierr.gpio);
	pr_debug("%s: mipierr.gpio(%d)=%d\n", __func__,
				drm_det_ctx.mipierr.gpio, ret);
	if (ret) {
		return -EIO;
	}
	return 0;
}

static void drm_det_mipierr_workqueue_handler(struct work_struct *work)
{
	pr_debug("%s: in\n", __func__);

	if (drm_det_mipierr_port_chk()) {
		pr_err("%s:mipi error port is high.\n", __func__);
#ifdef CONFIG_SHARP_SHTERM
		drm_det_shterm_send_event(SHBATTLOG_EVENT_DISP_ESD_MIPI_ERROR);
#endif /* CONFIG_SHARP_SHTERM */
		drm_det_mipierr_clear(drm_det_ctx.display);
		drm_det_request_recovery();
	} else {
		pr_debug("mipi error port is low.\n");
		enable_irq(drm_det_ctx.mipierr.irq);
	}
	pr_debug("%s: out\n", __func__);
	wake_unlock(&drm_det_ctx.mipierr.wakelock);
	return;
}

static int drm_det_mipierr_remove(struct platform_device *pdev)
{
	pr_debug("%s: in\n", __func__);
	return 0;
}

static int drm_det_detin_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_debug("%s: in\n", __func__);
	if (pdev == NULL) {
		pr_err("%s:pdev parameter error.\n", __func__);
		return -EPERM;
	}
	drm_det_ctx.detin.name = DETIN_NAME;
#ifdef CONFIG_ARCH_JOHNNY
	drm_det_ctx.detin.trigger = IRQF_TRIGGER_LOW;
#else
	drm_det_ctx.detin.trigger = IRQF_TRIGGER_FALLING;
#endif /*CONFIG_ARCH_JOHNNY*/
	drm_det_ctx.detin.workqueue_handler
		= drm_det_detin_workqueue_handler;
	ret = drm_det_irq_init(pdev, &drm_det_ctx.detin);
	if (!ret) {
		drm_det_ctx.detin.enable = true;
	} else {
		pr_warn("%s:detin port no config.\n", __func__);
		drm_det_ctx.detin.enable = false;
	}
	pr_debug("%s: out\n", __func__);
	return 0;
}

static int drm_det_detin_port_chk(void)
{
	if (!drm_det_ctx.detin.enable) {
		pr_debug("%s:not gpio configuration.\n", __func__);
		return 0;
	}
	if (!gpio_get_value(drm_det_ctx.detin.gpio)) {
		return -EIO;
	}
	return 0;
}

static void drm_det_detin_set_time(ktime_t cur_time) {
	int i = 0;

	for (i=(DETIN_ERR_ARRAY-1); i>0; i--) {
		drm_det_ctx.detin.err_time[i] =
				drm_det_ctx.detin.err_time[i-1];
	}

	drm_det_ctx.detin.err_time[0] = cur_time;
}

static void drm_det_detin_chk_cnt(void)
{
	ktime_t cur_time;
	s64 diff_ms;

	cur_time = ktime_get_boottime();
	if (drm_det_ctx.detin.err_time[DETIN_ERR_ARRAY-1].tv64 != 0) {
		diff_ms = ktime_ms_delta(cur_time,
			drm_det_ctx.detin.err_time[DETIN_ERR_ARRAY-1]);
		if (diff_ms <= DETIN_ERR_TIME) {
			goto error;
		}
	}

	drm_det_detin_set_time(cur_time);
	return;

error:
#ifndef CONFIG_ARCH_JOHNNY
	drm_det_ctx.retry_over = true;
#endif /* not CONFIG_ARCH_JOHNNY */
	memset(drm_det_ctx.detin.err_time, 0,
					sizeof(drm_det_ctx.detin.err_time));
	return;
}

static void drm_det_detin_workqueue_handler(struct work_struct *work)
{
	pr_debug("%s: in\n", __func__);

	if (drm_det_detin_port_chk()) {
		pr_err("%s:det port is low.\n", __func__);
#ifdef CONFIG_SHARP_SHTERM
		drm_det_shterm_send_event(SHBATTLOG_EVENT_DISP_ESD_DRVOFF);
#endif /* CONFIG_SHARP_SHTERM */
		drm_det_detin_chk_cnt();
		drm_det_request_recovery();
	} else {
		pr_debug("det port is high.\n");
		enable_irq(drm_det_ctx.detin.irq);
	}
	pr_debug("%s: out\n", __func__);
	wake_unlock(&drm_det_ctx.detin.wakelock);
	return;
}

static int drm_det_detin_remove(struct platform_device *pdev)
{
	pr_debug("%s: in\n", __func__);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id drm_det_mipierr_dt_match[] = {
	{ .compatible = "sharp,panel_mipierr", },
	{}
};
static const struct of_device_id drm_det_detin_dt_match[] = {
	{ .compatible = "sharp,panel_det", },
	{}
};
#else
#define drm_det_mipierr_dt_match NULL;
#define drm_det_detin_dt_match NULL;
#endif /* CONFIG_OF */

static struct platform_driver drm_det_mipierr_driver = {
	.probe = drm_det_mipierr_probe,
	.remove = drm_det_mipierr_remove,
	.shutdown = NULL,
	.driver = {
		/*
		 * Driver name must match the device name added in
		 * platform.c.
		 */
		.name = "sharp,panel_mipierr",
		.of_match_table = drm_det_mipierr_dt_match,
	},
};

static struct platform_driver drm_det_detin_driver = {
	.probe = drm_det_detin_probe,
	.remove = drm_det_detin_remove,
	.shutdown = NULL,
	.driver = {
		/*
		 * Driver name must match the device name added in
		 * platform.c.
		 */
		.name = "sharp,panel_det",
		.of_match_table = drm_det_detin_dt_match,
	},
};

static int drm_det_register_driver(void)
{
	int ret = 0;
	ret = platform_driver_register(&drm_det_mipierr_driver);
	if (!ret)
		ret = platform_driver_register(&drm_det_detin_driver);
	return ret;
}

static int __init drm_det_driver_init(void)
{
	int ret;

	pr_debug("%s: in\n", __func__);
	ret = drm_det_register_driver();
	if (ret) {
		pr_err("det_register_driver() failed!\n");
		return ret;
	}
	return 0;
}

static void __exit drm_det_driver_exit(void)
{
	pr_debug("%s: in\n", __func__);
	drm_det_remove();
	platform_driver_unregister(&drm_det_mipierr_driver);
	platform_driver_unregister(&drm_det_detin_driver);
}

module_init(drm_det_driver_init);
module_exit(drm_det_driver_exit);
