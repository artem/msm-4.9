/* drivers/input/touchscreen/shtps/sy3000/shtps_rmi.c
 *
 * Copyright (c) 2017, Sharp. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
/* COORDINATOR Qualcomm_CS1.1 BUILDERR MODIFY start */
/*#include <linux/wakelock.h>*/
/* COORDINATOR Qualcomm_CS1.1 BUILDERR MODIFY end */
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/input/mt.h>
#include <linux/notifier.h>

#include <linux/input/shtps_dev.h>

#include "shtps_rmi.h"
#include "shtps_cfg.h"
#include "shtps_param.h"

#include "shtps_fwctl.h"
#include "shtps_rmi_sub.h"
#include "shtps_rmi_devctl.h"
#include "shtps_rmi_debug.h"
#include "shtps_log.h"

#if defined(SHTPS_COVER_ENABLE)
	#include <misc/shflip.h>
#endif
#if defined(SHTPS_CHECK_HWID_ENABLE)
	#include <soc/qcom/sharp/sh_boot_manager.h>
#endif

/* -----------------------------------------------------------------------------------
 */
static DEFINE_MUTEX(shtps_ctrl_lock);
static DEFINE_MUTEX(shtps_loader_lock);

#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
	static DEFINE_MUTEX(shtps_proc_lock);
#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */

#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
	static DEFINE_MUTEX(shtps_facetouch_qosctrl_lock);
#endif /* CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT */

#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
	static DEFINE_MUTEX(shtps_wakeup_fail_touch_event_reject_lock);
#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */

/* -----------------------------------------------------------------------------------
 */
#define	TPS_MUTEX_LOG_LOCK(VALUE)		SHTPS_LOG_DBG_PRINT("mutex_lock:   -> (%s)\n",VALUE)
#define	TPS_MUTEX_LOG_UNLOCK(VALUE)		SHTPS_LOG_DBG_PRINT("mutex_unlock: <- (%s)\n",VALUE)

/* -----------------------------------------------------------------------------------
 */
struct shtps_rmi_spi*	gShtps_rmi_spi = NULL;

#if defined( SHTPS_LOG_OUTPUT_SWITCH_ENABLE )
	u8					gLogOutputEnable = 0;
#endif /* #if defined( SHTPS_LOG_OUTPUT_SWITCH_ENABLE ) */

#if defined( SHTPS_MODULE_PARAM_ENABLE )
	static int shtps_irq_wake_state = 0;
	static int shtps_rezero_state = 0;

	module_param(shtps_irq_wake_state, int, S_IRUGO);
	module_param(shtps_rezero_state, int, S_IRUGO | S_IWUSR);
#endif /* SHTPS_MODULE_PARAM_ENABLE */

int gShtps_panel_size_x = CONFIG_SHTPS_SY3000_LCD_SIZE_DEFAULT_X;
int gShtps_panel_size_y = CONFIG_SHTPS_SY3000_LCD_SIZE_DEFAULT_Y;

static BLOCKING_NOTIFIER_HEAD(shtps_notifier_list_touchevent);

/* -----------------------------------------------------------------------------------
 */
typedef int (shtps_state_func)(struct shtps_rmi_spi *ts, int param);
struct shtps_state_func {
	shtps_state_func	*enter;
	shtps_state_func	*start;
	shtps_state_func	*stop;
	shtps_state_func	*sleep;
	shtps_state_func	*wakeup;
	shtps_state_func	*start_ldr;
	shtps_state_func	*start_tm;
	shtps_state_func	*stop_tm;
	shtps_state_func	*facetouch_on;
	shtps_state_func	*facetouch_off;
	shtps_state_func	*interrupt;
	shtps_state_func	*timeout;
};

/* -----------------------------------------------------------------------------------
 */
#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
	static void shtps_irq_proc(struct shtps_rmi_spi *ts, u8 dummy);
	static void shtps_grip_proc(struct shtps_rmi_spi *ts, u8 request);
	static void shtps_setsleep_proc(struct shtps_rmi_spi *ts, u8 sleep);
	static void shtps_async_open_proc(struct shtps_rmi_spi *ts, u8 dummy);
	static void shtps_async_close_proc(struct shtps_rmi_spi *ts, u8 dummy);
	static void shtps_async_enable_proc(struct shtps_rmi_spi *ts, u8 dummy);
	static void shtps_suspend_spi_wake_lock(struct shtps_rmi_spi *ts, u8 lock);
	#ifdef SHTPS_DEVELOP_MODE_ENABLE
		static void shtps_exec_suspend_pending_proc_delayed(struct shtps_rmi_spi *ts);
		static void shtps_deter_suspend_spi_pending_proc_delayed_work_function(struct work_struct *work);
	#endif /* SHTPS_DEVELOP_MODE_ENABLE */
	static void shtps_exec_suspend_pending_proc(struct shtps_rmi_spi *ts);
	static void shtps_deter_suspend_spi_pending_proc_work_function(struct work_struct *work);
	static void shtps_cover_proc(struct shtps_rmi_spi *ts, u8 request);
#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */
static irqreturn_t shtps_irq_handler(int irq, void *dev_id);
static irqreturn_t shtps_irq(int irq, void *dev_id);
static void shtps_work_tmof(struct work_struct *data);
static enum hrtimer_restart shtps_delayed_rezero_timer_function(struct hrtimer *timer);
static void shtps_rezero_delayed_work_function(struct work_struct *work);
static void shtps_delayed_rezero(struct shtps_rmi_spi *ts, unsigned long delay_us);
static void shtps_delayed_rezero_cancel(struct shtps_rmi_spi *ts);

static void shtps_rezero_handle(struct shtps_rmi_spi *ts, u8 event, u8 gs);
static void shtps_reset_startuptime(struct shtps_rmi_spi *ts);
static unsigned long shtps_check_startuptime(struct shtps_rmi_spi *ts);

#if defined(SHTPS_PDT_READ_RETRY_ENABLE)
	static int shtps_map_construct(struct shtps_rmi_spi *ts, u8 func_check);
#else
	static int shtps_map_construct(struct shtps_rmi_spi *ts);
#endif /* SHTPS_PDT_READ_RETRY_ENABLE */
#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
	static u8 shtps_get_lpmode_state(struct shtps_rmi_spi *ts);
	static void shtps_set_doze_mode(struct shtps_rmi_spi *ts, int on);
	static void shtps_set_lpmode_init(struct shtps_rmi_spi *ts);
#endif /* #if defined( SHTPS_LOW_POWER_MODE_ENABLE ) */

#if defined(SHTPS_LPWG_MODE_ENABLE)
	static void shtps_lpwg_notify_interval_delayed_work_function(struct work_struct *work);
	static void shtps_lpwg_notify_interval_stop(struct shtps_rmi_spi *ts);
	static void shtps_lpwg_notify_interval_start(struct shtps_rmi_spi *ts);
	static void shtps_lpwg_wakelock_init(struct shtps_rmi_spi *ts);
	static void shtps_lpwg_wakelock_destroy(struct shtps_rmi_spi *ts);
	static void shtps_lpwg_wakelock(struct shtps_rmi_spi *ts, int on);
	static void shtps_set_lpwg_mode_cal(struct shtps_rmi_spi *ts);
	static void shtps_event_update(struct shtps_rmi_spi *ts, struct shtps_touch_info *info);
	static void shtps_notify_wakeup_event(struct shtps_rmi_spi *ts);
	static void shtps_notify_double_tap_event(struct shtps_rmi_spi *ts);
	static void shtps_read_touchevent_insleep(struct shtps_rmi_spi *ts, int state);
#endif /* SHTPS_LPWG_MODE_ENABLE */

#if defined(SHTPS_MULTI_FW_ENABLE)
	static void shtps_get_multi_fw_type(struct shtps_rmi_spi *ts);
#endif /* SHTPS_MULTI_FW_ENABLE */

static int shtps_init_param(struct shtps_rmi_spi *ts);
static void shtps_standby_param(struct shtps_rmi_spi *ts);
static void shtps_clr_startup_err(struct shtps_rmi_spi *ts);
static void shtps_notify_startup(struct shtps_rmi_spi *ts, u8 err);
static void shtps_set_startmode(struct shtps_rmi_spi *ts, u8 mode);
static int shtps_get_startmode(struct shtps_rmi_spi *ts);
static void shtps_set_touch_info(struct shtps_rmi_spi *ts, u8 *buf, struct shtps_touch_info *info);
static void shtps_calc_notify(struct shtps_rmi_spi *ts, u8 *buf, struct shtps_touch_info *info, u8 *event);

static int shtps_tm_irqcheck(struct shtps_rmi_spi *ts);
static int shtps_tm_wait_attn(struct shtps_rmi_spi *ts);
static void shtps_tm_wakeup(struct shtps_rmi_spi *ts);
static void shtps_tm_cancel(struct shtps_rmi_spi *ts);
static int shtps_start_tm(struct shtps_rmi_spi *ts);
static void shtps_stop_tm(struct shtps_rmi_spi *ts);
#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
	//nothing
#else
	static void shtps_irq_wake_disable(struct shtps_rmi_spi *ts);
	static void shtps_irq_wake_enable(struct shtps_rmi_spi *ts);
#endif /* !SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

static int shtps_irq_resuest(struct shtps_rmi_spi *ts);
static void shtps_irqtimer_start(struct shtps_rmi_spi *ts, long time_ms);
static void shtps_irqtimer_stop(struct shtps_rmi_spi *ts);

#if defined(SHTPS_IRQ_LOADER_CHECK_INT_STATUS_ENABLE)
	static int shtps_loader_irqclr(struct shtps_rmi_spi *ts);
#else
	static void shtps_loader_irqclr(struct shtps_rmi_spi *ts);
#endif /* SHTPS_IRQ_LOADER_CHECK_INT_STATUS_ENABLE */
#if defined( SHTPS_BOOT_FWUPDATE_ENABLE )
	int shtps_boot_fwupdate_enable_check(struct shtps_rmi_spi *ts);
#endif /* #if defined( SHTPS_BOOT_FWUPDATE_ENABLE ) */

