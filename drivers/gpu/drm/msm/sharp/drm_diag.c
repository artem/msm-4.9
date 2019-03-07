/* drivers/gpu/drm/msm/sharp/drm_diag.c  (Display Driver)
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
/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/iopoll.h>
#include <linux/debugfs.h>
#include <uapi/drm/sharp_drm.h>
#include "../dsi-staging/dsi_display.h"
#include "../dsi-staging/dsi_catalog.h"
#include "../dsi-staging/dsi_ctrl.h"
#include "../dsi-staging/dsi_ctrl_reg.h"
#include "../dsi-staging/dsi_clk.h"
#include "../dsi-staging/dsi_hw.h"
#include "../msm_drv.h"
#include "drm_diag.h"
#include "drm_mipi_dsi.h"
#include "drm_cmn.h"
#include "drm_mfr.h"
#include "drm_det.h"
/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
//#define USES_PANEL_HAYABUSA
#ifdef CONFIG_ARCH_JOHNNY
#define USES_PANEL_RM69350
#else /* CONFIG_ARCH_JOHNNY */
#define USES_PANEL_ROSETTA
#endif /* CONFIG_ARCH_JOHNNY */

#if defined(USES_PANEL_HAYABUSA)
#define DRM_DIAG_PANEL_VIDEO_MODE
#define DRM_DIAG_MIPI_CHECK_ENABLE
#define DRM_DIAG_PANEL_FLICKER
#define DRM_DIAG_PANEL_GMM_VOLTAGE
#define DRM_DIAG_GMM_MASK 0x3FF
#elif defined(USES_PANEL_ROSETTA)
#ifdef CONFIG_ARCH_DIO
#define DRM_DIAG_PANEL_VIDEO_MODE
#endif /* CONFIG_ARCH_DIO */
#define DRM_DIAG_MIPI_CHECK_ENABLE
#define DRM_DIAG_PANEL_FLICKER
#define DRM_DIAG_PANEL_GMM_VOLTAGE
#define DRM_DIAG_DEFAULT_PAGE 0x10
#define DRM_DIAG_PAGE_ADDR 0xFF
#define DRM_DIAG_GMM_MASK 0x3FF
#elif defined(USES_PANEL_RM69350)
#define DRM_DIAG_MIPI_CHECK_ENABLE
#define DRM_DIAG_DEFAULT_PAGE 0x00
#define DRM_DIAG_PAGE_ADDR 0xFE
#define DRM_DIAG_PANEL_GMM_VOLTAGE
#define DRM_DIAG_GMM_MASK 0x7FF
#endif  /* USES_PANEL_HAYABUSA */

#if defined(DRM_DIAG_PANEL_GMM_VOLTAGE) || defined(DRM_DIAG_PANEL_FLICKER)
#include <soc/qcom/sh_smem.h>
#endif /* defined(DRM_DIAG_PANEL_GMM_VOLTAGE) || defined(DRM_DIAG_PANEL_FLICKER) */

#define DRM_DSI_DSIPHY_REGULATOR_CTRL_0	(0x00)
#define DRM_DIAG_WAIT_1FRAME_US		(9090)

#ifdef DRM_DIAG_PANEL_FLICKER
#define FLICKER_SET_ALL			(DRM_REG_WRITE | DRM_SAVE_VALUE | \
					DRM_SAVE_VALUE_LOW | DRM_RESET_VALUE)
#endif /* DRM_DIAG_PANEL_FLICKER */

/* ------------------------------------------------------------------------- */
/* STRUCTURE                                                                 */
/* ------------------------------------------------------------------------- */
#define DRM_DIAG_DSI_MAX_CMDS_CNT       (512)
#define DRM_DIAG_DSI_ONE_PAYLOAD_LENGTH (4)
#define DRM_DIAG_DSI_PAYLOADS_LENGHTH   (DRM_DIAG_DSI_MAX_CMDS_CNT * DRM_DIAG_DSI_ONE_PAYLOAD_LENGTH)

struct drm_diag_panel_pad {
	unsigned char page;
	unsigned char addr;
	unsigned char len;
	unsigned char *data;
};

struct drm_diag_pad_item {
	struct drm_diag_panel_pad *pad;
	size_t len;
};

#ifdef DRM_DIAG_PANEL_GMM_VOLTAGE
#ifdef USES_PANEL_RM69350
#include "drm_diag_rm69350.h"
#else
#include "drm_diag_rosetta.h"
#endif /* USES_PANEL_RM69350 */
#endif  /* DRM_DIAG_PANEL_GMM_VOLTAGE */

/* ------------------------------------------------------------------------- */
/* FUNCTION                                                                  */
/* ------------------------------------------------------------------------- */

#ifdef DRM_DIAG_MIPI_CHECK_ENABLE
static void drm_diag_mipi_check_exec(uint8_t *result,
	uint8_t *sensitiv_master, uint8_t *sensitiv_slave,
	struct drm_mipichk_param *drm_mipi_check_param, struct dsi_display *pdisp);
static int drm_diag_mipi_check_manual(uint8_t *sensitiv_master, uint8_t *sensitiv_slave,
	struct drm_mipichk_param *drm_mipi_check_param, struct dsi_display *pdisp);
static void drm_diag_mipi_check_set_param(uint8_t amp, uint8_t *sensitiv_master,
			uint8_t *sensitiv_slave, struct dsi_display *pdisp);
static void drm_diag_mipi_check_get_param(uint8_t *amp, uint8_t *sensitiv_master,
			uint8_t *sensitiv_slave, struct dsi_display *pdisp);
#if defined(DRM_DIAG_PANEL_VIDEO_MODE)
static void drm_diag_mipi_check_test_video(uint8_t *result, uint8_t frame_cnt,
			struct dsi_display *pdisp);
static void drm_diag_mipi_check_test_video_sub(uint8_t *result, uint32_t sleep,
			struct dsi_display *pdisp);
#else /* DRM_DIAG_PANEL_VIDEO_MODE */
static void drm_diag_mipi_check_test_cmd(uint8_t *result,
			struct drm_mipichk_param *drm_mipi_check_param,
			struct dsi_display *pdisp);
#endif /* DRM_DIAG_PANEL_VIDEO_MODE */
static int drm_diag_read_sensitiv(uint8_t *sensitiv_master,
			uint8_t *sensitiv_slave, struct dsi_display *pdisp);
static int drm_diag_write_sensitiv(uint8_t *sensitiv_master,
			uint8_t *sensitiv_slave, struct dsi_display *pdisp);
#endif /* DRM_DIAG_MIPI_CHECK_ENABLE */

#ifdef DRM_DIAG_PANEL_FLICKER
#ifdef USES_PANEL_ROSETTA
static int drm_diag_rosetta_calc_vcom(struct drm_vcom *in, struct drm_rosetta_vcom *out, unsigned short vcomoffset);
static int drm_diag_rosetta_set_flicker_ctx(struct drm_flicker_param *flicker_param);
static int drm_diag_rosetta_get_flicker_low(struct dsi_display *pdisp, struct drm_flicker_param *flicker_param);
static int drm_diag_rosetta_get_flicker(struct dsi_display *pdisp, struct drm_flicker_param *flicker_param);
static int drm_diag_rosetta_set_flicker(struct dsi_display *pdisp, struct drm_flicker_param *flicker_param);
static int drm_diag_rosetta_send_flicker(struct dsi_display *pdisp, struct drm_rosetta_vcom vcom);
#endif /* USES_PANEL_ROSETTA */
static int drm_diag_set_flicker_if_adjusted(struct dsi_display *pdisp);
static int drm_diag_get_flicker(struct dsi_display *pdisp, struct drm_flicker_param *flicker_param);
static int drm_diag_read_flicker(struct dsi_display *pdisp, unsigned char out_buf[3]);
static int drm_diag_send_flicker_param(struct dsi_display *pdisp, struct drm_vcom *vcom);
static int drm_diag_init_flicker_param(struct drm_flicker_ctx *flicker_ctx);
#endif /* DRM_DIAG_PANEL_FLICKER */

#ifdef DRM_DIAG_PANEL_GMM_VOLTAGE
static int drm_diag_check_panel_type(enum panel_type panel_type);
static int drm_diag_get_wdtype_fromlen(int datalen);
static void drm_diag_panel_clear_cmds(void);
static bool drm_diag_panel_add_cmd(char addr, int datalen, char *data);
static int drm_diag_panel_kickoff_cmds(struct dsi_display *pdisp);
static int drm_diag_panel_read_paditems(struct dsi_display *pdisp,
				size_t paditemlen,
				const struct drm_diag_pad_item *paditems);
static void drm_diag_panel_update_gmm_volt_pad(
				union mdp_gmm_volt *gmm_volt);
static int drm_diag_panel_make_gmm_volt_cmds(void);
static int drm_diag_panel_make_gmm_volt_cmds_paditems(size_t paditemlen,
				const struct drm_diag_pad_item *paditems);
static void drm_diag_panel_copy_gmm_to_paddata(int paddlen,
				char *paddata,
				unsigned short *gmm);
static void drm_diag_panel_copy_gmm_from_paddata(int len, unsigned short *gmm,
				char *paddata);
static int drm_diag_panel_read_gmm_volt(struct dsi_display *pdisp);
static int drm_diag_panel_read_gmm_volt_dispoff(struct dsi_display *pdisp);
static void drm_diag_panel_copy_gmm_volt_from_pad(
				struct mdp_gmm_volt_info *gmm_volt_info);
static int drm_diag_set_panel_typ_volt(struct dsi_display *pdisp);
static int drm_diag_set_panel_typ_gmm(struct dsi_display *pdisp);
static int drm_diag_panel_set_adjust_gmm(struct dsi_display *pdisp,
			struct mdp_gmm_volt_info *gmm_volt_info);
static int drm_diag_panel_set_unadjust_gmm(struct dsi_display *pdisp,
			struct mdp_gmm_volt_info *gmm_volt_info);
static int drm_diag_set_panel_gmm(struct dsi_display *pdisp);
static int drm_diag_init_gmm_param(struct drm_gmmvolt_ctx *gmmvolt_ctx);
static int drm_diag_init_adjustable_params(
				struct shdisp_boot_context *shdisp_boot_ctx);
#ifdef USES_PANEL_RM69350
static void drm_diag_rm69350_update_gmm_pad(
				struct rm69350_gmm_volt *gmm_volt);
static void drm_diag_rm69350_update_volt_pad(
				struct rm69350_gmm_volt *gmm_volt);
static void drm_diag_rm69350_copy_gmm_volt_from_pad(
				struct rm69350_gmm_volt *gmm_volt);
static int drm_diag_init_rm69350_gmm_param(
				struct drm_gmmvolt_ctx *gmmvolt_ctx);
#else
static void drm_diag_rosetta_update_gmm_pad(
				struct rosetta_gmm_volt *gmm_volt);
static void drm_diag_rosetta_update_volt_pad(
				struct rosetta_gmm_volt *gmm_volt);
static void drm_diag_rosetta_copy_gmm_volt_from_pad(
				struct rosetta_gmm_volt *gmm_volt);