static int shtps_fwupdate_enable(struct shtps_rmi_spi *ts);
static int shtps_loader_wait_attn(struct shtps_rmi_spi *ts);
static void shtps_loader_wakeup(struct shtps_rmi_spi *ts);
static int shtps_exit_bootloader(struct shtps_rmi_spi *ts);
static int state_change(struct shtps_rmi_spi *ts, int state);
static int shtps_statef_nop(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_cmn_error(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_cmn_stop(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_cmn_facetouch_on(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_cmn_facetouch_off(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_idle_start(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_idle_start_ldr(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_idle_wakeup(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_idle_int(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_waiwakeup_stop(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_waiwakeup_int(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_waiwakeup_tmo(struct shtps_rmi_spi *ts, int param);
static int shtps_state_waiready_stop(struct shtps_rmi_spi *ts, int param);
static int shtps_state_waiready_int(struct shtps_rmi_spi *ts, int param);
static int shtps_state_waiready_tmo(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_active_enter(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_active_stop(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_active_sleep(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_active_starttm(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_active_facetouch_on(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_active_int(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_loader_enter(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_loader_stop(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_loader_int(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_facetouch_sleep(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_facetouch_starttm(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_facetouch_facetouch_off(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_facetouch_int(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_fwtm_enter(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_fwtm_stop(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_fwtm_stoptm(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_fwtm_int(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_sleep_enter(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_sleep_wakeup(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_sleep_starttm(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_sleep_facetouch_on(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_sleep_int(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_sleep_facetouch_enter(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_sleep_facetouch_stop(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_sleep_facetouch_wakeup(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_sleep_facetouch_starttm(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_sleep_facetouch_facetouch_off(struct shtps_rmi_spi *ts, int param);
static int shtps_statef_sleep_facetouch_int(struct shtps_rmi_spi *ts, int param);
static int shtps_rmi_open(struct input_dev *dev);
static void shtps_rmi_close(struct input_dev *dev);
static int shtps_init_internal_variables(struct shtps_rmi_spi *ts);
static void shtps_deinit_internal_variables(struct shtps_rmi_spi *ts);
static int shtps_init_inputdev(struct shtps_rmi_spi *ts, struct device *ctrl_dev_p, char *modalias);
static void shtps_deinit_inputdev(struct shtps_rmi_spi *ts);
#if defined(SHTPS_GLOVE_DETECT_ENABLE)
	static int shtps_set_glove_detect_init(struct shtps_rmi_spi *ts);
#endif /* SHTPS_GLOVE_DETECT_ENABLE */

#if defined(SHTPS_DEF_RECORD_LOG_FILE_ENABLE)
	extern int shtps_record_log_api_add_log(int type);
#endif /* SHTPS_DEF_RECORD_LOG_FILE_ENABLE */

/* -----------------------------------------------------------------------------------
 */
const static struct shtps_state_func state_idle = {
    .enter          = shtps_statef_nop,
    .start          = shtps_statef_idle_start,
    .stop           = shtps_statef_nop,
    .sleep          = shtps_statef_nop,
    .wakeup         = shtps_statef_idle_wakeup,
    .start_ldr      = shtps_statef_idle_start_ldr,
    .start_tm       = shtps_statef_cmn_error,
    .stop_tm        = shtps_statef_cmn_error,
    .facetouch_on   = shtps_statef_cmn_facetouch_on,
    .facetouch_off  = shtps_statef_cmn_facetouch_off,
    .interrupt      = shtps_statef_idle_int,
    .timeout        = shtps_statef_nop
};

const static struct shtps_state_func state_waiwakeup = {
    .enter          = shtps_statef_nop,
    .start          = shtps_statef_nop,
    .stop           = shtps_statef_waiwakeup_stop,
    .sleep          = shtps_statef_nop,
    .wakeup         = shtps_statef_nop,
    .start_ldr      = shtps_statef_cmn_error,
    .start_tm       = shtps_statef_cmn_error,
    .stop_tm        = shtps_statef_cmn_error,
    .facetouch_on   = shtps_statef_cmn_facetouch_on,
    .facetouch_off  = shtps_statef_cmn_facetouch_off,
    .interrupt      = shtps_statef_waiwakeup_int,
    .timeout        = shtps_statef_waiwakeup_tmo
};

const static struct shtps_state_func state_waiready = {
    .enter          = shtps_statef_nop,
    .start          = shtps_statef_nop,
    .stop           = shtps_state_waiready_stop,
    .sleep          = shtps_statef_nop,
    .wakeup         = shtps_statef_nop,
    .start_ldr      = shtps_statef_cmn_error,
    .start_tm       = shtps_statef_cmn_error,
    .stop_tm        = shtps_statef_cmn_error,
    .facetouch_on   = shtps_statef_cmn_facetouch_on,
    .facetouch_off  = shtps_statef_cmn_facetouch_off,
    .interrupt      = shtps_state_waiready_int,
    .timeout        = shtps_state_waiready_tmo
};

const static struct shtps_state_func state_active = {
    .enter          = shtps_statef_active_enter,
    .start          = shtps_statef_nop,
    .stop           = shtps_statef_active_stop,
    .sleep          = shtps_statef_active_sleep,
    .wakeup         = shtps_statef_nop,
    .start_ldr      = shtps_statef_cmn_error,
    .start_tm       = shtps_statef_active_starttm,
    .stop_tm        = shtps_statef_cmn_error,
    .facetouch_on   = shtps_statef_active_facetouch_on,
    .facetouch_off  = shtps_statef_nop,
    .interrupt      = shtps_statef_active_int,
    .timeout        = shtps_statef_nop
};

const static struct shtps_state_func state_loader = {
    .enter          = shtps_statef_loader_enter,
    .start          = shtps_statef_nop,
    .stop           = shtps_statef_loader_stop,
    .sleep          = shtps_statef_nop,
    .wakeup         = shtps_statef_nop,
    .start_ldr      = shtps_statef_nop,
    .start_tm       = shtps_statef_cmn_error,
    .stop_tm        = shtps_statef_cmn_error,
    .facetouch_on   = shtps_statef_cmn_facetouch_on,
    .facetouch_off  = shtps_statef_cmn_facetouch_off,
    .interrupt      = shtps_statef_loader_int,
    .timeout        = shtps_statef_nop
};

const static struct shtps_state_func state_facetouch = {
    .enter          = shtps_statef_nop,
    .start          = shtps_statef_nop,
    .stop           = shtps_statef_active_stop,
    .sleep          = shtps_statef_facetouch_sleep,
    .wakeup         = shtps_statef_nop,
    .start_ldr      = shtps_statef_cmn_error,
    .start_tm       = shtps_statef_facetouch_starttm,
    .stop_tm        = shtps_statef_cmn_error,
    .facetouch_on   = shtps_statef_nop,
    .facetouch_off  = shtps_statef_facetouch_facetouch_off,
    .interrupt      = shtps_statef_facetouch_int,
    .timeout        = shtps_statef_nop
};

const static struct shtps_state_func state_fwtm = {
    .enter          = shtps_statef_fwtm_enter,
    .start          = shtps_statef_nop,
    .stop           = shtps_statef_fwtm_stop,
    .sleep          = shtps_statef_nop,
    .wakeup         = shtps_statef_nop,
    .start_ldr      = shtps_statef_cmn_error,
    .start_tm       = shtps_statef_nop,
    .stop_tm        = shtps_statef_fwtm_stoptm,
    .facetouch_on   = shtps_statef_cmn_facetouch_on,
    .facetouch_off  = shtps_statef_cmn_facetouch_off,
    .interrupt      = shtps_statef_fwtm_int,
    .timeout        = shtps_statef_nop
};

const static struct shtps_state_func state_sleep = {
    .enter          = shtps_statef_sleep_enter,
    .start          = shtps_statef_nop,
    .stop           = shtps_statef_cmn_stop,
    .sleep          = shtps_statef_nop,
    .wakeup         = shtps_statef_sleep_wakeup,
    .start_ldr      = shtps_statef_cmn_error,
    .start_tm       = shtps_statef_sleep_starttm,
    .stop_tm        = shtps_statef_cmn_error,
    .facetouch_on   = shtps_statef_sleep_facetouch_on,
    .facetouch_off  = shtps_statef_nop,
    .interrupt      = shtps_statef_sleep_int,
    .timeout        = shtps_statef_nop
};

const static struct shtps_state_func state_sleep_facetouch = {
    .enter          = shtps_statef_sleep_facetouch_enter,
    .start          = shtps_statef_nop,
    .stop           = shtps_statef_sleep_facetouch_stop,
    .sleep          = shtps_statef_nop,
    .wakeup         = shtps_statef_sleep_facetouch_wakeup,
    .start_ldr      = shtps_statef_cmn_error,
    .start_tm       = shtps_statef_sleep_facetouch_starttm,
    .stop_tm        = shtps_statef_cmn_error,
    .facetouch_on   = shtps_statef_nop,
    .facetouch_off  = shtps_statef_sleep_facetouch_facetouch_off,
    .interrupt      = shtps_statef_sleep_facetouch_int,
    .timeout        = shtps_statef_nop
};

const static struct shtps_state_func *state_func_tbl[] = {
	&state_idle,
	&state_waiwakeup,
	&state_waiready,
	&state_active,
	&state_loader,
	&state_facetouch,
	&state_fwtm,
	&state_sleep,
	&state_sleep_facetouch,
};

#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
typedef void (*shtps_deter_suspend_spi_pending_func_t)(struct shtps_rmi_spi*, u8);
static const shtps_deter_suspend_spi_pending_func_t SHTPS_SUSPEND_PENDING_FUNC_TBL[] = {
	shtps_irq_proc,							/**< SHTPS_DETER_SUSPEND_SPI_PROC_IRQ */
	shtps_setsleep_proc,					/**< SHTPS_DETER_SUSPEND_SPI_PROC_SETSLEEP */
	shtps_ioctl_setlpwg_proc,				/**< SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SETLPWG */
	shtps_ioctl_setlpmode_proc,				/**< SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SETLPMODE */
	shtps_ioctl_setconlpmode_proc,			/**< SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SETCONLPMODE */
	shtps_ioctl_setlcdbrightlpmode_proc,	/**< SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SETLCDBRIGHTLPMODE */
	shtps_async_open_proc,					/**< SHTPS_DETER_SUSPEND_SPI_PROC_OPEN */
	shtps_async_close_proc,					/**< SHTPS_DETER_SUSPEND_SPI_PROC_CLOSE */
	shtps_async_enable_proc,				/**< SHTPS_DETER_SUSPEND_SPI_PROC_ENABLE */
	shtps_grip_proc,						/**< SHTPS_DETER_SUSPEND_SPI_PROC_GRIP */
	shtps_cover_proc,						/**< SHTPS_DETER_SUSPEND_SPI_PROC_COVER */
	shtps_ioctl_setlpwg_doubletap_proc,		/**< SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SETLPWG_DOUBLETAP */
	shtps_ioctl_setglove_proc,				/**< SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SETGLOVE */
	shtps_ioctl_set_high_report_mode_proc,	/**< SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SET_HIGH_REPORT_MODE */
};
#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */

/* -----------------------------------------------------------------------------------
 */
void shtps_mutex_lock_ctrl(void)
{
	TPS_MUTEX_LOG_LOCK("shtps_ctrl_lock");
	mutex_lock(&shtps_ctrl_lock);
}

void shtps_mutex_unlock_ctrl(void)
{
	TPS_MUTEX_LOG_UNLOCK("shtps_ctrl_lock");
	mutex_unlock(&shtps_ctrl_lock);
}

void shtps_mutex_lock_loader(void)
{
	TPS_MUTEX_LOG_LOCK("shtps_loader_lock");
	mutex_lock(&shtps_loader_lock);
}

void shtps_mutex_unlock_loader(void)
{
	TPS_MUTEX_LOG_UNLOCK("shtps_loader_lock");
	mutex_unlock(&shtps_loader_lock);
}

void shtps_mutex_lock_proc(void)
{
#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
	TPS_MUTEX_LOG_LOCK("shtps_proc_lock");
	mutex_lock(&shtps_proc_lock);
#endif
}

void shtps_mutex_unlock_proc(void)
{
#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
	TPS_MUTEX_LOG_UNLOCK("shtps_proc_lock");
	mutex_unlock(&shtps_proc_lock);
#endif
}

void shtps_mutex_lock_facetouch_qos_ctrl(void)
{
#if defined(CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT)
	TPS_MUTEX_LOG_LOCK("shtps_facetouch_qosctrl_lock");
	mutex_lock(&shtps_facetouch_qosctrl_lock);
#endif
}

void shtps_mutex_unlock_facetouch_qos_ctrl(void)
{
#if defined(CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT)
	TPS_MUTEX_LOG_UNLOCK("shtps_facetouch_qosctrl_lock");
	mutex_unlock(&shtps_facetouch_qosctrl_lock);
#endif
}

void shtps_mutex_lock_wakeup_fail_touch_event_reject(void)
{
#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
	TPS_MUTEX_LOG_LOCK("shtps_wakeup_fail_touch_event_reject_lock");
	mutex_lock(&shtps_wakeup_fail_touch_event_reject_lock);
#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */
}

void shtps_mutex_unlock_wakeup_fail_touch_event_reject(void)
{
#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
	TPS_MUTEX_LOG_UNLOCK("shtps_wakeup_fail_touch_event_reject_lock");
	mutex_unlock(&shtps_wakeup_fail_touch_event_reject_lock);
#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */
}

/* -----------------------------------------------------------------------------------
 */
#if defined(SHTPS_CHECK_HWID_ENABLE)
int shtps_system_get_hw_revision(void)
{
	SHTPS_LOG_FUNC_CALL();
	return sh_boot_get_hw_revision();
}

int shtps_system_get_hw_type(void)
{
	unsigned char handset;
	int ret = 0;

	SHTPS_LOG_FUNC_CALL();
	handset = sh_boot_get_handset();

	if(handset == 0){
		ret = SHTPS_HW_TYPE_BOARD;
	}else{
		ret = SHTPS_HW_TYPE_HANDSET;
	}

	return ret;
}
#endif /* SHTPS_CHECK_HWID_ENABLE */

/* -----------------------------------------------------------------------------------
 */
void shtps_system_set_sleep(struct shtps_rmi_spi *ts)
{
	#if defined(SHTPS_INPUT_POWER_MODE_CHANGE_ENABLE)
		SHTPS_LOG_FUNC_CALL();
		shtps_device_sleep(ts->ctrl_dev_p);
	#endif /* SHTPS_INPUT_POWER_MODE_CHANGE_ENABLE */
}

void shtps_system_set_wakeup(struct shtps_rmi_spi *ts)
{
	#if defined(SHTPS_POWER_OFF_IN_SLEEP_ENABLE)
		shtps_device_power_on();
	#endif /* SHTPS_POWER_OFF_IN_SLEEP_ENABLE */
	#if defined(SHTPS_INPUT_POWER_MODE_CHANGE_ENABLE)
		SHTPS_LOG_FUNC_CALL();
		shtps_device_wakeup(ts->ctrl_dev_p);
	#endif /* SHTPS_INPUT_POWER_MODE_CHANGE_ENABLE */
}

/* -----------------------------------------------------------------------------------
 */
#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
static void shtps_irq_proc(struct shtps_rmi_spi *ts, u8 dummy)
{
	SHTPS_LOG_FUNC_CALL();

	shtps_performance_check(SHTPS_PERFORMANCE_CHECK_STATE_CONT);
	shtps_wake_lock_idle(ts);	//SHTPS_CPU_IDLE_SLEEP_CONTROL_ENABLE

	#if defined(SHTPS_LPWG_MODE_ENABLE)
		if(0 != ts->lpwg.lpwg_switch && SHTPS_STATE_SLEEP == ts->state_mgr.state){
			shtps_lpwg_wakelock(ts, 1);
		}
	#endif /* defined(SHTPS_LPWG_MODE_ENABLE) */

	#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
		if(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECT_REZERO_ENABLE != 0){
			shtps_mutex_lock_wakeup_fail_touch_event_reject();
			ts->wakeup_fail_touch_event_reject_rezero_exec = 0;
			shtps_mutex_unlock_wakeup_fail_touch_event_reject();
		}
	#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */

	request_event(ts, SHTPS_EVENT_INTERRUPT, 1);

	#if defined(SHTPS_LPWG_MODE_ENABLE)
		shtps_lpwg_wakelock(ts, 0);
	#endif /* defined(SHTPS_LPWG_MODE_ENABLE) */

	shtps_wake_unlock_idle(ts);	//SHTPS_CPU_IDLE_SLEEP_CONTROL_ENABLE
	shtps_performance_check(SHTPS_PERFORMANCE_CHECK_STATE_END);
}

static void shtps_grip_proc(struct shtps_rmi_spi *ts, u8 request)
{
	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_LPWG_MODE_ENABLE) && defined(SHTPS_LPWG_GRIP_SUPPORT_ENABLE)
		#if defined(SHTPS_DEF_LPWG_GRIP_PROC_ASYNC_ENABLE)
			if(request == 0){
				shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_GRIP_OFF);
			}else{
				shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_GRIP_ON);
			}
		#else
			shtps_lpwg_grip_set_state(ts, request);
		#endif /* SHTPS_DEF_LPWG_GRIP_PROC_ASYNC_ENABLE */
	#endif /* SHTPS_LPWG_MODE_ENABLE && SHTPS_LPWG_GRIP_SUPPORT_ENABLE */
}

static void shtps_setsleep_proc(struct shtps_rmi_spi *ts, u8 sleep)
{
	SHTPS_LOG_FUNC_CALL();

	if(sleep){
		shtps_func_request_sync(ts, SHTPS_FUNC_REQ_EVENT_LCD_OFF);
	}else{
		shtps_func_request_sync(ts, SHTPS_FUNC_REQ_EVENT_LCD_ON);
	}
}

void shtps_ioctl_setlpwg_proc(struct shtps_rmi_spi *ts, u8 on)
{
	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_LPWG_SWEEP_ON_ENABLE)
		shtps_mutex_lock_ctrl();
		ts->lpwg.lpwg_sweep_on_req_state = on;
		SHTPS_LOG_DBG_PRINT(" [LPWG] lpwg_sweep_on_req_state = %d\n", ts->lpwg.lpwg_sweep_on_req_state);
		shtps_set_lpwg_sleep_check(ts);
		shtps_mutex_unlock_ctrl();
	#endif /* SHTPS_LPWG_SWEEP_ON_ENABLE */
}

void shtps_ioctl_setlpwg_doubletap_proc(struct shtps_rmi_spi *ts, u8 on)
{
	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_LPWG_DOUBLE_TAP_ENABLE)
		shtps_mutex_lock_ctrl();
		ts->lpwg.lpwg_double_tap_req_state = on;
		SHTPS_LOG_DBG_PRINT(" [LPWG] lpwg_double_tap_req_state = %d\n", ts->lpwg.lpwg_double_tap_req_state);
		shtps_set_lpwg_sleep_check(ts);
		shtps_mutex_unlock_ctrl();
	#endif /* SHTPS_LPWG_DOUBLE_TAP_ENABLE */
}

void shtps_ioctl_setlpmode_proc(struct shtps_rmi_spi *ts, u8 on)
{
#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
	SHTPS_LOG_FUNC_CALL();
	shtps_mutex_lock_ctrl();
	shtps_set_lpmode(ts, SHTPS_LPMODE_TYPE_NON_CONTINUOUS, SHTPS_LPMODE_REQ_COMMON, (int)on);
	shtps_mutex_unlock_ctrl();
#endif /* SHTPS_LOW_POWER_MODE_ENABLE */
}

void shtps_ioctl_setconlpmode_proc(struct shtps_rmi_spi *ts, u8 on)
{
#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
	SHTPS_LOG_FUNC_CALL();
	shtps_mutex_lock_ctrl();
	shtps_set_lpmode(ts, SHTPS_LPMODE_TYPE_CONTINUOUS, SHTPS_LPMODE_REQ_ECO, (int)on);
	shtps_mutex_unlock_ctrl();
#endif /* SHTPS_LOW_POWER_MODE_ENABLE */
}

void shtps_ioctl_setlcdbrightlpmode_proc(struct shtps_rmi_spi *ts, u8 on)
{
#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
	SHTPS_LOG_FUNC_CALL();
	shtps_mutex_lock_ctrl();
	shtps_set_lpmode(ts, SHTPS_LPMODE_TYPE_NON_CONTINUOUS, SHTPS_LPMODE_REQ_LCD_BRIGHT, (int)on);
	shtps_mutex_unlock_ctrl();
#endif /* SHTPS_LOW_POWER_MODE_ENABLE */
}

static void shtps_cover_proc(struct shtps_rmi_spi *ts, u8 request)
{
	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_COVER_ENABLE)
		if(request == 0){
			shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_COVER_OFF);
		}else{
			shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_COVER_ON);
		}
	#endif /* SHTPS_COVER_ENABLE */
}

void shtps_ioctl_setglove_proc(struct shtps_rmi_spi *ts, u8 on)
{
	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_GLOVE_DETECT_ENABLE)
		shtps_mutex_lock_ctrl();

		if(on == 0){
			ts->glove_enable_state = 0;

			if(ts->state_mgr.state == SHTPS_STATE_ACTIVE){
				shtps_set_glove_detect_disable(ts);
			}
		}else{
			ts->glove_enable_state = 1;

			if(ts->state_mgr.state == SHTPS_STATE_ACTIVE){
				shtps_set_glove_detect_enable(ts);
			}
		}

		shtps_mutex_unlock_ctrl();
	#endif /* SHTPS_GLOVE_DETECT_ENABLE */
}

void shtps_ioctl_set_high_report_mode_proc(struct shtps_rmi_spi *ts, u8 on)
{
	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_CTRL_FW_REPORT_RATE)
		shtps_mutex_lock_ctrl();

		if(on == 0){
			ts->fw_report_rate_req_state = 0;
		}else{
			ts->fw_report_rate_req_state = 1;
		}
		shtps_set_fw_report_rate(ts);

		shtps_mutex_unlock_ctrl();
	#endif /* SHTPS_CTRL_FW_REPORT_RATE */
}

static void shtps_async_open_proc(struct shtps_rmi_spi *ts, u8 dummy)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_OPEN);
}

static void shtps_async_close_proc(struct shtps_rmi_spi *ts, u8 dummy)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_CLOSE);
}

static void shtps_async_enable_proc(struct shtps_rmi_spi *ts, u8 dummy)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_ENABLE);
}

static void shtps_suspend_spi_wake_lock(struct shtps_rmi_spi *ts, u8 lock)
{
	SHTPS_LOG_FUNC_CALL();
	if(lock){
		if(ts->deter_suspend_spi.wake_lock_state == 0){
			ts->deter_suspend_spi.wake_lock_state = 1;
			wake_lock(&ts->deter_suspend_spi.wake_lock);
			pm_qos_update_request(&ts->deter_suspend_spi.pm_qos_lock_idle, SHTPS_QOS_LATENCY_DEF_VALUE);
		    SHTPS_LOG_DBG_PRINT("[suspend spi] wake_lock\n");
		}
	}else{
		if(ts->deter_suspend_spi.wake_lock_state == 1){
			ts->deter_suspend_spi.wake_lock_state = 0;
			wake_unlock(&ts->deter_suspend_spi.wake_lock);
			pm_qos_update_request(&ts->deter_suspend_spi.pm_qos_lock_idle, PM_QOS_DEFAULT_VALUE);
		    SHTPS_LOG_DBG_PRINT("[suspend spi] wake_unlock\n");
		}
	}
}

int shtps_check_suspend_state(struct shtps_rmi_spi *ts, int proc, u8 param)
{
	int ret = 0;
	
	SHTPS_LOG_FUNC_CALL();

	shtps_mutex_lock_proc();
	if(ts->deter_suspend_spi.suspend){
		shtps_suspend_spi_wake_lock(ts, 1);
		ts->deter_suspend_spi.pending_info[proc].pending= 1;
		ts->deter_suspend_spi.pending_info[proc].param  = param;
		ret = 1;
	}else{
		ts->deter_suspend_spi.pending_info[proc].pending= 0;
		ret = 0;
	}

	if(proc == SHTPS_DETER_SUSPEND_SPI_PROC_OPEN ||
		proc == SHTPS_DETER_SUSPEND_SPI_PROC_ENABLE)
	{
		if(ts->deter_suspend_spi.pending_info[SHTPS_DETER_SUSPEND_SPI_PROC_CLOSE].pending){
		    SHTPS_LOG_DBG_PRINT("[suspend spi] Pending flag of TPS Close Reqeust clear\n");
			ts->deter_suspend_spi.pending_info[SHTPS_DETER_SUSPEND_SPI_PROC_CLOSE].pending = 0;
		}
	}else if(proc == SHTPS_DETER_SUSPEND_SPI_PROC_CLOSE){
		if(ts->deter_suspend_spi.pending_info[SHTPS_DETER_SUSPEND_SPI_PROC_OPEN].pending){
		    SHTPS_LOG_DBG_PRINT("[suspend spi] Pending flag of TPS Open Reqeust clear\n");
			ts->deter_suspend_spi.pending_info[SHTPS_DETER_SUSPEND_SPI_PROC_OPEN].pending  = 0;
		}
		if(ts->deter_suspend_spi.pending_info[SHTPS_DETER_SUSPEND_SPI_PROC_ENABLE].pending){
		    SHTPS_LOG_DBG_PRINT("[suspend spi] Pending flag of TPS Enable Reqeust clear\n");
			ts->deter_suspend_spi.pending_info[SHTPS_DETER_SUSPEND_SPI_PROC_ENABLE].pending= 0;
		}
	}
	shtps_mutex_unlock_proc();
	return ret;
}

#ifdef SHTPS_DEVELOP_MODE_ENABLE
static void shtps_exec_suspend_pending_proc_delayed(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
    SHTPS_LOG_DBG_PRINT("%s() cancel_delayed_work()\n", __func__);
	cancel_delayed_work(&ts->deter_suspend_spi.pending_proc_work_delay);

    SHTPS_LOG_DBG_PRINT("%s() schedule_delayed_work(%d ms)\n", __func__, SHTPS_SUSPEND_SPI_RESUME_FUNC_DELAY);
	schedule_delayed_work(&ts->deter_suspend_spi.pending_proc_work_delay, 
							msecs_to_jiffies(SHTPS_SUSPEND_SPI_RESUME_FUNC_DELAY));

    SHTPS_LOG_DBG_PRINT("%s() done\n", __func__);
}

static void shtps_deter_suspend_spi_pending_proc_delayed_work_function(struct work_struct *work)
{
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct shtps_deter_suspend_spi *dss = container_of(dw, struct shtps_deter_suspend_spi, pending_proc_work_delay);
	struct shtps_rmi_spi *ts = container_of(dss, struct shtps_rmi_spi, deter_suspend_spi);

	SHTPS_LOG_FUNC_CALL();

	schedule_work(&ts->deter_suspend_spi.pending_proc_work);
	return;
}
#endif /* SHTPS_DEVELOP_MODE_ENABLE */

static void shtps_exec_suspend_pending_proc(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();

	cancel_work_sync(&ts->deter_suspend_spi.pending_proc_work);

	#ifdef SHTPS_DEVELOP_MODE_ENABLE
		if(SHTPS_SUSPEND_SPI_RESUME_FUNC_DELAY > 0){
			shtps_exec_suspend_pending_proc_delayed(ts);
		}else{
			schedule_work(&ts->deter_suspend_spi.pending_proc_work);
		}
	#else /* SHTPS_DEVELOP_MODE_ENABLE */
		schedule_work(&ts->deter_suspend_spi.pending_proc_work);
	#endif /* SHTPS_DEVELOP_MODE_ENABLE */

    SHTPS_LOG_DBG_PRINT("%s() done\n", __func__);
}

static void shtps_deter_suspend_spi_pending_proc_work_function(struct work_struct *work)
{
	struct shtps_deter_suspend_spi *dss = container_of(work, struct shtps_deter_suspend_spi, pending_proc_work);
	struct shtps_rmi_spi *ts = container_of(dss, struct shtps_rmi_spi, deter_suspend_spi);
	int i;

	SHTPS_LOG_FUNC_CALL();

	shtps_mutex_lock_proc();

	for(i = 0;i < SHTPS_DETER_SUSPEND_SPI_PROC_NUM;i++){
		if(ts->deter_suspend_spi.pending_info[i].pending){
			SHTPS_SUSPEND_PENDING_FUNC_TBL[i](ts, ts->deter_suspend_spi.pending_info[i].param);
			ts->deter_suspend_spi.pending_info[i].pending = 0;
		}
	}
	shtps_suspend_spi_wake_lock(ts, 0);

	shtps_mutex_unlock_proc();
}

void shtps_set_suspend_state(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();

	shtps_mutex_lock_proc();
	ts->deter_suspend_spi.suspend = 1;
	shtps_mutex_unlock_proc();
}

void shtps_clr_suspend_state(struct shtps_rmi_spi *ts)
{
	int i;
	int hold_process = 0;
	
	SHTPS_LOG_FUNC_CALL();

	shtps_mutex_lock_proc();
	ts->deter_suspend_spi.suspend = 0;
	for(i = 0;i < SHTPS_DETER_SUSPEND_SPI_PROC_NUM;i++){
		if(ts->deter_suspend_spi.pending_info[i].pending){
			hold_process = 1;
			break;
		}
	}

	if(hold_process){
		shtps_exec_suspend_pending_proc(ts);
	}else{
		shtps_suspend_spi_wake_lock(ts, 0);
	}
	shtps_mutex_unlock_proc();

	shtps_mutex_lock_ctrl();
	if(ts->deter_suspend_spi.suspend_irq_detect != 0){
		#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
			if(ts->deter_suspend_spi.suspend_irq_state == SHTPS_IRQ_STATE_ENABLE){
				SHTPS_LOG_DBG_PRINT("[suspend_spi] irq wake enable\n");
				SHTPS_LOG_DBG_PRINT("[suspend_spi] irq enable\n");
				shtps_irq_enable(ts);
			}
		#else
			if(ts->deter_suspend_spi.suspend_irq_wake_state == SHTPS_IRQ_WAKE_ENABLE){
				SHTPS_LOG_DBG_PRINT("[suspend_spi] irq wake enable\n");
				shtps_irq_wake_enable(ts);
			}
			if(ts->deter_suspend_spi.suspend_irq_state == SHTPS_IRQ_STATE_ENABLE){
				SHTPS_LOG_DBG_PRINT("[suspend_spi] irq enable\n");
				shtps_irq_enable(ts);
			}
		#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

		ts->deter_suspend_spi.suspend_irq_detect = 0;
	}
	shtps_mutex_unlock_ctrl();
}
#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */

/* -----------------------------------------------------------------------------------
 */
#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
int shtps_get_fingermax(struct shtps_rmi_spi *ts);

static int shtps_get_facetouchmode(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	return ts->facetouch.mode;
}

static void shtps_set_facetouchmode(struct shtps_rmi_spi *ts, int mode)
{
	SHTPS_LOG_FUNC_CALL_INPARAM(mode);
	_log_msg_sync( LOGMSG_ID__SET_FACETOUCH_MODE, "%d", mode);
	ts->facetouch.mode = mode;
	if(mode == 0){
		ts->facetouch.detect = 0;
	}
}

static void shtps_facetouch_init(struct shtps_rmi_spi *ts)
{
	ts->facetouch.mode = 0;
	ts->facetouch.state = 0;
	ts->facetouch.detect = 0;
	ts->facetouch.palm_thresh = -1;
	ts->facetouch.wake_sig = 0;
	ts->facetouch.palm_det = 0;
	ts->facetouch.wakelock_state = 0;
	ts->facetouch.touch_num = 0;

	init_waitqueue_head(&ts->facetouch.wait_off);
    wake_lock_init(&ts->facetouch.wake_lock, WAKE_LOCK_SUSPEND, "shtps_facetouch_wake_lock");
	ts->facetouch.qos_cpu_latency.type = PM_QOS_REQ_ALL_CORES;
	pm_qos_add_request(&ts->facetouch.qos_cpu_latency, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
}

void shtps_facetouch_wakelock(struct shtps_rmi_spi *ts, u8 on)
{
	if(on){
		if(ts->state_mgr.state == SHTPS_STATE_SLEEP_FACETOUCH){
			mutex_lock(&shtps_facetouch_qosctrl_lock);
			if(ts->facetouch.wakelock_state == 0){
				SHTPS_LOG_DBG_PRINT("face touch wake_lock on\n");
				wake_lock(&ts->facetouch.wake_lock);
				pm_qos_update_request(&ts->facetouch.qos_cpu_latency, SHTPS_QOS_LATENCY_DEF_VALUE);
				ts->facetouch.wakelock_state = 1;
			}
			mutex_unlock(&shtps_facetouch_qosctrl_lock);
		}
	}else{
		mutex_lock(&shtps_facetouch_qosctrl_lock);
		if(ts->facetouch.wakelock_state != 0){
			SHTPS_LOG_DBG_PRINT("face touch wake_lock off\n");
			wake_unlock(&ts->facetouch.wake_lock);
			pm_qos_update_request(&ts->facetouch.qos_cpu_latency, PM_QOS_DEFAULT_VALUE);
			ts->facetouch.wakelock_state = 0;
		}
		mutex_unlock(&shtps_facetouch_qosctrl_lock);
	}
}

static void shtps_event_all_cancel(struct shtps_rmi_spi *ts)
{
	int	i;
	int isEvent = 0;
	u8 	fingerMax = shtps_get_fingermax(ts);

	SHTPS_LOG_FUNC_CALL();

	for(i = 0;i < fingerMax;i++){
		if(ts->report_info.fingers[i].state != 0x00){
			isEvent = 1;
			shtps_report_touch_on(ts, i,
								  SHTPS_TOUCH_CANCEL_COORDINATES_X,
								  SHTPS_TOUCH_CANCEL_COORDINATES_Y,
								  1,
								  ts->report_info.fingers[i].wx,
								  ts->report_info.fingers[i].wy,
								  ts->report_info.fingers[i].z);
		}
	}
	if(isEvent){
		input_sync(ts->input);
		
		for(i = 0;i < fingerMax;i++){
			if(ts->report_info.fingers[i].state != 0x00){
				isEvent = 1;
				shtps_report_touch_off(ts, i,
									  SHTPS_TOUCH_CANCEL_COORDINATES_X,
									  SHTPS_TOUCH_CANCEL_COORDINATES_Y,
									  0,
									  ts->report_info.fingers[i].wx,
									  ts->report_info.fingers[i].wy,
									  ts->report_info.fingers[i].z);
			}
		}
		ts->touch_state.numOfFingers = 0;
		memset(&ts->report_info, 0, sizeof(ts->report_info));
	}
}

static int shtps_chck_palm(struct shtps_rmi_spi *ts)
{
	int i;
	u8 	fingerMax = shtps_get_fingermax(ts);
	for (i = 0; i < fingerMax; i++) {
		if(ts->fw_report_info.fingers[i].state == SHTPS_TOUCH_STATE_PALM){
			SHTPS_LOG_DBG_PRINT("Detect palm :id[%d]\n",i);
			return 1;
		}
	}
	return 0;
}

static void shtps_notify_facetouch(struct shtps_rmi_spi *ts)
{
	if(ts->facetouch.state == 0){
		shtps_facetouch_wakelock(ts, 1);
		ts->facetouch.state = 1;
		ts->facetouch.detect = 1;
		wake_up_interruptible(&ts->facetouch.wait_off);
		SHTPS_LOG_DBG_PRINT("face touch detect. wake_up()\n");
	}else{
		SHTPS_LOG_DBG_PRINT("face touch detect but pre-state isn't face off touch.\n");
	}
}

static void shtps_notify_facetouchoff(struct shtps_rmi_spi *ts, int force)
{
	if(ts->facetouch.state != 0 || force != 0){
		shtps_facetouch_wakelock(ts, 1);
		ts->facetouch.state = 0;
		ts->facetouch.detect = 1;
		wake_up_interruptible(&ts->facetouch.wait_off);
		SHTPS_LOG_DBG_PRINT("face touch off detect(force flag = %d). wake_up()\n", force);
	}else{
		SHTPS_LOG_DBG_PRINT("touch off detect but pre-state isn't face touch.\n");
	}
}

static void shtps_check_facetouch(struct shtps_rmi_spi *ts, struct shtps_touch_info *info)
{
	int i;
	u8 	palm_det  = 0;
	u8 	fingerMax = shtps_get_fingermax(ts);
	u8	numOfFingers = 0;

	SHTPS_LOG_DBG_PRINT("%s() gesture flag = 0x%02x\n", __func__, info->gs1);
	
	if(SHTPS_FACETOUCH_DETECT_CHECK_PALMFLAG == 1 && shtps_chck_palm(ts) == 1){
		SHTPS_LOG_DBG_PRINT("Detect palm touch by palm flag.\n");
		palm_det = 1;
	}else{
		for(i = 0;i < fingerMax;i++){
			if(info->fingers[i].state == SHTPS_TOUCH_STATE_FINGER){
				if(SHTPS_FACETOUCH_DETECT_CHECK_FINGERWIDTH == 1){
					if( (ts->fw_report_info.fingers[i].wx >= SHTPS_FACETOUCH_DETECT_PALMDET_W_THRESHOLD) ||
						(ts->fw_report_info.fingers[i].wy >= SHTPS_FACETOUCH_DETECT_PALMDET_W_THRESHOLD) )
					{
						SHTPS_LOG_DBG_PRINT("Detect palm touch by finger width.\n");
						palm_det = 1;
						break;
					}
				}
				numOfFingers++;
			}
		}
	}
	
	if(SHTPS_FACETOUCH_DETECT_CHECK_MULTITOUCH == 1 && numOfFingers >= SHTPS_FACETOUCH_DETECT_CHECK_MULTITOUCH_FINGER_NUM){
		SHTPS_LOG_DBG_PRINT("Detect palm touch by multi-touch.\n");
		palm_det = 1;
	}
	
	if(palm_det != 0 && ts->facetouch.state == 0){
		shtps_notify_facetouch(ts);
	}
	
	if(ts->facetouch.palm_det != 0){
		if(palm_det == 0 && ts->fw_report_info.finger_num == 0){
			ts->facetouch.palm_det = 0;
		}
	}else{
		ts->facetouch.palm_det = palm_det;
	}
}

static void shtps_check_facetouchoff(struct shtps_rmi_spi *ts)
{
	if(ts->facetouch.palm_det == 0 && ts->facetouch.state != 0){
		shtps_notify_facetouchoff(ts, 0);
	}
}

static void shtps_facetouch_forcecal_handle(struct shtps_rmi_spi *ts)
{
	ts->facetouch.palm_det = 0;
	if(ts->facetouch.state != 0){
		shtps_notify_facetouchoff(ts, 0);
	}
}

static void shtps_read_touchevent_infacetouchmode(struct shtps_rmi_spi *ts)
{
	u8 buf[2 + SHTPS_FINGER_MAX * 8];
	u8 i;
	struct shtps_touch_info info;
	u8 *pFingerInfoBuf;
	u8 irq_sts, ext_sts;
	u8 fingerMax = shtps_get_fingermax(ts);
	int numOfFingers = 0;

	SHTPS_LOG_FUNC_CALL();



	memset(buf, 0, sizeof(buf));
	shtps_fwctl_get_fingerinfo(ts, buf, 0, &irq_sts, &ext_sts, &pFingerInfoBuf);
	shtps_set_touch_info(ts, pFingerInfoBuf, &info);

	{
		u8 buf[2] = {0xFF};
		shtps_fwctl_get_ObjectAttention(ts, buf);
		ts->finger_state[0] = buf[0];
		if (fingerMax > 8){
			ts->finger_state[1] = buf[1];
		}
		SHTPS_LOG_DBG_PRINT("[debug]buf[0]:0x%02x  buf[1]:0x%02x\n", ts->finger_state[0], ts->finger_state[1]);
	}





	for (i = 0; i < fingerMax; i++) {
		if(info.fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
			numOfFingers++;
		}
	}
	info.finger_num = numOfFingers;
	
	shtps_fwctl_get_gesture(ts, fingerMax, buf, &info.gs1, &info.gs2);
	
//	SHTPS_LOG_DBG_PRINT("%s() numOfFingers = %d, palm det = 0x%02x\n", __func__, ts->fw_report_info.finger_num, buf[offset + fingerMax * 5]);
	
	shtps_check_facetouch(ts, &info);
	shtps_check_facetouchoff(ts);

	if(SHTPS_FACETOUCH_DETECT_OFF_NOTIFY_BY_ALLTU){
		if( (ts->facetouch.touch_num != 0) && (ts->fw_report_info.finger_num == 0) ){
			if(SHTPS_FACETOUCH_DETECT_CHECK_PALMFLAG == 1){
				if(ts->facetouch.palm_det == 0){
					shtps_notify_facetouchoff(ts, 1);
				}
			}else{
				shtps_notify_facetouchoff(ts, 1);
			}
		}
	}
	
	ts->facetouch.touch_num = info.finger_num;
	
	memcpy(&ts->fw_report_info_store, &ts->fw_report_info, sizeof(ts->fw_report_info));
}

#if defined( SHTPS_FACETOUCH_OFF_DETECT_DOZE_ENABLE )
static int shtps_is_lpmode(struct shtps_rmi_spi *ts)
{
	#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
		if(((ts->lpmode_req_state | ts->lpmode_continuous_req_state) == SHTPS_LPMODE_REQ_NONE) ||
		   ((ts->lpmode_req_state | ts->lpmode_continuous_req_state) == SHTPS_LPMODE_REQ_LCD_BRIGHT)){
			return 0;
		}else{
			return 1;
		}
	#else
		return 0;
	#endif /* #if defined( SHTPS_LOW_POWER_MODE_ENABLE ) */
}
#endif /* #if defined( SHTPS_FACETOUCH_OFF_DETECT_DOZE_ENABLE ) */

#endif /* CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT */

/* -----------------------------------------------------------------------------------
 */
static irqreturn_t shtps_irq_handler(int irq, void *dev_id)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_performance_check(SHTPS_PERFORMANCE_CHECK_STATE_START);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t shtps_irq(int irq, void *dev_id)
{
	struct shtps_rmi_spi	*ts = dev_id;

	SHTPS_LOG_FUNC_CALL();
	#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
		if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_SPI_PROC_IRQ, 0) == 0){
			shtps_irq_proc(ts, 0);
		}else{
			shtps_mutex_lock_ctrl();
			ts->deter_suspend_spi.suspend_irq_state = ts->irq_mgr.state;
			ts->deter_suspend_spi.suspend_irq_wake_state = ts->irq_mgr.wake;
			ts->deter_suspend_spi.suspend_irq_detect = 1;
			SHTPS_LOG_DBG_PRINT("[suspend_spi] irq detect <irq_state:%d><irq_wake_state:%d>\n",
									ts->deter_suspend_spi.suspend_irq_state, ts->deter_suspend_spi.suspend_irq_wake_state);
			SHTPS_LOG_DBG_PRINT("[suspend_spi] irq disable\n");
			SHTPS_LOG_DBG_PRINT("[suspend_spi] irq wake disable\n");
			#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
				shtps_irq_disable(ts);
			#else
				shtps_irq_wake_disable(ts);
				shtps_irq_disable(ts);
			#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */
			shtps_mutex_unlock_ctrl();
		}
	#else
		shtps_performance_check(SHTPS_PERFORMANCE_CHECK_STATE_CONT);

		shtps_wake_lock_idle(ts);	//SHTPS_CPU_IDLE_SLEEP_CONTROL_ENABLE

		_log_msg_send( LOGMSG_ID__IRQ_NOTIFY, "");
		_log_msg_recv( LOGMSG_ID__IRQ_NOTIFY, "");

		#if defined(SHTPS_LPWG_MODE_ENABLE)
			if(0 != ts->lpwg.lpwg_switch && SHTPS_STATE_SLEEP == ts->state_mgr.state){
				shtps_lpwg_wakelock(ts, 1);
			}
		#endif /* defined(SHTPS_LPWG_MODE_ENABLE) */

		#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
			if(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECT_REZERO_ENABLE != 0){
				shtps_mutex_lock_wakeup_fail_touch_event_reject();
				ts->wakeup_fail_touch_event_reject_rezero_exec = 0;
				shtps_mutex_unlock_wakeup_fail_touch_event_reject();
			}
		#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */

		request_event(ts, SHTPS_EVENT_INTERRUPT, 1);

		#if defined(SHTPS_LPWG_MODE_ENABLE)
			shtps_lpwg_wakelock(ts, 0);
		#endif /* defined(SHTPS_LPWG_MODE_ENABLE) */

		shtps_wake_unlock_idle(ts);	//SHTPS_CPU_IDLE_SLEEP_CONTROL_ENABLE

		shtps_performance_check(SHTPS_PERFORMANCE_CHECK_STATE_END);
	#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */

	return IRQ_HANDLED;
}

/* -----------------------------------------------------------------------------------
 */
static void shtps_work_tmof(struct work_struct *data)
{
	struct delayed_work  *dw = container_of(data, struct delayed_work, work);
	struct shtps_rmi_spi *ts = container_of(dw, struct shtps_rmi_spi, tmo_check);

	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__TIMER_TIMEOUT, "");
	request_event(ts, SHTPS_EVENT_TIMEOUT, 0);
}

static enum hrtimer_restart shtps_delayed_rezero_timer_function(struct hrtimer *timer)
{
	struct shtps_rmi_spi *ts = container_of(timer, struct shtps_rmi_spi, rezero_delayed_timer);

	SHTPS_LOG_FUNC_CALL();
	_log_msg_send( LOGMSG_ID__DELAYED_REZERO_TRIGGER, "");
	schedule_work(&ts->rezero_delayed_work);
	return HRTIMER_NORESTART;
}

static void shtps_rezero_delayed_work_function(struct work_struct *work)
{
	struct shtps_rmi_spi *ts = container_of(work, struct shtps_rmi_spi, rezero_delayed_work);
	u8 rezero_exec = 0;
	int reset_exec = 0;
	int drv_state = SHTPS_STATE_IDLE;

	SHTPS_LOG_FUNC_CALL();
	_log_msg_recv( LOGMSG_ID__DELAYED_REZERO_TRIGGER, "");
	shtps_mutex_lock_ctrl();
	drv_state = ts->state_mgr.state;
	shtps_mutex_unlock_ctrl();

	if(drv_state != SHTPS_STATE_ACTIVE){
		#if defined(SHTPS_LPWG_MODE_ENABLE)
			ts->lpwg.block_touchevent = 0;
			ts->lpwg.tu_rezero_req = 0;
		#endif /* SHTPS_LPWG_MODE_ENABLE */

		#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
			ts->wakeup_touch_event_inhibit_state = 0;
		#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */

		SHTPS_LOG_DBG_PRINT("%s() skip by drv state[%d]\n", __func__, drv_state);

		return;
	}

	shtps_mutex_lock_ctrl();

	#if defined(SHTPS_LPWG_MODE_ENABLE)
		if(ts->lpwg.block_touchevent != 0){
			if((ts->finger_state[0] | ts->finger_state[1] | ts->finger_state[2]) == 0){
				rezero_exec = 1;
				ts->lpwg.block_touchevent = 0;
				ts->lpwg.tu_rezero_req = 0;
				#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
					ts->wakeup_touch_event_inhibit_state = 0;
					SHTPS_LOG_WAKEUP_FAIL_TOUCH_EVENT_REJECT("TouchEvent Inhibit end by rezero exec\n");
				#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */
			}else{
				SHTPS_LOG_DBG_PRINT("LPWG delayed rezero fail by touch state\n");
				ts->lpwg.tu_rezero_req = 1;
			}
		}else{
			rezero_exec = 1;
			ts->lpwg.block_touchevent = 0;
			ts->lpwg.tu_rezero_req = 0;
			#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
				ts->wakeup_touch_event_inhibit_state = 0;
				SHTPS_LOG_WAKEUP_FAIL_TOUCH_EVENT_REJECT("TouchEvent Inhibit end by rezero exec\n");
			#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */
		}
	#else
		rezero_exec = 1;
		#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
			ts->wakeup_touch_event_inhibit_state = 0;
			SHTPS_LOG_WAKEUP_FAIL_TOUCH_EVENT_REJECT("TouchEvent Inhibit end by rezero exec\n");
		#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */
	#endif /* SHTPS_LPWG_MODE_ENABLE */

	if(rezero_exec){
		shtps_rezero(ts);

		#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
			if(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECT_REZERO_ENABLE != 0){
				shtps_mutex_lock_wakeup_fail_touch_event_reject();
				ts->wakeup_fail_touch_event_reject_rezero_exec = 1;
				shtps_mutex_unlock_wakeup_fail_touch_event_reject();
			}
		#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */
	}

	shtps_mutex_unlock_ctrl();

	#if defined(SHTPS_DYNAMIC_RESET_CONTROL_ENABLE)
		if(SHTPS_DYNAMIC_RESET_WHEN_WAKEUP_REZERO_ENABLE != 0){
			reset_exec = shtps_filter_dynamic_reset_wakeup_rezero_process(ts);
		}
	#endif /* SHTPS_DYNAMIC_RESET_CONTROL_ENABLE */

	if(reset_exec != 0){
		#if defined(SHTPS_LPWG_MODE_ENABLE)
			ts->lpwg.block_touchevent = 0;
			ts->lpwg.tu_rezero_req = 0;
		#endif /* SHTPS_LPWG_MODE_ENABLE */

		#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
			ts->wakeup_touch_event_inhibit_state = 0;
			SHTPS_LOG_WAKEUP_FAIL_TOUCH_EVENT_REJECT("TouchEvent Inhibit end by reset exec\n");
		#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */
	}
}

static void shtps_delayed_rezero(struct shtps_rmi_spi *ts, unsigned long delay_us)
{
	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__DELAYED_REZERO_SET, "%lu", delay_us);
	hrtimer_cancel(&ts->rezero_delayed_timer);
	hrtimer_start(&ts->rezero_delayed_timer, ktime_set(0, delay_us * 1000), HRTIMER_MODE_REL);
}

static void shtps_delayed_rezero_cancel(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__DELAYED_REZERO_CANCEL, "");
	hrtimer_try_to_cancel(&ts->rezero_delayed_timer);
}

void shtps_rezero(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	SHTPS_LOG_DBG_PRINT("fw rezero execute\n");
	shtps_fwctl_rezero(ts);
	shtps_event_force_touchup(ts);

	#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
		shtps_facetouch_forcecal_handle(ts);
	#endif /* CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT */

	#if defined( SHTPS_MODULE_PARAM_ENABLE )
		shtps_rezero_state = 1;
	#endif /* SHTPS_MODULE_PARAM_ENABLE */

	#if defined(SHTPS_DEF_RECORD_LOG_FILE_ENABLE)
		shtps_record_log_api_add_log(SHTPS_RECORD_LOG_TYPE_REZERO);
	#endif /* SHTPS_DEF_RECORD_LOG_FILE_ENABLE */

	shtps_report_stflib_kevt(ts, SHTPS_DEF_STFLIB_KEVT_CLEAR_REQUEST, SHTPS_DEF_STFLIB_KEVT_CLEAR_REQUEST_REZERO);
}

void shtps_rezero_request(struct shtps_rmi_spi *ts, u8 request, u8 trigger)
{
	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__REZERO_REQUEST, "%d|%d", request, trigger);
	if(request & SHTPS_REZERO_REQUEST_WAKEUP_REZERO){
		#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
			if(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECT_REZERO_ENABLE != 0){
				ts->wakeup_touch_event_inhibit_state = 1;
				SHTPS_LOG_WAKEUP_FAIL_TOUCH_EVENT_REJECT("TouchEvent Inhibit start until rezero exec\n");
			}
		#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */
		shtps_delayed_rezero(ts, SHTPS_SLEEP_OUT_WAIT_US);
	}
	if(request & SHTPS_REZERO_REQUEST_REZERO){
		shtps_rezero(ts);
	}
}

static void shtps_rezero_handle(struct shtps_rmi_spi *ts, u8 event, u8 gs)
{
}

void shtps_set_startup_min_time(struct shtps_rmi_spi *ts, int time)
{
	SHTPS_LOG_DBG_PRINT("%s() %dms -> %dms\n", __func__, ts->state_mgr.startup_min_time, time);
	ts->state_mgr.startup_min_time = time;
}

static void shtps_reset_startuptime(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	ts->state_mgr.starttime = jiffies + msecs_to_jiffies(ts->state_mgr.startup_min_time);
}

static unsigned long shtps_check_startuptime(struct shtps_rmi_spi *ts)
{
	unsigned long remainingtime;
	SHTPS_LOG_FUNC_CALL();
	if(time_after(jiffies, ts->state_mgr.starttime)){
		return 0;
	}
	remainingtime = jiffies_to_msecs(ts->state_mgr.starttime - jiffies);
	if(remainingtime > ts->state_mgr.startup_min_time){
		remainingtime = ts->state_mgr.startup_min_time;
	}
	return remainingtime;
}

int shtps_start(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	return request_event(ts, SHTPS_EVENT_START, 0);
}

void shtps_shutdown(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	request_event(ts, SHTPS_EVENT_STOP, 0);
}

/* -----------------------------------------------------------------------------------
 */
#if defined(SHTPS_PDT_READ_RETRY_ENABLE)
static int shtps_map_construct(struct shtps_rmi_spi *ts, u8 func_check)
#else
static int shtps_map_construct(struct shtps_rmi_spi *ts)
#endif /* SHTPS_PDT_READ_RETRY_ENABLE */
{
	int rc;
	SHTPS_LOG_FUNC_CALL();
#if defined(SHTPS_PDT_READ_RETRY_ENABLE)
	rc = shtps_fwctl_map_construct(ts, func_check);
#else
	rc = shtps_fwctl_map_construct(ts, 0);
#endif

	return rc;
}

void shtps_reset(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	SHTPS_LOG_ANALYSIS("HW Reset execute\n");
	shtps_device_reset(ts->rst_pin);
}

#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
static u8 shtps_get_lpmode_state(struct shtps_rmi_spi *ts)
{
	u8 req = (ts->lpmode_req_state | ts->lpmode_continuous_req_state);

	#if defined(SHTPS_GLOVE_DETECT_ENABLE)
		if(ts->glove_enable_state == 1){
			if(ts->state_mgr.state == SHTPS_STATE_ACTIVE){
				#if defined(SHTPS_COVER_ENABLE)
					if(ts->cover_state == 0){
						req = SHTPS_LPMODE_REQ_NONE;
					}
				#else
					req = SHTPS_LPMODE_REQ_NONE;
				#endif /* SHTPS_COVER_ENABLE */
			}
		}
	#endif /* SHTPS_GLOVE_DETECT_ENABLE */

	if(SHTPS_LOW_POWER_MODE_LCD_BRIGHT_DOZE_ENABLE == 0){
		req &= ~SHTPS_LPMODE_REQ_LCD_BRIGHT;
	}

	if(SHTPS_LOW_POWER_MODE_DOZE_ENABLE == 0){
		req = SHTPS_LPMODE_REQ_NONE;
	}

	if(req == SHTPS_LPMODE_REQ_NONE){
		return 1;
	}
	return 0;
}
#endif /* SHTPS_LOW_POWER_MODE_ENABLE */

void shtps_sleep(struct shtps_rmi_spi *ts, int on)
{
	SHTPS_LOG_FUNC_CALL_INPARAM(on);

	if(on){
		shtps_delayed_rezero_cancel(ts);
		shtps_fwctl_set_doze(ts);

		shtps_fwctl_set_sleepmode_on(ts);

		#if defined( SHTPS_ACTIVE_SLEEP_WAIT_ALWAYS_ENABLE )
			if(SHTPS_SLEEP_IN_WAIT_MS > 0){
				msleep(SHTPS_SLEEP_IN_WAIT_MS);
			}
		#endif /* SHTPS_ACTIVE_SLEEP_WAIT_ALWAYS_ENABLE */
	}else{
		#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
			if(shtps_get_lpmode_state(ts) != 0){
				shtps_fwctl_set_active(ts);
			}else{
				shtps_fwctl_set_doze(ts);
			}
		#else
			shtps_fwctl_set_active(ts);
		#endif /* #if defined( SHTPS_LOW_POWER_MODE_ENABLE ) */

		shtps_fwctl_set_sleepmode_off(ts);

		#if defined( SHTPS_ACTIVE_SLEEP_WAIT_ALWAYS_ENABLE )
			if(SHTPS_SLEEP_OUT_WAIT_MS > 0){
				msleep(SHTPS_SLEEP_OUT_WAIT_MS);
			}
		#endif /* SHTPS_ACTIVE_SLEEP_WAIT_ALWAYS_ENABLE */
	}

	#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
		SHTPS_LOG_DBG_PRINT("[LPMODE]sleep request recieved. req_state = 0x%02x, continuous_req_state = 0x%02x\n",
								ts->lpmode_req_state, ts->lpmode_continuous_req_state);
	#endif /* #if defined( SHTPS_LOW_POWER_MODE_ENABLE ) */
}

#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
int shtps_check_set_doze_enable(void)
{
	if(SHTPS_LOW_POWER_MODE_CHEK_HW_REV_ENABLE == 0){
		return 1;
	}

	#if defined(SHTPS_CHECK_HWID_ENABLE)
		if(shtps_system_get_hw_type() != SHTPS_HW_TYPE_HANDSET){
			return 0;
		}else{
			{
				u8 hwrev;
				
				hwrev = shtps_system_get_hw_revision();
				
				if(SHTPS_GET_HW_VERSION_RET_ES0_ES1 == hwrev
				){
					return 0;
				}
			}
		}
	#endif /* SHTPS_CHECK_HWID_ENABLE */
	return 1;
}

static void shtps_set_doze_mode(struct shtps_rmi_spi *ts, int on)
{
	SHTPS_LOG_FUNC_CALL_INPARAM(on);

	if(ts->state_mgr.state == SHTPS_STATE_IDLE){
		return;
	}

	if(shtps_fwctl_is_sleeping(ts) != 0){
		return;
	}

	if(on){
		shtps_fwctl_set_doze(ts);
		SHTPS_LOG_DBG_PRINT("doze mode on\n");
	}else{
		shtps_fwctl_set_active(ts);
		SHTPS_LOG_DBG_PRINT("doze mode off\n");
	}
}

static void shtps_set_lpmode_init(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	if(shtps_get_lpmode_state(ts) != 0){
		shtps_set_doze_mode(ts, 0);
	}else{
		shtps_set_doze_mode(ts, 1);
	}

	SHTPS_LOG_DBG_PRINT("[LPMODE]lpmode init. req_state = 0x%02x, continuous_req_state = 0x%02x\n",
							ts->lpmode_req_state, ts->lpmode_continuous_req_state);
}

void shtps_set_lpmode(struct shtps_rmi_spi *ts, int type, int req, int on)
{
	int changed = 0;

	SHTPS_LOG_FUNC_CALL();
	if(on){
		if(type == SHTPS_LPMODE_TYPE_NON_CONTINUOUS){
			if((ts->lpmode_req_state & req) == 0){
				ts->lpmode_req_state |= req;
				changed = 1;
				SHTPS_LOG_DBG_PRINT("[LPMODE]<ON> type = NON CONTINUOUS, req = %s\n",
										(req == SHTPS_LPMODE_REQ_COMMON)? "common" :
										(req == SHTPS_LPMODE_REQ_ECO)? "eco" :
										(req == SHTPS_LPMODE_REQ_LCD_BRIGHT)? "lcd_bright" : "unknown");
			}
		}else{
			if((ts->lpmode_continuous_req_state & req) == 0){
				ts->lpmode_continuous_req_state |= req;
				changed = 1;
				SHTPS_LOG_DBG_PRINT("[LPMODE]<ON> type = CONTINUOUS, req = %s\n",
										(req == SHTPS_LPMODE_REQ_COMMON)? "common" :
										(req == SHTPS_LPMODE_REQ_ECO)? "eco" :
										(req == SHTPS_LPMODE_REQ_LCD_BRIGHT)? "lcd_bright" : "unknown");
			}
		}
	}
	else{
		if(type == SHTPS_LPMODE_TYPE_NON_CONTINUOUS){
			if((ts->lpmode_req_state & req) != 0){
				ts->lpmode_req_state &= ~req;
				changed = 1;
				SHTPS_LOG_DBG_PRINT("[LPMODE]<OFF> type = NON CONTINUOUS, req = %s\n",
										(req == SHTPS_LPMODE_REQ_COMMON)? "common" :
										(req == SHTPS_LPMODE_REQ_ECO)? "eco" :
										(req == SHTPS_LPMODE_REQ_LCD_BRIGHT)? "lcd_bright" : "unknown");
			}
		}else{
			if((ts->lpmode_continuous_req_state & req) != 0){
				ts->lpmode_continuous_req_state &= ~req;
				changed = 1;
				SHTPS_LOG_DBG_PRINT("[LPMODE]<OFF> type = CONTINUOUS, req = %s\n",
										(req == SHTPS_LPMODE_REQ_COMMON)? "common" :
										(req == SHTPS_LPMODE_REQ_ECO)? "eco" :
										(req == SHTPS_LPMODE_REQ_LCD_BRIGHT)? "lcd_bright" : "unknown");
			}
		}
	}

	if(changed){
		if(shtps_get_lpmode_state(ts) != 0){
			if(ts->state_mgr.state == SHTPS_STATE_ACTIVE){
				shtps_set_doze_mode(ts, 0);
			}
		}else{
			shtps_set_doze_mode(ts, 1);
		}
		#if defined(SHTPS_CTRL_FW_REPORT_RATE)
			if((req == SHTPS_LPMODE_REQ_ECO)
				#if defined(SHTPS_DEF_CTRL_FW_REPORT_RATE_LINKED_LCD_BRIGHT_ENABLE)
					|| (req == SHTPS_LPMODE_REQ_LCD_BRIGHT)
				#endif /* SHTPS_DEF_CTRL_FW_REPORT_RATE_LINKED_LCD_BRIGHT_ENABLE */
				){
				shtps_set_fw_report_rate(ts);
			}
		#endif /* SHTPS_CTRL_FW_REPORT_RATE */
	}

	SHTPS_LOG_DBG_PRINT("[LPMODE]lpmode request recieved. req_state = 0x%02x, continuous_req_state = 0x%02x\n",
							ts->lpmode_req_state, ts->lpmode_continuous_req_state);
}
#endif /* #if defined( SHTPS_LOW_POWER_MODE_ENABLE ) */

#if defined(SHTPS_GLOVE_DETECT_ENABLE)
int shtps_set_glove_detect_enable(struct shtps_rmi_spi *ts)
{
	int rc;
	int req = 1;
	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_COVER_ENABLE)
		if(ts->cover_state != 0){
			req = 0;
		}
	#endif /* SHTPS_COVER_ENABLE */

	if(req == 1){
		rc = shtps_fwctl_glove_enable(ts);
		if(!rc){
			shtps_fwctl_set_active(ts);
		}
		return rc;
	}

	return 0;
}

int shtps_set_glove_detect_disable(struct shtps_rmi_spi *ts)
{
	int rc;
	SHTPS_LOG_FUNC_CALL();

	rc = shtps_fwctl_glove_disable(ts);
	if(!rc){
		if(ts->state_mgr.state == SHTPS_STATE_ACTIVE){
			if(shtps_get_lpmode_state(ts) != 0){
				shtps_set_doze_mode(ts, 0);
			}else{
				shtps_set_doze_mode(ts, 1);
			}
		}
	}
	return rc;
}

static int shtps_set_glove_detect_init(struct shtps_rmi_spi *ts)
{
	int rc;
	SHTPS_LOG_FUNC_CALL();

	if(ts->glove_enable_state == 0){
		rc = shtps_set_glove_detect_disable(ts);
	}else{
		rc = shtps_set_glove_detect_enable(ts);
	}

	return rc;
}
#endif /* SHTPS_GLOVE_DETECT_ENABLE */

#if defined(SHTPS_CTRL_FW_REPORT_RATE)
void shtps_set_fw_report_rate(struct shtps_rmi_spi *ts)
{
	int ret;
	u8 set_rate = SHTPS_CTRL_FW_REPORT_RATE_PARAM_NORMAL;

	if(ts->fw_report_rate_req_state == 1){
		set_rate = SHTPS_CTRL_FW_REPORT_RATE_PARAM_HIGH;
	}

	#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
		if( ((ts->lpmode_req_state | ts->lpmode_continuous_req_state) & SHTPS_LPMODE_REQ_ECO) != 0 ){
			set_rate = SHTPS_CTRL_FW_REPORT_RATE_PARAM_NORMAL;
		}
	#endif /* SHTPS_LOW_POWER_MODE_ENABLE */

	#if defined(SHTPS_DEF_CTRL_FW_REPORT_RATE_LINKED_LCD_BRIGHT_ENABLE)
		if( ((ts->lpmode_req_state | ts->lpmode_continuous_req_state) & SHTPS_LPMODE_REQ_LCD_BRIGHT) != 0 ){
			set_rate = SHTPS_CTRL_FW_REPORT_RATE_PARAM_LCD_BRIGHT_LOW;
		}
	#endif /* SHTPS_DEF_CTRL_FW_REPORT_RATE_LINKED_LCD_BRIGHT_ENABLE */

	if(ts->fw_report_rate_cur != set_rate){
		if(ts->state_mgr.state != SHTPS_STATE_IDLE){
			ret = shtps_fwctl_set_custom_report_rate(ts, set_rate);
			if(ret > 0){
				ts->fw_report_rate_cur = ret;
			}
			else{
				ts->fw_report_rate_cur = 0;
			}
		}
	}

	if(ts->fw_report_rate_cur == SHTPS_CTRL_FW_REPORT_RATE_PARAM_NORMAL){
		shtps_report_stflib_kevt(ts, SHTPS_DEF_STFLIB_KEVT_SCANRATE_MODE, SHTPS_CTRL_FW_REPORT_RATE_STFLIB_KEVT_NORMAL);
	}else if(ts->fw_report_rate_cur == SHTPS_CTRL_FW_REPORT_RATE_PARAM_HIGH){
		shtps_report_stflib_kevt(ts, SHTPS_DEF_STFLIB_KEVT_SCANRATE_MODE, SHTPS_CTRL_FW_REPORT_RATE_STFLIB_KEVT_HIGH);
	}else{
		#if defined(SHTPS_DEF_CTRL_FW_REPORT_RATE_LINKED_LCD_BRIGHT_ENABLE)
			if(ts->fw_report_rate_cur == SHTPS_CTRL_FW_REPORT_RATE_PARAM_LCD_BRIGHT_LOW){
				shtps_report_stflib_kevt(ts, SHTPS_DEF_STFLIB_KEVT_SCANRATE_MODE, SHTPS_CTRL_FW_REPORT_RATE_STFLIB_KEVT_LCD_BRIGHT_LOW);
			}
		#endif /* SHTPS_DEF_CTRL_FW_REPORT_RATE_LINKED_LCD_BRIGHT_ENABLE */
	}
}
#endif /* SHTPS_CTRL_FW_REPORT_RATE */

#if defined(SHTPS_LPWG_MODE_ENABLE)
static void shtps_lpwg_notify_interval_delayed_work_function(struct work_struct *work)
{
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct shtps_lpwg_ctrl *lpwg_p = container_of(dw, struct shtps_lpwg_ctrl, notify_interval_delayed_work);

	SHTPS_LOG_FUNC_CALL();

	shtps_mutex_lock_ctrl();
	lpwg_p->notify_enable = 1;
	shtps_mutex_unlock_ctrl();

	SHTPS_LOG_DBG_PRINT("[LPWG] notify interval end\n");
}

static void shtps_lpwg_notify_interval_stop(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();

	cancel_delayed_work(&ts->lpwg.notify_interval_delayed_work);
}

static void shtps_lpwg_notify_interval_start(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();

	shtps_lpwg_notify_interval_stop(ts);
	schedule_delayed_work(&ts->lpwg.notify_interval_delayed_work, msecs_to_jiffies(SHTPS_LPWG_MIN_NOTIFY_INTERVAL));

	SHTPS_LOG_DBG_PRINT("[LPWG] notify interval start\n");
}

static void shtps_lpwg_wakelock_init(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	memset(&ts->lpwg, 0, sizeof(ts->lpwg));

	ts->lpwg.notify_enable = 1;
	ts->lpwg.lpwg_switch = 0;
	ts->lpwg.tu_rezero_req = 0;
	ts->lpwg.block_touchevent = 0;

	#if defined(SHTPS_PROXIMITY_SUPPORT_ENABLE)
		ts->lpwg_proximity_get_data = -1;
	#endif /* SHTPS_PROXIMITY_SUPPORT_ENABLE */

	#if defined(SHTPS_LPWG_SWEEP_ON_ENABLE)
		#if defined(SHTPS_LPWG_GRIP_SUPPORT_ENABLE)
			ts->lpwg.lpwg_sweep_on_req_state  = SHTPS_LPWG_SWEEP_ON_REQ_GRIP_ONLY;
		#else
			ts->lpwg.lpwg_sweep_on_req_state  = SHTPS_LPWG_SWEEP_ON_REQ_OFF;
		#endif /* SHTPS_LPWG_GRIP_SUPPORT_ENABLE */
	#endif /* SHTPS_LPWG_SWEEP_ON_ENABLE */

    wake_lock_init(&ts->lpwg.wake_lock, WAKE_LOCK_SUSPEND, "shtps_lpwg_wake_lock");
	ts->lpwg.pm_qos_lock_idle.type = PM_QOS_REQ_ALL_CORES;
	pm_qos_add_request(&ts->lpwg.pm_qos_lock_idle, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
	ts->lpwg.pm_qos_idle_value = SHTPS_LPWG_QOS_LATENCY_DEF_VALUE;
	INIT_DELAYED_WORK(&ts->lpwg.notify_interval_delayed_work, shtps_lpwg_notify_interval_delayed_work_function);
}

static void shtps_lpwg_wakelock_destroy(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	wake_lock_destroy(&ts->lpwg.wake_lock);
	pm_qos_remove_request(&ts->lpwg.pm_qos_lock_idle);
}

static void shtps_lpwg_wakelock(struct shtps_rmi_spi *ts, int on)
{
	SHTPS_LOG_FUNC_CALL();
	if(on){
	    wake_lock(&ts->lpwg.wake_lock);
	    pm_qos_update_request(&ts->lpwg.pm_qos_lock_idle, ts->lpwg.pm_qos_idle_value);
		SHTPS_LOG_DBG_PRINT("lpwg wake lock ON\n");
	}else{
		wake_unlock(&ts->lpwg.wake_lock);
	    pm_qos_update_request(&ts->lpwg.pm_qos_lock_idle, PM_QOS_DEFAULT_VALUE);
		SHTPS_LOG_DBG_PRINT("lpwg wake lock OFF\n");
	}
}

void shtps_set_lpwg_mode_on(struct shtps_rmi_spi *ts)
{
	if(ts->lpwg.lpwg_switch == 1){
		return;
	}

	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_PROXIMITY_SUPPORT_ENABLE)
		ts->lpwg_proximity_get_data = -1;
	#endif /* SHTPS_PROXIMITY_SUPPORT_ENABLE */

	shtps_lpwg_notify_interval_stop(ts);
	ts->lpwg.notify_enable = 1;
	ts->lpwg.lpwg_switch = 1;
	ts->lpwg.is_notified = 0;
	ts->lpwg.tu_rezero_req = 0;
	ts->lpwg.block_touchevent = 0;

	#if defined( SHTPS_HOST_LPWG_MODE_ENABLE )
		if(shtps_check_host_lpwg_enable() == 1){
			int i;
			for(i = 0; i < SHTPS_FINGER_MAX; i++){
				ts->lpwg.pre_info.fingers[i].state = SHTPS_TOUCH_STATE_NO_TOUCH;
				ts->lpwg.pre_info.fingers[i].x     = 0xFFFF;
				ts->lpwg.pre_info.fingers[i].y     = 0xFFFF;
			}
			ts->lpwg.swipe_check_time = 0;
		}
	#endif /* SHTPS_HOST_LPWG_MODE_ENABLE */

	shtps_fwctl_set_lpwg_mode_on(ts);

	SHTPS_LOG_DBG_PRINT("LPWG mode ON\n");
}

void shtps_set_lpwg_mode_off(struct shtps_rmi_spi *ts)
{
	if(ts->lpwg.lpwg_switch == 0){
		return;
	}
	SHTPS_LOG_FUNC_CALL();

	ts->lpwg.lpwg_switch = 0;

	shtps_fwctl_set_lpwg_mode_off(ts);

	SHTPS_LOG_DBG_PRINT("LPWG mode OFF\n");
}

static void shtps_set_lpwg_mode_cal(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_fwctl_set_lpwg_mode_cal(ts);
	if(SHTPS_LPWG_MODE_ON_AFTER_REZERO_ENABLE){
		shtps_rezero_request(ts, SHTPS_REZERO_REQUEST_REZERO, 0);
	}
}

int shtps_is_lpwg_active(struct shtps_rmi_spi *ts)
{
	int ret = SHTPS_LPWG_REQ_NONE;
	
	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_LPWG_SWEEP_ON_ENABLE)
		if(ts->lpwg.lpwg_sweep_on_req_state == SHTPS_LPWG_SWEEP_ON_REQ_ON){
			ret |= SHTPS_LPWG_REQ_SWEEP_ON;
		}
		#if defined(SHTPS_LPWG_GRIP_SUPPORT_ENABLE)
			else if(ts->lpwg.lpwg_sweep_on_req_state == SHTPS_LPWG_SWEEP_ON_REQ_GRIP_ONLY){
				if(ts->lpwg.grip_state){
					ret |= SHTPS_LPWG_REQ_SWEEP_ON;
				}
			}
		#endif /* SHTPS_LPWG_GRIP_SUPPORT_ENABLE */
	#endif /* SHTPS_LPWG_SWEEP_ON_ENABLE */

	if(ts->lpwg.lpwg_double_tap_req_state != 0){
		ret |= SHTPS_LPWG_REQ_DOUBLE_TAP;
	}

	#if defined(SHTPS_ALWAYS_ON_DISPLAY_ENABLE)
		if(ts->lpwg.lpwg_double_tap_aod_req_state != 0){
			ret |= SHTPS_LPWG_REQ_DOUBLE_TAP;
		}
	#endif /* SHTPS_ALWAYS_ON_DISPLAY_ENABLE */

	#if defined(SHTPS_COVER_ENABLE)
		if(ts->cover_state != 0){
			ret = SHTPS_LPWG_REQ_NONE;
		}
	#endif /* SHTPS_COVER_ENABLE */

	return ret;
}

int shtps_set_lpwg_sleep_check(struct shtps_rmi_spi *ts)
{
	u8 lpwg_req;
	u8 switch_state;
	u8 on_req;
	u8 off_req;
	u8 lpwg_set_state_old;
	u8 doze_param[3];

	SHTPS_LOG_FUNC_CALL();

	if(ts->state_mgr.state != SHTPS_STATE_SLEEP){
		return 0;
	}

	lpwg_set_state_old = ts->lpwg.lpwg_set_state;
	lpwg_req = shtps_is_lpwg_active(ts);
	switch_state = ts->lpwg.lpwg_set_state ^ lpwg_req;
	on_req = switch_state & lpwg_req;
	off_req = switch_state & ts->lpwg.lpwg_set_state;

	/* --- */
	if( (ts->lpwg.lpwg_set_state == SHTPS_LPWG_SET_STATE_OFF) &&
		(on_req != SHTPS_LPWG_REQ_NONE) )
	{
		#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
			shtps_irq_enable(ts);
		#else
			shtps_irq_wake_enable(ts);
		#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

		if( shtps_fwctl_get_dev_state(ts) == SHTPS_DEV_STATE_SLEEP ){
			shtps_system_set_wakeup(ts);
		}
	}

	/* --- */
	#if defined(SHTPS_LPWG_SWEEP_ON_ENABLE)
		if( (on_req & SHTPS_LPWG_REQ_SWEEP_ON) != 0 ){
			SHTPS_LOG_DBG_PRINT("[LPWG] sweep on enable\n");
			shtps_fwctl_set_lpwg_sweep_on(ts, 1);
			ts->lpwg.lpwg_set_state |= SHTPS_LPWG_SET_STATE_SWEEP_ON;
		}
		if( (off_req & SHTPS_LPWG_REQ_SWEEP_ON) != 0 ){
			SHTPS_LOG_DBG_PRINT("[LPWG] sweep on disable\n");
			shtps_fwctl_set_lpwg_sweep_on(ts, 0);
			ts->lpwg.lpwg_set_state &= (~SHTPS_LPWG_SET_STATE_SWEEP_ON);
		}
	#endif /* SHTPS_LPWG_SWEEP_ON_ENABLE */

	/* --- */
	if( (on_req & SHTPS_LPWG_REQ_DOUBLE_TAP) != 0 ){
		SHTPS_LOG_DBG_PRINT("[LPWG] double tap enable\n");
		shtps_fwctl_set_lpwg_double_tap(ts, 1);
		ts->lpwg.lpwg_set_state |= SHTPS_LPWG_SET_STATE_DOUBLE_TAP;
	}
	if( (off_req & SHTPS_LPWG_REQ_DOUBLE_TAP) != 0 ){
		SHTPS_LOG_DBG_PRINT("[LPWG] double tap disable\n");
		shtps_fwctl_set_lpwg_double_tap(ts, 0);
		ts->lpwg.lpwg_set_state &= (~SHTPS_LPWG_SET_STATE_DOUBLE_TAP);
	}

	/* --- */
	if(ts->lpwg.lpwg_set_state == SHTPS_LPWG_SET_STATE_OFF){
		if(lpwg_set_state_old != SHTPS_LPWG_SET_STATE_OFF){
			doze_param[0] = SHTPS_LPWG_DOZE_INTERVAL_DEF;
			doze_param[1] = SHTPS_LPWG_DOZE_WAKEUP_THRESHOLD_DEF;
			doze_param[2] = SHTPS_LPWG_DOZE_HOLDOFF_DEF;
			shtps_fwctl_set_doze_param(ts, doze_param, sizeof(doze_param));
		}

		shtps_set_lpwg_mode_off(ts);
		shtps_sleep(ts, 1);
		shtps_system_set_sleep(ts);

		#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
			shtps_irq_disable(ts);
		#else
			shtps_irq_wake_disable(ts);
		#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */
	}
	else{
		if( (on_req != SHTPS_LPWG_REQ_NONE) || (off_req != SHTPS_LPWG_REQ_NONE) ){
			doze_param[0] = SHTPS_LPWG_DOZE_INTERVAL;
			doze_param[1] = SHTPS_LPWG_DOZE_WAKEUP_THRESHOLD;
			doze_param[2] = SHTPS_LPWG_DOZE_HOLDOFF;

		#if defined(SHTPS_LPWG_SWEEP_ON_ENABLE)
			if(lpwg_req == SHTPS_LPWG_REQ_SWEEP_ON){
				doze_param[2] = SHTPS_LPWG_DOZE_HOLDOFF_SWEEP_ON;
			}else if(lpwg_req == SHTPS_LPWG_REQ_DOUBLE_TAP){
		#else /* SHTPS_LPWG_SWEEP_ON_ENABLE */
			if(lpwg_req == SHTPS_LPWG_REQ_DOUBLE_TAP){
		#endif /* SHTPS_LPWG_SWEEP_ON_ENABLE */
				doze_param[2] = SHTPS_LPWG_DOZE_HOLDOFF_DOUBLE_TAP;
			}else{
				doze_param[2] = SHTPS_LPWG_DOZE_HOLDOFF;
			}

			shtps_fwctl_set_doze_param(ts, doze_param, sizeof(doze_param));
		}

		if(lpwg_set_state_old == SHTPS_LPWG_SET_STATE_OFF){
			shtps_set_lpwg_mode_on(ts);
			shtps_set_lpwg_mode_cal(ts);
		}
	}

	return 0;
}

int shtps_set_lpwg_wakeup_check(struct shtps_rmi_spi *ts)
{
	u8 doze_param[3] = {SHTPS_LPWG_DOZE_INTERVAL_DEF,
						SHTPS_LPWG_DOZE_WAKEUP_THRESHOLD_DEF,
						SHTPS_LPWG_DOZE_HOLDOFF_DEF};

	SHTPS_LOG_FUNC_CALL();

	if(ts->lpwg.lpwg_set_state != SHTPS_LPWG_SET_STATE_OFF){
		#if defined(SHTPS_LPWG_SWEEP_ON_ENABLE)
			if( (ts->lpwg.lpwg_set_state & SHTPS_LPWG_SET_STATE_SWEEP_ON) != 0 ){
				SHTPS_LOG_DBG_PRINT("[LPWG] sweep on disable\n");
				shtps_fwctl_set_lpwg_sweep_on(ts, 0);
				ts->lpwg.lpwg_set_state &= (~SHTPS_LPWG_SET_STATE_SWEEP_ON);
			}
		#endif /* SHTPS_LPWG_SWEEP_ON_ENABLE */

		if( (ts->lpwg.lpwg_set_state & SHTPS_LPWG_REQ_DOUBLE_TAP) != 0 ){
			SHTPS_LOG_DBG_PRINT("[LPWG] double tap disable\n");
			shtps_fwctl_set_lpwg_double_tap(ts, 0);
			ts->lpwg.lpwg_set_state &= (~SHTPS_LPWG_REQ_DOUBLE_TAP);
		}

		shtps_fwctl_set_doze_param(ts, doze_param, sizeof(doze_param));
		shtps_set_lpwg_mode_off(ts);
		shtps_read_touchevent(ts, SHTPS_STATE_IDLE);
		ts->lpwg.wakeup_time = jiffies;
	}else{
		shtps_system_set_wakeup(ts);
	}

	return 0;
}

#if defined(SHTPS_LPWG_GRIP_SUPPORT_ENABLE)
void shtps_lpwg_grip_set_state(struct shtps_rmi_spi *ts, u8 request)
{
	shtps_mutex_lock_ctrl();

	ts->lpwg.grip_state = request;
	SHTPS_LOG_DBG_PRINT("[LPWG] grip_state = %d\n", ts->lpwg.grip_state);
	shtps_set_lpwg_sleep_check(ts);
	shtps_mutex_unlock_ctrl();
}
#endif /* SHTPS_LPWG_GRIP_SUPPORT_ENABLE */

#endif /* SHTPS_LPWG_MODE_ENABLE */

#if defined(SHTPS_COVER_ENABLE)
static int shtps_cover_status_handler_func(struct notifier_block *nb,
					    unsigned long on, void *unused);

static struct notifier_block cover_status_handler = {
	.notifier_call = shtps_cover_status_handler_func,
};

static void shtps_set_cover_mode_on(struct shtps_rmi_spi *ts)
{
	#if defined(SHTPS_DEF_COVER_CHANGE_REPORT_NUM_MAX_ENABLE)
		shtps_fwctl_cover_set_report_num_max(ts);
	#endif /* SHTPS_DEF_COVER_CHANGE_REPORT_NUM_MAX_ENABLE */

	#if defined(SHTPS_DEF_COVER_CHANGE_FINGER_AMP_THRESH_ENABLE)
		if(ts->cover_change_finger_amp_thresh_for_film == 1){
			shtps_fwctl_cover_set_finger_amplitude_threshold(ts, SHTPS_REG_PRM_COVER_FINGER_AMP_THRESH_FOR_FILM);
		}
		else{
			shtps_fwctl_cover_set_finger_amplitude_threshold(ts, SHTPS_REG_PRM_COVER_FINGER_AMP_THRESH);
		}
	#endif /* SHTPS_DEF_COVER_CHANGE_FINGER_AMP_THRESH_ENABLE */

	shtps_fwctl_cover_mode_on(ts);
}
static void shtps_set_cover_mode_off(struct shtps_rmi_spi *ts)
{
	shtps_fwctl_cover_mode_off(ts);
}
static void shtps_set_cover_mode_init(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL_INPARAM(ts->cover_state);

	if(ts->cover_state == 0){
		shtps_set_cover_mode_off(ts);
	}else{
		shtps_set_cover_mode_on(ts);
	}
}

void shtps_cover_set_state(struct shtps_rmi_spi *ts, u8 request)
{
	SHTPS_LOG_FUNC_CALL_INPARAM(request);

	shtps_mutex_lock_ctrl();

	if(ts->cover_state != request){
		ts->cover_state = request;
		SHTPS_LOG_DBG_PRINT("[Cover] cover_state = %d\n", ts->cover_state);

		if( (ts->state_mgr.state == SHTPS_STATE_ACTIVE) ||
			(ts->state_mgr.state == SHTPS_STATE_FACETOUCH) ||
			(ts->state_mgr.state == SHTPS_STATE_SLEEP) ||
			(ts->state_mgr.state == SHTPS_STATE_SLEEP_FACETOUCH) )
		{
			if(ts->cover_state == 0){
				shtps_set_cover_mode_off(ts);
			}else{
				shtps_set_cover_mode_on(ts);
			}
		}

		#if defined(SHTPS_GLOVE_DETECT_ENABLE)
			if(ts->glove_enable_state != 0){
				if(ts->state_mgr.state == SHTPS_STATE_ACTIVE){
					if(ts->cover_state == 0){
						shtps_set_glove_detect_enable(ts);
					}else{
						shtps_set_glove_detect_disable(ts);
					}
				}
			}
		#endif /* SHTPS_GLOVE_DETECT_ENABLE */
	}

	shtps_set_lpwg_sleep_check(ts);

	shtps_mutex_unlock_ctrl();
}

void shtps_cover_notifier_callback(struct shtps_rmi_spi *ts, int on)
{
	u8 request = (on == 0)? 0 : 1;

	SHTPS_LOG_FUNC_CALL_INPARAM(on);

	if(ts == NULL){
		return;
	}

	#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
		if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_SPI_PROC_COVER, request) == 0){
			if(request == 0){
				shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_COVER_OFF);
			}else{
				shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_COVER_ON);
			}
		}
	#else
		if(request == 0){
			shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_COVER_OFF);
		}else{
			shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_COVER_ON);
		}
	#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */
}

#endif /* SHTPS_COVER_ENABLE */

int shtps_fwdate(struct shtps_rmi_spi *ts, u8 *year, u8 *month)
{
	int rc;
	u8 retry = 3;

	SHTPS_LOG_FUNC_CALL();
	do{
		rc = shtps_fwctl_get_fwdate(ts, year, month);
		if(rc == 0)	break;
	}while(retry-- > 0);

	return rc;
}

int shtps_get_serial_number(struct shtps_rmi_spi *ts, u8 *buf)
{
	SHTPS_LOG_FUNC_CALL();
	return shtps_fwctl_get_serial_number(ts, buf);
}

u16 shtps_fwver(struct shtps_rmi_spi *ts)
{
	int rc;
	u16 ver = 0;
	u8 retry = 3;

	SHTPS_LOG_FUNC_CALL();
	do{
		rc = shtps_fwctl_get_fwver(ts, &ver);
		if(rc == 0)	break;
	}while(retry-- > 0);

	return ver;
}

u16 shtps_fwver_builtin(struct shtps_rmi_spi *ts)
{
	u16 ver = 0;

	SHTPS_LOG_FUNC_CALL();
	#if defined(SHTPS_MULTI_FW_ENABLE)
		ver = SHTPS_MULTI_FW_INFO_TBL[ts->multi_fw_type].ver;
		if(ver == 0){
			ver = SHTPS_FWVER_NEWER;
		}
	#else
		ver = SHTPS_FWVER_NEWER;
	#endif /* SHTPS_MULTI_FW_ENABLE */

	return ver;
}

int shtps_fwsize_builtin(struct shtps_rmi_spi *ts)
{
	int size = 0;

	SHTPS_LOG_FUNC_CALL();
	#if defined(SHTPS_MULTI_FW_ENABLE)
		size = SHTPS_MULTI_FW_INFO_TBL[ts->multi_fw_type].size;
		if(size == 0){
			size = SHTPS_FWSIZE_NEWER;
		}
	#else
		size = SHTPS_FWSIZE_NEWER;
	#endif /* SHTPS_MULTI_FW_ENABLE */

	return size;
}

unsigned char* shtps_fwdata_builtin(struct shtps_rmi_spi *ts)
{
	unsigned char *data = NULL;

	SHTPS_LOG_FUNC_CALL();
	#if defined(SHTPS_MULTI_FW_ENABLE)
		data = (unsigned char *)SHTPS_MULTI_FW_INFO_TBL[ts->multi_fw_type].data;
		if(data == NULL){
			data = (unsigned char *)tps_fw_data;
		}
	#else
		data = (unsigned char *)tps_fw_data;
	#endif /* SHTPS_MULTI_FW_ENABLE */

	return data;
}

#if defined(SHTPS_MULTI_FW_ENABLE)
static int shtps_get_multi_fw_info_table_no(int fwkind)
{
	int i;
	for(i = 0; i < SHTPS_MULTI_FW_INFO_SIZE; i++){
		if(SHTPS_MULTI_FW_INFO_TBL[i].fwkind == fwkind){
			return i;
		}
	}
	return SHTPS_MULTI_FW_INFO_SIZE - 1;
}

static void shtps_get_multi_fw_type(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();

	ts->multi_fw_type = shtps_get_multi_fw_info_table_no(SHTPS_FW_KIND_UNKNOWN);

	#if defined(SHTPS_CHECK_HWID_ENABLE)
	{
		u8 productid_buf[30] = {0};
		int ret;

		ret = shtps_fwctl_get_productid(ts, productid_buf);
		if(ret >= 0){
			if( (strncmp(productid_buf, SHTPS_FWUPDATE_PRODUCTID, strlen(SHTPS_FWUPDATE_PRODUCTID)) == 0) ||
			    (strncmp(productid_buf, SHTPS_FWUPDATE_PRODUCTID2, strlen(SHTPS_FWUPDATE_PRODUCTID2)) == 0) ){

				u8 hwrev;

				if(shtps_system_get_hw_type() != SHTPS_HW_TYPE_HANDSET){
					hwrev = SHTPS_GET_HW_VERSION_RET_ES0_ES1;
				}else{
					hwrev = shtps_system_get_hw_revision();
				}

				switch(hwrev){
					case SHTPS_GET_HW_VERSION_RET_ES0_ES1:
					case SHTPS_GET_HW_VERSION_RET_PP1:
						ts->multi_fw_type = shtps_get_multi_fw_info_table_no(SHTPS_FW_KIND_PP1);
						break;
					case SHTPS_GET_HW_VERSION_RET_PP2:
					case SHTPS_GET_HW_VERSION_RET_MP:
						ts->multi_fw_type = shtps_get_multi_fw_info_table_no(SHTPS_FW_KIND_PP2);
						break;
					default:
						ts->multi_fw_type = shtps_get_multi_fw_info_table_no(SHTPS_FW_KIND_UNKNOWN);
						break;
				}
			}
		}
	}
	#endif /* SHTPS_CHECK_HWID_ENABLE */

	#if defined( SHTPS_CHECK_LCD_RESOLUTION_ENABLE )
	{
		int lcdkind;
		u8 productid_buf[30] = {0};
		int ret;

		lcdkind = shtps_get_lcd_resolution_kind(ts);
		ret = shtps_fwctl_get_productid(ts, productid_buf);

		if(ret >= 0){
			if(lcdkind == SHTPS_LCD_RESOLUTION_KIND_WQHD){
				if(strncmp(productid_buf, SHTPS_FWUPDATE_PRODUCTID, strlen(SHTPS_FWUPDATE_PRODUCTID)) == 0){
					ts->multi_fw_type = shtps_get_multi_fw_info_table_no(SHTPS_FW_KIND_WQHD);
				}
			}
			else if(lcdkind == SHTPS_LCD_RESOLUTION_KIND_FHD){
				if( (strncmp(productid_buf, SHTPS_FWUPDATE_PRODUCTID, strlen(SHTPS_FWUPDATE_PRODUCTID)) == 0) ||
				    (strncmp(productid_buf, SHTPS_FWUPDATE_PRODUCTID2, strlen(SHTPS_FWUPDATE_PRODUCTID2)) == 0) ){
					ts->multi_fw_type = shtps_get_multi_fw_info_table_no(SHTPS_FW_KIND_FHD);
				}
			}
		}
	}
	#endif /* SHTPS_CHECK_LCD_RESOLUTION_ENABLE */
}
#endif /* SHTPS_MULTI_FW_ENABLE */

int shtps_fwupdate_builtin_enable(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
#if defined(SHTPS_MULTI_FW_ENABLE)
	if(ts->multi_fw_type == shtps_get_multi_fw_info_table_no(SHTPS_FW_KIND_UNKNOWN)){
		return 0;
	}
#else
	{
		#if defined(SHTPS_DEF_FWCTL_IC_TYPE_3400)
		u8 productid_buf[30] = {0};
		int ret;

		ret = shtps_fwctl_get_productid(ts, productid_buf);
		if(	(ret < 0) || 
			((strncmp(productid_buf, SHTPS_FWUPDATE_PRODUCTID, strlen(SHTPS_FWUPDATE_PRODUCTID)) != 0) &&
			 (strncmp(productid_buf, SHTPS_FWUPDATE_PRODUCTID2, strlen(SHTPS_FWUPDATE_PRODUCTID2)) != 0)) ){
			return 0;
		}
		#endif
	}
#endif /* SHTPS_MULTI_FW_ENABLE */
	return 1;
}

static int shtps_init_param(struct shtps_rmi_spi *ts)
{
	int rc;

	_log_msg_sync( LOGMSG_ID__FW_INIT, "");
	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_MULTI_FW_ENABLE)
		shtps_get_multi_fw_type(ts);
		SHTPS_LOG_DBG_PRINT("multi fw type is [%s]\n", SHTPS_MULTI_FW_INFO_TBL[ts->multi_fw_type].name);
	#endif /* SHTPS_MULTI_FW_ENABLE */

	rc = shtps_fwctl_initparam(ts);
	SPI_ERR_CHECK(rc, err_exit);

	rc = shtps_fwctl_initparam_activemode(ts);
	SPI_ERR_CHECK(rc, err_exit);

	#if defined(SHTPS_LPWG_MODE_ENABLE)
		shtps_fwctl_initparam_lpwgmode(ts);
		SPI_ERR_CHECK(rc, err_exit);
	#endif /* SHTPS_LPWG_MODE_ENABLE */

	#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
		shtps_set_lpmode_init(ts);
	#endif /* SHTPS_LOW_POWER_MODE_ENABLE */

	#if defined(SHTPS_COVER_ENABLE)
		shtps_set_cover_mode_init(ts);
	#endif /* SHTPS_COVER_ENABLE */

	#if defined(SHTPS_CTRL_FW_REPORT_RATE)
		ts->fw_report_rate_cur = 0;
		shtps_set_fw_report_rate(ts);
	#endif /* SHTPS_CTRL_FW_REPORT_RATE */

	#if defined(SHTPS_GLOVE_DETECT_ENABLE)
		shtps_set_glove_detect_init(ts);
	#endif /* SHTPS_GLOVE_DETECT_ENABLE */

	return 0;

err_exit:
	return -1;
}

static void shtps_standby_param(struct shtps_rmi_spi *ts)
{
	_log_msg_sync( LOGMSG_ID__FW_STANDBY, "");
	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
		ts->wakeup_touch_event_inhibit_state = 0;
	#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */

	shtps_sleep(ts, 1);
}

static void shtps_clr_startup_err(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	ts->state_mgr.starterr = SHTPS_STARTUP_SUCCESS;
}

int shtps_wait_startup(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__FW_STARTUP_COMP_WAIT, "");
	wait_event_interruptible(ts->wait_start,
		ts->state_mgr.state == SHTPS_STATE_ACTIVE          ||
		ts->state_mgr.state == SHTPS_STATE_BOOTLOADER      ||
		ts->state_mgr.state == SHTPS_STATE_FWTESTMODE      ||
		ts->state_mgr.state == SHTPS_STATE_SLEEP           ||
		ts->state_mgr.state == SHTPS_STATE_FACETOUCH       ||
		ts->state_mgr.state == SHTPS_STATE_SLEEP_FACETOUCH ||
		ts->state_mgr.starterr == SHTPS_STARTUP_FAILED);

	_log_msg_recv( LOGMSG_ID__FW_STARTUP_COMP, "%d|%d", ts->state_mgr.state, ts->state_mgr.starterr);

	return ts->state_mgr.starterr;
}

int shtps_wait_startup_timeout(struct shtps_rmi_spi *ts, int timeout_ms)
{
	int rc;

	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__FW_STARTUP_COMP_WAIT, "");
	rc = wait_event_interruptible_timeout(ts->wait_start,
			ts->state_mgr.state == SHTPS_STATE_ACTIVE          ||
			ts->state_mgr.state == SHTPS_STATE_BOOTLOADER      ||
			ts->state_mgr.state == SHTPS_STATE_FWTESTMODE      ||
			ts->state_mgr.state == SHTPS_STATE_SLEEP           ||
			ts->state_mgr.state == SHTPS_STATE_FACETOUCH       ||
			ts->state_mgr.state == SHTPS_STATE_SLEEP_FACETOUCH ||
			ts->state_mgr.starterr == SHTPS_STARTUP_FAILED,
			msecs_to_jiffies(timeout_ms));

	_log_msg_recv( LOGMSG_ID__FW_STARTUP_COMP, "%d|%d", ts->state_mgr.state, ts->state_mgr.starterr);

	if(rc == 0){
		return -1;
	}

	return ts->state_mgr.starterr;
}

static void shtps_notify_startup(struct shtps_rmi_spi *ts, u8 err)
{
	SHTPS_LOG_FUNC_CALL_INPARAM(err);
	ts->state_mgr.starterr = err;
	_log_msg_send( LOGMSG_ID__FW_STARTUP_COMP, "%d|%d", ts->state_mgr.state, ts->state_mgr.starterr);
	wake_up_interruptible(&ts->wait_start);
}

static void shtps_set_startmode(struct shtps_rmi_spi *ts, u8 mode)
{
	SHTPS_LOG_FUNC_CALL_INPARAM(mode);
	_log_msg_sync( LOGMSG_ID__FW_STARTUP_MODE, "%d", mode);
	ts->state_mgr.mode = mode;
}

static int shtps_get_startmode(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	return ts->state_mgr.mode;
}

int shtps_get_fingermax(struct shtps_rmi_spi *ts)
{
	return shtps_fwctl_get_fingermax(ts);
}

/* -----------------------------------------------------------------------------------
 */
int shtps_get_diff(unsigned short pos1, unsigned short pos2, unsigned long factor)
{
	int diff;
//	SHTPS_LOG_FUNC_CALL();
	diff = (pos1 * factor / 10000) - (pos2 * factor / 10000);
	return (diff >= 0)? diff : -diff;
}

int shtps_get_fingerwidth(struct shtps_rmi_spi *ts, int num, struct shtps_touch_info *info)
{
	int w = (info->fingers[num].wx >= info->fingers[num].wy)? info->fingers[num].wx : info->fingers[num].wy;
	return (w < SHTPS_FINGER_WIDTH_MIN)? SHTPS_FINGER_WIDTH_MIN : w;
}

void shtps_set_eventtype(u8 *event, u8 type)
{
//	SHTPS_LOG_FUNC_CALL();
	*event = type;
}

static void shtps_set_touch_info(struct shtps_rmi_spi *ts, u8 *buf, struct shtps_touch_info *info)
{
	int i;
	int fingerMax = shtps_get_fingermax(ts);
	u8* fingerInfo;
	int FingerNum = 0;

	SHTPS_LOG_FUNC_CALL();

	for(i=0;i<fingerMax;i++){
		fingerInfo = shtps_fwctl_get_finger_info_buf(ts, i, fingerMax, buf);

		ts->fw_report_info.fingers[i].state	= shtps_fwctl_get_finger_state(ts, i, fingerMax, buf);
		ts->fw_report_info.fingers[i].x		= shtps_fwctl_get_finger_pos_x(ts, fingerInfo);
		ts->fw_report_info.fingers[i].y		= shtps_fwctl_get_finger_pos_y(ts, fingerInfo);
		ts->fw_report_info.fingers[i].wx	= shtps_fwctl_get_finger_wx(ts, fingerInfo);
		ts->fw_report_info.fingers[i].wy	= shtps_fwctl_get_finger_wy(ts, fingerInfo);
		ts->fw_report_info.fingers[i].z		= shtps_fwctl_get_finger_z(ts, fingerInfo);

		#if defined(SHTPS_COORDINATES_POINT_SYMMETRY_ENABLE)
			if(ts->fw_report_info.fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
				ts->fw_report_info.fingers[i].x = shtps_fwctl_get_maxXPosition(ts) - ts->fw_report_info.fingers[i].x;
				ts->fw_report_info.fingers[i].y = shtps_fwctl_get_maxYPosition(ts) - ts->fw_report_info.fingers[i].y;
			}
		#endif /* SHTPS_COORDINATES_POINT_SYMMETRY_ENABLE */

		info->fingers[i].state	= ts->fw_report_info.fingers[i].state;
		info->fingers[i].x		= ts->fw_report_info.fingers[i].x;
		info->fingers[i].y		= ts->fw_report_info.fingers[i].y;
		info->fingers[i].wx		= ts->fw_report_info.fingers[i].wx;
		info->fingers[i].wy		= ts->fw_report_info.fingers[i].wy;
		info->fingers[i].z		= ts->fw_report_info.fingers[i].z;

		if(ts->fw_report_info.fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
			FingerNum++;
			SHTPS_LOG_EVENT(
				DBG_PRINTK("[%s]Touch Info[%d] x=%d, y=%d, wx=%d, wy=%d, z=%d\n",
					ts->fw_report_info.fingers[i].state == SHTPS_TOUCH_STATE_FINGER ? "Finger" :
					ts->fw_report_info.fingers[i].state == SHTPS_TOUCH_STATE_PALM ? "Palm" :
					ts->fw_report_info.fingers[i].state == SHTPS_TOUCH_STATE_GLOVE ? "Glove" :
					ts->fw_report_info.fingers[i].state == SHTPS_TOUCH_STATE_UNKNOWN ? "Unknown" : "Other" ,
					i,
					ts->fw_report_info.fingers[i].x,
					ts->fw_report_info.fingers[i].y,
					ts->fw_report_info.fingers[i].wx,
					ts->fw_report_info.fingers[i].wy,
					ts->fw_report_info.fingers[i].z
				);
			);

			#if defined(SHTPS_DEF_DISALLOW_Z_ZERO_TOUCH_EVENT_ENABLE)
				if(ts->fw_report_info.fingers[i].z == 0){
					ts->fw_report_info.fingers[i].z = 1;
					info->fingers[i].z = 1;
					SHTPS_LOG_EVENT(
						DBG_PRINTK("[warn][TouchEvent][%d] z=0 detected. change to z=1\n", i);
					);
				}
			#endif /* SHTPS_DEF_DISALLOW_Z_ZERO_TOUCH_EVENT_ENABLE */
		}
		else if(ts->fw_report_info_store.fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
			SHTPS_LOG_EVENT(
				DBG_PRINTK("[NoTouch]Touch Info[%d] x=%d, y=%d, wx=%d, wy=%d, z=%d\n",
					i,
					ts->fw_report_info.fingers[i].x,
					ts->fw_report_info.fingers[i].y,
					ts->fw_report_info.fingers[i].wx,
					ts->fw_report_info.fingers[i].wy,
					ts->fw_report_info.fingers[i].z
				);
			);
		}
	}
	
	ts->fw_report_info.finger_num = FingerNum;
}


static void shtps_calc_notify(struct shtps_rmi_spi *ts, u8 *buf, struct shtps_touch_info *info, u8 *event)
{
	int i;
	int fingerMax = shtps_get_fingermax(ts);
	int numOfFingers = 0;
	int diff_x;
	int diff_y;

	SHTPS_LOG_FUNC_CALL();

	shtps_set_eventtype(event, 0xff);
	shtps_set_touch_info(ts, buf, info);

	shtps_filter_main(ts, info);

	for (i = 0; i < fingerMax; i++) {
		if(info->fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
			numOfFingers++;
		}
	}
	info->finger_num = numOfFingers;

	/* set event type */
	for(i = 0;i < fingerMax;i++){
		_log_msg_sync( LOGMSG_ID__FW_EVENT, "%d|%d|%d|%d|%d|%d|%d|%d|%d", i, info->fingers[i].state,
							info->fingers[i].x * SHTPS_POS_SCALE_X(ts) / 10000,
							info->fingers[i].y * SHTPS_POS_SCALE_Y(ts) / 10000,
							info->fingers[i].x,
							info->fingers[i].y,
							info->fingers[i].wx,
							info->fingers[i].wy,
							info->fingers[i].z);

		if(info->fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
			if(ts->report_info.fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
				diff_x = shtps_get_diff(info->fingers[i].x, ts->report_info.fingers[i].x, SHTPS_POS_SCALE_X(ts));
				diff_y = shtps_get_diff(info->fingers[i].y, ts->report_info.fingers[i].y, SHTPS_POS_SCALE_Y(ts));

				if((diff_x > 0) || (diff_y > 0)){
					shtps_set_eventtype(event, SHTPS_EVENT_DRAG);
				}
			}
		}

		if(info->fingers[i].state != ts->report_info.fingers[i].state){
			shtps_set_eventtype(event, SHTPS_EVENT_MTDU);
		}
	}

	shtps_fwctl_get_gesture(ts, fingerMax, buf, &info->gs1, &info->gs2);

	if(numOfFingers > 0){
		ts->poll_info.stop_count = 0;
		if(ts->touch_state.numOfFingers == 0){
			shtps_set_eventtype(event, SHTPS_EVENT_TD);
		}
		if(numOfFingers >= 2 && ts->touch_state.numOfFingers < 2){
			shtps_rezero_handle(ts, SHTPS_REZERO_HANDLE_EVENT_MTD, info->gs2);
		}
		shtps_rezero_handle(ts, SHTPS_REZERO_HANDLE_EVENT_TOUCH, info->gs2);
	}else{
		if(ts->touch_state.numOfFingers != 0){
			shtps_set_eventtype(event, SHTPS_EVENT_TU);
		}
		shtps_rezero_handle(ts, SHTPS_REZERO_HANDLE_EVENT_TOUCHUP, info->gs2);
	}

	#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
		if(ts->state_mgr.state == SHTPS_STATE_FACETOUCH){
			shtps_check_facetouch(ts, info);
		}
	#endif /* CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT */
}


void shtps_report_touch_on(struct shtps_rmi_spi *ts, int finger, int x, int y, int w, int wx, int wy, int z)
{
	int lcd_x;
	int lcd_y;

//	SHTPS_LOG_FUNC_CALL();
	if( (x == SHTPS_TOUCH_CANCEL_COORDINATES_X) && (y == SHTPS_TOUCH_CANCEL_COORDINATES_Y) ){
		lcd_x = x;
		lcd_y = y;
	}else{
		lcd_x = x * SHTPS_POS_SCALE_X(ts) / 10000;
		lcd_y = y * SHTPS_POS_SCALE_Y(ts) / 10000;
		if(lcd_x >= CONFIG_SHTPS_SY3000_LCD_SIZE_X){
			lcd_x = CONFIG_SHTPS_SY3000_LCD_SIZE_X - 1;
		}
		if(lcd_y >= CONFIG_SHTPS_SY3000_LCD_SIZE_Y){
			lcd_y = CONFIG_SHTPS_SY3000_LCD_SIZE_Y - 1;
		}
	}

	input_mt_slot(ts->input, finger);
	input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
	#if defined(SHTPS_NOTIFY_TOUCH_MINOR_ENABLE)
		input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, (wx > wy ? wx : wy));
		input_report_abs(ts->input, ABS_MT_TOUCH_MINOR, (wx < wy ? wx : wy));
		input_report_abs(ts->input, ABS_MT_ORIENTATION, (wx > wy ? 1 : 0));
	#else
		input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, w);
	#endif /* SHTPS_NOTIFY_TOUCH_MINOR_ENABLE */
	input_report_abs(ts->input, ABS_MT_POSITION_X,  lcd_x);
	input_report_abs(ts->input, ABS_MT_POSITION_Y,  lcd_y);
	input_report_abs(ts->input, ABS_MT_PRESSURE,    z);

	SHTPS_LOG_EVENT(
		DBG_PRINTK("[finger]Notify event[%d] touch=100(%d), x=%d(%d), y=%d(%d) w=%d(%d,%d)\n",
						finger, z, lcd_x, x, lcd_y, y, w, wx, wy);
	);
	_log_msg_sync( LOGMSG_ID__EVENT_NOTIFY, "%d|100|%d|%d|%d|%d|%d|%d|%d|%d",
						finger, lcd_x, x, lcd_y, y, w, wx, wy, z);

	#if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE )
		if(shtps_touch_eventlog_is_recording() != 0){
			shtps_touch_eventlog_rec_event_info(SHTPS_TOUCH_STATE_FINGER, finger, x, y, w, wx, wy, z);
		}
	#endif /* #if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE ) */
}

void shtps_report_touch_off(struct shtps_rmi_spi *ts, int finger, int x, int y, int w, int wx, int wy, int z)
{
//	SHTPS_LOG_FUNC_CALL();
	input_mt_slot(ts->input, finger);
	input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);

	SHTPS_LOG_EVENT(
		DBG_PRINTK("[finger]Notify event[%d] touch=0(%d), x=%d(%d), y=%d(%d) w=%d(%d,%d)\n",
						finger, z, (x * SHTPS_POS_SCALE_X(ts) / 10000), x, (y * SHTPS_POS_SCALE_Y(ts) / 10000), y,
						w, wx, wy);
	);
	_log_msg_sync( LOGMSG_ID__EVENT_NOTIFY, "%d|0|%d|%d|%d|%d|%d|%d|%d|%d",
						finger, (x * SHTPS_POS_SCALE_X(ts) / 10000), x, (y * SHTPS_POS_SCALE_Y(ts) / 10000), y,
						w, wx, wy, z);

	#if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE )
		if(shtps_touch_eventlog_is_recording() != 0){
			shtps_touch_eventlog_rec_event_info(SHTPS_TOUCH_STATE_NO_TOUCH, finger, x, y, w, wx, wy, z);
		}
	#endif /* #if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE ) */
}

#if defined(SHTPS_GLOVE_DETECT_ENABLE)
void shtps_report_touch_glove_on(struct shtps_rmi_spi *ts, int finger, int x, int y, int w, int wx, int wy, int z)
{
	int lcd_x;
	int lcd_y;

//	SHTPS_LOG_FUNC_CALL();
	if( (x == SHTPS_TOUCH_CANCEL_COORDINATES_X) && (y == SHTPS_TOUCH_CANCEL_COORDINATES_Y) ){
		lcd_x = x;
		lcd_y = y;
	}else{
		lcd_x = x * SHTPS_POS_SCALE_X(ts) / 10000;
		lcd_y = y * SHTPS_POS_SCALE_Y(ts) / 10000;
		if(lcd_x >= CONFIG_SHTPS_SY3000_LCD_SIZE_X){
			lcd_x = CONFIG_SHTPS_SY3000_LCD_SIZE_X - 1;
		}
		if(lcd_y >= CONFIG_SHTPS_SY3000_LCD_SIZE_Y){
			lcd_y = CONFIG_SHTPS_SY3000_LCD_SIZE_Y - 1;
		}
	}

	input_mt_slot(ts->input, finger);
	input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
	#if defined(SHTPS_NOTIFY_TOUCH_MINOR_ENABLE)
		input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, (wx > wy ? wx : wy));
		input_report_abs(ts->input, ABS_MT_TOUCH_MINOR, (wx < wy ? wx : wy));
		input_report_abs(ts->input, ABS_MT_ORIENTATION, (wx > wy ? 1 : 0));
	#else
		input_report_abs(ts->input, ABS_MT_TOUCH_MAJOR, w);
	#endif /* SHTPS_NOTIFY_TOUCH_MINOR_ENABLE */
	input_report_abs(ts->input, ABS_MT_POSITION_X,  lcd_x);
	input_report_abs(ts->input, ABS_MT_POSITION_Y,  lcd_y);
	input_report_abs(ts->input, ABS_MT_PRESSURE,    0x100 + z);

	SHTPS_LOG_EVENT(
		DBG_PRINTK("[glove]Notify event[%d] touch=100(%d), x=%d(%d), y=%d(%d) w=%d(%d,%d)\n",
						finger, z, lcd_x, x, lcd_y, y, w, wx, wy);
	);
	_log_msg_sync( LOGMSG_ID__EVENT_NOTIFY, "%d|100|%d|%d|%d|%d|%d|%d|%d|%d",
						finger, lcd_x, x, lcd_y, y, w, wx, wy, z);

	#if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE )
		if(shtps_touch_eventlog_is_recording() != 0){
			shtps_touch_eventlog_rec_event_info(SHTPS_TOUCH_STATE_GLOVE, finger, x, y, w, wx, wy, z);
		}
	#endif /* #if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE ) */
}

void shtps_report_touch_glove_off(struct shtps_rmi_spi *ts, int finger, int x, int y, int w, int wx, int wy, int z)
{
//	SHTPS_LOG_FUNC_CALL();
	input_mt_slot(ts->input, finger);
	input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);

	SHTPS_LOG_EVENT(
		DBG_PRINTK("[glove]Notify event[%d] touch=0(%d), x=%d(%d), y=%d(%d) w=%d(%d,%d)\n",
						finger, z, (x * SHTPS_POS_SCALE_X(ts) / 10000), x, (y * SHTPS_POS_SCALE_Y(ts) / 10000), y,
						w, wx, wy);
	);
	_log_msg_sync( LOGMSG_ID__EVENT_NOTIFY, "%d|0|%d|%d|%d|%d|%d|%d|%d|%d",
						finger, (x * SHTPS_POS_SCALE_X(ts) / 10000), x, (y * SHTPS_POS_SCALE_Y(ts) / 10000), y,
						w, wx, wy, z);

	#if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE )
		if(shtps_touch_eventlog_is_recording() != 0){
			shtps_touch_eventlog_rec_event_info(SHTPS_TOUCH_STATE_NO_TOUCH, finger, x, y, w, wx, wy, z);
		}
	#endif /* #if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE ) */
}
#endif /* SHTPS_GLOVE_DETECT_ENABLE */

void shtps_event_report(struct shtps_rmi_spi *ts, struct shtps_touch_info *info, u8 event)
{
	int	i;
	int fingerMax = shtps_get_fingermax(ts);

	SHTPS_LOG_FUNC_CALL();

	for(i = 0;i < fingerMax;i++){
		if(info->fingers[i].state == SHTPS_TOUCH_STATE_FINGER){
			shtps_report_touch_on(ts, i,
								  info->fingers[i].x,
								  info->fingers[i].y,
								  shtps_get_fingerwidth(ts, i, info),
								  info->fingers[i].wx,
								  info->fingers[i].wy,
								  info->fingers[i].z);

		#if defined(SHTPS_GLOVE_DETECT_ENABLE)
			}else if(info->fingers[i].state == SHTPS_TOUCH_STATE_GLOVE){
				shtps_report_touch_glove_on(ts, i,
									  info->fingers[i].x,
									  info->fingers[i].y,
									  shtps_get_fingerwidth(ts, i, info),
									  info->fingers[i].wx,
									  info->fingers[i].wy,
									  info->fingers[i].z);
		#endif /* SHTPS_GLOVE_DETECT_ENABLE */

		#if defined(SHTPS_GLOVE_DETECT_ENABLE)
			}else if(ts->report_info.fingers[i].state == SHTPS_TOUCH_STATE_GLOVE){
				shtps_report_touch_glove_off(ts, i,
									  info->fingers[i].x,
									  info->fingers[i].y,
									  shtps_get_fingerwidth(ts, i, info),
									  info->fingers[i].wx,
									  info->fingers[i].wy,
									  info->fingers[i].z);
		#endif /* SHTPS_GLOVE_DETECT_ENABLE */

		}else if(ts->report_info.fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
			shtps_report_touch_off(ts, i,
								  info->fingers[i].x,
								  info->fingers[i].y,
								  shtps_get_fingerwidth(ts, i, info),
								  info->fingers[i].wx,
								  info->fingers[i].wy,
								  info->fingers[i].z);
		}
	}
	shtps_performance_check(SHTPS_PERFORMANCE_CHECK_STATE_CONT);
	input_sync(ts->input);
	shtps_performance_check(SHTPS_PERFORMANCE_CHECK_STATE_CONT);

	ts->touch_state.numOfFingers = info->finger_num;

	ts->diag.event = 1;
	memcpy(&ts->report_info, info, sizeof(ts->report_info));

	shtps_perflock_set_event(ts, event);	//SHTPS_CPU_CLOCK_CONTROL_ENABLE

	wake_up_interruptible(&ts->diag.wait);
}

#if defined(SHTPS_LPWG_MODE_ENABLE)
static void shtps_event_update(struct shtps_rmi_spi *ts, struct shtps_touch_info *info)
{
	int i;
	int fingerMax = shtps_get_fingermax(ts);
	int numOfFingers = 0;

	SHTPS_LOG_FUNC_CALL();

	for(i = 0;i < fingerMax;i++){
		if(info->fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
			numOfFingers++;
		}
	}
	ts->touch_state.numOfFingers = numOfFingers;
}
#endif /* #if defined(SHTPS_LPWG_MODE_ENABLE) */

void shtps_event_force_touchup(struct shtps_rmi_spi *ts)
{
	int	i;
	int isEvent = 0;
	int fingerMax = shtps_get_fingermax(ts);

	SHTPS_LOG_FUNC_CALL();

	blocking_notifier_call_chain(&shtps_notifier_list_touchevent, 0, NULL);

	shtps_filter_force_touchup(ts);

	#if defined( SHTPS_TOUCHCANCEL_BEFORE_FORCE_TOUCHUP_ENABLE )
		for(i = 0;i < fingerMax;i++){
			if(ts->report_info.fingers[i].state != 0x00){
				isEvent = 1;
				shtps_report_touch_on(ts, i,
									  SHTPS_TOUCH_CANCEL_COORDINATES_X,
									  SHTPS_TOUCH_CANCEL_COORDINATES_Y,
									  shtps_get_fingerwidth(ts, i, &ts->report_info),
									  ts->report_info.fingers[i].wx,
									  ts->report_info.fingers[i].wy,
									  ts->report_info.fingers[i].z);
			}
		}
		if(isEvent){
			input_sync(ts->input);
			isEvent = 0;
		}
	#endif /* SHTPS_TOUCHCANCEL_BEFORE_FORCE_TOUCHUP_ENABLE */

	for(i = 0;i < fingerMax;i++){
		if(ts->report_info.fingers[i].state != 0x00){
			isEvent = 1;
			shtps_report_touch_off(ts, i,
								  ts->report_info.fingers[i].x,
								  ts->report_info.fingers[i].y,
								  0,
								  0,
								  0,
								  0);
		}
	}
	if(isEvent){
		input_sync(ts->input);
		ts->touch_state.numOfFingers = 0;
		memset(&ts->report_info, 0, sizeof(ts->report_info));
	}
}

void shtps_report_stflib_kevt(struct shtps_rmi_spi *ts, int event, int value)
{
	int report_val;

	SHTPS_LOG_DBG_PRINT("[stflib_kevt] report event type=%s, value=%d\n",
							(event == SHTPS_DEF_STFLIB_KEVT_SCANRATE_MODE)? "Scan rate mode change" :
							(event == SHTPS_DEF_STFLIB_KEVT_CHARGER_ARMOR)? "Charger armor" :
							(event == SHTPS_DEF_STFLIB_KEVT_CLEAR_REQUEST)? "Clear request" :
							(event == SHTPS_DEF_STFLIB_KEVT_TIMEOUT)?       "Timer timeout" :
							(event == SHTPS_DEF_STFLIB_KEVT_CHANGE_PARAM)?  "Change param" :
							(event == SHTPS_DEF_STFLIB_KEVT_CALIBRATION)?   "Calibration Position" :
							(event == SHTPS_DEF_STFLIB_KEVT_CHANGE_PANEL_SIZE)?   "Change panel size" :
							(event == SHTPS_DEF_STFLIB_KEVT_COVER_MODE)?   "Cover Mode" :
							"Unknown", value);

	report_val = ((event << 8) & 0xFF00) | (value & 0xFF);
	input_report_abs(ts->input, ABS_MISC, report_val);
	input_report_abs(ts->input, ABS_MISC, SHTPS_DEF_STFLIB_KEVT_DUMMY);
	input_sync(ts->input);
}

#if defined(SHTPS_LPWG_MODE_ENABLE)
static void shtps_notify_wakeup_event(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	SHTPS_LOG_DBG_PRINT("wakeup touch blocked until rezero\n");

	ts->lpwg.block_touchevent = 1;
	ts->lpwg.is_notified = 1;
	ts->lpwg.notify_time = jiffies;
	ts->lpwg.notify_enable = 0;

	#if defined(SHTPS_LPWG_SWEEP_ON_ENABLE)
		input_report_key(ts->input_key, KEY_SWEEPON, 1);
		input_sync(ts->input_key);

		input_report_key(ts->input_key, KEY_SWEEPON, 0);
		input_sync(ts->input_key);
	#endif /* SHTPS_LPWG_SWEEP_ON_ENABLE */

	shtps_lpwg_notify_interval_start(ts);
}

#if defined(SHTPS_PROXIMITY_SUPPORT_ENABLE)
void shtps_notify_cancel_wakeup_event(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_LPWG_SWEEP_ON_ENABLE)
		input_report_key(ts->input_key, KEY_SWEEPON, 1);
		input_sync(ts->input_key);

		input_report_key(ts->input_key, KEY_SWEEPON, 0);
		input_sync(ts->input_key);
	#endif /* SHTPS_LPWG_SWEEP_ON_ENABLE */
}
#endif /* SHTPS_PROXIMITY_SUPPORT_ENABLE */

static void shtps_notify_double_tap_event(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();

	input_report_key(ts->input_key, KEY_DOUBLETAP, 1);
	input_sync(ts->input_key);

	input_report_key(ts->input_key, KEY_DOUBLETAP, 0);
	input_sync(ts->input_key);
}
#endif /* SHTPS_LPWG_MODE_ENABLE */

/* -----------------------------------------------------------------------------------
 */
static int shtps_tm_irqcheck(struct shtps_rmi_spi *ts)
{
	int rc;
	u8 sts;

	SHTPS_LOG_FUNC_CALL();
	rc = shtps_fwctl_irqclear_get_irqfactor(ts, &sts);
	if((rc == 0) && ((sts & SHTPS_IRQ_ANALOG) != 0x00)){
		return 1;

	}else{
		return 0;

	}
}

static int shtps_tm_wait_attn(struct shtps_rmi_spi *ts)
{
	int rc;

	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__FW_TESTMODE_ATTN_WAIT, "");
	rc = wait_event_interruptible_timeout(ts->diag.tm_wait_ack,
			ts->diag.tm_ack == 1 || ts->diag.tm_stop == 1,
			msecs_to_jiffies(SHTPS_FWTESTMODE_ACK_TMO));
	_log_msg_recv( LOGMSG_ID__FW_TESTMODE_ATTN, "");

#if defined( SHTPS_LOG_SEQ_ENABLE )
	if(rc == 0){
		_log_msg_sync( LOGMSG_ID__FW_TESTMODE_ATTN_TIMEOUT, "");
		SHTPS_LOG_ERR_PRINT("shtps_tm_wait_attn() :ATTN_TIMEOUT\n");
	}
#endif /* #if defined( SHTPS_LOG_SEQ_ENABLE ) */

	if(ts->diag.tm_ack == 0 || ts->diag.tm_stop == 1){
		SHTPS_LOG_ERR_PRINT("shtps_tm_wait_attn() tm_ack:%d, tm_stop:%d\n", ts->diag.tm_ack, ts->diag.tm_stop);
		return -1;
	}

	ts->diag.tm_ack = 0;
	return 0;
}

static void shtps_tm_wakeup(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	ts->diag.tm_ack = 1;
	_log_msg_send( LOGMSG_ID__FW_TESTMODE_ATTN, "");
	wake_up_interruptible(&ts->diag.tm_wait_ack);
}

static void shtps_tm_cancel(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	ts->diag.tm_stop = 1;

	wake_up_interruptible(&ts->diag.tm_wait_ack);
}

int shtps_get_tm_rxsize(struct shtps_rmi_spi *ts)
{
	return shtps_fwctl_get_tm_rxsize(ts);
}

int shtps_get_tm_txsize(struct shtps_rmi_spi *ts)
{
	return shtps_fwctl_get_tm_txsize(ts);
}

static int shtps_start_tm(struct shtps_rmi_spi *ts)
{
	int rc;

	SHTPS_LOG_FUNC_CALL();

	ts->diag.tm_stop = 0;
	ts->diag.tm_ack = 0;

	rc = shtps_fwctl_start_testmode(ts, ts->diag.tm_mode);

	return rc;
}

int shtps_baseline_offset_disable(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	return shtps_fwctl_baseline_offset_disable(ts);
}

static void shtps_stop_tm(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();

	shtps_tm_cancel(ts);
	shtps_fwctl_stop_testmode(ts);
	shtps_init_param(ts);
}