static int drm_diag_init_rosetta_gmm_param(
				struct drm_gmmvolt_ctx *gmmvolt_ctx);
#endif /* USES_PANEL_RM69350 */
#endif /* DRM_DIAG_PANEL_GMM_VOLTAGE */
static void drm_diag_init_mutex(void);

/* ------------------------------------------------------------------------- */
/* EXTERNAL FUNCTION                                                         */
/* ------------------------------------------------------------------------- */
#ifdef CONFIG_ARCH_DIO
extern int msm_atomic_update_panel_timing_resume(struct dsi_display *display);
#endif /* CONFIG_ARCH_DIO */

/* ------------------------------------------------------------------------- */
/* CONTEXT                                                                   */
/* ------------------------------------------------------------------------- */
#if defined(DRM_DIAG_PANEL_GMM_VOLTAGE)
struct drm_gmm_kerl_ctx {
	/* gmm adjusted status */
	unsigned char			gmm_adj_status;
	/* gmm/volt/other data(used by post_on_cmd) */
	union mdp_gmm_volt		gmm_volt_adjusted;
	int				cmds_cnt;
	struct dsi_cmd_desc		cmds[DRM_DIAG_DSI_MAX_CMDS_CNT];
	int				free_payload_pos;
	char				cmds_payloads[DRM_DIAG_DSI_PAYLOADS_LENGHTH];
};
#endif /* defined(DRM_DIAG_PANEL_GMM_VOLTAGE) */