void shtps_read_tmdata(struct shtps_rmi_spi *ts, u8 mode)
{
	int rc;
	SHTPS_LOG_FUNC_CALL();

	if(mode == SHTPS_TMMODE_FRAMELINE){
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_cmd_tm_frameline(ts, ts->diag.tm_mode);
		shtps_mutex_unlock_ctrl();
		if(rc == 0){
			if(shtps_tm_wait_attn(ts) != 0) goto tm_read_cancel;
		}
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_get_tm_frameline(ts, ts->diag.tm_mode, ts->diag.tm_data);
		shtps_mutex_unlock_ctrl();

	}else if(mode == SHTPS_TMMODE_BASELINE){
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_cmd_tm_baseline(ts, ts->diag.tm_mode);
		shtps_mutex_unlock_ctrl();
		if(rc == 0){
			if(shtps_tm_wait_attn(ts) != 0) goto tm_read_cancel;
		}
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_get_tm_baseline(ts, ts->diag.tm_mode, ts->diag.tm_data);
		shtps_mutex_unlock_ctrl();

	}else if(mode == SHTPS_TMMODE_BASELINE_RAW){
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_cmd_tm_baseline_raw(ts, ts->diag.tm_mode);
		shtps_mutex_unlock_ctrl();
		if(rc == 0){
			if(shtps_tm_wait_attn(ts) != 0) goto tm_read_cancel;
		}
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_get_tm_baseline_raw(ts, ts->diag.tm_mode, ts->diag.tm_data);
		shtps_mutex_unlock_ctrl();

	}else if(mode == SHTPS_TMMODE_HYBRID_ADC){
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_cmd_tm_hybrid_adc(ts, ts->diag.tm_mode);
		shtps_mutex_unlock_ctrl();
		if(rc == 0){
			if(shtps_tm_wait_attn(ts) != 0) goto tm_read_cancel;
		}
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_get_tm_hybrid_adc(ts, ts->diag.tm_mode, ts->diag.tm_data);
		shtps_mutex_unlock_ctrl();

	}else if(mode == SHTPS_TMMODE_ADC_RANGE){
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_cmd_tm_adc_range(ts, ts->diag.tm_mode);
		shtps_mutex_unlock_ctrl();
		if(rc == 0){
			if(shtps_tm_wait_attn(ts) != 0) goto tm_read_cancel;
		}
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_get_tm_adc_range(ts, ts->diag.tm_mode, ts->diag.tm_data);
		shtps_mutex_unlock_ctrl();

	}else if(mode == SHTPS_TMMODE_MOISTURE){
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_cmd_tm_moisture(ts, ts->diag.tm_mode);
		shtps_mutex_unlock_ctrl();
		if(rc == 0){
			if(shtps_tm_wait_attn(ts) != 0) goto tm_read_cancel;
		}
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_get_tm_moisture(ts, ts->diag.tm_mode, ts->diag.tm_data);
		shtps_mutex_unlock_ctrl();

	}else if(mode == SHTPS_TMMODE_MOISTURE_NO_MASK){
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_cmd_tm_moisture_no_mask(ts, ts->diag.tm_mode);
		shtps_mutex_unlock_ctrl();
		if(rc == 0){
			if(shtps_tm_wait_attn(ts) != 0) goto tm_read_cancel;
		}
		shtps_mutex_lock_ctrl();
		rc = shtps_fwctl_get_tm_moisture_no_mask(ts, ts->diag.tm_mode, ts->diag.tm_data);
		shtps_mutex_unlock_ctrl();

	}else{
		memset(ts->diag.tm_data, 0, sizeof(ts->diag.tm_data));
	}
	return;

tm_read_cancel:
	memset(ts->diag.tm_data, 0, sizeof(ts->diag.tm_data));
	return;
}

/* -----------------------------------------------------------------------------------
 */
#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
	//nothing
#else
static void shtps_irq_wake_disable(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__IRQ_WAKE_DISABLE, "%d", ts->irq_mgr.wake);
	if(ts->irq_mgr.wake != SHTPS_IRQ_WAKE_DISABLE){
		disable_irq_wake(ts->irq_mgr.irq);
		ts->irq_mgr.wake = SHTPS_IRQ_WAKE_DISABLE;

		#if defined( SHTPS_MODULE_PARAM_ENABLE )
			shtps_irq_wake_state = SHTPS_IRQ_WAKE_DISABLE;
		#endif /* SHTPS_MODULE_PARAM_ENABLE */
	}
}

static void shtps_irq_wake_enable(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__IRQ_WAKE_ENABLE, "%d", ts->irq_mgr.wake);
	if(ts->irq_mgr.wake != SHTPS_IRQ_WAKE_ENABLE){
		enable_irq_wake(ts->irq_mgr.irq);
		ts->irq_mgr.wake = SHTPS_IRQ_WAKE_ENABLE;

		#if defined( SHTPS_MODULE_PARAM_ENABLE )
			shtps_irq_wake_state = SHTPS_IRQ_WAKE_ENABLE;
		#endif /* SHTPS_MODULE_PARAM_ENABLE */
	}
}
#endif /* !SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

void shtps_irq_disable(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__IRQ_DISABLE, "%d", ts->irq_mgr.state);
	if(ts->irq_mgr.state != SHTPS_IRQ_STATE_DISABLE){
		disable_irq_nosync(ts->irq_mgr.irq);
		ts->irq_mgr.state = SHTPS_IRQ_STATE_DISABLE;
	}
	
	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		if(ts->irq_mgr.wake != SHTPS_IRQ_WAKE_DISABLE){
			disable_irq_wake(ts->irq_mgr.irq);
			ts->irq_mgr.wake = SHTPS_IRQ_WAKE_DISABLE;

			#if defined( SHTPS_MODULE_PARAM_ENABLE )
				shtps_irq_wake_state = SHTPS_IRQ_WAKE_DISABLE;
			#endif /* SHTPS_MODULE_PARAM_ENABLE */
		}
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */
}

void shtps_irq_enable(struct shtps_rmi_spi *ts)	
{
	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__IRQ_ENABLE, "%d", ts->irq_mgr.state);
	
	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		if(ts->irq_mgr.wake != SHTPS_IRQ_WAKE_ENABLE){
			enable_irq_wake(ts->irq_mgr.irq);
			ts->irq_mgr.wake = SHTPS_IRQ_WAKE_ENABLE;

			#if defined( SHTPS_MODULE_PARAM_ENABLE )
				shtps_irq_wake_state = SHTPS_IRQ_WAKE_ENABLE;
			#endif /* SHTPS_MODULE_PARAM_ENABLE */
		}
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

	if(ts->irq_mgr.state != SHTPS_IRQ_STATE_ENABLE){
		enable_irq(ts->irq_mgr.irq);
		ts->irq_mgr.state = SHTPS_IRQ_STATE_ENABLE;
	}
}

static int shtps_irq_resuest(struct shtps_rmi_spi *ts)
{
	int rc;

	SHTPS_LOG_FUNC_CALL();

	_log_msg_sync( LOGMSG_ID__IRQ_REQUEST, "%d", ts->irq_mgr.irq);

	#if defined(SHTPS_IRQ_LEVEL_ENABLE)
		rc = request_threaded_irq(ts->irq_mgr.irq,
							  shtps_irq_handler,
							  shtps_irq,
							  IRQF_TRIGGER_LOW | IRQF_ONESHOT,
							  SH_TOUCH_DEVNAME,
							  ts);
	#else
		rc = request_threaded_irq(ts->irq_mgr.irq,
							  shtps_irq_handler,
							  shtps_irq,
							  IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
							  SH_TOUCH_DEVNAME,
							  ts);
	#endif /* SHTPS_IRQ_LEVEL_ENABLE */

	if(rc){
		_log_msg_sync( LOGMSG_ID__IRQ_REQUEST_NACK, "");
		SHTPS_LOG_ERR_PRINT("request_threaded_irq error:%d\n",rc);
		return -1;
	}

	ts->irq_mgr.state = SHTPS_IRQ_STATE_ENABLE;
	ts->irq_mgr.wake  = SHTPS_IRQ_WAKE_DISABLE;
	shtps_irq_disable(ts);
	return 0;
}

static void shtps_irqtimer_start(struct shtps_rmi_spi *ts, long time_ms)
{
	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__IRQ_TIMER_START, "%lu", time_ms);
	schedule_delayed_work(&ts->tmo_check, msecs_to_jiffies(time_ms));
}

static void shtps_irqtimer_stop(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__IRQ_TIMER_CANCEL, "");
	cancel_delayed_work(&ts->tmo_check);
}

#if defined(SHTPS_HOST_LPWG_MODE_ENABLE)
int shtps_check_host_lpwg_enable(void)
{
	
	if(SHTPS_HOST_LPWG_HW_REV_CHK_ENABLE == 0){
		return SHTPS_HOST_LPWG_ENABLE;
	}else{
		#if defined(SHTPS_CHECK_HWID_ENABLE)
			if(shtps_system_get_hw_type() != SHTPS_HW_TYPE_HANDSET){
				return 1;
			}else{
				{
					u8 hwrev;
					
					hwrev = shtps_system_get_hw_revision();
					
					if(SHTPS_GET_HW_VERSION_RET_ES0_ES1 == hwrev
					){
						return 1;
					}
				}
			}
		#endif /* SHTPS_CHECK_HWID_ENABLE */
	}
	return 0;
}
#endif /* SHTPS_HOST_LPWG_MODE_ENABLE */