struct drm_diag_context {
#ifdef DRM_DIAG_PANEL_FLICKER
	struct drm_flicker_ctx drm_flicker_ctx;
#endif /* DRM_DIAG_PANEL_FLICKER */
#ifdef DRM_DIAG_PANEL_GMM_VOLTAGE
	struct drm_gmm_kerl_ctx mdss_gmm_ctx;
#endif /* DRM_DIAG_PANEL_GMM_VOLTAGE */
	struct mutex blank_lock;
	struct mutex displaythread_lock;
	struct mutex chkstatus_lock;
};
static struct drm_diag_context drm_diag_ctx;

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_init_mutex(void)
{
	mutex_init(&drm_diag_ctx.blank_lock);
	mutex_init(&drm_diag_ctx.displaythread_lock);
	mutex_init(&drm_diag_ctx.chkstatus_lock);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_panel_switch_panel_page(struct dsi_display *pdisp,
				short page)
{
	char pageaddr = DRM_DIAG_PAGE_ADDR;
	if(page < 0) {
		return 0;
	}
	return drm_cmn_panel_dcs_write1(pdisp, pageaddr, page);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_diag_set_adjusted(void)
{
	struct dsi_display *pdisp = NULL;

	pdisp = msm_drm_get_dsi_displey();
	if (!pdisp) {
		pr_err("%s: dsi_display is NULL.\n", __func__);
		return -EINVAL;
	}

#ifdef DRM_DIAG_PANEL_FLICKER
	drm_diag_set_flicker_if_adjusted(pdisp);
#endif /* DRM_DIAG_PANEL_FLICKER */
#ifdef DRM_DIAG_PANEL_GMM_VOLTAGE
	drm_diag_set_panel_gmm(pdisp);
#endif /* DRM_DIAG_PANEL_GMM_VOLTAGE */
#ifdef CONFIG_ARCH_DIO
	msm_atomic_update_panel_timing_resume(pdisp);
	usleep_range(20000, 20000);
	drm_diag_panel_switch_panel_page(pdisp, DRM_DIAG_DEFAULT_PAGE);
	drm_cmn_panel_dcs_write0(pdisp, 0x29);
#endif /* CONFIG_ARCH_DIO */
	return 0;
}

#ifdef DRM_DIAG_MIPI_CHECK_ENABLE
#ifdef USES_PANEL_RM69350
extern void drm_det_mipierr_irq_clear(void);
extern int drm_det_check_mipierr_gpio(void);
#endif /* USES_PANEL_RM69350 */
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_dsi_ack_err_status(struct dsi_ctrl *dsi_ctrl)
{
	u32 status;
	u32 ack = 0x10000000;
	struct dsi_ctrl_hw *ctrl = NULL;

	if (dsi_ctrl == NULL) {
		pr_err("%s: dsi_ctrl is NULL.\n", __func__);
		return -EINVAL;
	}

	ctrl = &dsi_ctrl->hw;
	/* DSI_ACK_ERR_STATUS */
	status = DSI_R32(ctrl, DSI_ACK_ERR_STATUS);
	if (status) {
		DSI_W32(ctrl, DSI_ACK_ERR_STATUS, status);
		/* Writing of an extra 0 needed to clear error bits */
		DSI_W32(ctrl, DSI_ACK_ERR_STATUS, 0);

		status &= ~(ack);
		if(status){
			pr_err("%s: status=0x%08x\n", __func__, status);
			return -EIO;
		}
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_dsi_cmd_bta_sw_trigger(struct dsi_ctrl *dsi_ctrl)
{
	int ret = 0;
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

	ret = drm_diag_dsi_ack_err_status(dsi_ctrl);

	pr_debug("%s: out status=0x%08x ret=%d\n", __func__, status, ret);

	return ret;
}
#endif /* DRM_DIAG_MIPI_CHECK_ENABLE */

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_diag_mipi_check(struct drm_mipichk_param *drm_mipi_check_param)
{
	int ret = 0;
#ifdef DRM_DIAG_MIPI_CHECK_ENABLE
	uint8_t amp_data = 0;
	#define SENSITIV_DATA_NUM		(2)
	uint8_t sens_data_master[SENSITIV_DATA_NUM];
	uint8_t *sens_pdata_slave = NULL;
	uint8_t sens_data_slave[SENSITIV_DATA_NUM];
	u32 interrupt_status0;
	u32 interrupt_status1;
	struct dsi_display *pdisp = NULL;
	struct dsi_ctrl *dsi0_ctrl;
	struct dsi_ctrl *dsi1_ctrl;
	struct dsi_ctrl_hw *ctrl0;
	struct dsi_ctrl_hw *ctrl1;

	pdisp = msm_drm_get_dsi_displey();
	if (!pdisp) {
		pr_err("%s: dsi_display is NULL.\n", __func__);
		return -EINVAL;
	}

	dsi0_ctrl = pdisp->ctrl[0].ctrl;
	dsi1_ctrl = pdisp->ctrl[1].ctrl;
	ctrl0 = &dsi0_ctrl->hw;
	ctrl1 = &dsi1_ctrl->hw;

	pr_debug("%s: in master=%p slave=%p\n", __func__, dsi0_ctrl, dsi1_ctrl);

#ifndef USES_PANEL_RM69350
	if (!dsi1_ctrl) {
		pr_err("%s: dsi1_ctrl is NULL.\n", __func__);
		return -ENXIO;
	}
#endif /* USES_PANEL_RM69350 */

#ifdef USES_PANEL_RM69350
	drm_det_pre_panel_off();
#endif /* USES_PANEL_RM69350 */

	interrupt_status0 = dsi_ctrl_hw_cmn_get_interrupt_status(ctrl0);
	dsi_ctrl_hw_cmn_enable_status_interrupts(ctrl0, 0);
	if(dsi1_ctrl) {
		interrupt_status1 = dsi_ctrl_hw_cmn_get_interrupt_status(ctrl1);
		dsi_ctrl_hw_cmn_enable_status_interrupts(ctrl1, 0);
	}

	drm_diag_dsi_cmd_bta_sw_trigger(dsi0_ctrl);
	if (dsi1_ctrl) {
		drm_diag_dsi_cmd_bta_sw_trigger(dsi1_ctrl);
	}

	amp_data = 0;
	memset(sens_data_master, 0, SENSITIV_DATA_NUM);
	memset(sens_data_slave , 0, SENSITIV_DATA_NUM);
	if (dsi1_ctrl) {
		sens_pdata_slave = sens_data_slave;
	}

	drm_diag_mipi_check_get_param(
			&amp_data,
			sens_data_master,
			sens_pdata_slave,
			pdisp);

	ret = drm_diag_mipi_check_manual(
			sens_data_master,
			sens_pdata_slave,
			drm_mipi_check_param,
			pdisp);

	drm_diag_mipi_check_set_param(
			amp_data,
			sens_data_master,
			sens_pdata_slave,
			pdisp);

	dsi_ctrl_hw_cmn_clear_interrupt_status(ctrl0, 0xFFFFFFFF);
	dsi_ctrl_hw_cmn_enable_status_interrupts(ctrl0, interrupt_status0);
	if(dsi1_ctrl) {
	dsi_ctrl_hw_cmn_clear_interrupt_status(ctrl1, 0xFFFFFFFF);
		dsi_ctrl_hw_cmn_enable_status_interrupts(ctrl1, interrupt_status1);
	}
#ifdef USES_PANEL_RM69350
	drm_det_mipierr_irq_clear();
	drm_det_post_panel_on();
#endif /* USES_PANEL_RM69350 */
#endif /* DRM_DIAG_MIPI_CHECK_ENABLE */
	pr_debug("%s: out\n", __func__);

	return ret;
}

#ifdef DRM_DIAG_MIPI_CHECK_ENABLE
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_mipi_check_manual(uint8_t *sensitiv_master, uint8_t *sensitiv_slave,
	struct drm_mipichk_param *drm_mipi_check_param, struct dsi_display *pdisp)
{
	uint8_t result[2] = {DRM_MIPICHK_RESULT_OK, DRM_MIPICHK_RESULT_OK};
	uint8_t dummy[2] = {DRM_MIPICHK_RESULT_OK, DRM_MIPICHK_RESULT_OK};
	struct mdp_mipi_check_param *mipi_check_param =
					drm_mipi_check_param->mipi_check_param;
	struct drm_mipichk_param drm_mipichk_param_dummy;
	struct mdp_mipi_check_param mipichk_param_dummy;

	pr_debug("%s: in\n", __func__);

	if ((mipi_check_param->amp & ~(DRM_MIPICHK_AMP_NUM - 1)) != 0) {
		pr_err("%s: out of range. amp=0x%02X\n",
			__func__, mipi_check_param->amp);
		return -EINVAL;
	}

	if ((mipi_check_param->sensitiv & ~(DRM_MIPICHK_SENSITIV_NUM - 1)) != 0) {
		pr_err("%s: out of range. sensitiv=0x%02X\n",
			__func__, mipi_check_param->sensitiv);
		return -EINVAL;
	}

	drm_diag_mipi_check_exec(result, sensitiv_master, sensitiv_slave,
					drm_mipi_check_param, pdisp);

	if ((result[0] != DRM_MIPICHK_RESULT_OK) ||(result[1] != DRM_MIPICHK_RESULT_OK)) {
		pr_debug("%s: recovery display.\n", __func__);
		memcpy(&drm_mipichk_param_dummy, drm_mipi_check_param, sizeof(struct drm_mipichk_param));
		mipichk_param_dummy.frame_cnt = 1;
		mipichk_param_dummy.amp = DRM_MIPICHK_AMP_NUM - 1;
		mipichk_param_dummy.sensitiv = DRM_MIPICHK_SENSITIV_NUM - 1;
		drm_mipichk_param_dummy.mipi_check_param = &mipichk_param_dummy;
		drm_diag_mipi_check_exec(dummy, sensitiv_master, sensitiv_slave,
					&drm_mipichk_param_dummy, pdisp);
	}

	mipi_check_param->result_master = result[0];
	mipi_check_param->result_slave  = result[1];

	pr_debug("%s: out master=%d slave=%d\n", __func__,
		mipi_check_param->result_master, mipi_check_param->result_slave);

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_mipi_check_exec(uint8_t *result,
	uint8_t *sensitiv_master, uint8_t *sensitiv_slave,
	struct drm_mipichk_param *drm_mipi_check_param,
	struct dsi_display *pdisp)
{
	struct mdp_mipi_check_param *mipichk_param_p =
			drm_mipi_check_param->mipi_check_param;
	uint8_t set_amp;
	uint8_t set_sensitiv_master[2] = {0, 0};
	uint8_t set_sensitiv_slave[2] = {0, 0};
#ifndef USES_PANEL_RM69350
	struct dsi_ctrl *dsi1_ctrl = pdisp->ctrl[1].ctrl;
#endif /* USES_PANEL_RM69350 */
	static const uint8_t amp_tbl[DRM_MIPICHK_AMP_NUM] = {
		0x03,
		0x02,
		0x00,
		0x01,
		0x04,
		0x05,
		0x06,
		0x07
	};

	pr_debug("%s: in frame_cnt=0x%02X amp=0x%02X sensitiv=0x%02X\n",
		__func__, mipichk_param_p->frame_cnt,
		mipichk_param_p->amp, mipichk_param_p->sensitiv);

	set_amp = (amp_tbl[mipichk_param_p->amp] << 1) | 1;

#if defined(USES_PANEL_ROSETTA)
	set_sensitiv_master[0]  = (mipichk_param_p->sensitiv & 0x0F);
	set_sensitiv_master[0] |= *(sensitiv_master) & 0xF0;
	if (dsi1_ctrl) {
		set_sensitiv_slave[0]   = (mipichk_param_p->sensitiv & 0x0F);
		set_sensitiv_slave[0]  |= *(sensitiv_slave) & 0xF0;
	}
#elif defined(USES_PANEL_RM69350)
// mipierr_facter setting (not support sensitiv)
	set_sensitiv_master[0]  = 0xFD;
	set_sensitiv_master[1]  = 0xFF;
#elif defined(USES_PANEL_HAYABUSA)
	set_sensitiv_master[0]  = mipichk_param_p->sensitiv << 4;
	set_sensitiv_master[0] |= *(sensitiv_master) & 0x0F;
	if (dsi1_ctrl) {
		set_sensitiv_slave[0]   = mipichk_param_p->sensitiv << 4;
		set_sensitiv_slave[0]  |= *(sensitiv_slave) & 0x0F;
	}
#elif defined(USES_PANEL_SAZABI)
	set_sensitiv_master[0]  = *(sensitiv_master);
	set_sensitiv_master[1]  = (mipichk_param_p->sensitiv << 4) & 0x70;
	set_sensitiv_master[1] |= *(sensitiv_master + 1) & 0x8F;
	if (dsi1_ctrl) {
		set_sensitiv_slave[0]  = *(sensitiv_slave);
		set_sensitiv_slave[1]  = (mipichk_param_p->sensitiv << 4) & 0x70;
		set_sensitiv_slave[1] |= *(sensitiv_slave + 1) & 0x8F;
	}
#endif /* USES_PANEL_HAYABUSA */

	drm_diag_mipi_check_set_param(set_amp, set_sensitiv_master,
		set_sensitiv_slave, pdisp);

#if defined(DRM_DIAG_PANEL_VIDEO_MODE)
	drm_diag_mipi_check_test_video(result, mipichk_param_p->frame_cnt, pdisp);
#else /* DRM_DIAG_PANEL_VIDEO_MODE */
	drm_diag_mipi_check_test_cmd(result, drm_mipi_check_param, pdisp);
#endif /* DRM_DIAG_PANEL_VIDEO_MODE */

	pr_debug("%s: out\n", __func__);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_mipi_check_set_param(uint8_t amp, uint8_t *sensitiv_master,
			uint8_t *sensitiv_slave, struct dsi_display *pdisp)
{
	pr_debug("%s: amp=0x%02X sensitiv_master=0x%02X,0x%02X\n", __func__,
		amp, sensitiv_master[0], sensitiv_master[1]);

#if defined(MIPICHK_AMP)
	/* MMSS_DSI_0_PHY_REG_DSIPHY_REGULATOR_CTRL_0 */
	MIPI_OUTP((ctrl->phy_regulator_io.base) + MDSS_DSI_DSIPHY_REGULATOR_CTRL_0, amp);
	if (sctrl) {
		MIPI_OUTP((sctrl->phy_regulator_io.base) + MDSS_DSI_DSIPHY_REGULATOR_CTRL_0, amp);
	}
	wmb();
#endif /* MIPICHK_AMP */

	drm_diag_write_sensitiv(sensitiv_master, sensitiv_slave, pdisp);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_mipi_check_get_param(uint8_t *amp, uint8_t *sensitiv_master,
			uint8_t *sensitiv_slave, struct dsi_display *pdisp)
{
	int ret = 0;

#if defined(MIPICHK_AMP)
	/* MMSS_DSI_0_PHY_REG_DSIPHY_REGULATOR_CTRL_0 */
	*amp = MIPI_INP((ctrl->phy_regulator_io.base)+ MDSS_DSI_DSIPHY_REGULATOR_CTRL_0);
#endif /* MIPICHK_AMP */

#if defined(DRM_DIAG_PANEL_VIDEO_MODE)
	drm_cmn_video_transfer_ctrl(false);
	ret = drm_diag_read_sensitiv(sensitiv_master, sensitiv_slave, pdisp);
	drm_cmn_video_transfer_ctrl(true);
#else  /* DRM_DIAG_PANEL_VIDEO_MODE */
	ret = drm_diag_read_sensitiv(sensitiv_master, sensitiv_slave, pdisp);
#endif  /* DRM_DIAG_PANEL_VIDEO_MODE */

	pr_debug("%s: amp=0x%02X sensitiv_master=0x%02X,0x%02X ret=%d\n",
		__func__, *amp, sensitiv_master[0], sensitiv_master[1], ret);
}

#if defined(DRM_DIAG_PANEL_VIDEO_MODE)
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_mipi_check_test_video(uint8_t *result, uint8_t frame_cnt,
			struct dsi_display *pdisp)
{
	uint32_t sleep;

	sleep = frame_cnt * DRM_DIAG_WAIT_1FRAME_US;
	pr_debug("%s: frame_cnt=%d sleep=%d\n", __func__, frame_cnt, sleep);

#ifdef CONFIG_SHARP_DRM_HR_VID /* CUST_ID_00015 */
	drm_mfr_suspend_ctrl(true);
#endif /* CONFIG_SHARP_DRM_HR_VID */
	drm_diag_mipi_check_test_video_sub(result, sleep, pdisp);
#ifdef CONFIG_SHARP_DRM_HR_VID /* CUST_ID_00015 */
	drm_mfr_suspend_ctrl(false);
#endif /* CONFIG_SHARP_DRM_HR_VID */
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_mipi_check_test_video_sub(uint8_t *result, uint32_t sleep,
			struct dsi_display *pdisp)
{
	struct dsi_ctrl *dsi0_ctrl = pdisp->ctrl[0].ctrl;
	struct dsi_ctrl *dsi1_ctrl = pdisp->ctrl[1].ctrl;

	pr_debug("%s: sleep start.\n",__func__);
	usleep_range(sleep, sleep);
	pr_debug("%s: sleep finish.\n",__func__);

	if (drm_diag_dsi_cmd_bta_sw_trigger(dsi0_ctrl)) {
		result[0] = DRM_MIPICHK_RESULT_NG;
	} else {
		result[0] = DRM_MIPICHK_RESULT_OK;
	}

	if (dsi1_ctrl) {
		if (drm_diag_dsi_cmd_bta_sw_trigger(dsi1_ctrl)) {
			result[1] = DRM_MIPICHK_RESULT_NG;
		} else {
			result[1] = DRM_MIPICHK_RESULT_OK;
		}
	}
}
#else /* DRM_DIAG_PANEL_VIDEO_MODE */
extern int drm_mode_page_flip_diag(struct drm_device *dev,
				struct drm_file *file_priv);
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_mipi_check_test_cmd(uint8_t *result,
			struct drm_mipichk_param *drm_mipi_check_param,
			struct dsi_display *pdisp)
{
	int i;
	int frame_cnt = drm_mipi_check_param->mipi_check_param->frame_cnt;
#ifndef USES_PANEL_RM69350
	struct dsi_ctrl *dsi0_ctrl = pdisp->ctrl[0].ctrl;
	struct dsi_ctrl *dsi1_ctrl = pdisp->ctrl[1].ctrl;
#endif /* USES_PANEL_RM69350 */

	pr_debug("%s: in\n", __func__);

	for (i = 0; i < frame_cnt; i++) {
		pr_debug("%s: frame=%d\n", __func__, i);
		drm_mode_page_flip_diag(
			drm_mipi_check_param->dev,
			drm_mipi_check_param->file);
	}

#ifdef USES_PANEL_RM69350
	if (drm_det_check_mipierr_gpio()) {
		// H:MIPIERR
		result[0] = DRM_MIPICHK_RESULT_NG;
	} else {
		// L:not MIPIERR
		result[0] = DRM_MIPICHK_RESULT_OK;
	}
#else /* USES_PANEL_RM69350 */
	if (drm_diag_dsi_cmd_bta_sw_trigger(dsi0_ctrl)) {
		result[0] = DRM_MIPICHK_RESULT_NG;
	} else {
		result[0] = DRM_MIPICHK_RESULT_OK;
	}

	if (dsi1_ctrl) {
		if (drm_diag_dsi_cmd_bta_sw_trigger(dsi1_ctrl)) {
			result[1] = DRM_MIPICHK_RESULT_NG;
		} else {
			result[1] = DRM_MIPICHK_RESULT_OK;
		}
	}
#endif /* USES_PANEL_RM69350 */
	pr_debug("%s: out\n", __func__);

}
#endif /* DRM_DIAG_PANEL_VIDEO_MODE */

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_read_sensitiv(uint8_t *sensitiv_master,
			uint8_t *sensitiv_slave, struct dsi_display *pdisp)
{
	int ret = 0;
#if defined(USES_PANEL_ROSETTA)
	unsigned char sensitiv_addr[2] = {0xA9, 0xBE};
	unsigned char r_buf = 0x00;

	ret = drm_diag_panel_switch_panel_page(pdisp, 0xD0);
	if (ret) {
		pr_err("%s: failed to switch page\n", __func__);
		return ret;
	}

	ret = drm_cmn_panel_dcs_read(pdisp, sensitiv_addr[0], 1, &r_buf);

	if (ret) {
		pr_err("%s: failed to read\n", __func__);
		return ret;
	}
	sensitiv_master[0] = r_buf;

	if (sensitiv_slave != NULL) {
		r_buf = 0;
		ret = drm_cmn_panel_dcs_read(pdisp,
					 sensitiv_addr[1], 1, &r_buf);
		sensitiv_slave[0] = r_buf;
	}
#elif defined(USES_PANEL_RM69350)
// mipierr_facter setting (not support sensitiv)
	int i = 0;
	unsigned char sensitiv_addr[2] = {0x36, 0x37};
	unsigned char r_buf = 0x00;

	ret = drm_diag_panel_switch_panel_page(pdisp, 0xD2);
	if (ret) {
		pr_err("%s: failed to switch page\n", __func__);
		return ret;
	}

	for (i=0; i<ARRAY_SIZE(sensitiv_addr); i++) {
		ret = drm_cmn_panel_dcs_read(pdisp,
					sensitiv_addr[i], 1, &r_buf);
		if (ret) {
			pr_err("%s: failed to read (%d)\n", __func__, i);
			return ret;
		}
		sensitiv_master[i] = r_buf;
	}

	if (sensitiv_slave != NULL) {
		sensitiv_slave[0] = 0;
	}
#elif defined(USES_PANEL_HAYABUSA)
	unsigned char sensitiv_addr[2] = {0x7E, 0x97};
	unsigned char r_buf = 0x00;

	ret = drm_diag_panel_switch_panel_page(
				&(ctrl->panel_data), 0xE0);
	if (ret) {
		pr_err("%s: failed to switch page\n", __func__);
		return ret;
	}

	ret = drm_cmn_panel_dcs_read(&(ctrl->panel_data),
					sensitiv_addr[0], 1, &r_buf);
	if (ret) {
		pr_err("%s: failed to read\n", __func__);
		return ret;
	}
	sensitiv_master[0] = r_buf;

	if (sensitiv_slave != NULL) {
		r_buf = 0;
		ret = drm_cmn_panel_dcs_read(&(ctrl->panel_data),
					 sensitiv_addr[1], 1, &r_buf);
		sensitiv_slave[0] = r_buf;
	}
#elif defined(USES_PANEL_SAZABI)
	struct dcs_cmd_req cmdreq;
	char bank[] = {0xF0, 0x46, 0x23, 0x11, 0x01, 0x00};
	struct dsi_cmd_desc bank_cmd = {
		{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(bank)},
		bank
	};
	unsigned char read_buf[2] = {0x00, 0x00};
	char read_cmd[] = {0xB4};
	struct dsi_cmd_desc gen_read_cmd = {
		{DTYPE_GEN_READ2, 1, 0, 1, 5, sizeof(read_cmd)},
		read_cmd
	};

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &bank_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_LP_MODE;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &gen_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT | CMD_REQ_LP_MODE;
	cmdreq.rlen = sizeof(read_buf);
	cmdreq.rbuf = read_buf;
	cmdreq.cb = NULL; /* call back */
	ret = mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	memcpy(sensitiv_master, read_buf, sizeof(read_buf));
#endif /* USES_PANEL_HAYABUSA */
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_write_sensitiv(uint8_t *sensitiv_master,
			uint8_t *sensitiv_slave, struct dsi_display *pdisp)
{
	int ret = 0;
#if defined(USES_PANEL_ROSETTA)
	unsigned char sensitiv_addr[2] = {0xA9, 0xBE};
	unsigned char w_buf = 0x00;

	ret = drm_diag_panel_switch_panel_page(pdisp, 0xD0);
	if (ret) {
		pr_err("%s: failed to switch page\n", __func__);
		return ret;
	}

	w_buf = sensitiv_master[0];
	ret = drm_cmn_panel_dcs_write1(pdisp, sensitiv_addr[0], w_buf);
	if (ret) {
		pr_err("%s: failed to write\n", __func__);
		return ret;
	}

	if (sensitiv_slave != NULL) {
		w_buf = sensitiv_slave[0];
		ret = drm_cmn_panel_dcs_write1(pdisp, sensitiv_addr[1], w_buf);
	}
#elif defined(USES_PANEL_RM69350)
// mipierr_facter setting (not support sensitiv)
	int i = 0;
	unsigned char sensitiv_addr[2] = {0x36, 0x37};
	unsigned char w_buf = 0x00;

	ret = drm_diag_panel_switch_panel_page(pdisp, 0xFE);
	if (ret) {
		pr_err("%s: failed to switch page\n", __func__);
		return ret;
	}

	for (i=0; i<ARRAY_SIZE(sensitiv_addr); i++) {
		w_buf = sensitiv_master[i];
		ret = drm_cmn_panel_dcs_write1(pdisp,sensitiv_addr[i], w_buf);
		if (ret) {
			pr_err("%s: failed to write (%d)\n", __func__, i);
			return ret;
		}
	}

#elif defined(USES_PANEL_HAYABUSA)
	unsigned char sensitiv_addr[2] = {0x7E, 0x97};
	unsigned char w_buf = 0x00;

	ret = drm_diag_panel_switch_panel_page(
				&(ctrl->panel_data), 0xE0);
	if (ret) {
		pr_err("%s: failed to switch page\n", __func__);
		return ret;
	}

	w_buf = sensitiv_master[0];
	ret = drm_cmn_panel_dcs_write1(&(ctrl->panel_data),
					sensitiv_addr[0], w_buf);
	if (ret) {
		pr_err("%s: failed to write\n", __func__);
		return ret;
	}

	if (sensitiv_slave != NULL) {
		w_buf = sensitiv_slave[0];
		ret = drm_cmn_panel_dcs_write1(&(ctrl->panel_data),
					sensitiv_addr[1], w_buf);
	}
#elif defined(USES_PANEL_SAZABI)
	struct dcs_cmd_req cmdreq;
	char bank[] = {0xF0, 0x46, 0x23, 0x11, 0x01, 0x00};
	struct dsi_cmd_desc bank_cmd = {
		{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(bank)},
		bank
	};
	char sensitiv[] = {0xB4, 0x00, 0x00};
	struct dsi_cmd_desc sensitiv_cmd = {
		{DTYPE_GEN_LWRITE, 1, 0, 0, 1, sizeof(sensitiv)},
		sensitiv
	};

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &bank_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_LP_MODE;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);

	sensitiv[1] = *(sensitiv_master);
	sensitiv[2] = *(sensitiv_master + 1);

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &sensitiv_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL | CMD_REQ_LP_MODE;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	ret = mdss_dsi_cmdlist_put(ctrl, &cmdreq);
#endif /* USES_PANEL_HAYABUSA */
	return ret;
}

#endif /* DRM_DIAG_MIPI_CHECK_ENABLE */

#ifdef DRM_DIAG_PANEL_FLICKER
#ifdef USES_PANEL_ROSETTA
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_rosetta_calc_vcom(struct drm_vcom *in, struct drm_rosetta_vcom *out, unsigned short vcomoffset)
{
	unsigned short tmp;
	unsigned short vcomadj;
	unsigned short vcomdcoff;

	pr_debug("%s: vcom=0x%04x vcom_low=0x%04x vcom_offset=0x%04x\n", __func__, in->vcom, in->vcom_low, vcomoffset);

	if (out == NULL) {
		pr_err("%s: <NULL_POINTER> out.\n", __func__);
		return -EPERM;
	}

	vcomadj = in->vcom + vcomoffset;
	if (vcomadj > VCOM_MAX) {
		vcomadj = VCOM_MAX;
	}
#if (VCOM_MIN > 0)
	if (vcomadj < VCOM_MIN) {
		vcomadj = VCOM_MIN;
	}
#endif
	pr_debug("%s: vcomadj=0x%04x\n", __func__, vcomadj);

	out->vcom1_l = vcomadj & 0xFF;
	out->vcom2_l = out->vcom1_l;
	out->vcom12_h = 0x00;
	if ((vcomadj >> 8) & 0x01) {
		out->vcom12_h |= 0x0C;
	}

	pr_debug("%s: VCOM1_L=0x%02x VCOM2_L=0x%02x VCOM12_H=0x%02x\n", __func__, out->vcom1_l, out->vcom2_l, out->vcom12_h);

	if (in->vcom_low >= in->vcom) {
		tmp = ((in->vcom_low - in->vcom) & 0x0F);
	} else {
		tmp = (((in->vcom - in->vcom_low - 1) & 0x0F) | 0x10);
	}
	out->lpvcom1 = ((tmp & 0x1F) | 0x60);
	out->lpvcom2 = ((tmp & 0x1F) | 0x40);

	vcomdcoff = ((vcomadj&0xFF) + 1) / 2;
	out->vcomoff_l = (unsigned char) (vcomdcoff & 0xFF);

	if (vcomadj & 0x100) {
		out->vcomoff_h = (unsigned char)(0x20);
	} else {
		out->vcomoff_h = (unsigned char)(0x00);
	}

	pr_debug("%s: LPVCOM1=0x%02x LPVCOM2=0x%02x VCOMOFF_L=0x%02x VCOMOFF_H=0x%02x", __func__, out->lpvcom1, out->lpvcom2,
		out->vcomoff_l, out->vcomoff_h);

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_rosetta_set_flicker_ctx(struct drm_flicker_param *flicker_param)
{
	if (flicker_param->request & DRM_SAVE_VALUE) {
		drm_diag_ctx.drm_flicker_ctx.vcom.vcom = flicker_param->vcom;
		drm_diag_ctx.drm_flicker_ctx.nvram = 0x9000 | flicker_param->vcom;
	}
	if (flicker_param->request & DRM_SAVE_VALUE_LOW) {
		drm_diag_ctx.drm_flicker_ctx.vcom.vcom_low = flicker_param->vcom;
	}
	if (flicker_param->request & DRM_RESET_VALUE) {
		drm_diag_ctx.drm_flicker_ctx.vcom.vcom = 0;
		drm_diag_ctx.drm_flicker_ctx.vcom.vcom_low = 0;
		drm_diag_ctx.drm_flicker_ctx.nvram = 0;
	}
	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_rosetta_send_flicker(struct dsi_display *pdisp, struct drm_rosetta_vcom vcom)
{
	int ret = 0;
	unsigned char addr_value[][2] = {
		{0xFF, 0x20},
		{0x8A, 0x00},
		{0x8B, 0x00},
		{0x88, 0x00},
		{0xFF, 0x26},
		{0x80, 0x00},
		{0x81, 0x00},
		{0xFF, 0x28},
		{0x15, 0x00},
		{0x16, 0x00},
	};

	struct dsi_cmd_desc flicker_cmd[] = {
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, addr_value[0], 0, NULL}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, addr_value[1], 0, NULL}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, addr_value[2], 0, NULL}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, addr_value[3], 0, NULL}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, addr_value[4], 0, NULL}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, addr_value[5], 0, NULL}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, addr_value[6], 0, NULL}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, addr_value[7], 0, NULL}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, addr_value[8], 0, NULL}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, addr_value[9], 0, NULL}, 1, 0},
	};

	addr_value[1][1] = vcom.vcom1_l;
	addr_value[2][1] = vcom.vcom2_l;
	addr_value[3][1] = vcom.vcom12_h;
	addr_value[5][1] = vcom.lpvcom1;
	addr_value[6][1] = vcom.lpvcom2;
	addr_value[8][1] = vcom.vcomoff_l;
	addr_value[9][1] = vcom.vcomoff_h;

	ret = drm_cmn_panel_cmds_transfer(pdisp, flicker_cmd, ARRAY_SIZE(flicker_cmd));

	pr_debug("%s:end ret=%d\n", __func__, ret);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_rosetta_set_flicker(struct dsi_display *pdisp, struct drm_flicker_param *flicker_param)
{
	int ret = 0;
	struct drm_vcom vcom;
	vcom.vcom = flicker_param->vcom;
	vcom.vcom_low = flicker_param->vcom;

	if (flicker_param->request & DRM_REG_WRITE) {
#if defined(DRM_DIAG_PANEL_VIDEO_MODE)
		drm_cmn_video_transfer_ctrl(false);
		ret = drm_diag_send_flicker_param(pdisp, &vcom);
		drm_diag_panel_switch_panel_page(pdisp, DRM_DIAG_DEFAULT_PAGE);
		drm_cmn_video_transfer_ctrl(true);
#else  /* DRM_DIAG_PANEL_VIDEO_MODE */
		ret = drm_diag_send_flicker_param(pdisp, &vcom);
		drm_diag_panel_switch_panel_page(pdisp, DRM_DIAG_DEFAULT_PAGE);
#endif  /* DRM_DIAG_PANEL_VIDEO_MODE */
		if (ret != 0) {
			pr_err("%s: <RESULT_FAILURE> drm_diag_send_flicker_param.\n", __func__);
			return ret;
		}
	}
	drm_diag_rosetta_set_flicker_ctx(flicker_param);

	return 0;
}
#endif /* USES_PANEL_ROSETTA */

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_send_flicker_param(struct dsi_display *pdisp, struct drm_vcom *vcom)
{
	int ret = 0;
#ifdef USES_PANEL_ROSETTA
	struct drm_rosetta_vcom out;

	memset(&out, 0, sizeof(out));
	ret = drm_diag_rosetta_calc_vcom(vcom, &out, 0);
	if (ret != 0) {
		pr_err("%s: <RESULT_FAILURE> drm_diag_rosetta_calc_vcom.\n", __func__);
		return ret;
	}
	ret = drm_diag_rosetta_send_flicker(pdisp, out);
#endif /* USES_PANEL_ROSETTA */
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_set_flicker_if_adjusted(struct dsi_display *pdisp)
{
	int ret = 0;
	unsigned short nvram = drm_diag_ctx.drm_flicker_ctx.nvram;

	if (!pdisp) {
		pr_err("%s: dsi_display is NULL.\n", __func__);
		return -ENXIO;
	}
	if (IS_FLICKER_ADJUSTED(nvram)) {
		ret = drm_diag_send_flicker_param(pdisp, &drm_diag_ctx.drm_flicker_ctx.vcom);
		pr_debug("%s: flicker adjusted. vcom=0x%x vcom_low=0x%x\n",
			__func__, drm_diag_ctx.drm_flicker_ctx.vcom.vcom,
			drm_diag_ctx.drm_flicker_ctx.vcom.vcom_low);
	} else {
		pr_debug("%s:flicker not adjusted\n", __func__);
	}
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_read_flicker(struct dsi_display *pdisp, unsigned char out_buf[3])
{
	int ret = 0;
	int ret2 = 0;
	unsigned char read_buf[3];
	unsigned char addr_value[][2] = {
		{0xFF, 0x20},
		{0x8A, 0x00},
		{0x88, 0x00},
		{0xFF, 0x26},
		{0x80, 0x00},
	};

	struct dsi_cmd_desc flicker_read_cmd[] = {
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, addr_value[0], 0, NULL        }, 1, 0},
		{{0, MIPI_DSI_DCS_READ  , MIPI_DSI_MSG_USE_LPM, 0, 0, 1, addr_value[1], 1, &read_buf[0]}, 1, 0},
		{{0, MIPI_DSI_DCS_READ  , MIPI_DSI_MSG_USE_LPM, 0, 0, 1, addr_value[2], 1, &read_buf[1]}, 1, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, addr_value[3], 0, NULL        }, 1, 0},
		{{0, MIPI_DSI_DCS_READ  , MIPI_DSI_MSG_USE_LPM, 0, 0, 1, addr_value[4], 1, &read_buf[2]}, 1, 0},
	};

	memset(read_buf, 0x00, sizeof(read_buf));
	ret = drm_cmn_panel_cmds_transfer(pdisp, flicker_read_cmd, ARRAY_SIZE(flicker_read_cmd));
	if (ret) {
		pr_err("%s: <RESULT_FAILURE> read flicker ret=%d.\n", __func__, ret);
		return ret;
	}
	memcpy(out_buf, read_buf, sizeof(read_buf));

	pr_debug("%s: Read. VCOM1_L=0x%02x VCOM12_H=0x%02x LPVCOM2=0x%02x\n",
		__func__, out_buf[0], out_buf[1], out_buf[2]);

	/* to page default */
	ret2 = drm_diag_panel_switch_panel_page(pdisp, DRM_DIAG_DEFAULT_PAGE);
	if (ret2) {
		pr_err("%s: <RESULT_FAILURE> to page 0x%02x ret=%d.\n", __func__, DRM_DIAG_DEFAULT_PAGE, ret2);
		ret = ret2;
	}
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_rosetta_get_flicker(struct dsi_display *pdisp, struct drm_flicker_param *flicker_param)
{
	int ret = 0;
	unsigned char readbuf[3];

	memset(readbuf, 0x00, sizeof(readbuf));

#if defined(DRM_DIAG_PANEL_VIDEO_MODE)
	drm_cmn_video_transfer_ctrl(false);
	ret = drm_diag_read_flicker(pdisp, readbuf);
	drm_cmn_video_transfer_ctrl(true);
#else  /* DRM_DIAG_PANEL_VIDEO_MODE */
	ret = drm_diag_read_flicker(pdisp, readbuf);
#endif  /* DRM_DIAG_PANEL_VIDEO_MODE */
	if (ret < 0) {
		pr_err("%s: <RESULT_FAILURE> drm_diag_read_flicker.\n", __func__);
		return ret;
	}
	flicker_param->vcom = readbuf[0];
	flicker_param->vcom |= ((readbuf[1] & 0x04) ? 0x0100 : 0x0000);

	pr_debug("%s: out vcom=0x%04X\n", __func__, flicker_param->vcom);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_rosetta_get_flicker_low(struct dsi_display *pdisp, struct drm_flicker_param *flicker_param)
{
	int ret = 0;

	unsigned char readbuf[3];
	unsigned short tmp_vcom;
	memset(readbuf, 0x00, sizeof(readbuf));

#if defined(DRM_DIAG_PANEL_VIDEO_MODE)
	drm_cmn_video_transfer_ctrl(false);
	ret = drm_diag_read_flicker(pdisp, readbuf);
	drm_cmn_video_transfer_ctrl(true);
#else  /* DRM_DIAG_PANEL_VIDEO_MODE */
	ret = drm_diag_read_flicker(pdisp, readbuf);
#endif  /* DRM_DIAG_PANEL_VIDEO_MODE */
	if (ret < 0) {
		pr_err("%s: <RESULT_FAILURE> drm_diag_read_flicker.\n", __func__);
		return ret;
	}

	tmp_vcom = readbuf[0];
	tmp_vcom |= ((readbuf[1] & 0x04) ? 0x0100 : 0x0000);
	if (readbuf[2] & 0x10) {
		flicker_param->vcom = tmp_vcom - (readbuf[2] & 0x0f) - 1;
	} else {
		flicker_param->vcom = tmp_vcom + (readbuf[2] & 0x0f);
	}
	pr_debug("%s: out vcom_low=0x%04X\n", __func__, flicker_param->vcom);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_get_flicker(struct dsi_display *pdisp, struct drm_flicker_param *flicker_param)
{
	int ret = 0;

	switch (flicker_param->request) {
	case DRM_GET_VALUE:
		ret = drm_diag_rosetta_get_flicker(pdisp, flicker_param);
		break;
	case DRM_GET_VALUE_LOW:
		ret = drm_diag_rosetta_get_flicker_low(pdisp, flicker_param);
		break;
	default:
		pr_err("%s: request param error.\n", __func__);
		return -EPERM;
	}
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_init_flicker_param(struct drm_flicker_ctx *flicker_ctx)
{
	int ret = 0;

	if (!IS_FLICKER_ADJUSTED(flicker_ctx->nvram)) {
		pr_debug("%s: flicker is not adjusted\n", __func__);
		return ret;
	}

	drm_diag_ctx.drm_flicker_ctx.vcom.vcom = flicker_ctx->vcom.vcom;
	drm_diag_ctx.drm_flicker_ctx.vcom.vcom_low = flicker_ctx->vcom.vcom_low;
	drm_diag_ctx.drm_flicker_ctx.nvram = flicker_ctx->nvram;
	return ret;
}
#endif /* DRM_DIAG_PANEL_FLICKER */

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_diag_set_flicker_param(struct drm_flicker_param *flicker_param)
{
	int ret = 0;
#ifdef DRM_DIAG_PANEL_FLICKER
	struct dsi_display *pdisp = NULL;

	if (!flicker_param) {
		pr_err("%s: flicker_param is NULL.\n", __func__);
		return -EPERM;
	}

	if ((flicker_param->request & FLICKER_SET_ALL) == 0) {
		pr_err("%s: request invalid.\n", __func__);
		return -EPERM;
	}

	pdisp = msm_drm_get_dsi_displey();
	if (!pdisp) {
		pr_err("%s: dsi_display is NULL.\n", __func__);
		return -EINVAL;
	}

	if (flicker_param->vcom > VCOM_MAX) {
		pr_err("%s: over VCOM_MAX(0x%04d)\n", __func__, flicker_param->vcom);
		return -EINVAL;
	}
#if (VCOM_MIN > 0)
	if (flicker_param->vcom < VCOM_MIN) {
		pr_err("%s: under VCOM_MIN(0x%04d)\n", __func__, flicker_param->vcom);
		return -EINVAL;
	}
#endif

	ret = drm_diag_rosetta_set_flicker(pdisp, flicker_param);
	if (ret != 0) {
		pr_err("%s: <RESULT_FAILURE> drm_diag_rosetta_set_flicker.\n", __func__);
		return ret;
	}
#endif /* DRM_DIAG_PANEL_FLICKER */
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_diag_get_flicker_param(struct drm_flicker_param *flicker_param)
{
	int ret = 0;
#ifdef DRM_DIAG_PANEL_FLICKER
	struct dsi_display *pdisp = NULL;

	if (!flicker_param) {
		pr_err("%s: flicker_param is NULL.\n", __func__);
		return -EPERM;
	}

	pdisp = msm_drm_get_dsi_displey();
	if (!pdisp) {
		pr_err("%s: dsi_display is NULL.\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: in request=0x%04X vcom=0x%04X\n", __func__, flicker_param->request, flicker_param->vcom);
	ret = drm_diag_get_flicker(pdisp, flicker_param);
#endif /* DRM_DIAG_PANEL_FLICKER */
	return ret;
}

#ifdef DRM_DIAG_PANEL_GMM_VOLTAGE
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_check_panel_type(enum panel_type panel_type)
{
	pr_debug("%s: panel_type=%u\n", __func__, panel_type);

#ifdef USES_PANEL_RM69350
	if (panel_type != PANEL_TYPE_RM69350) {
		pr_err("%s: panel type isn't same hw req(%d):hw(%d)\n",
			__func__, panel_type ,PANEL_TYPE_RM69350);
		return -EINVAL;
	}
#else
	if (panel_type != PANEL_TYPE_ROSETTA) {
		pr_err("%s: panel type isn't same hw req(%d):hw(%d)\n",
			__func__, panel_type ,PANEL_TYPE_ROSETTA);
		return -EINVAL;
	}
#endif
	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_panel_copy_gmm_to_paddata(int paddlen,
				char *paddata,
				unsigned short *gmm)
{
	int paddatapos = 0;
	int gmmpos = 0;
	while(paddatapos < paddlen) {
		paddata[paddatapos++] = ((gmm[gmmpos] >> 8) & ((DRM_DIAG_GMM_MASK & 0xFF00) >> 8));
		paddata[paddatapos++] = (gmm[gmmpos++] & (DRM_DIAG_GMM_MASK & 0x00FF));
	}
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_panel_copy_gmm_from_paddata(int len, unsigned short *gmm,
				char *paddata)
{
	int cnt = 0;
	int paddatapos = 0;
	for (cnt = 0; cnt < len; ++cnt) {
		paddatapos = cnt*2;
		gmm[cnt] =  (paddata[paddatapos] << 8) | (paddata[paddatapos+1]);
		gmm[cnt] &= DRM_DIAG_GMM_MASK;
	}
}


/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_get_wdtype_fromlen(int datalen)
{
	int dtype = MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM;
	switch(datalen) {
	case 0:
		dtype = MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM;
		break;
	case 1:
		dtype = MIPI_DSI_DCS_LONG_WRITE;
		break;
	default:
		dtype = MIPI_DSI_DCS_LONG_WRITE;
	}
	return dtype;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_panel_clear_cmds(void)
{
	struct drm_gmm_kerl_ctx *gmm_ctx = &drm_diag_ctx.mdss_gmm_ctx;
	/* clear dsi cmds structure and payload buffer */
	memset(&gmm_ctx->cmds, 0, sizeof(gmm_ctx->cmds));
	memset(&gmm_ctx->cmds_payloads, 0,
			sizeof(gmm_ctx->cmds_payloads));
	gmm_ctx->cmds_cnt = 0;
	gmm_ctx->free_payload_pos = 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_diag_panel_add_cmd(char addr, int datalen, char *data)
{
	struct dsi_cmd_desc *dsc = NULL;
	char *payload = NULL;
	struct drm_gmm_kerl_ctx *gmm_ctx = &drm_diag_ctx.mdss_gmm_ctx;

	pr_debug("%s: addr=0x%02x, datalen=%d, data[0] = 0x%02x\n",
			__func__, addr, datalen, data[0]);

	if (gmm_ctx->cmds_cnt >= DRM_DIAG_DSI_MAX_CMDS_CNT) {
		pr_err("%s: failed to add: addr(0x%02x), datalen(%d),"
			" data[0](0x%02x) cmdcnt(%d)\n",
			__func__, addr, datalen, data[0],
			gmm_ctx->cmds_cnt);
		return false;
	}

	if (gmm_ctx->free_payload_pos + (datalen + 1)
		>= DRM_DIAG_DSI_PAYLOADS_LENGHTH) {
		pr_err("%s: failed to add: addr(0x%02x), datalen(%d), "
			"data[0](0x%02x) free_payload_cnt(%d)\n",
			__func__, addr, datalen, data[0],
			DRM_DIAG_DSI_PAYLOADS_LENGHTH
				 - gmm_ctx->free_payload_pos);
		return false;
	}

	payload = &gmm_ctx->cmds_payloads[gmm_ctx->free_payload_pos];
	*payload = addr;
	memcpy(payload + 1, data, datalen);

	dsc = &gmm_ctx->cmds[gmm_ctx->cmds_cnt];
	memset(dsc, 0, sizeof(*dsc));
	dsc->msg.type = drm_diag_get_wdtype_fromlen(datalen);
	dsc->msg.tx_buf = payload;
	dsc->msg.tx_len = (datalen + 1);
	dsc->msg.channel = 0;
	dsc->msg.flags = MIPI_DSI_MSG_USE_LPM;
	dsc->msg.ctrl = 0;
	dsc->post_wait_ms = 0;
	dsc->last_command = true;

	gmm_ctx->cmds_cnt++;
	gmm_ctx->free_payload_pos += (datalen + 1); /* addr + data */

	return true;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_panel_kickoff_cmds(struct dsi_display *pdisp)
{
	int ret = 0;
	struct dsi_cmd_desc *cmds;
	u32 count;
	struct drm_gmm_kerl_ctx *gmm_ctx = &drm_diag_ctx.mdss_gmm_ctx;

	pr_debug("%s: added_cmds=%d\n", __func__, gmm_ctx->cmds_cnt);
	if ((!gmm_ctx->cmds_cnt)
		|| (gmm_ctx->cmds_cnt > DRM_DIAG_DSI_MAX_CMDS_CNT))
		return 0;

	if (!pdisp) {
		pr_warn("%s: typ_gmm_cmds is not present\n", __func__);
		return -ENXIO;
	}

	cmds  = (struct dsi_cmd_desc*)gmm_ctx->cmds;
	count = gmm_ctx->cmds_cnt;

	if (count == 0) {
		pr_debug("No commands to be sent for state\n");
		return 0;
	}

	ret = drm_cmn_panel_cmds_transfer(pdisp, cmds, count);
	pr_debug("%s: out ret=%d\n", __func__, ret);
	return ret;
}
#endif /* DRM_DIAG_PANEL_GMM_VOLTAGE */

#ifdef DRM_DIAG_PANEL_GMM_VOLTAGE
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_panel_update_gmm_volt_pad(
				union mdp_gmm_volt *gmm_volt)
{
#ifdef USES_PANEL_RM69350
	drm_diag_rm69350_update_gmm_pad(&gmm_volt->rm69350);
	drm_diag_rm69350_update_volt_pad(&gmm_volt->rm69350);
#else
	drm_diag_rosetta_update_gmm_pad(&gmm_volt->rosetta);
	drm_diag_rosetta_update_volt_pad(&gmm_volt->rosetta);
#endif /* USES_PANEL_RM69350 */
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_panel_make_gmm_volt_cmds(void)
{
	int ret = 0;
	size_t paditemlen = 0;
	const struct drm_diag_pad_item *paditems = NULL;
#ifdef USES_PANEL_RM69350
	paditemlen = ARRAY_SIZE(drm_diag_rm69350_gmm_volt_pads);
	paditems = drm_diag_rm69350_gmm_volt_pads;
#else
	paditemlen = ARRAY_SIZE(drm_diag_rosetta_gmm_volt_pads);
	paditems = drm_diag_rosetta_gmm_volt_pads;
#endif /* USES_PANEL_RM69350 */
	ret = drm_diag_panel_make_gmm_volt_cmds_paditems(paditemlen, paditems);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_panel_read_paditems(struct dsi_display *pdisp,
				size_t paditemlen,
				const struct drm_diag_pad_item *paditems)
{
	int ret = 0;
	size_t paditempos = 0;
	char lastpage = 0;
	char rbuf[256];

	pr_debug("%s: in\n", __func__);
	if ((paditemlen == 0) || (!paditems)) {
		pr_err("%s: paditems error(len[%zu], paditems[0x%p])\n",
			__func__, paditemlen, paditems);
		return -EINVAL;
	}

	for (paditempos = 0; paditempos < paditemlen; paditempos++) {
		struct drm_diag_panel_pad *pad
			 = paditems[paditempos].pad;
		size_t padlen = paditems[paditempos].len;
		size_t padpos = 0;

		for (padpos = 0; padpos < padlen; padpos++) {
			/* send change page cmd, when page changes */
			if (lastpage != pad[padpos].page) {
				lastpage = pad[padpos].page;
				ret = drm_diag_panel_switch_panel_page(
					pdisp, lastpage);
				if (ret) {
					pr_err("%s: failed to change page "
						   "paditempos[%zu], "
						   "padpos[%zu],"
						   "page[0x%02x]\n",
						   __func__,
						   paditempos,
						   padpos,
						   lastpage);
					goto err_end;
				}
			}
			/* clear read buffer */
			memset(rbuf, 0, sizeof(rbuf));

			ret = drm_cmn_panel_dcs_read(pdisp,
						pad[padpos].addr,
						pad[padpos].len,
						rbuf);
			if (ret) {
				pr_err("%s: failed to read page(0x%02x), "
					   "addr(0x%02x)\n", __func__,
					   pad[padpos].page,
					   pad[padpos].addr);
				goto err_end;
			}
			memcpy(pad[padpos].data, rbuf,
					sizeof(char) * pad[padpos].len);
		}
	}

	// change page to default
	drm_diag_panel_switch_panel_page(pdisp, DRM_DIAG_DEFAULT_PAGE);

	ret = 0;

err_end:
	pr_debug("%s: out ret=%d\n", __func__, ret);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_panel_make_gmm_volt_cmds_paditems(size_t paditemlen,
				const struct drm_diag_pad_item *paditems)
{
	int ret = 0;
	size_t paditempos = 0;
	char lastpage = 0;
	char lastpagepayload[2]    = {DRM_DIAG_PAGE_ADDR, 0x00};
	bool added_all = true;

	pr_debug("%s: in\n", __func__);
	if ((paditemlen == 0) || (!paditems)) {
		pr_err("%s: paditems error(len[%zu], paditems[0x%p])\n",
			__func__, paditemlen, paditems);
		return -EINVAL;
	}

	for (paditempos = 0; paditempos < paditemlen; paditempos++) {
		struct drm_diag_panel_pad *pad
			 = paditems[paditempos].pad;
		size_t padlen = paditems[paditempos].len;
		size_t padpos = 0;

		for (padpos = 0; padpos < padlen; padpos++) {
			/* add change page cmd, when page changes */
			if (lastpage != pad[padpos].page) {
				lastpage = pad[padpos].page;
				lastpagepayload[1] = lastpage;
				added_all &= drm_diag_panel_add_cmd(
					lastpagepayload[0], 1,
					&lastpagepayload[1]);
			}

			/* add cmd (gmm or volt and other) */
			added_all &= drm_diag_panel_add_cmd(
					pad[padpos].addr, pad[padpos].len,
					pad[padpos].data);
		}
	}

	if (!added_all) {
		pr_err("%s: failed to add dsicmd\n", __func__);
		ret = -EINVAL;
	}
	pr_debug("%s: out ret = %d, added_all=%d\n", __func__, ret, added_all);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_panel_read_gmm_volt(struct dsi_display *pdisp)
{
	int ret = 0;
	size_t paditemlen = 0;
	const struct drm_diag_pad_item *paditems = NULL;

#ifdef USES_PANEL_RM69350
	paditemlen = ARRAY_SIZE(drm_diag_rm69350_gmm_volt_pads);
	paditems = drm_diag_rm69350_gmm_volt_pads;
#else
	paditemlen = ARRAY_SIZE(drm_diag_rosetta_gmm_volt_pads);
	paditems = drm_diag_rosetta_gmm_volt_pads;
#endif /* USES_PANEL_RM69350 */

	ret = drm_diag_panel_read_paditems(pdisp, paditemlen, paditems);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_panel_read_gmm_volt_dispoff(struct dsi_display *pdisp)
{
	int ret = 0;

	drm_det_pre_panel_off();

#if defined(DRM_DIAG_PANEL_VIDEO_MODE)
	drm_cmn_video_transfer_ctrl(false);
#endif  /* DRM_DIAG_PANEL_VIDEO_MODE */
	drm_diag_panel_switch_panel_page(pdisp, DRM_DIAG_DEFAULT_PAGE);
	drm_cmn_panel_dcs_write0(pdisp, 0x28);
#if defined(DRM_DIAG_PANEL_VIDEO_MODE)
	drm_cmn_start_video();
	usleep_range( DRM_DIAG_WAIT_1FRAME_US, DRM_DIAG_WAIT_1FRAME_US);
	drm_cmn_stop_video();
#else  /* DRM_DIAG_PANEL_VIDEO_MODE */
	usleep_range( DRM_DIAG_WAIT_1FRAME_US, DRM_DIAG_WAIT_1FRAME_US);
#endif  /* DRM_DIAG_PANEL_VIDEO_MODE */

	drm_diag_panel_read_gmm_volt(pdisp);

	drm_cmn_panel_dcs_write0(pdisp, 0x29);
#if defined(CONFIG_SHARP_DRM_HR_VID)
	drm_cmn_video_transfer_ctrl(true);
#endif  /* CONFIG_SHARP_DRM_HR_VID */

	drm_det_post_panel_on();
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_panel_copy_gmm_volt_from_pad(
				struct mdp_gmm_volt_info *gmm_volt_info)
{
#ifdef USES_PANEL_RM69350
	drm_diag_rm69350_copy_gmm_volt_from_pad(
					&gmm_volt_info->gmm_volt.rm69350);
#else
	drm_diag_rosetta_copy_gmm_volt_from_pad(
					&gmm_volt_info->gmm_volt.rosetta);
#endif /* USES_PANEL_RM69350 */
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_set_panel_typ_volt(struct dsi_display *pdisp)
{
	int ret = 0;
	struct dsi_cmd_desc *cmds;
	u32 count;
//	enum dsi_cmd_set_state state;

	pr_debug("%s: send typ volt\n", __func__);
	if (!pdisp) {
		pr_warn("%s: typ_volt_cmds is not present\n", __func__);
		return -ENXIO;
	}

	cmds  = pdisp->panel->volt_cmds.panel_typ_cmds;
	count = pdisp->panel->volt_cmds.count;
//	state = pdisp->panel->volt_cmds.state;

	if (count == 0) {
		pr_debug("No commands to be sent for state\n");
		return 0;
	}

	ret = drm_cmn_panel_cmds_transfer(pdisp, cmds, count);

	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */

static int drm_diag_set_panel_typ_gmm(struct dsi_display *pdisp)
{
	int ret = 0;
	struct dsi_cmd_desc *cmds;
	u32 count;
//	enum dsi_cmd_set_state state;

	pr_debug("%s: send typ gmm\n", __func__);
	if (!pdisp) {
		pr_warn("%s: typ_gmm_cmds is not present\n", __func__);
		return -ENXIO;
	}

	cmds  = pdisp->panel->gmm_cmds.panel_typ_cmds;
	count = pdisp->panel->gmm_cmds.count;
//	state = pdisp->panel->gmm_cmds.state;

	if (count == 0) {
		pr_debug("No commands to be sent for state\n");
		return 0;
	}

	ret = drm_cmn_panel_cmds_transfer(pdisp, cmds, count);

	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */

static int drm_diag_set_panel_gmm(struct dsi_display *pdisp)
{
	int ret = 0;
	struct drm_gmm_kerl_ctx *gmm_ctx = &drm_diag_ctx.mdss_gmm_ctx;

	if (!pdisp) {
		pr_err("%s: dsi_display is NULL.\n", __func__);
		return -ENXIO;
	}

	// gmm is adjusted
	if (gmm_ctx->gmm_adj_status == DRM_GMM_ADJ_STATUS_OK) {
		pr_debug("%s: build and send adjusted gmm\n", __func__);
		/* update pad from adjusted gmm */
		drm_diag_panel_update_gmm_volt_pad(
			&gmm_ctx->gmm_volt_adjusted);

		/* refresh dsi cmds buffer */
		drm_diag_panel_clear_cmds();

		/* build dsi cmds from paddata(gmm/volt/other) */
		ret = drm_diag_panel_make_gmm_volt_cmds();
		if (ret) {
			return ret;
		}

		/* send gmm/volt/other cmds to panel H/W */
		ret = drm_diag_panel_kickoff_cmds(pdisp);
	// gmm isn't adjust
	} else {
		return ret;
	}

	if (ret > 0) {
		ret = 0;
	} else if (ret < 0) {
		pr_err("%s: failed to send gmm adj_staus=0x%02x (%d)\n",
			__func__, gmm_ctx->gmm_adj_status, ret);
	}
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */

static int drm_diag_panel_set_adjust_gmm(struct dsi_display *pdisp,
			struct mdp_gmm_volt_info *gmm_volt_info)
{
	int ret = 0;
	struct drm_gmm_kerl_ctx *gmm_ctx = &drm_diag_ctx.mdss_gmm_ctx;

	/* convert and store gmm/volt/other to paddata */
	drm_diag_panel_update_gmm_volt_pad(&gmm_volt_info->gmm_volt);

	/* refresh dsi cmds buffer */
	drm_diag_panel_clear_cmds();

	/* build dsi cmds from paddata(gmm/volt/other) */
	ret = drm_diag_panel_make_gmm_volt_cmds();
	if (ret) {
		return ret;
	}

	/* send gmm/volt/other cmds to panel H/W */
#if defined(DRM_DIAG_PANEL_VIDEO_MODE)
	drm_cmn_video_transfer_ctrl(false);
	ret = drm_diag_panel_kickoff_cmds(pdisp);
	drm_cmn_video_transfer_ctrl(true);
#else   /* DRM_DIAG_PANEL_VIDEO_MODE */
	ret = drm_diag_panel_kickoff_cmds(pdisp);
#endif  /* DRM_DIAG_PANEL_VIDEO_MODE */
	if (ret > 0) {
		ret = 0;
	}

	drm_diag_panel_switch_panel_page(pdisp, DRM_DIAG_DEFAULT_PAGE);

	/* copy adjusted gmm/volt/other for panel on cmds */
	memcpy(&gmm_ctx->gmm_volt_adjusted,
		&gmm_volt_info->gmm_volt,
		sizeof(gmm_ctx->gmm_volt_adjusted));
	gmm_ctx->gmm_adj_status = DRM_GMM_ADJ_STATUS_OK;

	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */

static int drm_diag_panel_set_unadjust_gmm(struct dsi_display *pdisp,
			struct mdp_gmm_volt_info *gmm_volt_info)
{
	int ret = 0;
#if defined(DRM_DIAG_PANEL_VIDEO_MODE)
	drm_cmn_video_transfer_ctrl(false);
#endif  /* DRM_DIAG_PANEL_VIDEO_MODE */
	drm_diag_ctx.mdss_gmm_ctx.gmm_adj_status = DRM_GMM_ADJ_STATUS_NOT_SET;
	ret = drm_diag_set_panel_typ_volt(pdisp);
	if (ret < 0) {
		goto error;
	}
	ret = drm_diag_set_panel_typ_gmm(pdisp);
	if (ret < 0) {
		goto error;
	}
	ret = 0;
error:
	drm_diag_panel_switch_panel_page(pdisp, DRM_DIAG_DEFAULT_PAGE);
#if defined(DRM_DIAG_PANEL_VIDEO_MODE)
	drm_cmn_video_transfer_ctrl(true);
#endif  /* DRM_DIAG_PANEL_VIDEO_MODE */
	return ret;
}

#ifdef USES_PANEL_RM69350
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_rm69350_copy_gmm_volt_from_pad(
				struct rm69350_gmm_volt *gmm_volt)
{
	size_t cnt = 0;

	pr_debug("%s: in\n", __func__);

	/* copy gmmR from paddata */
	drm_diag_panel_copy_gmm_from_paddata(
			ARRAY_SIZE(gmm_volt->gmmR),
			gmm_volt->gmmR,
			drm_diag_rm69350_gmm_R_data);

	/* copy gmmG from paddata */
	drm_diag_panel_copy_gmm_from_paddata(
			ARRAY_SIZE(gmm_volt->gmmG),
			gmm_volt->gmmG,
			drm_diag_rm69350_gmm_G_data);

	/* copy gmmB from paddata */
	drm_diag_panel_copy_gmm_from_paddata(
			ARRAY_SIZE(gmm_volt->gmmB),
			gmm_volt->gmmB,
			drm_diag_rm69350_gmm_B_data);

	// copy voltage from paddata
	gmm_volt->vglr    = drm_diag_rm69350_volt_data[cnt++];
	gmm_volt->vghr    = drm_diag_rm69350_volt_data[cnt++];
	gmm_volt->vgsp   = drm_diag_rm69350_volt_data[cnt++];
	gmm_volt->vgmp1   = drm_diag_rm69350_volt_data[cnt++];
	gmm_volt->vgmp2 = drm_diag_rm69350_volt_data[cnt++];

	pr_debug("%s: out\n", __func__);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_rm69350_update_gmm_pad(
				struct rm69350_gmm_volt *gmm_volt)
{
	pr_debug("%s: in\n", __func__);

	/* update gmmR */
	drm_diag_panel_copy_gmm_to_paddata(
			ARRAY_SIZE(drm_diag_rm69350_gmm_R_data),
			drm_diag_rm69350_gmm_R_data,
			gmm_volt->gmmR);

	/* update gmmG */
	drm_diag_panel_copy_gmm_to_paddata(
			ARRAY_SIZE(drm_diag_rm69350_gmm_G_data),
			drm_diag_rm69350_gmm_G_data,
			gmm_volt->gmmG);

	/* update gmmB */
	drm_diag_panel_copy_gmm_to_paddata(
			ARRAY_SIZE(drm_diag_rm69350_gmm_B_data),
			drm_diag_rm69350_gmm_B_data,
			gmm_volt->gmmB);

	pr_debug("%s: out\n", __func__);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_rm69350_update_volt_pad(
				struct rm69350_gmm_volt *gmm_volt)
{
	int cnt = 0;
	pr_debug("%s: in\n", __func__);

	/* update voltage */
	drm_diag_rm69350_volt_data[cnt++] = gmm_volt->vglr;
	drm_diag_rm69350_volt_data[cnt++] = gmm_volt->vghr;
	drm_diag_rm69350_volt_data[cnt++] = gmm_volt->vgsp;
	drm_diag_rm69350_volt_data[cnt++] = gmm_volt->vgmp1;
	drm_diag_rm69350_volt_data[cnt++] = gmm_volt->vgmp2;

	pr_debug("%s: out\n", __func__);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_init_rm69350_gmm_param(
				struct drm_gmmvolt_ctx *gmmvolt_ctx)
{
	int ret = 0;
	struct drm_gmm_kerl_ctx *gmm_ctx = &drm_diag_ctx.mdss_gmm_ctx;

	gmm_ctx->gmm_adj_status = gmmvolt_ctx->status;
	memcpy(&gmm_ctx->gmm_volt_adjusted.rm69350,
		&gmmvolt_ctx->gmm_volt.rm69350,
		sizeof(gmm_ctx->gmm_volt_adjusted.rm69350));

	return ret;
}
#else /* USES_PANEL_RM69350 */
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_rosetta_copy_gmm_volt_from_pad(
				struct rosetta_gmm_volt *gmm_volt)
{
	size_t cnt = 0;

	pr_debug("%s: in\n", __func__);

	/* copy gmmR from paddata */
	drm_diag_panel_copy_gmm_from_paddata(
			ARRAY_SIZE(gmm_volt->gmmR),
			gmm_volt->gmmR,
			drm_diag_rosetta_gmm_R_data);

	/* copy gmmG from paddata */
	drm_diag_panel_copy_gmm_from_paddata(
			ARRAY_SIZE(gmm_volt->gmmG),
			gmm_volt->gmmG,
			drm_diag_rosetta_gmm_G_data);

	/* copy gmmB from paddata */
	drm_diag_panel_copy_gmm_from_paddata(
			ARRAY_SIZE(gmm_volt->gmmB),
			gmm_volt->gmmB,
			drm_diag_rosetta_gmm_B_data);

	// copy voltage from paddata
	gmm_volt->vgh    = drm_diag_rosetta_volt_data[cnt++];
	gmm_volt->vgl    = drm_diag_rosetta_volt_data[cnt++];
	gmm_volt->vgho   = drm_diag_rosetta_volt_data[cnt++];
	gmm_volt->vglo   = drm_diag_rosetta_volt_data[cnt++];
	gmm_volt->gvddp2 = drm_diag_rosetta_volt_data[cnt++];
	gmm_volt->gvddp  = drm_diag_rosetta_volt_data[cnt++];
	gmm_volt->gvddn  = drm_diag_rosetta_volt_data[cnt++];

	pr_debug("%s: out\n", __func__);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_rosetta_update_gmm_pad(
				struct rosetta_gmm_volt *gmm_volt)
{
	pr_debug("%s: in\n", __func__);

	/* update gmmR */
	drm_diag_panel_copy_gmm_to_paddata(
			ARRAY_SIZE(drm_diag_rosetta_gmm_R_data),
			drm_diag_rosetta_gmm_R_data,
			gmm_volt->gmmR);

	/* update gmmG */
	drm_diag_panel_copy_gmm_to_paddata(
			ARRAY_SIZE(drm_diag_rosetta_gmm_G_data),
			drm_diag_rosetta_gmm_G_data,
			gmm_volt->gmmG);

	/* update gmmB */
	drm_diag_panel_copy_gmm_to_paddata(
			ARRAY_SIZE(drm_diag_rosetta_gmm_B_data),
			drm_diag_rosetta_gmm_B_data,
			gmm_volt->gmmB);

	pr_debug("%s: out\n", __func__);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_diag_rosetta_update_volt_pad(
				struct rosetta_gmm_volt *gmm_volt)
{
	int cnt = 0;
	pr_debug("%s: in\n", __func__);

	/* update voltage */
	drm_diag_rosetta_volt_data[cnt++] = gmm_volt->vgh;
	drm_diag_rosetta_volt_data[cnt++] = gmm_volt->vgl;
	drm_diag_rosetta_volt_data[cnt++] = gmm_volt->vgho;
	drm_diag_rosetta_volt_data[cnt++] = gmm_volt->vglo;
	drm_diag_rosetta_volt_data[cnt++] = gmm_volt->gvddp2;
	drm_diag_rosetta_volt_data[cnt++] = gmm_volt->gvddp;
	drm_diag_rosetta_volt_data[cnt++] = gmm_volt->gvddn;

	pr_debug("%s: out\n", __func__);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_init_rosetta_gmm_param(
				struct drm_gmmvolt_ctx *gmmvolt_ctx)
{
	int ret = 0;
	struct drm_gmm_kerl_ctx *gmm_ctx = &drm_diag_ctx.mdss_gmm_ctx;

	gmm_ctx->gmm_adj_status = gmmvolt_ctx->status;
	memcpy(&gmm_ctx->gmm_volt_adjusted.rosetta,
		&gmmvolt_ctx->gmm_volt.rosetta,
		sizeof(gmm_ctx->gmm_volt_adjusted.rosetta));

	return ret;
}
#endif /* USES_PANEL_RM69350 */

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_diag_init_gmm_param(struct drm_gmmvolt_ctx *gmmvolt_ctx)
{
	int ret = 0;

	if (gmmvolt_ctx->status != DRM_GMM_ADJ_STATUS_OK) {
		pr_debug("%s: gmm is not adjusted\n", __func__);
		return ret;
	}
#ifdef USES_PANEL_RM69350
	drm_diag_init_rm69350_gmm_param(gmmvolt_ctx);
#else /* USES_PANEL_RM69350 */
	drm_diag_init_rosetta_gmm_param(gmmvolt_ctx);
#endif /* USES_PANEL_RM69350 */
	return ret;
}
#endif /* DRM_DIAG_PANEL_GMM_VOLTAGE */

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_diag_panel_set_gmm(struct mdp_gmm_volt_info *gmm_volt_info)
{
	int ret = 0;
#ifdef DRM_DIAG_PANEL_GMM_VOLTAGE
	struct dsi_display *pdisp = NULL;
	pr_debug("%s: in\n", __func__);

	if (!gmm_volt_info) {
		pr_err("%s: argment err gmm_volt_info(0x%p)\n",
			__func__, gmm_volt_info);
		return -EINVAL;
	}

	pdisp = msm_drm_get_dsi_displey();
	if(!pdisp) {
		pr_err("%s: dsi_displey err\n", __func__);
		return -EINVAL;
	}

	ret = drm_diag_check_panel_type(
				gmm_volt_info->panel_type);
	if (ret) {
		return ret;
	}

	switch(gmm_volt_info->request) {
	case DRM_GMMVOLT_REQ_ADJUST:
		ret = drm_diag_panel_set_adjust_gmm(
					pdisp, gmm_volt_info);
		break;
	case DRM_GMMVOLT_REQ_UNADJUST:
		ret = drm_diag_panel_set_unadjust_gmm(
					pdisp, gmm_volt_info);
		break;
	default:
		pr_err("%s: unsupported request(%d)\n",
			__func__, gmm_volt_info->request);
		return -EINVAL;
	}

#endif /* DRM_DIAG_PANEL_GMM_VOLTAGE */
	pr_debug("%s: out ret=%d\n", __func__, ret);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_diag_panel_get_gmm(struct mdp_gmm_volt_info *gmm_volt_info)
{
	int ret = 0;
#ifdef DRM_DIAG_PANEL_GMM_VOLTAGE
	struct dsi_display *pdisp = NULL;
	pr_debug("%s: in\n", __func__);

	if (!gmm_volt_info) {
		pr_err("%s: argment err gmm_volt_info(0x%p)\n",
			__func__, gmm_volt_info);
		return -EINVAL;
	}

	pdisp = msm_drm_get_dsi_displey();
	if(!pdisp) {
		pr_err("%s: dsi_displey err\n", __func__);
		return -EINVAL;
	}

	ret = drm_diag_check_panel_type(
				gmm_volt_info->panel_type);
	if (ret) {
		return ret;
	}

	ret = drm_diag_panel_read_gmm_volt_dispoff(pdisp);
	if (ret) {
		return ret;
	}

	/* copy read to gmm_volt */
	drm_diag_panel_copy_gmm_volt_from_pad(gmm_volt_info);

#endif /* DRM_DIAG_PANEL_GMM_VOLTAGE */
	pr_debug("%s: out ret=%d\n", __func__, ret);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
#if defined(DRM_DIAG_PANEL_GMM_VOLTAGE) || defined(DRM_DIAG_PANEL_FLICKER)
static int drm_diag_init_adjustable_params(
				struct shdisp_boot_context *shdisp_boot_ctx)
{
	int ret = 0;
#ifdef DRM_DIAG_PANEL_FLICKER
	drm_diag_init_flicker_param(&shdisp_boot_ctx->flicker_ctx);
#endif /* DRM_DIAG_PANEL_FLICKER */
#ifdef DRM_DIAG_PANEL_GMM_VOLTAGE
	drm_diag_init_gmm_param(&shdisp_boot_ctx->gmmvolt_ctx);
#endif /* DRM_DIAG_PANEL_GMM_VOLTAGE */
	return ret;
}
#endif /* defined(DRM_DIAG_PANEL_GMM_VOLTAGE) || defined(DRM_DIAG_PANEL_FLICKER) */

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_diag_init(void)
{
	int ret = 0;
#if defined(DRM_DIAG_PANEL_GMM_VOLTAGE) || defined(DRM_DIAG_PANEL_FLICKER)
	struct shdisp_boot_context *shdisp_boot_ctx = NULL;
	sharp_smem_common_type * smem = NULL;
#endif /* defined(DRM_DIAG_PANEL_GMM_VOLTAGE) || defined(DRM_DIAG_PANEL_FLICKER) */

	drm_diag_init_mutex();

#if defined(DRM_DIAG_PANEL_GMM_VOLTAGE) || defined(DRM_DIAG_PANEL_FLICKER)
	smem = sh_smem_get_common_address();

	if (smem == NULL) {
		pr_err("%s: failed to "
			"sh_smem_get_common_address()\n", __func__);
		return ret;
	}

	shdisp_boot_ctx = (struct shdisp_boot_context*)smem->shdisp_data_buf;

	drm_debug = smem->shdiag_debugflg;

	drm_diag_init_adjustable_params(shdisp_boot_ctx);
#endif /* defined(DRM_DIAG_PANEL_GMM_VOLTAGE) || defined(DRM_DIAG_PANEL_FLICKER) */
	return ret;
}