#if defined(SHTPS_LPWG_MODE_ENABLE)
static void shtps_read_touchevent_insleep(struct shtps_rmi_spi *ts, int state)
{
	#if defined( SHTPS_HOST_LPWG_MODE_ENABLE )
		struct shtps_touch_info info;
		u8* fingerInfo;
		unsigned short diff_x;
		unsigned short diff_y;
	#endif /* SHTPS_HOST_LPWG_MODE_ENABLE */
	u8 irq_sts;
	u8 val = 0;

	SHTPS_LOG_FUNC_CALL();

	memset(&info, 0, sizeof(info));

	shtps_fwctl_irqclear_get_irqfactor(ts, &irq_sts);

	#if defined( SHTPS_HOST_LPWG_MODE_ENABLE )
	{
		u8 buf[2 + 8];
		if(shtps_check_host_lpwg_enable() == 1){
			int fingerMax = shtps_get_fingermax(ts);
			memset(buf, 0, sizeof(buf));
			shtps_fwctl_get_one_fingerinfo(ts, 0, buf, &fingerInfo);

			info.fingers[0].state	= shtps_fwctl_get_finger_state(ts, 0, fingerMax, fingerInfo);
			info.fingers[0].x		= (shtps_fwctl_get_finger_pos_x(ts, fingerInfo) * SHTPS_POS_SCALE_X(ts) / 10000);
			info.fingers[0].y		= (shtps_fwctl_get_finger_pos_y(ts, fingerInfo) * SHTPS_POS_SCALE_Y(ts) / 10000);

			SHTPS_LOG_DBG_PRINT("[LPWG] TouchInfo <state: %d><x: %d><y: %d>\n", info.fingers[0].state, info.fingers[0].x, info.fingers[0].y);

			if( (info.fingers[0].state == SHTPS_TOUCH_STATE_FINGER) && (ts->lpwg.pre_info.fingers[0].state == SHTPS_TOUCH_STATE_NO_TOUCH) ){
				ts->lpwg.pre_info.fingers[0].state = info.fingers[0].state;
				ts->lpwg.pre_info.fingers[0].x     = info.fingers[0].x;
				ts->lpwg.pre_info.fingers[0].y     = info.fingers[0].y;
				ts->lpwg.swipe_check_time = jiffies + msecs_to_jiffies(SHTPS_LPWG_SWIPE_CHECK_TIME_MS);

				SHTPS_LOG_DBG_PRINT("[LPWG] swipe check zero pos <x: %d><y: %d>\n", ts->lpwg.pre_info.fingers[0].x, ts->lpwg.pre_info.fingers[0].y);
			}
			else if( (info.fingers[0].state == SHTPS_TOUCH_STATE_FINGER) && (ts->lpwg.pre_info.fingers[0].state == SHTPS_TOUCH_STATE_FINGER) ){
				if( time_after(jiffies, ts->lpwg.swipe_check_time) == 0 ){
					diff_x = (info.fingers[0].x >= ts->lpwg.pre_info.fingers[0].x) ?
								(info.fingers[0].x - ts->lpwg.pre_info.fingers[0].x) :
								(ts->lpwg.pre_info.fingers[0].x - info.fingers[0].x);

					diff_y = (info.fingers[0].y >= ts->lpwg.pre_info.fingers[0].y) ?
								(info.fingers[0].y - ts->lpwg.pre_info.fingers[0].y) :
								(ts->lpwg.pre_info.fingers[0].y - info.fingers[0].y);

					SHTPS_LOG_DBG_PRINT("[LPWG] swipe distance (%d, %d)\n", diff_x, diff_y);

					if( (diff_x > SHTPS_LPWG_SWIPE_DIST_THRESHOLD) || (diff_y > SHTPS_LPWG_SWIPE_DIST_THRESHOLD) ){
						val = SHTPS_LPWG_DETECT_GESTURE_TYPE_SWIPE;
					}
				}
				else{
					ts->lpwg.pre_info.fingers[0].x = info.fingers[0].x;
					ts->lpwg.pre_info.fingers[0].y = info.fingers[0].y;
					ts->lpwg.swipe_check_time = jiffies + msecs_to_jiffies(SHTPS_LPWG_SWIPE_CHECK_TIME_MS);

					SHTPS_LOG_DBG_PRINT("[LPWG] swipe check zero pos update<x: %d><y: %d>\n", ts->lpwg.pre_info.fingers[0].x, ts->lpwg.pre_info.fingers[0].y);
				}
			}
		}
		else{
			shtps_fwctl_get_gesturetype(ts, &val);
		}
	}
	#else
		shtps_fwctl_get_gesturetype(ts, &val);
	#endif /* SHTPS_HOST_LPWG_MODE_ENABLE */

	if(val == SHTPS_GESTURE_TYPE_ONE_FINGER_SWIPE){
		if(ts->lpwg.notify_enable != 0)
		{
			#if defined(SHTPS_PROXIMITY_SUPPORT_ENABLE)
			{
				if(SHTPS_LPWG_PROXIMITY_SUPPORT_ENABLE != 0){
					if(SHTPS_LPWG_PROXIMITY_CHECK_ASYNC_ENABLE != 0){
						shtps_notify_wakeup_event(ts);
						SHTPS_LOG_DBG_PRINT("[LPWG] proximity check async req\n");
						shtps_lpwg_proximity_check_start(ts);
					}else{
						ts->lpwg_proximity_get_data = shtps_proximity_check(ts);
						if(ts->lpwg_proximity_get_data != SHTPS_PROXIMITY_NEAR){
							shtps_notify_wakeup_event(ts);
						}else{
							SHTPS_LOG_DBG_PRINT("[LPWG] proximity near\n");
						}
					}
				}
				else{
					shtps_notify_wakeup_event(ts);
				}
			}
			#else
				shtps_notify_wakeup_event(ts);
			#endif /* SHTPS_PROXIMITY_SUPPORT_ENABLE */
		}
		else{
			SHTPS_LOG_DBG_PRINT("[LPWG] notify event blocked\n");
		}
	}else if(val == SHTPS_GESTURE_TYPE_ONE_FINGER_DOUBLE_TAP) {
		if(ts->lpwg.notify_enable != 0)
		{
			#if defined(SHTPS_PROXIMITY_SUPPORT_ENABLE)
			{
				if(SHTPS_LPWG_PROXIMITY_SUPPORT_ENABLE != 0){
					if(SHTPS_LPWG_PROXIMITY_CHECK_ASYNC_ENABLE != 0){
						shtps_notify_double_tap_event(ts);
						SHTPS_LOG_DBG_PRINT("[LPWG] proximity check async req\n");
						shtps_lpwg_proximity_check_start(ts);
					}else{
						if( shtps_proximity_check(ts) != SHTPS_PROXIMITY_NEAR ){
							shtps_notify_double_tap_event(ts);
						}else{
							SHTPS_LOG_DBG_PRINT("[LPWG] proximity near\n");
						}
					}
				}
				else{
					shtps_notify_double_tap_event(ts);
				}
			}
			#else
				shtps_notify_double_tap_event(ts);
			#endif /* SHTPS_PROXIMITY_SUPPORT_ENABLE */
		}
		else{
			SHTPS_LOG_DBG_PRINT("[LPWG] notify event blocked\n");
		}
	}else{
		SHTPS_LOG_DBG_PRINT("LPWG: Not Support Gesture Type Detect\n");\
	}

	#if defined( SHTPS_HOST_LPWG_MODE_ENABLE )
		if(shtps_check_host_lpwg_enable() != 0){
			if(info.fingers[0].state != SHTPS_TOUCH_STATE_FINGER){
				ts->lpwg.pre_info.fingers[0].state = SHTPS_TOUCH_STATE_NO_TOUCH;
				ts->lpwg.pre_info.fingers[0].x     = 0xFFFF;
				ts->lpwg.pre_info.fingers[0].y     = 0xFFFF;
				ts->lpwg.swipe_check_time = 0;
			}
		}
	#endif /* SHTPS_HOST_LPWG_MODE_ENABLE */
}
#endif /*  SHTPS_LPWG_MODE_ENABLE */

void shtps_read_touchevent(struct shtps_rmi_spi *ts, int state)
{
	u8 buf[2 + SHTPS_FINGER_MAX * 8];
	u8 event;
	u8 irq_sts, ext_sts;
	u8 *pFingerInfoBuf;
	struct shtps_touch_info info;

	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__READ_EVENT, "%d", state);

	memset(buf, 0, sizeof(buf));
	memset(&info, 0, sizeof(info));
	shtps_performance_check(SHTPS_PERFORMANCE_CHECK_STATE_CONT);
	shtps_fwctl_get_fingerinfo(ts, buf, 0, &irq_sts, &ext_sts, &pFingerInfoBuf);
	shtps_performance_check(SHTPS_PERFORMANCE_CHECK_STATE_CONT);

	#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
		if(ts->wakeup_touch_event_inhibit_state != 0){
			memset(pFingerInfoBuf, 0, sizeof(buf) - 2);
			SHTPS_LOG_DBG_PRINT("[wakeup_fail_event] TouchEvent Cleared by not rezero exec yet\n");
		}

		shtps_mutex_lock_wakeup_fail_touch_event_reject();
		if(ts->wakeup_fail_touch_event_reject_rezero_exec != 0){
			memset(pFingerInfoBuf, 0, sizeof(buf) - 2);
			ts->wakeup_fail_touch_event_reject_rezero_exec = 0;
			SHTPS_LOG_DBG_PRINT("[wakeup_fail_event] TouchEvent Cleared by rezero touch conflict\n");
		}
		shtps_mutex_unlock_wakeup_fail_touch_event_reject();
	#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */

	switch(state){
	case SHTPS_STATE_SLEEP:
	case SHTPS_STATE_IDLE:
		break;

	case SHTPS_STATE_SLEEP_FACETOUCH:
		break;

	case SHTPS_STATE_FACETOUCH:
	case SHTPS_STATE_ACTIVE:
	default:
		#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
			{
				int fingerMax = shtps_get_fingermax(ts);
				if(SHTPS_STATE_FACETOUCH == state && (pFingerInfoBuf[fingerMax * 5] & 0x02) != 0){
					shtps_event_all_cancel(ts);
				}
			}
		#endif /* #if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT ) */

		shtps_calc_notify(ts, pFingerInfoBuf, &info, &event);

		if(event == SHTPS_EVENT_TD){
			blocking_notifier_call_chain(&shtps_notifier_list_touchevent, 1, NULL);
		}else if(event == SHTPS_EVENT_TU){
			blocking_notifier_call_chain(&shtps_notifier_list_touchevent, 0, NULL);
		}

		if(event != 0xff){
			shtps_perflock_enable_start(ts, event, &info);	//SHTPS_CPU_CLOCK_CONTROL_ENABLE

			#if defined(SHTPS_LPWG_MODE_ENABLE)
				if(ts->lpwg.block_touchevent == 0){
					shtps_event_report(ts, &info, event);
				}else{
					SHTPS_LOG_DBG_PRINT("LPWG touch event blocked\n");
					shtps_event_update(ts, &info);
				}
			#else
				shtps_event_report(ts, &info, event);
			#endif /* SHTPS_LPWG_MODE_ENABLE */
		}

		#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
			if(SHTPS_STATE_FACETOUCH == state){
				shtps_check_facetouchoff(ts);
			}
		#endif /* #if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT ) */

		#if defined(SHTPS_LPWG_MODE_ENABLE)
			if(ts->lpwg.block_touchevent != 0){
				u8 rezero_req = 0;

				if(time_after(jiffies,
					ts->lpwg.wakeup_time + msecs_to_jiffies(SHTPS_LPWG_BLOCK_TIME_MAX_MS)))
				{
					rezero_req = 1;
					SHTPS_LOG_DBG_PRINT("LPWG force rezero\n");
				}else if((ts->lpwg.tu_rezero_req != 0) && 
							(ts->finger_state[0] | ts->finger_state[1] | ts->finger_state[2]) == 0)
				{
					rezero_req = 1;
					SHTPS_LOG_DBG_PRINT("LPWG rezero by touch up\n");
				}

				if(rezero_req != 0){
					ts->touch_state.numOfFingers = 0;
					ts->lpwg.block_touchevent = 0;
					ts->lpwg.tu_rezero_req = 0;
					shtps_rezero_request(ts,
										 SHTPS_REZERO_REQUEST_REZERO,
										 SHTPS_REZERO_TRIGGER_WAKEUP);

					#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
						ts->wakeup_touch_event_inhibit_state = 0;
						SHTPS_LOG_WAKEUP_FAIL_TOUCH_EVENT_REJECT("TouchEvent Inhibit end by rezero exec\n");
					#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */
				}
			}
		#endif /* SHTPS_LPWG_MODE_ENABLE */

		#if defined ( SHTPS_ACCUMULATE_FW_REPORT_INFO_ENABLE )
			shtps_accumulate_fw_report_info_add(&ts->fw_report_info);
		#endif /* #if defined ( SHTPS_ACCUMULATE_FW_REPORT_INFO_ENABLE ) */

		memcpy(&ts->fw_report_info_store, &ts->fw_report_info, sizeof(ts->fw_report_info));

		break;
	}
}

#if defined(SHTPS_IRQ_LOADER_CHECK_INT_STATUS_ENABLE)
static int shtps_loader_irqclr(struct shtps_rmi_spi *ts)
{
	int rc;
	u8 sts;
	SHTPS_LOG_FUNC_CALL();
	rc =  shtps_fwctl_irqclear_get_irqfactor(ts, &sts);
	if(rc == 0){
		return sts;
	}else{
		return 0;
	}
}
#else
static void shtps_loader_irqclr(struct shtps_rmi_spi *ts)
{
	u8 sts;
	SHTPS_LOG_FUNC_CALL();
	shtps_fwctl_irqclear_get_irqfactor(ts, &sts);
}
#endif /* SHTPS_IRQ_LOADER_CHECK_INT_STATUS_ENABLE */

/* -----------------------------------------------------------------------------------
 */
static int shtps_touchpanel_connect_check(struct shtps_rmi_spi *ts)
{
	int rc;
	u8 buf;

	SHTPS_LOG_FUNC_CALL();
	rc = shtps_fwctl_get_pageselect(ts, &buf);
	
	if(rc){
		return 1;
	}else{
		return 0;
	}
}
/* -----------------------------------------------------------------------------------
 */
#if defined( SHTPS_BOOT_FWUPDATE_ENABLE )
int shtps_boot_fwupdate_enable_check(struct shtps_rmi_spi *ts)	
{
	SHTPS_LOG_FUNC_CALL();
	#if defined(SHTPS_BOOT_FWUPDATE_ONLY_ON_HANDSET)
	{
		sharp_smem_common_type *sharp_smem;
		unsigned char handset;

		sharp_smem = sh_smem_get_common_address();
		handset = sharp_smem->sh_hw_handset;

		if(handset == 0){
			return 0;
		}
	}
	#endif /* SHTPS_BOOT_FWUPDATE_ONLY_ON_HANDSET */
	
	#if defined( SHTPS_BOOT_FWUPDATE_SKIP_ES0_ENABLE )
	{
		u8 hwrev;
		hwrev = shtps_system_get_hw_revision();
		if(hwrev == SHTPS_GET_HW_VERSION_RET_ES_0){
			SHTPS_LOG_DBG_PRINT("fwupdate diable - hwrev ES0[%d]\n", hwrev);
			return 0;
		}
	}
	#endif /* SHTPS_BOOT_FWUPDATE_SKIP_ES0_ENABLE */
	
	#if defined( SHTPS_TPIN_CHECK_ENABLE )
		if(shtps_tpin_enable_check(ts) != 0){
			return 0;
		}
	#endif /* SHTPS_TPIN_CHECK_ENABLE */

	return 1;
}
#endif /* #if defined( SHTPS_BOOT_FWUPDATE_ENABLE ) */

static int shtps_fwupdate_enable(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
#if defined( SHTPS_FWUPDATE_DISABLE )
	return 0;
#else
	return 1;
#endif /* #if defined( SHTPS_FWUPDATE_DISABLE ) */
}

static int shtps_loader_wait_attn(struct shtps_rmi_spi *ts)
{
	int rc;

	SHTPS_LOG_FUNC_CALL();
	SHTPS_LOG_DBG_PRINT("shtps_loader_wait_attn() start:%d\n", ts->loader.ack);

	rc = wait_event_interruptible_timeout(ts->loader.wait_ack,
			ts->loader.ack == 1,
			msecs_to_jiffies(SHTPS_BOOTLOADER_ACK_TMO));

	if(0 == rc && 0 == ts->loader.ack){
		_log_msg_sync( LOGMSG_ID__BL_ATTN_ERROR, "");
		SHTPS_LOG_ERR_PRINT("shtps_loader_wait_attn() ACK:%d\n", ts->loader.ack);
		return -1;
	}

	if(0 == rc){
		_log_msg_sync( LOGMSG_ID__BL_ATTN_TIMEOUT, "");
		SHTPS_LOG_ERR_PRINT("shtps_loader_wait_attn() warning rc = %d\n", rc);
	}

	ts->loader.ack = 0;

	SHTPS_LOG_DBG_PRINT("shtps_loader_wait_attn() end:%d\n", ts->loader.ack);
	return 0;
}

static void shtps_loader_wakeup(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	ts->loader.ack = 1;
	wake_up_interruptible(&ts->loader.wait_ack);
}

int shtps_enter_bootloader(struct shtps_rmi_spi *ts)
{
	int rc;

	SHTPS_LOG_FUNC_CALL();

	shtps_mutex_lock_loader();

	request_event(ts, SHTPS_EVENT_STOP, 0);
	msleep(SHTPS_SLEEP_IN_WAIT_MS);
	if(request_event(ts, SHTPS_EVENT_STARTLOADER, 0) != 0){
		SHTPS_LOG_ERR_PRINT("shtps_enter_bootloader() start loader error\n");
		goto err_exit;
	}
	shtps_wait_startup(ts);

	shtps_sleep(ts, 1);
	#if !defined(SHTPS_ACTIVE_SLEEP_WAIT_ALWAYS_ENABLE)
		msleep(SHTPS_SLEEP_IN_WAIT_MS);
	#endif /* !SHTPS_ACTIVE_SLEEP_WAIT_ALWAYS_ENABLE */

	ts->loader.ack = 0;
	shtps_fwctl_loader_cmd_enterbl(ts);
	rc = shtps_loader_wait_attn(ts);
	if(rc){
		SHTPS_LOG_ERR_PRINT("shtps_enter_bootloader() mode change error\n");
		goto err_exit;
	}

	shtps_fwctl_set_dev_state(ts, SHTPS_DEV_STATE_LOADER);

#if defined(SHTPS_PDT_READ_RETRY_ENABLE)
	if(shtps_map_construct(ts, 0) != 0){
#else
	if(shtps_map_construct(ts) != 0){
#endif /* SHTPS_PDT_READ_RETRY_ENABLE */

		SHTPS_LOG_ERR_PRINT("shtps_map_construct() error!!\n");
	}

#if defined(SHTPS_DEF_FWCTL_IC_TYPE_3700)
	shtps_fwctl_loader_cmd_get_partition_table(ts);
	rc = shtps_loader_wait_attn(ts);
	if(rc){
		SHTPS_LOG_ERR_PRINT("shtps_map_construct() get partition length error\n");
		goto err_exit;
	}
	rc = shtps_fwctl_loader_get_partition_table(ts);
	if(rc){
		SHTPS_LOG_ERR_PRINT("shtps_map_construct() get partition length error\n");
		goto err_exit;
	}
#endif

	SHTPS_LOG_DBG_PRINT("shtps_enter_bootloader() done\n");
	shtps_mutex_unlock_loader();
	return 0;

err_exit:
	shtps_mutex_unlock_loader();
	return -1;
}

static int shtps_exit_bootloader(struct shtps_rmi_spi *ts)
{
	int rc;

	SHTPS_LOG_FUNC_CALL();
	SHTPS_LOG_ANALYSIS("SW Reset execute\n");

	rc = shtps_fwctl_loader_exit(ts);
	request_event(ts, SHTPS_EVENT_STOP, 0);

	SHTPS_LOG_DBG_PRINT("shtps_exit_bootloader() done:%d\n",rc);
	return rc;
}

int shtps_lockdown_bootloader(struct shtps_rmi_spi *ts, u8* fwdata)
{
	return 0;
}

int shtps_flash_erase(struct shtps_rmi_spi *ts)
{
	int rc;

	SHTPS_LOG_FUNC_CALL();
	if(!shtps_fwupdate_enable(ts)){
		return 0;
	}
	shtps_mutex_lock_loader();

	rc = shtps_fwctl_loader_cmd_erase(ts);
	if(rc == 0)	rc = shtps_loader_wait_attn(ts);
	if(rc == 0)	rc = shtps_fwctl_loader_get_result_erase(ts);
#if defined(SHTPS_DEF_FWCTL_IC_TYPE_3700)
	if(rc == 0)	rc = shtps_fwctl_loader_cmd_erase_config(ts);
	if(rc == 0)	rc = shtps_loader_wait_attn(ts);
	if(rc == 0)	rc = shtps_fwctl_loader_get_result_erase(ts);
#endif
	if(rc == 0)	rc = shtps_fwctl_loader_init_writeconfig(ts);

	shtps_mutex_unlock_loader();
	SHTPS_LOG_DBG_PRINT("shtps_flash_erase() done:%d\n",rc);
	return rc;
}

int shtps_flash_writeImage(struct shtps_rmi_spi *ts, u8 *fwdata)
{
	int rc;

	SHTPS_LOG_FUNC_CALL();
	if(!shtps_fwupdate_enable(ts)){
		return 0;
	}

	shtps_mutex_lock_loader();

	rc = shtps_fwctl_loader_write_image(ts, fwdata, shtps_fwctl_loader_get_blocksize(ts));
	if(rc == 0)	rc = shtps_fwctl_loader_cmd_writeimage(ts);
	if(rc == 0)	rc = shtps_loader_wait_attn(ts);
	if(rc == 0)	rc = shtps_fwctl_loader_get_result_writeimage(ts);

	shtps_mutex_unlock_loader();

	SHTPS_LOG_DBG_PRINT("shtps_flash_writeImage() done\n");
	return rc;
}

int shtps_flash_writeConfig(struct shtps_rmi_spi *ts, u8 *fwdata)
{
	int rc;
	int block;
	int blockNum;
	int blockSize;

	SHTPS_LOG_FUNC_CALL();
	if(!shtps_fwupdate_enable(ts)){
		return 0;
	}

	shtps_mutex_lock_loader();

	blockNum  = (int)shtps_fwctl_loader_get_config_blocknum(ts);
	blockSize = (int)shtps_fwctl_loader_get_blocksize(ts);
	rc = shtps_fwctl_loader_init_writeconfig(ts);
	for(block = 0;(rc == 0)&&(block < blockNum);block++){
		rc = shtps_fwctl_loader_write_config(ts, &fwdata[block * blockSize], blockSize);
		if(rc == 0)	rc = shtps_fwctl_loader_cmd_writeconfig(ts);
		if(rc == 0)	rc = shtps_loader_wait_attn(ts);
		if(rc == 0)	rc = shtps_fwctl_loader_get_result_writeconfig(ts);
	}
	if(rc == 0)	rc = shtps_exit_bootloader(ts);

	shtps_mutex_unlock_loader();
	SHTPS_LOG_DBG_PRINT("shtps_flash_writeConfig() done:%d\n",rc);
	return rc;
}

int shtps_fw_update(struct shtps_rmi_spi *ts, const unsigned char *fw_data)
{
	int i;
	unsigned long blockSize;
	unsigned long blockNum;

	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__BOOT_FW_UPDATE, "");

	if(0 != shtps_enter_bootloader(ts)){
		SHTPS_LOG_ERR_PRINT("error - shtps_enter_bootloader()\n");
		_log_msg_sync( LOGMSG_ID__BOOT_FW_UPDATE_FAIL, "0");
		return -1;
	}

	if(0 != shtps_lockdown_bootloader(ts, (u8*)&fw_data[0x00d0])){
		SHTPS_LOG_ERR_PRINT("error - shtps_lockdown_bootloader()\n");
		_log_msg_sync( LOGMSG_ID__BOOT_FW_UPDATE_FAIL, "1");
		return -1;
	}

	if(0 != shtps_flash_erase(ts)){
		SHTPS_LOG_ERR_PRINT("error - shtps_flash_erase()\n");
		_log_msg_sync( LOGMSG_ID__BOOT_FW_UPDATE_FAIL, "2");
		return -1;
	}

	blockSize = shtps_fwctl_loader_get_blocksize(ts);
	blockNum  = shtps_fwctl_loader_get_firm_blocknum(ts);

#if defined(SHTPS_DEF_FWCTL_IC_TYPE_3400)
	for(i = 0;i < blockNum;i++){
		if(0 != shtps_flash_writeImage(ts, (u8*)&fw_data[0x0100 + i * blockSize])){
			SHTPS_LOG_ERR_PRINT("error - shtps_flash_writeImage(%d)\n", i);
			_log_msg_sync( LOGMSG_ID__BOOT_FW_UPDATE_FAIL, "3");
			return -1;
		}
	}

	if(0 != shtps_flash_writeConfig(ts, (u8*)&fw_data[0x0100 + (blockNum * blockSize)])){
		SHTPS_LOG_ERR_PRINT("error - shtps_flash_writeConfig()\n");
		_log_msg_sync( LOGMSG_ID__BOOT_FW_UPDATE_FAIL, "4");
		return -1;
	}
#else
	for(i = 0;i < blockNum;i++){
		if(0 != shtps_flash_writeImage(ts, (u8*)&fw_data[0x1110 + i * blockSize])){
			SHTPS_LOG_ERR_PRINT("error - shtps_flash_writeImage(%d)\n", i);
			_log_msg_sync( LOGMSG_ID__BOOT_FW_UPDATE_FAIL, "3");
			return -1;
		}
	}

	if(0 != shtps_flash_writeConfig(ts, (u8*)&fw_data[0x08F0])){
		SHTPS_LOG_ERR_PRINT("error - shtps_flash_writeConfig()\n");
		_log_msg_sync( LOGMSG_ID__BOOT_FW_UPDATE_FAIL, "4");
		return -1;
	}
#endif

	DBG_PRINTK("fw update done\n");
	_log_msg_sync( LOGMSG_ID__BOOT_FW_UPDATE_DONE, "");

	return 0;
}

/* -----------------------------------------------------------------------------------
*/
int shtps_set_veilview_state(struct shtps_rmi_spi *ts, unsigned long arg)
{
	return 0;
}

int shtps_get_veilview_pattern(struct shtps_rmi_spi *ts)
{
	return SHTPS_VEILVIEW_PATTERN;
}

/* -----------------------------------------------------------------------------------
 */
#if defined ( SHTPS_DEVELOP_MODE_ENABLE )
static char* shtps_get_state_str(int state)
{
	if(state == SHTPS_STATE_IDLE){
		 return "IDLE";
	}
	else if(state == SHTPS_STATE_WAIT_WAKEUP){
		 return "WAIT_WAKEUP";
	}
	else if(state == SHTPS_STATE_WAIT_READY){
		 return "WAIT_READY";
	}
	else if(state == SHTPS_STATE_ACTIVE){
		 return "ACTIVE";
	}
	else if(state == SHTPS_STATE_BOOTLOADER){
		 return "BOOTLOADER";
	}
	else if(state == SHTPS_STATE_FACETOUCH){
		 return "FACETOUCH";
	}
	else if(state == SHTPS_STATE_FWTESTMODE){
		 return "FWTESTMODE";
	}
	else if(state == SHTPS_STATE_SLEEP){
		 return "SLEEP";
	}
	else if(state == SHTPS_STATE_SLEEP_FACETOUCH){
		 return "SLEEP_FACETOUCH";
	}
	return "UNKNOWN";
}

static char* shtps_get_event_str(int event)
{
	if(event == SHTPS_EVENT_START){
		 return "START";
	}
	else if(event == SHTPS_EVENT_STOP){
		 return "STOP";
	}
	else if(event == SHTPS_EVENT_SLEEP){
		 return "SLEEP";
	}
	else if(event == SHTPS_EVENT_WAKEUP){
		 return "WAKEUP";
	}
	else if(event == SHTPS_EVENT_STARTLOADER){
		 return "STARTLOADER";
	}
	else if(event == SHTPS_EVENT_STARTTM){
		 return "STARTTM";
	}
	else if(event == SHTPS_EVENT_STOPTM){
		 return "STOPTM";
	}
	else if(event == SHTPS_EVENT_FACETOUCHMODE_ON){
		 return "FACETOUCHMODE_ON";
	}
	else if(event == SHTPS_EVENT_FACETOUCHMODE_OFF){
		 return "FACETOUCHMODE_OFF";
	}
	else if(event == SHTPS_EVENT_INTERRUPT){
		 return "INTERRUPT";
	}
	else if(event == SHTPS_EVENT_TIMEOUT){
		 return "TIMEOUT";
	}
	return "UNKNOWN";
}
#endif /* defined ( SHTPS_DEVELOP_MODE_ENABLE ) */

int request_event(struct shtps_rmi_spi *ts, int event, int param)	
{
	int ret;

	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__REQUEST_EVENT, "%d|%d", event, ts->state_mgr.state);

	SHTPS_LOG_DBG_PRINT("event %s(%d) in state %s(%d)\n", shtps_get_event_str(event), event, shtps_get_state_str(ts->state_mgr.state), ts->state_mgr.state);

	shtps_mutex_lock_ctrl();

	switch(event){
	case SHTPS_EVENT_START:
		ret = state_func_tbl[ts->state_mgr.state]->start(ts, param);
		break;
	case SHTPS_EVENT_STOP:
		ret = state_func_tbl[ts->state_mgr.state]->stop(ts, param);
		break;
	case SHTPS_EVENT_SLEEP:
		ret = state_func_tbl[ts->state_mgr.state]->sleep(ts, param);
		break;
	case SHTPS_EVENT_WAKEUP:
		ret = state_func_tbl[ts->state_mgr.state]->wakeup(ts, param);
		break;
	case SHTPS_EVENT_STARTLOADER:
		ret = state_func_tbl[ts->state_mgr.state]->start_ldr(ts, param);
		break;
	case SHTPS_EVENT_STARTTM:
		ret = state_func_tbl[ts->state_mgr.state]->start_tm(ts, param);
		break;
	case SHTPS_EVENT_STOPTM:
		ret = state_func_tbl[ts->state_mgr.state]->stop_tm(ts, param);
		break;
	case SHTPS_EVENT_FACETOUCHMODE_ON:
		ret = state_func_tbl[ts->state_mgr.state]->facetouch_on(ts, param);
		break;
	case SHTPS_EVENT_FACETOUCHMODE_OFF:
		ret = state_func_tbl[ts->state_mgr.state]->facetouch_off(ts, param);
		break;
	case SHTPS_EVENT_INTERRUPT:
		ret = state_func_tbl[ts->state_mgr.state]->interrupt(ts, param);
		break;
	case SHTPS_EVENT_TIMEOUT:
		ret = state_func_tbl[ts->state_mgr.state]->timeout(ts, param);
		break;
	default:
		ret = -1;
		break;
	}

	shtps_mutex_unlock_ctrl();

	return ret;
}

static int state_change(struct shtps_rmi_spi *ts, int state)
{
	int ret = 0;
	int old_state = ts->state_mgr.state;

	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__STATE_CHANGE, "%d|%d", ts->state_mgr.state, state);
	SHTPS_LOG_DBG_PRINT("state %s(%d) -> %s(%d)\n", shtps_get_state_str(ts->state_mgr.state), ts->state_mgr.state, shtps_get_state_str(state), state);

	if(ts->state_mgr.state != state){
		ts->state_mgr.state = state;
		ret = state_func_tbl[ts->state_mgr.state]->enter(ts, old_state);
	}
	return ret;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_nop(struct shtps_rmi_spi *ts, int param)
{
	_log_msg_sync( LOGMSG_ID__STATEF_NOP, "%d", ts->state_mgr.state);
	SHTPS_LOG_FUNC_CALL();
	return 0;
}

static int shtps_statef_cmn_error(struct shtps_rmi_spi *ts, int param)
{
	_log_msg_sync( LOGMSG_ID__STATEF_ERROR, "%d", ts->state_mgr.state);
	SHTPS_LOG_FUNC_CALL();
	return -1;
}

static int shtps_statef_cmn_stop(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__STATEF_STOP, "%d", ts->state_mgr.state);
	
	blocking_notifier_call_chain(&shtps_notifier_list_touchevent, 0, NULL);
	
	shtps_standby_param(ts);
	shtps_irq_disable(ts);
	shtps_system_set_sleep(ts);
	#if defined(SHTPS_POWER_OFF_IN_SLEEP_ENABLE)
		shtps_device_power_off();
	#endif /* SHTPS_POWER_OFF_IN_SLEEP_ENABLE */
	state_change(ts, SHTPS_STATE_IDLE);
	return 0;
}

static int shtps_statef_cmn_facetouch_on(struct shtps_rmi_spi *ts, int param)
{
#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
		shtps_set_facetouchmode(ts, 1);
#endif /* CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT */
	return 0;
}

static int shtps_statef_cmn_facetouch_off(struct shtps_rmi_spi *ts, int param)
{
#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
		shtps_set_facetouchmode(ts, 0);
#endif /* CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT */
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_idle_start(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	#if defined( SHTPS_TPIN_CHECK_ENABLE )
		if(shtps_tpin_enable_check(ts) != 0){
			return -1;
		}
	#endif /* SHTPS_TPIN_CHECK_ENABLE */

	shtps_system_set_wakeup(ts);
	shtps_clr_startup_err(ts);
	shtps_reset(ts);
	shtps_irq_enable(ts);
	shtps_irqtimer_start(ts, 100);
	shtps_set_startmode(ts, SHTPS_MODE_NORMAL);
	state_change(ts, SHTPS_STATE_WAIT_WAKEUP);
	return 0;
}

static int shtps_statef_idle_start_ldr(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	#if defined( SHTPS_TPIN_CHECK_ENABLE )
		if(shtps_tpin_enable_check(ts) != 0){
			return -1;
		}
	#endif /* SHTPS_TPIN_CHECK_ENABLE */

	shtps_system_set_wakeup(ts);
	shtps_clr_startup_err(ts);
	shtps_reset(ts);
	shtps_irq_enable(ts);
	shtps_irqtimer_start(ts, 100);
	shtps_set_startmode(ts, SHTPS_MODE_LOADER);
	state_change(ts, SHTPS_STATE_WAIT_WAKEUP);
	return 0;
}

static int shtps_statef_idle_wakeup(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	#if defined( SHTPS_TPIN_CHECK_ENABLE )
		if(shtps_tpin_enable_check(ts) != 0){
			return -1;
		}
	#endif /* SHTPS_TPIN_CHECK_ENABLE */

	shtps_system_set_wakeup(ts);
	shtps_clr_startup_err(ts);
	shtps_reset(ts);
	shtps_irq_enable(ts);
	shtps_irqtimer_start(ts, 100);
	shtps_set_startmode(ts, SHTPS_MODE_NORMAL);
	state_change(ts, SHTPS_STATE_WAIT_WAKEUP);
	return 0;
}

static int shtps_statef_idle_int(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	#if defined( SHTPS_POWER_OFF_IN_SLEEP_ENABLE )
		;
	#else
		shtps_read_touchevent(ts, SHTPS_STATE_IDLE);
	#endif /* SHTPS_POWER_OFF_IN_SLEEP_ENABLE */
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_waiwakeup_stop(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_irqtimer_stop(ts);
	shtps_statef_cmn_stop(ts, param);
	return 0;
}

static int shtps_statef_waiwakeup_int(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_reset_startuptime(ts);
	shtps_irqtimer_stop(ts);
	shtps_irqtimer_start(ts, 1000);
	state_change(ts, SHTPS_STATE_WAIT_READY);
	return 0;
}

static int shtps_statef_waiwakeup_tmo(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_irqtimer_start(ts, 1000);
	state_change(ts, SHTPS_STATE_WAIT_READY);
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_state_waiready_stop(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_irqtimer_stop(ts);
	shtps_statef_cmn_stop(ts, param);
	return 0;
}

static int shtps_state_waiready_int(struct shtps_rmi_spi *ts, int param)
{
#if defined(SHTPS_PDT_READ_RETRY_ENABLE)
	int rc;
	int retry = SHTPS_PDT_READ_RETRY_COUNT;
#endif /* SHTPS_PDT_READ_RETRY_ENABLE */
	unsigned long time;
	SHTPS_LOG_FUNC_CALL();
	if((time = shtps_check_startuptime(ts)) != 0){
		SHTPS_LOG_DBG_PRINT("startup wait time : %lu\n", time);
		msleep(time);
	}

	shtps_irqtimer_stop(ts);

#if defined(SHTPS_PDT_READ_RETRY_ENABLE)
	do{
		rc = shtps_map_construct(ts, 1);
		if(rc){
			msleep(SHTPS_PDT_READ_RETRY_INTERVAL);
		}
	}while(rc != 0 && retry-- > 0);
#else
	if(shtps_map_construct(ts) != 0){
		SHTPS_LOG_ERR_PRINT("shtps_map_construct() error!!\n");
		shtps_statef_cmn_stop(ts, 0);
		shtps_notify_startup(ts, SHTPS_STARTUP_FAILED);
		return -1;
	}
#endif /* SHTPS_PDT_READ_RETRY_ENABLE */

	if(SHTPS_MODE_NORMAL == shtps_get_startmode(ts)){
		state_change(ts, SHTPS_STATE_ACTIVE);
	}else{
		state_change(ts, SHTPS_STATE_BOOTLOADER);
	}

#if defined(SHTPS_PDT_READ_RETRY_ENABLE)
	if(rc != 0){
		shtps_notify_startup(ts, SHTPS_STARTUP_FAILED);
	}else
#endif /* SHTPS_PDT_READ_RETRY_ENABLE */
	shtps_notify_startup(ts, SHTPS_STARTUP_SUCCESS);
	return 0;
}

static int shtps_state_waiready_tmo(struct shtps_rmi_spi *ts, int param)
{
#if defined(SHTPS_PDT_READ_RETRY_ENABLE)
	int rc;
	int retry = SHTPS_PDT_READ_RETRY_COUNT;

	if(shtps_touchpanel_connect_check(ts) != 0){
		shtps_statef_cmn_stop(ts, param);
		shtps_notify_startup(ts, SHTPS_STARTUP_FAILED);
		return -1;
	}

	do{
		rc = shtps_map_construct(ts, 1);
		if(rc){
			msleep(SHTPS_PDT_READ_RETRY_INTERVAL);
		}
	}while(rc != 0 && retry-- > 0);
#else
	if(shtps_map_construct(ts) != 0){
		SHTPS_LOG_ERR_PRINT("shtps_map_construct() error!!\n");
		shtps_statef_cmn_stop(ts, 0);
		shtps_notify_startup(ts, SHTPS_STARTUP_FAILED);
		return -1;
	}
#endif /* SHTPS_PDT_READ_RETRY_ENABLE */

	if(SHTPS_MODE_NORMAL == shtps_get_startmode(ts)){
		state_change(ts, SHTPS_STATE_ACTIVE);
	}else{
		state_change(ts, SHTPS_STATE_BOOTLOADER);
	}

#if defined(SHTPS_PDT_READ_RETRY_ENABLE)
	if(rc != 0){
		shtps_notify_startup(ts, SHTPS_STARTUP_FAILED);
	}else
#endif /* SHTPS_PDT_READ_RETRY_ENABLE */
	shtps_notify_startup(ts, SHTPS_STARTUP_SUCCESS);
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_active_enter(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	if(param == SHTPS_STATE_WAIT_READY){
		shtps_init_param(ts);
		if(ts->poll_info.boot_rezero_flag == 0){
			ts->poll_info.boot_rezero_flag = 1;
		}
		shtps_read_touchevent(ts, ts->state_mgr.state);
		#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
			shtps_irq_enable(ts);
		#else
			shtps_irq_wake_enable(ts);
		#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */
	}

	#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
		if(1 == shtps_get_facetouchmode(ts)){
			state_change(ts, SHTPS_STATE_FACETOUCH);
		}
	#endif /* CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT */

	#if defined(SHTPS_SYSTEM_BOOT_MODE_CHECK_ENABLE)
		if( (ts->system_boot_mode == SH_BOOT_O_C) || (ts->system_boot_mode == SH_BOOT_U_O_C) ){
			state_change(ts, SHTPS_STATE_SLEEP);
		}
	#endif /* SHTPS_SYSTEM_BOOT_MODE_CHECK_ENABLE */

	return 0;
}

static int shtps_statef_active_stop(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		shtps_irq_disable(ts);
	#else
		shtps_irq_wake_disable(ts);
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

	return shtps_statef_cmn_stop(ts, param);
}

static int shtps_statef_active_sleep(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	state_change(ts, SHTPS_STATE_SLEEP);
	return 0;
}

static int shtps_statef_active_starttm(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	state_change(ts, SHTPS_STATE_FWTESTMODE);
	return 0;
}

static int shtps_statef_active_facetouch_on(struct shtps_rmi_spi *ts, int param)
{
#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
		shtps_set_facetouchmode(ts, 1);
		state_change(ts, SHTPS_STATE_FACETOUCH);
#endif /* CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT */
	return 0;
}

static int shtps_statef_active_int(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_read_touchevent(ts, SHTPS_STATE_ACTIVE);
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_loader_enter(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_wake_lock_for_fwupdate(ts);	//SHTPS_CPU_SLEEP_CONTROL_FOR_FWUPDATE_ENABLE

	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		shtps_irq_enable(ts);
	#else
		shtps_irq_wake_enable(ts);
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

	shtps_loader_irqclr(ts);
	return 0;
}

static int shtps_statef_loader_stop(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		shtps_irq_disable(ts);
	#else
		shtps_irq_wake_disable(ts);
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

	shtps_wake_unlock_for_fwupdate(ts);	//SHTPS_CPU_SLEEP_CONTROL_FOR_FWUPDATE_ENABLE

	return shtps_statef_cmn_stop(ts, param);
}

static int shtps_statef_loader_int(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();

	#if defined(SHTPS_IRQ_LOADER_CHECK_INT_STATUS_ENABLE)
		if(shtps_loader_irqclr(ts) != 0){
			shtps_loader_wakeup(ts);
		}
	#else
		shtps_loader_irqclr(ts);
		shtps_loader_wakeup(ts);
	#endif /* SHTPS_IRQ_LOADER_CHECK_INT_STATUS_ENABLE */

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_facetouch_sleep(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
		#if defined( SHTPS_FACETOUCH_OFF_DETECT_DOZE_ENABLE )
			#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
				shtps_set_doze_mode(ts, 1);
			#endif /* SHTPS_LOW_POWER_MODE_ENABLE */
		#endif /* SHTPS_FACETOUCH_OFF_DETECT_DOZE_ENABLE */
	#endif /* #if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT ) */
	state_change(ts, SHTPS_STATE_SLEEP_FACETOUCH);
	return 0;
}

static int shtps_statef_facetouch_starttm(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	state_change(ts, SHTPS_STATE_FWTESTMODE);
	return 0;
}

static int shtps_statef_facetouch_facetouch_off(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
		shtps_set_facetouchmode(ts, 0);
		state_change(ts, SHTPS_STATE_ACTIVE);
#endif /* #if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT ) */
	return 0;
}

static int shtps_statef_facetouch_int(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_read_touchevent(ts, SHTPS_STATE_FACETOUCH);
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_fwtm_enter(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_tm_irqcheck(ts);

	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		shtps_irq_enable(ts);
	#else
		shtps_irq_wake_enable(ts);
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

	shtps_start_tm(ts);
	return 0;
}

static int shtps_statef_fwtm_stop(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		shtps_irq_disable(ts);
	#else
		shtps_irq_wake_disable(ts);
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

	return shtps_statef_cmn_stop(ts, param);
}

static int shtps_statef_fwtm_stoptm(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_stop_tm(ts);
	state_change(ts, SHTPS_STATE_ACTIVE);
	return 0;
}

static int shtps_statef_fwtm_int(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	if(shtps_tm_irqcheck(ts)){
		shtps_tm_wakeup(ts);
	}
	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_sleep_enter(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	#if defined( SHTPS_DEVELOP_MODE_ENABLE )
		shtps_debug_sleep_enter();
	#endif /* #if defined( SHTPS_DEVELOP_MODE_ENABLE ) */

	shtps_perflock_sleep(ts);	//SHTPS_CPU_CLOCK_CONTROL_ENABLE

	shtps_filter_sleep_enter(ts);

	shtps_event_force_touchup(ts);

	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		shtps_irq_disable(ts);
	#else
		shtps_irq_wake_disable(ts);
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

	{
		u8 buf;
		shtps_fwctl_irqclear_get_irqfactor(ts, &buf);
	}

	#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
		ts->lpmode_req_state = SHTPS_LPMODE_REQ_NONE;
	#endif /* #if defined( SHTPS_LOW_POWER_MODE_ENABLE ) */

	#if defined(SHTPS_GLOVE_DETECT_ENABLE)
		if(ts->glove_enable_state != 0){
			shtps_set_glove_detect_disable(ts);
		}
	#endif /* SHTPS_GLOVE_DETECT_ENABLE */

	#if defined(SHTPS_LPWG_MODE_ENABLE)
		shtps_set_lpwg_sleep_check(ts);
	#else
		shtps_sleep(ts, 1);
		shtps_system_set_sleep(ts);
	#endif /* SHTPS_LPWG_MODE_ENABLE */

	shtps_report_stflib_kevt(ts, SHTPS_DEF_STFLIB_KEVT_CLEAR_REQUEST, SHTPS_DEF_STFLIB_KEVT_CLEAR_REQUEST_SLEEP);

	#if defined(SHTPS_POWER_OFF_IN_SLEEP_ENABLE)
		#if defined(SHTPS_LPWG_MODE_ENABLE)
			if (ts->lpwg.lpwg_switch == 0){
				return shtps_statef_cmn_stop(ts, param);
			}
		#else
			return shtps_statef_cmn_stop(ts, param);
		#endif /* SHTPS_LPWG_MODE_ENABLE */
	#endif /* SHTPS_POWER_OFF_IN_SLEEP_ENABLE */

	return 0;
}

static int shtps_statef_sleep_wakeup(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		shtps_irq_enable(ts);
	#else
		shtps_irq_wake_enable(ts);
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

	#if defined(SHTPS_LPWG_MODE_ENABLE)
		shtps_set_lpwg_wakeup_check(ts);
	#else
		shtps_system_set_wakeup(ts);
	#endif /* SHTPS_LPWG_MODE_ENABLE */

	shtps_sleep(ts, 0);

	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		{
			u8 buf;
			shtps_fwctl_irqclear_get_irqfactor(ts, &buf);
		}
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

	#if defined(SHTPS_GLOVE_DETECT_ENABLE)
		if(ts->glove_enable_state != 0){
			shtps_set_glove_detect_enable(ts);
		}
	#endif /* SHTPS_GLOVE_DETECT_ENABLE */

	shtps_rezero_request(ts,
						 SHTPS_REZERO_REQUEST_WAKEUP_REZERO,
						 SHTPS_REZERO_TRIGGER_WAKEUP);
	state_change(ts, SHTPS_STATE_ACTIVE);
	return 0;
}

static int shtps_statef_sleep_starttm(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_system_set_wakeup(ts);
	shtps_sleep(ts, 0);
	#if !defined(SHTPS_ACTIVE_SLEEP_WAIT_ALWAYS_ENABLE)
		if(SHTPS_SLEEP_OUT_WAIT_MS > 0){
			msleep(SHTPS_SLEEP_OUT_WAIT_MS);
		}
	#endif /* !SHTPS_ACTIVE_SLEEP_WAIT_ALWAYS_ENABLE */
	state_change(ts, SHTPS_STATE_FWTESTMODE);
	return 0;
}

static int shtps_statef_sleep_facetouch_on(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
		#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
			shtps_irq_enable(ts);
		#else
			shtps_irq_wake_enable(ts);
		#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */
		
		#if defined(SHTPS_LPWG_MODE_ENABLE)
			shtps_set_lpwg_wakeup_check(ts);
		#else
			shtps_system_set_wakeup(ts);
		#endif /* SHTPS_LPWG_MODE_ENABLE */

		shtps_sleep(ts, 0);

		#if defined( SHTPS_FACETOUCH_OFF_DETECT_DOZE_ENABLE )
			#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
				shtps_set_doze_mode(ts, 1);
			#endif /* SHTPS_LOW_POWER_MODE_ENABLE */
		#endif /* SHTPS_FACETOUCH_OFF_DETECT_DOZE_ENABLE */

		shtps_set_facetouchmode(ts, 1);
#endif /* #if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT ) */
	state_change(ts, SHTPS_STATE_SLEEP_FACETOUCH);
	return 0;
}

static int shtps_statef_sleep_int(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
	#if defined(SHTPS_LPWG_MODE_ENABLE)
		if (ts->lpwg.lpwg_switch){
			shtps_read_touchevent_insleep(ts, SHTPS_STATE_SLEEP);
		}else{
			shtps_read_touchevent(ts, SHTPS_STATE_SLEEP);
		}
	#else
		shtps_read_touchevent(ts, SHTPS_STATE_SLEEP);
	#endif /* SHTPS_LPWG_MODE_ENABLE */

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_statef_sleep_facetouch_enter(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
	ts->facetouch.touch_num = ts->touch_state.numOfFingers;

	#if defined(SHTPS_CPU_CLOCK_CONTROL_ENABLE)
		shtps_perflock_sleep(ts);
	#endif /* SHTPS_CPU_CLOCK_CONTROL_ENABLE */

	shtps_event_all_cancel(ts);

	return 0;
#else
	#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
		ts->lpmode_req_state = SHTPS_LPMODE_REQ_NONE;
	#endif /* #if defined( SHTPS_LOW_POWER_MODE_ENABLE ) */

	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		shtps_irq_disable(ts);
	#else
		shtps_irq_wake_disable(ts);
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

	shtps_sleep(ts, 1);
	shtps_system_set_sleep(ts);
	return 0;
#endif /* #if !defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT ) */
}

static int shtps_statef_sleep_facetouch_stop(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
	shtps_facetouch_wakelock(ts, 0);
	
	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		shtps_irq_disable(ts);
	#else
		shtps_irq_wake_disable(ts);
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */

#endif /* #if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT ) */
	return shtps_statef_cmn_stop(ts, param);
}

static int shtps_statef_sleep_facetouch_wakeup(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
	shtps_facetouch_wakelock(ts, 0);

	#if defined( SHTPS_FACETOUCH_OFF_DETECT_DOZE_ENABLE )
		if(shtps_is_lpmode(ts) == 0){
			#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
				shtps_set_doze_mode(ts, 0);
			#endif /* SHTPS_LOW_POWER_MODE_ENABLE */
		}
	#endif /* SHTPS_FACETOUCH_OFF_DETECT_DOZE_ENABLE */

#else
	#if defined(SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE)
		shtps_irq_enable(ts);
	#else
		shtps_irq_wake_enable(ts);
	#endif /* SHTPS_IRQ_LINKED_WITH_IRQWAKE_ENABLE */
	
	shtps_system_set_wakeup(ts);
	shtps_sleep(ts, 0);
#endif /* #if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT ) */
	shtps_rezero_request(ts, SHTPS_REZERO_REQUEST_WAKEUP_REZERO, 0);
	state_change(ts, SHTPS_STATE_FACETOUCH);
	return 0;
}

static int shtps_statef_sleep_facetouch_starttm(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
	shtps_facetouch_wakelock(ts, 0);
	shtps_sleep(ts, 0);
#else
	shtps_system_set_wakeup(ts);
	shtps_sleep(ts, 0);
#endif /* #if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT ) */
	state_change(ts, SHTPS_STATE_FWTESTMODE);
	return 0;
}

static int shtps_statef_sleep_facetouch_facetouch_off(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
	shtps_facetouch_wakelock(ts, 0);
	shtps_set_facetouchmode(ts, 0);
#endif /* #if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT ) */
//	shtps_set_facetouchmode(ts, 0);
	state_change(ts, SHTPS_STATE_SLEEP);
	return 0;
}

static int shtps_statef_sleep_facetouch_int(struct shtps_rmi_spi *ts, int param)
{
	SHTPS_LOG_FUNC_CALL();
#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
	shtps_read_touchevent_infacetouchmode(ts);
#else
	shtps_read_touchevent(ts, SHTPS_STATE_SLEEP);
#endif /* #if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT ) */

	return 0;
}

/* -----------------------------------------------------------------------------------
 */
static int shtps_rmi_open(struct input_dev *dev)
{
	struct shtps_rmi_spi *ts = (struct shtps_rmi_spi*)input_get_drvdata(dev);

	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__OPEN, "%ld", shtps_sys_getpid());
	SHTPS_LOG_DBG_PRINT("[shtps]Open(PID:%ld)\n", shtps_sys_getpid());

	shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_OPEN);

	return 0;
}

static void shtps_rmi_close(struct input_dev *dev)
{
	struct shtps_rmi_spi *ts = (struct shtps_rmi_spi*)input_get_drvdata(dev);

	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__CLOSE, "%ld", shtps_sys_getpid());
	SHTPS_LOG_DBG_PRINT("[shtps]Close(PID:%ld)\n", shtps_sys_getpid());

	shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_CLOSE);
}

static int shtps_init_internal_variables(struct shtps_rmi_spi *ts)
{
	int result = 0;
	
	SHTPS_LOG_FUNC_CALL();

	INIT_DELAYED_WORK(&ts->tmo_check, shtps_work_tmof);

	init_waitqueue_head(&ts->wait_start);
	init_waitqueue_head(&ts->loader.wait_ack);
	init_waitqueue_head(&ts->diag.wait);
	init_waitqueue_head(&ts->diag.tm_wait_ack);

	hrtimer_init(&ts->rezero_delayed_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ts->rezero_delayed_timer.function = shtps_delayed_rezero_timer_function;
	INIT_WORK(&ts->rezero_delayed_work, shtps_rezero_delayed_work_function);

	result = shtps_func_async_init(ts);
	if(result < 0){
		goto fail_init;
	}

	shtps_set_startup_min_time(ts, SHTPS_STARTUP_MIN_TIME);

	ts->state_mgr.state = SHTPS_STATE_IDLE;
	ts->loader.ack = 0;
	ts->diag.event = 0;
	ts->offset.enabled = 0;

	memset(&ts->poll_info,   0, sizeof(ts->poll_info));
	memset(&ts->fw_report_info, 0, sizeof(ts->fw_report_info));
	memset(&ts->fw_report_info_store, 0, sizeof(ts->fw_report_info_store));
	memset(&ts->report_info, 0, sizeof(ts->report_info));
	memset(&ts->touch_state, 0, sizeof(ts->touch_state));

	shtps_perflock_init(ts);	//SHTPS_CPU_CLOCK_CONTROL_ENABLE

	shtps_cpu_idle_sleep_wake_lock_init(ts);	//SHTPS_CPU_IDLE_SLEEP_CONTROL_ENABLE
	shtps_fwupdate_wake_lock_init(ts);			//SHTPS_CPU_SLEEP_CONTROL_FOR_FWUPDATE_ENABLE
	shtps_record_log_file_init(ts);				//SHTPS_DEF_RECORD_LOG_FILE_ENABLE

	shtps_fwctl_set_dev_state(ts, SHTPS_DEV_STATE_SLEEP);
	shtps_performance_check_init();

	#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
		shtps_facetouch_init(ts);
	#endif /* #if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT ) */

	#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
		ts->lpmode_req_state = SHTPS_LPMODE_REQ_NONE;
		ts->lpmode_continuous_req_state = SHTPS_LPMODE_REQ_NONE;
	#endif /*` #if defined( SHTPS_LOW_POWER_MODE_ENABLE ) */

	#if defined(SHTPS_LPWG_MODE_ENABLE)
		shtps_lpwg_wakelock_init(ts);
	#endif /* SHTPS_LPWG_MODE_ENABLE */

	#if defined(SHTPS_SYSTEM_BOOT_MODE_CHECK_ENABLE)
		ts->system_boot_mode = sh_boot_get_bootmode();
	#endif /* SHTPS_SYSTEM_BOOT_MODE_CHECK_ENABLE */

	#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
	    ts->deter_suspend_spi.wake_lock_state = 0;
		memset(&ts->deter_suspend_spi, 0, sizeof(ts->deter_suspend_spi));
	    wake_lock_init(&ts->deter_suspend_spi.wake_lock, WAKE_LOCK_SUSPEND, "shtps_resume_wake_lock");
		ts->deter_suspend_spi.pm_qos_lock_idle.type = PM_QOS_REQ_ALL_CORES;
		pm_qos_add_request(&ts->deter_suspend_spi.pm_qos_lock_idle, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
		INIT_WORK(&ts->deter_suspend_spi.pending_proc_work, shtps_deter_suspend_spi_pending_proc_work_function);
		#ifdef SHTPS_DEVELOP_MODE_ENABLE
			INIT_DELAYED_WORK(&ts->deter_suspend_spi.pending_proc_work_delay, shtps_deter_suspend_spi_pending_proc_delayed_work_function);
		#endif /* SHTPS_DEVELOP_MODE_ENABLE */
	#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */

	#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
		ts->wakeup_touch_event_inhibit_state = 0;
	#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */

	#if defined(SHTPS_PROXIMITY_SUPPORT_ENABLE)
		wake_lock_init(&ts->wake_lock_proximity, WAKE_LOCK_SUSPEND, "shtps_wake_lock_proximity");
		ts->pm_qos_lock_idle_proximity.type = PM_QOS_REQ_ALL_CORES;
		pm_qos_add_request(&ts->pm_qos_lock_idle_proximity, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);
		INIT_DELAYED_WORK(&ts->proximity_check_delayed_work, shtps_lpwg_proximity_check_delayed_work_function);
		ts->proximity_input_index = -1;
	#endif /* SHTPS_PROXIMITY_SUPPORT_ENABLE */

	/* -------------------------------------------------------------------------- */
	shtps_filter_init(ts);

	/* -------------------------------------------------------------------------- */
	return 0;

fail_init:
	shtps_func_async_deinit(ts);

	return result;
}


static void shtps_deinit_internal_variables(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	if(ts){
		hrtimer_cancel(&ts->rezero_delayed_timer);

		shtps_filter_deinit(ts);

		shtps_perflock_deinit(ts);					//SHTPS_CPU_CLOCK_CONTROL_ENABLE
		shtps_func_async_deinit(ts);
		shtps_cpu_idle_sleep_wake_lock_deinit(ts);	//SHTPS_CPU_IDLE_SLEEP_CONTROL_ENABLE
		shtps_fwupdate_wake_lock_deinit(ts);		//SHTPS_CPU_SLEEP_CONTROL_FOR_FWUPDATE_ENABLE
		shtps_record_log_file_deinit(ts);			//SHTPS_DEF_RECORD_LOG_FILE_ENABLE

		#if defined(SHTPS_LPWG_MODE_ENABLE)
			shtps_lpwg_wakelock_destroy(ts);
		#endif /* SHTPS_LPWG_MODE_ENABLE */

		#if defined(SHTPS_PROXIMITY_SUPPORT_ENABLE)
			wake_lock_destroy(&ts->wake_lock_proximity);
		#endif /* SHTPS_PROXIMITY_SUPPORT_ENABLE */
	}
}

static int shtps_init_inputdev(struct shtps_rmi_spi *ts, struct device *ctrl_dev_p, char *modalias)
{
	SHTPS_LOG_FUNC_CALL();
	ts->input = input_allocate_device();
	if (!ts->input){
		SHTPS_LOG_ERR_PRINT("Failed input_allocate_device\n");
		return -ENOMEM;
	}

	ts->input->name 		= modalias;
	ts->input->phys			= ts->phys;
	ts->input->id.vendor	= 0x0001;
	ts->input->id.product	= 0x0002;
	ts->input->id.version	= 0x0100;
	ts->input->dev.parent	= ctrl_dev_p;
	ts->input->open			= shtps_rmi_open;
	ts->input->close		= shtps_rmi_close;

	/** set properties */
	__set_bit(EV_ABS, ts->input->evbit);
	__set_bit(INPUT_PROP_DIRECT, ts->input->propbit);

	input_set_drvdata(ts->input, ts);
	input_mt_init_slots(ts->input, SHTPS_FINGER_MAX, 0);

	if(ts->input->mt == NULL){
		input_free_device(ts->input);
		return -ENOMEM;
	}

	/** set parameters */
	input_set_abs_params(ts->input, ABS_MT_TOUCH_MAJOR, 0, SHTPS_VAL_FINGER_WIDTH_MAXSIZE, 0, 0);
	#if defined(SHTPS_NOTIFY_TOUCH_MINOR_ENABLE)
		input_set_abs_params(ts->input, ABS_MT_TOUCH_MINOR, 0, SHTPS_VAL_FINGER_WIDTH_MAXSIZE, 0, 0);
		input_set_abs_params(ts->input, ABS_MT_ORIENTATION, 0, 1, 0, 0);
	#endif /* SHTPS_NOTIFY_TOUCH_MINOR_ENABLE */
	input_set_abs_params(ts->input, ABS_MT_POSITION_X,  0, CONFIG_SHTPS_SY3000_LCD_SIZE_X - 1, 0, 0);
	input_set_abs_params(ts->input, ABS_MT_POSITION_Y,  0, CONFIG_SHTPS_SY3000_LCD_SIZE_Y - 1, 0, 0);

	#if defined(SHTPS_GLOVE_DETECT_ENABLE)
		input_set_abs_params(ts->input, ABS_MT_PRESSURE,    0, 511, 0, 0);
	#else
		input_set_abs_params(ts->input, ABS_MT_PRESSURE,    0, 255, 0, 0);
	#endif /* SHTPS_GLOVE_DETECT_ENABLE */

	input_set_abs_params(ts->input, ABS_MISC, 0, SHTPS_DEF_STFLIB_KEVT_SIZE_MAX, 0, 0);

	/** register input device */
	if(input_register_device(ts->input) != 0){
		input_free_device(ts->input);
		return -EFAULT;
	}

	return 0;
}

static void shtps_deinit_inputdev(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	if(ts && ts->input){
		if(ts->input->mt){
			input_mt_destroy_slots(ts->input);
		}
		input_free_device(ts->input);
	}
}

#if defined(SHTPS_LPWG_MODE_ENABLE)
static int shtps_init_inputdev_key(struct shtps_rmi_spi *ts, struct device *ctrl_dev_p)
{
	SHTPS_LOG_FUNC_CALL();
	ts->input_key = input_allocate_device();
	if(!ts->input_key){
		return -ENOMEM;
	}

	ts->input_key->name 		= "shtps_key";
	ts->input_key->phys         = ts->phys;
	ts->input_key->id.vendor	= 0x0000;
	ts->input_key->id.product	= 0x0000;
	ts->input_key->id.version	= 0x0000;
	ts->input_key->dev.parent	= ctrl_dev_p;

	__set_bit(EV_KEY, ts->input_key->evbit);

	input_set_drvdata(ts->input_key, ts);

	#if defined(SHTPS_LPWG_MODE_ENABLE)
		#if defined(SHTPS_LPWG_SWEEP_ON_ENABLE)
			__set_bit(KEY_SWEEPON, ts->input_key->keybit);
		#endif /* SHTPS_LPWG_SWEEP_ON_ENABLE */
		__set_bit(KEY_DOUBLETAP, ts->input_key->keybit);
	#endif /*  SHTPS_LPWG_MODE_ENABLE */
	
	__clear_bit(KEY_RESERVED, ts->input_key->keybit);

	if(input_register_device(ts->input_key)){
		input_free_device(ts->input_key);
		return -EFAULT;
	}
	
	return 0;
}

static void shtps_deinit_inputdev_key(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	if(ts && ts->input_key){
		input_free_device(ts->input_key);
	}
}
#endif /* SHTPS_LPWG_MODE_ENABLE */

#ifdef CONFIG_DRM_MSM
int shtps_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct msm_drm_notifier *evdata = data;
	int blank;
	struct shtps_rmi_spi *ts = gShtps_rmi_spi;

	SHTPS_LOG_FUNC_CALL_INPARAM((int)event);

	if(ts == NULL){
		return 0;
	}

	if (evdata && evdata->data) {
		if(evdata->id == MSM_DRM_PRIMARY_DISPLAY){
			#if defined(SHTPS_ALWAYS_ON_DISPLAY_ENABLE)
				if (event == MSM_DRM_EVENT_DPMS) {
					blank = *(int *)evdata->data;

					if(blank == MSM_DRM_EVENT_DPMS_OFF){
						#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
							if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_SPI_PROC_SETSLEEP, 1) == 0){
								shtps_setsleep_proc(ts, 1);
							}
						#else
							shtps_func_request_sync(ts, SHTPS_FUNC_REQ_EVENT_LCD_OFF);
						#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */
					}
					else if(blank == MSM_DRM_EVENT_DPMS_LP1){
						ts->lpwg.lpwg_double_tap_aod_req_state = 1;

						#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
							if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_SPI_PROC_SETSLEEP, 1) == 0){
								shtps_setsleep_proc(ts, 0);
								shtps_wait_startup_timeout(ts, 1000);
								shtps_setsleep_proc(ts, 1);
							}
						#else
							shtps_func_request_sync(ts, SHTPS_FUNC_REQ_EVENT_LCD_ON);
							shtps_wait_startup_timeout(ts, 1000);
							shtps_func_request_sync(ts, SHTPS_FUNC_REQ_EVENT_LCD_OFF);
						#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */
					}
					else if(blank == MSM_DRM_EVENT_DPMS_ON){
						ts->lpwg.lpwg_double_tap_aod_req_state = 0;

						#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
							if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_SPI_PROC_SETSLEEP, 0) == 0){
								shtps_setsleep_proc(ts, 0);
							}
						#else
							shtps_func_request_sync(ts, SHTPS_FUNC_REQ_EVENT_LCD_ON);
						#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */
					}
				}
			#else
				if ((event == MSM_DRM_EVENT_BLANK) || (event == MSM_DRM_EARLY_EVENT_BLANK)) {
					blank = *(int *)evdata->data;

					if((event == MSM_DRM_EARLY_EVENT_BLANK) && (blank == MSM_DRM_BLANK_POWERDOWN)){
						#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
							if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_SPI_PROC_SETSLEEP, 1) == 0){
								shtps_setsleep_proc(ts, 1);
							}
						#else
							shtps_func_request_sync(ts, SHTPS_FUNC_REQ_EVENT_LCD_OFF);
						#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */
					}
					else if((event == MSM_DRM_EVENT_BLANK) && (blank == MSM_DRM_BLANK_UNBLANK)){
						#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
							if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_SPI_PROC_SETSLEEP, 0) == 0){
								shtps_setsleep_proc(ts, 0);
							}
						#else
							shtps_func_request_sync(ts, SHTPS_FUNC_REQ_EVENT_LCD_ON);
						#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */
					}
				}
			#endif /* SHTPS_ALWAYS_ON_DISPLAY_ENABLE */
		}
	}
	return 0;
}
#else
#ifdef CONFIG_FB
int shtps_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;
	struct shtps_rmi_spi *ts = gShtps_rmi_spi;

	SHTPS_LOG_FUNC_CALL();

	if(ts == NULL){
		return 0;
	}

	if (evdata && evdata->info && evdata->data) {
		if(evdata->info->node == 0){
			if ((event == FB_EVENT_BLANK) || (event == FB_EARLY_EVENT_BLANK)) {
				blank = *(int *)evdata->data;

				if((event == FB_EARLY_EVENT_BLANK) && (blank == FB_BLANK_POWERDOWN)){
					#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
						if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_SPI_PROC_SETSLEEP, 1) == 0){
							shtps_setsleep_proc(ts, 1);
						}
					#else
						shtps_func_request_sync(ts, SHTPS_FUNC_REQ_EVENT_LCD_OFF);
					#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */
				}
				else if((event == FB_EVENT_BLANK) && (blank == FB_BLANK_UNBLANK)){
					#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
						if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_SPI_PROC_SETSLEEP, 0) == 0){
							shtps_setsleep_proc(ts, 0);
						}
					#else
						shtps_func_request_sync(ts, SHTPS_FUNC_REQ_EVENT_LCD_ON);
					#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */
				}
			}
		}
	}
	return 0;
}
#endif
#endif

int shtps_rmi_core_probe(
						struct device *ctrl_dev_p,
						struct shtps_ctrl_functbl *func_p,
						void *ctrl_p,
						char *modalias,
						int gpio_irq)
{
	extern void shtps_init_io_debugfs(struct shtps_rmi_spi *ts);
	extern void shtps_init_debugfs(struct shtps_rmi_spi *ts);

	int result = 0;
	struct shtps_rmi_spi *ts;
	#ifdef CONFIG_OF
		; // nothing
	#else
		struct shtps_platform_data *pdata = ctrl_dev_p->platform_data;
	#endif /* CONFIG_OF */

	SHTPS_LOG_FUNC_CALL();
	shtps_mutex_lock_ctrl();

	ts = kzalloc(sizeof(struct shtps_rmi_spi), GFP_KERNEL);
	if(!ts){
		SHTPS_LOG_ERR_PRINT("memory allocation error\n");
		result = -ENOMEM;
		shtps_mutex_unlock_ctrl();
		goto fail_alloc_mem;
	}

	result = shtps_fwctl_init(ts, ctrl_p, func_p);
	if(result){
		SHTPS_LOG_ERR_PRINT("no ctrl tpsdevice:error\n");
		result = -EFAULT;
		shtps_mutex_unlock_ctrl();
		goto fail_alloc_mem;
	}

	if(shtps_init_internal_variables(ts)){
		shtps_mutex_unlock_ctrl();
		goto fail_init_internal_variables;
	}

	/** set device info */
	dev_set_drvdata(ctrl_dev_p, ts);
	gShtps_rmi_spi = ts;
	ts->ctrl_dev_p = ctrl_dev_p;


	#ifdef CONFIG_OF
		ts->irq_mgr.irq	= irq_of_parse_and_map(ctrl_dev_p->of_node, 0);
		ts->rst_pin		= of_get_named_gpio(ctrl_dev_p->of_node, "shtps_rmi,rst_pin", 0);
		ts->tpin		= of_get_named_gpio(ctrl_dev_p->of_node, "shtps_rmi,test_mode_gpio", 0);
		if(!gpio_is_valid(ts->tpin)){
			ts->tpin = 0;
		}
		else{
			ts->pinctrl = devm_pinctrl_get(ctrl_dev_p);
			if(IS_ERR_OR_NULL(ts->pinctrl)){
				ts->tpin = 0;
			}
			else{
				ts->tpin_state_active = pinctrl_lookup_state(ts->pinctrl, "test_mode_pull_up");
				if(IS_ERR_OR_NULL(ts->tpin_state_active)){
					ts->tpin = 0;
				}

				ts->tpin_state_suspend = pinctrl_lookup_state(ts->pinctrl, "test_mode_pull_down");
				if(IS_ERR_OR_NULL(ts->tpin_state_active)){
					ts->tpin = 0;
				}
			}
		}
	#else
		ts->rst_pin		= pdata->gpio_rst;
		ts->irq_mgr.irq	= gpio_irq;
		ts->tpin		= 0;
	#endif /* CONFIG_OF */

    if(!gpio_is_valid(ts->rst_pin)){
		SHTPS_LOG_ERR_PRINT("gpio resource error\n");
		result = -EFAULT;
		shtps_mutex_unlock_ctrl();
		goto fail_get_dtsinfo;
	}

	snprintf(ts->phys, sizeof(ts->phys), "%s", dev_name(ctrl_dev_p));

	/** setup device */
	#ifdef CONFIG_OF
		result = shtps_device_setup(ctrl_dev_p, ts->rst_pin);
		if(result){
			SHTPS_LOG_ERR_PRINT("Failed shtps_device_setup\n");
			shtps_mutex_unlock_ctrl();
			goto fail_device_setup;
		}
	#else
		if (pdata && pdata->setup) {
			result = pdata->setup(ctrl_dev_p);
			if (result){
				shtps_mutex_unlock_ctrl();
				goto fail_alloc_mem;
			}
		}
	#endif /* CONFIG_OF */

	#if defined(SHTPS_POWER_OFF_IN_SLEEP_ENABLE)
	{
		struct shtps_power_on_off_info power_on_off_info;

		power_on_off_info.dev				= ctrl_dev_p;
		power_on_off_info.rst_pin			= ts->rst_pin;
		power_on_off_info.tp_i2c_sda		= of_get_named_gpio(ctrl_dev_p->of_node, "shtps_rmi,tp_i2c_sda", 0);
		power_on_off_info.tp_i2c_scl		= of_get_named_gpio(ctrl_dev_p->of_node, "shtps_rmi,tp_i2c_scl", 0);
		power_on_off_info.pinctrl			= ts->pinctrl;
		power_on_off_info.panel_active		= pinctrl_lookup_state(ts->pinctrl, "panel_active");
		power_on_off_info.panel_standby		= pinctrl_lookup_state(ts->pinctrl, "panel_standby");
		power_on_off_info.tp_int			= of_get_named_gpio(ctrl_dev_p->of_node, "shtps_rmi,tp_int", 0);
		power_on_off_info.int_active		= pinctrl_lookup_state(ts->pinctrl, "int_active");
		power_on_off_info.int_standby		= pinctrl_lookup_state(ts->pinctrl, "int_standby");

		shtps_device_power_on_off_init(&power_on_off_info);
	}
	#else
		#if defined(SHTPS_DYNAMIC_RESET_ESD_ENABLE)
		{
			struct shtps_esd_regulator_info esd_regulator_info;

			esd_regulator_info.dev				= ctrl_dev_p;
			esd_regulator_info.rst_pin			= ts->rst_pin;
			esd_regulator_info.tp_spi_mosi		= of_get_named_gpio(ctrl_dev_p->of_node, "shtps_rmi,tp_spi_mosi", 0);
			esd_regulator_info.tp_spi_miso		= of_get_named_gpio(ctrl_dev_p->of_node, "shtps_rmi,tp_spi_miso", 0);
			esd_regulator_info.tp_spi_cs_n		= of_get_named_gpio(ctrl_dev_p->of_node, "shtps_rmi,tp_spi_cs_n", 0);
			esd_regulator_info.tp_spi_clk		= of_get_named_gpio(ctrl_dev_p->of_node, "shtps_rmi,tp_spi_clk", 0);
			esd_regulator_info.pinctrl			= ts->pinctrl;
			esd_regulator_info.esd_spi			= pinctrl_lookup_state(ts->pinctrl, "esd_spi");
			esd_regulator_info.esd_gpio			= pinctrl_lookup_state(ts->pinctrl, "esd_gpio");
			esd_regulator_info.tp_int			= of_get_named_gpio(ctrl_dev_p->of_node, "shtps_rmi,tp_int", 0);
			esd_regulator_info.int_active		= pinctrl_lookup_state(ts->pinctrl, "int_active");
			esd_regulator_info.int_standby		= pinctrl_lookup_state(ts->pinctrl, "int_standby");

			shtps_esd_regulator_init(&esd_regulator_info);
		}
		#endif /* SHTPS_DYNAMIC_RESET_ESD_ENABLE */
	#endif /* SHTPS_POWER_OFF_IN_SLEEP_ENABLE */

	if(shtps_irq_resuest(ts)){
		result = -EFAULT;
		SHTPS_LOG_ERR_PRINT("shtps:request_irq error\n");
		shtps_mutex_unlock_ctrl();
		goto fail_irq_request;
	}

	shtps_mutex_unlock_ctrl();

	#ifdef CONFIG_OF
		#if defined( SHTPS_CHECK_LCD_RESOLUTION_ENABLE )
			if(IS_ERR_OR_NULL(ts->pinctrl) == 0){
				ts->lcd_det_gpio = of_get_named_gpio(ctrl_dev_p->of_node, "shtps_rmi,lcd_det_gpio", 0);
				ts->lcd_det_state_active = pinctrl_lookup_state(ts->pinctrl, "lcd_det_pull_up");
				ts->lcd_det_state_suspend = pinctrl_lookup_state(ts->pinctrl, "lcd_det_pull_down");

				if(shtps_get_lcd_resolution_kind(ts) == SHTPS_LCD_RESOLUTION_KIND_FHD){
					gShtps_panel_size_x = CONFIG_SHTPS_SY3000_LCD_SIZE_FHD_X;
					gShtps_panel_size_y = CONFIG_SHTPS_SY3000_LCD_SIZE_FHD_Y;
				}
			}
		#endif /* SHTPS_CHECK_LCD_RESOLUTION_ENABLE */

		#if defined(SHTPS_DEF_COVER_CHANGE_FINGER_AMP_THRESH_ENABLE)
			if(IS_ERR_OR_NULL(ts->pinctrl) == 0){
				if(of_property_read_bool(ctrl_dev_p->of_node, "shtps_rmi,cover_change_finger_amp_thresh_for_film") == true){
					ts->cover_change_finger_amp_thresh_for_film = 1;
				}
				else{
					ts->cover_change_finger_amp_thresh_for_film = 0;
				}
			}
		#endif /* SHTPS_DEF_COVER_CHANGE_FINGER_AMP_THRESH_ENABLE */
	#endif /* CONFIG_OF */

	/** init device info */
	result = shtps_init_inputdev(ts, ctrl_dev_p, modalias);
	if(result != 0){
		SHTPS_LOG_DBG_PRINT("Failed init input device\n");
		goto fail_init_inputdev;
	}
	
	#if defined(SHTPS_LPWG_MODE_ENABLE)
		result = shtps_init_inputdev_key(ts, ctrl_dev_p);
		if(result != 0){
			SHTPS_LOG_DBG_PRINT("Failed init input key-device\n");
			goto fail_init_inputdev_key;
		}
	#endif /* defined(SHTPS_LPWG_MODE_ENABLE) */

	/** init sysfs I/F */
	shtps_init_io_debugfs(ts);

	/** init debug fs */
	shtps_init_debugfs(ts);

	#if defined( SHTPS_DEVELOP_MODE_ENABLE )
	{
		struct shtps_debug_init_param param;
		param.shtps_root_kobj = ts->kobj;

		/** init debug function data */
		shtps_debug_init(&param);
	}
	#endif /* #if defined( SHTPS_DEVELOP_MODE_ENABLE ) */

	#if defined(SHTPS_CPU_CLOCK_CONTROL_ENABLE)
		shtps_perflock_register_notifier();
	#endif /*  defined(SHTPS_CPU_CLOCK_CONTROL_ENABLE) */

	#ifdef CONFIG_DRM_MSM
		ts->drm_notif.notifier_call = shtps_drm_notifier_callback;
		msm_drm_register_client(&ts->drm_notif);
	#else
		#ifdef CONFIG_FB
			ts->fb_notif.notifier_call = shtps_fb_notifier_callback;
			fb_register_client(&ts->fb_notif);
		#endif
	#endif

	#if defined(SHTPS_COVER_ENABLE)
		result = register_shflip_notifier(&cover_status_handler);
		if (result < 0) {
			SHTPS_LOG_ERR_PRINT("shtps:register_shflip_notifier error\n");
			goto fail_register_flip_notifier;
		}
	#endif /* SHTPS_COVER_ENABLE */

	result = shtpsif_init();
	if(result != 0){
		SHTPS_LOG_DBG_PRINT("Failed init shtpsif\n");
		goto fail_init_shtpsif;
	}

	SHTPS_LOG_DBG_PRINT("shtps_rmi_probe() done\n");
	return 0;

fail_init_shtpsif:
	#if defined(SHTPS_COVER_ENABLE)
		unregister_shflip_notifier(&cover_status_handler);
	#endif /* SHTPS_COVER_ENABLE */

#if defined(SHTPS_COVER_ENABLE)
fail_register_flip_notifier:
#endif /* SHTPS_COVER_ENABLE */

	#ifdef CONFIG_DRM_MSM
		msm_drm_unregister_client(&ts->drm_notif);
	#else
		#ifdef CONFIG_FB
			fb_unregister_client(&ts->fb_notif);
		#endif
	#endif

	#if defined(SHTPS_CPU_CLOCK_CONTROL_ENABLE)
		shtps_perflock_unregister_notifier();
	#endif /* defined(SHTPS_CPU_CLOCK_CONTROL_ENABLE) */

#if defined(SHTPS_LPWG_MODE_ENABLE)
fail_init_inputdev_key:
	input_free_device(ts->input);
#endif /* defined(SHTPS_LPWG_MODE_ENABLE) */

fail_init_inputdev:
fail_irq_request:
fail_get_dtsinfo:
	shtps_deinit_internal_variables(ts);

fail_init_internal_variables:
fail_device_setup:
	kfree(ts);
	
fail_alloc_mem:
	return result;
}

int shtps_rmi_core_remove(struct shtps_rmi_spi *ts, struct device *ctrl_dev_p)
{
	extern void shtps_deinit_io_debugfs(struct shtps_rmi_spi *ts);
	extern void shtps_deinit_debugfs(struct shtps_rmi_spi *ts);

	#ifndef CONFIG_OF
		struct shtps_platform_data *pdata = (struct shtps_platform_data *)&ctrl_dev_p->platform_data;
	#endif /* !CONFIG_OF */

	SHTPS_LOG_FUNC_CALL();

	shtpsif_exit();

	gShtps_rmi_spi = NULL;

	if(ts){
		#if defined(SHTPS_COVER_ENABLE)
			unregister_shflip_notifier(&cover_status_handler);
		#endif /* SHTPS_COVER_ENABLE */

		#ifdef CONFIG_DRM_MSM
			msm_drm_unregister_client(&ts->drm_notif);
		#else
			#ifdef CONFIG_FB
				fb_unregister_client(&ts->fb_notif);
			#endif
		#endif

		#if defined(SHTPS_CPU_CLOCK_CONTROL_ENABLE)
			shtps_perflock_unregister_notifier();
		#endif /* defined(SHTPS_CPU_CLOCK_CONTROL_ENABLE) */

		free_irq(ts->irq_mgr.irq, ts);

		#ifdef CONFIG_OF
			shtps_device_teardown(ctrl_dev_p, ts->rst_pin);
		#else
			if (pdata && pdata->teardown){
				pdata->teardown(&ctrl_dev_p);
			}
		#endif /* CONFIG_OF */

		shtps_deinit_internal_variables(ts);
		#if defined( SHTPS_DEVELOP_MODE_ENABLE )
			shtps_debug_deinit();
		#endif /* #if defined( SHTPS_DEVELOP_MODE_ENABLE ) */
		shtps_deinit_debugfs(ts);
		shtps_deinit_io_debugfs(ts);

		shtps_deinit_inputdev(ts);
		#if defined(SHTPS_LPWG_MODE_ENABLE)
			shtps_deinit_inputdev_key(ts);
		#endif /* SHTPS_LPWG_MODE_ENABLE */

		shtps_fwctl_deinit(ts);

		#if defined(SHTPS_DYNAMIC_RESET_ESD_ENABLE)
			shtps_esd_regulator_get();
			shtps_esd_regulator_remove();
		#endif /* SHTPS_DYNAMIC_RESET_ESD_ENABLE */

		kfree(ts);
	}

	_log_msg_sync( LOGMSG_ID__REMOVE_DONE, "");
	SHTPS_LOG_DBG_PRINT("shtps_rmi_remove() done\n");
	return 0;
}

int shtps_rmi_core_suspend(struct shtps_rmi_spi *ts)
{
	#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
		shtps_set_suspend_state(ts);
	#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */

	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__SUSPEND, "");

	return 0;
}

int shtps_rmi_core_resume(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	_log_msg_sync( LOGMSG_ID__RESUME, "");

	#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
		shtps_clr_suspend_state(ts);
	#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */

	return 0;
}


/* -----------------------------------------------------------------------------------
 */
void msm_tps_setsleep(int on)
{
	SHTPS_LOG_FUNC_CALL_INPARAM(on);
}
EXPORT_SYMBOL(msm_tps_setsleep);

int msm_tps_set_veilview_state_on(void)
{
	struct shtps_rmi_spi *ts = gShtps_rmi_spi;
	int rc;

	SHTPS_LOG_FUNC_CALL();

	rc = shtps_set_veilview_state(ts, 1);
	return rc;
}
EXPORT_SYMBOL(msm_tps_set_veilview_state_on);

int msm_tps_set_veilview_state_off(void)
{
	struct shtps_rmi_spi *ts = gShtps_rmi_spi;
	int rc;

	SHTPS_LOG_FUNC_CALL();

	rc = shtps_set_veilview_state(ts, 0);
	return rc;
}
EXPORT_SYMBOL(msm_tps_set_veilview_state_off);

int msm_tps_get_veilview_pattern(void)
{
	struct shtps_rmi_spi *ts = gShtps_rmi_spi;
	int rc;

	SHTPS_LOG_FUNC_CALL();

	rc = shtps_get_veilview_pattern(ts);
	return rc;
}
EXPORT_SYMBOL(msm_tps_get_veilview_pattern);

void msm_tps_set_grip_state(int on)
{
#if defined(SHTPS_LPWG_MODE_ENABLE) && defined(SHTPS_LPWG_GRIP_SUPPORT_ENABLE)
	struct shtps_rmi_spi *ts = gShtps_rmi_spi;
	u8 request = (on == 0)? 0 : 1;

	SHTPS_LOG_FUNC_CALL_INPARAM(on);

	if(ts == NULL){
		return;
	}

	#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
		if(shtps_check_suspend_state(ts, SHTPS_DETER_SUSPEND_SPI_PROC_GRIP, request) == 0){
			#if defined(SHTPS_DEF_LPWG_GRIP_PROC_ASYNC_ENABLE)
				if(request == 0){
					shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_GRIP_OFF);
				}else{
					shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_GRIP_ON);
				}
			#else
				shtps_lpwg_grip_set_state(ts, request);
			#endif /* SHTPS_DEF_LPWG_GRIP_PROC_ASYNC_ENABLE */
		}
	#else
		#if defined(SHTPS_DEF_LPWG_GRIP_PROC_ASYNC_ENABLE)
			if(request == 0){
				shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_GRIP_OFF);
			}else{
				shtps_func_request_async(ts, SHTPS_FUNC_REQ_EVENT_GRIP_ON);
			}
		#else
			shtps_lpwg_grip_set_state(ts, request);
		#endif /* SHTPS_DEF_LPWG_GRIP_PROC_ASYNC_ENABLE */
	#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */
#endif /* SHTPS_LPWG_MODE_ENABLE && SHTPS_LPWG_GRIP_SUPPORT_ENABLE */
}
EXPORT_SYMBOL(msm_tps_set_grip_state);

#if defined(SHTPS_COVER_ENABLE)
static int shtps_cover_status_handler_func(struct notifier_block *nb,
					    unsigned long on, void *unused)
{
	struct shtps_rmi_spi *ts = gShtps_rmi_spi;
	shtps_cover_notifier_callback(ts, on);
	return 0;
}
#endif /* SHTPS_COVER_ENABLE */

int shtps_api_register_notifier_touchevent(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&shtps_notifier_list_touchevent, nb);
}
EXPORT_SYMBOL(shtps_api_register_notifier_touchevent);

int shtps_api_unregister_notifier_touchevent(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&shtps_notifier_list_touchevent, nb);
}
EXPORT_SYMBOL(shtps_api_unregister_notifier_touchevent);

int shtps_get_logflag(void)
{
	#if defined( SHTPS_DEVELOP_MODE_ENABLE )
		return gLogOutputEnable;
	#else
		return 0;
	#endif /* #if defined( SHTPS_DEVELOP_MODE_ENABLE ) */
}

long shtps_sys_getpid(void)
{
	return current->pid;
}

#if defined( SHTPS_DEVELOP_MODE_ENABLE )
int shtps_read_touchevent_from_outside(void)
{
	struct shtps_rmi_spi *ts = gShtps_rmi_spi;
	int ret;

	shtps_wake_lock_idle(ts);	//SHTPS_CPU_IDLE_SLEEP_CONTROL_ENABLE
	shtps_mutex_lock_ctrl();

	if(ts->state_mgr.state == SHTPS_STATE_ACTIVE){
		shtps_read_touchevent(ts, SHTPS_STATE_ACTIVE);
		ret = 0;
	}
	else{
		ret = -1;
	}

	shtps_mutex_unlock_ctrl();
	shtps_wake_unlock_idle(ts);	//SHTPS_CPU_IDLE_SLEEP_CONTROL_ENABLE

	return ret;
}

int shtps_update_inputdev(struct shtps_rmi_spi *ts, int x, int y)
{

	struct device *ctrl_dev_p;
	char *modalias;

	SHTPS_LOG_FUNC_CALL();

	if(ts == NULL || ts->input == NULL){
		SHTPS_LOG_ERR_PRINT("ts or ts->input is NULL!\n");
		return -1;
	}

	ctrl_dev_p = ts->input->dev.parent;
	modalias = (char *)ts->input->name;

	if(x > 0 && y > 0){
		CONFIG_SHTPS_SY3000_LCD_SIZE_X = x;
		CONFIG_SHTPS_SY3000_LCD_SIZE_Y = y;
	}
	else{
		shtps_map_construct(ts, 0);
		CONFIG_SHTPS_SY3000_LCD_SIZE_X = SHTPS_GET_PANEL_SIZE_X(ts);
		CONFIG_SHTPS_SY3000_LCD_SIZE_Y = SHTPS_GET_PANEL_SIZE_Y(ts);
	}

	/* free old inputdev */
	input_unregister_device(ts->input);

	/* create new inputdev */
	shtps_init_inputdev(ts, ctrl_dev_p, modalias);

	/* wait open end */
	msleep(1000);

	shtps_fwctl_update_max_position(ts, CONFIG_SHTPS_SY3000_LCD_SIZE_X - 1, CONFIG_SHTPS_SY3000_LCD_SIZE_Y - 1);
	shtps_report_stflib_kevt(ts, SHTPS_DEF_STFLIB_KEVT_CHANGE_PANEL_SIZE, 0);

	return 0;
}
#endif /* #if defined( SHTPS_DEVELOP_MODE_ENABLE ) */


MODULE_DESCRIPTION("SHARP TOUCHPANEL DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");
