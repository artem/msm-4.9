/* drivers/gpu/drm/msm/sharp/drm_mfr.c  (Display Driver)
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
#define pr_fmt(fmt)	"drm_mfr:[%s] " fmt, __func__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/completion.h>

#include "drm_cmn.h"
#include "drm_mfr.h"
#include "drm_mfr_pwrsave.h"
#include <drm/drmP.h>
#include <drm/sharp_drm.h>
#include "../sde/sde_hw_intf.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define DRMMFR_DBG(fmt, ...) DRM_DEBUG_MFR(fmt, ##__VA_ARGS__)
#define DRMMFR_ERR(fmt, ...) pr_err(fmt"\n", ##__VA_ARGS__)

// mfr-control and state-change controll by only swt...
// #define DRMMFR_ONLYSWT

#define DRMMFR_TRANSIT_PREPARE_TICKS (3)
#define COMMON_VIDEO_FRAMERATE (60)
#define COMMON_WAIT_FOR_VID_START_MS (84)
#define DRMMFR_1SEC_NS (1000*1000*1000)
#define DRMMFR_VBLANK_AREA (3041)	/* vdisplay(3040) + 1 */
#define DRMMFR_STOP_WARNING_AREA (2400)	/* vdisplay(3040) * 4 / 5 */

enum swt_state {
	SWT_IDLE,           // stopped
	SWT_RUNNING,        // started
	SWT_EXPIRED,        // expired
	SWT_CANCEL_PENDING, // after failed to cancel_timer
};

static char *state_str[] = {
	"POWER_ON",
	"UPDATE_FRAME",
	"BLANK_FRAME",
	"REPEAT_FRAME",
	"WAIT_FRAME",
	"HR_VIDEO_WAIT",
	"HR_VIDEO_PREPARE",
	"HR_VIDEO_REFRESH",
	"HR_VIDEO_MFR_WAIT",
	"HR_VIDEO_MFR_REFRESH",
	"HR_VIDEO_MFR_MAX",
	"SUSPEND",
	"COMMIT",
	"PRE_COMMIT",
	"POWER_OFF",
	// "WAIT_CHG_VIDEO_FR",
};

enum drm_mfr_hr_video_state {
	POWER_ON,
	UPDATE_FRAME,
	BLANK_FRAME,
	REPEAT_FRAME,
	WAIT_FRAME,
	HR_VIDEO_WAIT,
	HR_VIDEO_PREPARE,
	HR_VIDEO_REFRESH,
	HR_VIDEO_MFR_WAIT,
	HR_VIDEO_MFR_REFRESH,
	HR_VIDEO_MFR_MAX,
	SUSPEND,
	COMMIT,
	PRE_COMMIT,
	POWER_OFF,
	// WAIT_CHG_VIDEO_FR,
};

static char *vid_output_state_str[] = {
	"VID_OUTPUT_STOPPED",
	"VID_OUTPUT_STARTED",
	"VID_OUTPUT_STOPPING",
};

enum drm_mfr_vid_output_state {
	VID_OUTPUT_STOPPED,     // after stopped video-output.
	VID_OUTPUT_STARTED,     // after requested start video-output
	VID_OUTPUT_STOPPING,    // after requested stop  video-output
};

static char *vsync_type_str[] = {
	"VSYNC_TYPE_HW",
	"VSYNC_TYPE_SW",
	"VSYNC_TYPE_NOTVSYNC",
};

enum drm_mfr_vsync_type {
	VSYNC_TYPE_HW,
	VSYNC_TYPE_SW,
	VSYNC_TYPE_NOTVSYNC,
};

static char *pol_type_str[] = {
	"POL_STATE_INIT",
	"POL_STATE_POS",
	"POL_STATE_NEG",
};

enum drm_mfr_hr_pol_state {
	POL_STATE_INIT,
	POL_STATE_POS,
	POL_STATE_NEG,
};

/* ------------------------------------------------------------------------- */
/* STRUCTURE                                                                 */
/* ------------------------------------------------------------------------- */
struct drm_spt_ctx {
	int mfr_spt_len;
	int *mfr_spt_values;
};

struct swtimer {
	int state;
	u64 base;
	u64 expire;
	struct hrtimer hrt;
};

struct drm_vid_ctx {
	int vid_rate;       // video rate
	int mfr_rate;       // video control rate
	int mfr_wait_ticks;
	bool will_commit;
	enum drm_mfr_hr_video_state  cur_vid_state;
	int cur_vid_ticks;
	enum drm_mfr_vid_output_state vid_output_state;
	bool video_start_byswvsync;

	int cont_commit_ticks;
	bool keep_enabled_rsc;

	struct drm_mfr_ctx *mfr_ctx;
	const struct drm_mfr_controller *mfr_ctrl;

	// polarity parameters
	enum drm_mfr_hr_pol_state pol_state;
	int pol_sum;
	ktime_t pol_checked_ktime; // = last hw-vsync ktime
	bool unbalanced_polarity;

	int clocks_refcount;
	int vblank_refcount;
	int underrun_refcount;

	ktime_t last_expectvsynctype_v_ktime;
	bool long_swvsync;
	ktime_t long_swvsync_base_ktime;

	bool request_stop_vid_on_next_vsync; // flag for stop-video on vsync
					     // set true for it when
					     // preswvsync can't stop video..

	int restart_vid_counter;
	int vtotal;

	bool wait_for_video_start;
	struct completion video_started_comp;
};


struct drm_swt_ctx {
	bool enabled_display;             // display was enabled
	bool finish_1st_swvsync_restart;
	int swvsync_rate;

	struct swtimer prevsync_timer;
	struct swtimer swvsync_timer;

	bool reqeust_vsync_by_user;

	spinlock_t vsync_lock;

	const int *cpvid_rate;
	const int *cpmfr_rate;	// reference for drm_vid_ctx.mfr_rate
				// sharing current-mfr-mode value.

	int adjust_vidstart_svvsync_us;

	struct drm_mfr_ctx *mfr_ctx;
	const struct drm_mfr_controller *mfr_ctrl;
};

struct drm_mfr_ctx {
	bool controllable;
	int framerate;
	const struct drm_mfr_controller *mfr_ctrl;
	struct drm_swt_ctx *swt_ctx;
	struct drm_vid_ctx *vid_ctx;
	struct drm_spt_ctx *spt_ctx;
	struct drm_psv_ctx *psv_ctx;

	spinlock_t mfr_lock;
	spinlock_t psv_lock;
	bool vsync_is_requested;      // enabled by vendor_drm
	bool underrun_is_requested;   // enabled by vendor_drm

	int old_drm_mfr_mfrate;
};
// spin_lock priority
// -- outside--
// mfr_lock    (mfr-ctrl-lock)
//  vsync_lock (swt-lock)
//   psv_lock  (psv-lock)
// -- inside --

struct drm_mfr_vid_stat_handler {
	int (*prev_handler)(const struct drm_mfr_vid_stat_handler *sh,
		struct drm_vid_ctx *, enum drm_mfr_vsync_type);
	int (*v_handler)(const struct drm_mfr_vid_stat_handler *sh,
		struct drm_vid_ctx *, enum drm_mfr_vsync_type);
	int ticks_for_nextstate;
};

/* ------------------------------------------------------------------------- */
/* FUNCTION                                                                  */
/* ------------------------------------------------------------------------- */
static int drm_mfr_create_context(void);
static void drm_mfr_destroy_context(void);
static int drm_mfr_create_swt_ctx(struct drm_swt_ctx **ppswt_ctx,
				struct drm_mfr_ctx *mfr_ctx,
				const int *cpvid_rate, const int *cpmfr_rate);
static void drm_mfr_destroy_swt_ctx(struct drm_swt_ctx *ppswt_ctx);
static int drm_mfr_create_spt_ctx(struct drm_spt_ctx **ppspt_ctx);
static void drm_mfr_destroy_spt_ctx(struct drm_spt_ctx *spt_ctx);
static int drm_mfr_bind(void);
static void drm_mfr_unbind(void);

static void drm_mfr_swt_panel_power(struct drm_swt_ctx *swt_ctx,
				bool en);
static int drm_mfr_pre_hw_vsync_cb(struct drm_mfr_ctx *mfr_ctx);
static int drm_mfr_post_hw_vsync_cb(struct drm_mfr_ctx *mfr_ctx);

static bool drm_mfr_validate_interface(
				const struct drm_mfr_controller *mfr_ctrl);

static void drm_mfr_swt_prevsync_ctrl(struct drm_swt_ctx *swt_ctx,
					bool en);
static void drm_mfr_swt_swvsync_ctrl(struct drm_swt_ctx *swt_ctx,
					bool en);
static bool drm_mfr_swt_stop_long_swvsync_ifrunning(
						struct drm_mfr_ctx *mfr_ctx);
static void drm_mfr_swt_stop_long_swvsync_and_restart(
						struct drm_mfr_ctx *mfr_ctx);
static int drm_mfr_swt_calc_hwvsync_ns(struct drm_swt_ctx *swt_ctx);
static int drm_mfr_swt_calc_swvsync_ns(struct drm_swt_ctx *swt_ctx);

static int drm_mfr_swt_calc_1st_swvsync_ns(struct drm_swt_ctx *swt_ctx,
						ktime_t base_vsync_ktime);
static int drm_mfr_swt_calc_next_swvsync(struct drm_swt_ctx *swt_ctx,
						ktime_t base_vsync_ktime,
						bool finish_restart_1st);
static int drm_mfr_swt_calc_restart_swvsync_ns(
						struct drm_swt_ctx *swt_ctx,
						ktime_t base_vsync_ktime,
						bool finish_restart_1st);
static int drm_mfr_swt_calc_to_hr_video_wait_end_swvsync_ns(
						struct drm_swt_ctx *swt_ctx,
						ktime_t base_vsync_ktime,
						bool finish_restart_1st);

// this is timing of video start / stop
static enum hrtimer_restart drm_mfr_swt_prevsync_cb(struct hrtimer *hrtvsync);

// this is sw-vsync hanlder, it is used in hw-vsync is stopped
static enum hrtimer_restart drm_mfr_swt_swvsync_cb(struct hrtimer *hrtvsync);

static int drm_mfr_vid_restart_video_and_wait(struct drm_mfr_ctx *mfr_ctx,
				int restart_vid/*,
				int cont_commit_frame_count*/);
static int drm_mfr_vid_wakeup_waiting_video(struct drm_vid_ctx *vid_ctx);

/* pre-vsync handler **/
static int drm_mfr_vid_state_pvhandler_updateframe(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);
static int drm_mfr_vid_state_pvhandler_repeatframe(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);
static int drm_mfr_vid_state_pvhandler_hr_video_refresh(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);
static int drm_mfr_vid_state_pvhandler_hr_video_mfr_refresh(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);
static int drm_mfr_vid_state_vhandler_suspend(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);


/** sw/hw vsync handler **/
static int drm_mfr_vid_state_vhandler_poweron(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);
static int drm_mfr_vid_state_vhandler_blankframe(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);
static int drm_mfr_vid_state_vhandler_hr_video_prepare(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);
static int drm_mfr_vid_state_vhandler_hr_video_mfr_wait(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);
static int drm_mfr_vid_state_vhandler_commit(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);
static int drm_mfr_vid_state_vhandler_pre_commit(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);

static void drm_mfr_vid_add_ticks(struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);
static void drm_mfr_vid_handle_onvsync_cmn(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);
static void drm_mfr_vid_cur_vid_state_prevhandler(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);
static void drm_mfr_vid_cur_vid_state_vhandler(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);

static int drm_mfr_vid_vblank_virt_callback(struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);

static enum drm_mfr_hr_video_state drm_mfr_get_nextstate_fromcommit(
						struct drm_vid_ctx *vid_ctx);
static void drm_mfr_update_mfr_wait_ticks_ifneeded(struct drm_vid_ctx *vid_ctx);

static int drm_mfr_vid_update_state_onvsync(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype);

static void drm_mfr_vid_init_polarity_state(struct drm_vid_ctx *vid_ctx);
static void drm_mfr_vid_update_polarity_onhwvsync(struct drm_vid_ctx *vid_ctx,
			enum drm_mfr_vid_output_state last_vid_output_state);
static bool drm_mfr_vid_need_reverse_polarity(struct drm_vid_ctx *vid_ctx);

static bool drm_mfr_vid_enable_output_video(struct drm_vid_ctx *vid_ctx,
						bool en);
static bool drm_mfr_vid_safe_enable_output_video(struct drm_vid_ctx *vid_ctx);
static void drm_mfr_vid_failsafe_enable_output_video(
						struct drm_vid_ctx *vid_ctx);
static bool drm_mfr_vid_keep_enabled_rsc(struct drm_vid_ctx *vid_ctx);
static bool drm_mfr_vid_set_keep_enabled_rsc(struct drm_vid_ctx *vid_ctx,
								bool transfer);
static int drm_mfr_vid_enable_clocks(struct drm_vid_ctx *vid_ctx,
						bool en, bool needsync);
static int drm_mfr_vid_enable_hwvsync(struct drm_vid_ctx *vid_ctx,
						bool en, bool need_sync);
static int drm_mfr_vid_enable_underrun(struct drm_vid_ctx *vid_ctx,
						bool en, bool needsync);
static void drm_mfr_vid_enable_rsc(struct drm_vid_ctx *vid_ctx, bool needsync);
static void drm_mfr_vid_disable_rsc_onswvsync_ifneeded(
				const struct drm_mfr_vid_stat_handler *sh,
						struct drm_vid_ctx *vid_ctx);
static bool drm_mfr_vid_can_use_long_swvsync(struct drm_vid_ctx *vid_ctx);

static enum drm_mfr_vid_output_state
		drm_mfr_vid_update_vid_output_status_on_vsync(
					struct drm_vid_ctx *vid_ctx);

static const struct drm_mfr_vid_stat_handler *
	drm_mfr_vid_get_target_state_handler(
				enum drm_mfr_hr_video_state cur_vid_state);

static bool drm_mfr_vid_update_video_start_byswvsync(
			struct drm_vid_ctx *vid_ctx,
			enum drm_mfr_vid_output_state last_vid_ouput_state);

static bool drm_mfr_vid_clear_video_start_byswvsync(
						struct drm_vid_ctx *vid_ctx);

static void drm_mfr_vid_update_last_expectvsynctype_v_ktime(
			struct drm_vid_ctx *vid_ctx, ktime_t vsync_time);
static ktime_t drm_mfr_vid_get_last_expectvsynctype_v_ktime(
						struct drm_vid_ctx *vid_ctx);

static bool drm_mfr_vid_can_stop_video_onprevsync(
						struct drm_vid_ctx *vid_ctx);
static int drm_mfr_vid_get_restart_vid_ticks(struct drm_vid_ctx *vid_ctx);
static bool drm_mfr_vid_can_disable_rsc_onswvsync(
				const struct drm_mfr_vid_stat_handler *sh,
						struct drm_vid_ctx *vid_ctx);
static bool drm_mfr_vid_is_skip_swvsync_handling(
			struct drm_vid_ctx *vid_ctx,
			enum drm_mfr_vid_output_state last_vid_ouput_state);
static bool drm_mfr_vid_is_restartvideo_nextswvsync(
					struct drm_vid_ctx *vid_ctx);
static bool drm_mfr_vid_need_video_stop_state(
				enum drm_mfr_hr_video_state cur_vid_state);
static void drm_mfr_vid_update_mfr_params(struct drm_vid_ctx *vid_ctx);
static int drm_mfr_calcrate_mfrrate(int mfr);
static bool drm_mfr_spt_supported_mfr_rate(struct drm_spt_ctx *spt_ctx,
								int mfr_rate);

/* ------------------------------------------------------------------------- */
/* EXTERNAL FUNCTION                                                         */
/* ------------------------------------------------------------------------- */


/* ------------------------------------------------------------------------- */
/* CONTEXT                                                                   */
/* ------------------------------------------------------------------------- */
static struct drm_mfr_ctx *mfr_context = NULL;

static int drm_mfr_swvsync_rate = 100;          //100, 120
static int drm_mfr_videorate = 100;             //120, 110
static int drm_mfr_mfrate = 100;
static int drm_mfr_prevsync_t_offset_100us = 20;// 10 * 100us = 2msec
static int drm_mfr_ctrl_clocks = 0;
static int drm_mfr_early_rsc_on_ticks = 2; //(DRMMFR_TRANSIT_PREPARE_TICKS-1);
static int drm_mfr_adjust_vidstart_us = 60; // 60us
static int drm_mfr_use_long_swvsync = 1;
static int drm_mfr_mipiclkcng_tick = 2;

module_param(drm_mfr_swvsync_rate, int, 0600);
module_param(drm_mfr_videorate, int, 0600);
module_param_named(drm_mfr, drm_mfr_mfrate, int, 0600);
module_param(drm_mfr_prevsync_t_offset_100us, int, 0600);
module_param(drm_mfr_ctrl_clocks, int, 0600);
// module_param(drm_mfr_early_rsc_on_ticks, int, 0600);
module_param(drm_mfr_adjust_vidstart_us, int, 0600);
module_param(drm_mfr_use_long_swvsync, int, 0600);
module_param(drm_mfr_mipiclkcng_tick, int, 0600);

static int vid_output_cnt = 0;

/* ------------------------------------------------------------------------- */
/* STATE_HANDLER                                                             */
/* ------------------------------------------------------------------------- */
const struct drm_mfr_vid_stat_handler drm_mfr_state_handlers[] = {
	/* POWER_ON, */ {
	NULL,
	drm_mfr_vid_state_vhandler_poweron,
	3, },

	/* UPDATE_FRAME */ {
	drm_mfr_vid_state_pvhandler_updateframe,
	NULL,
	1,},

	/* BLANK_FRAME */ {
	NULL,
	drm_mfr_vid_state_vhandler_blankframe,
	6,},

	/* REPEAT_FRAME */ {
	drm_mfr_vid_state_pvhandler_repeatframe,
	NULL,
	2/* or.. */,},

	/* WAIT_FRAME */ {
	NULL,
	NULL,
	1,},

	/* HR_VIDEO_WAIT */ {
	NULL,
	NULL,
	-1/* ... */,},

	/* HR_VIDEO_PREPARE */ {
	NULL,
	drm_mfr_vid_state_vhandler_hr_video_prepare,
	DRMMFR_TRANSIT_PREPARE_TICKS, },

	/* HR_VIDEO_REFRESH */ {
	drm_mfr_vid_state_pvhandler_hr_video_refresh,
	NULL,
	1,},

	/* HR_VIDEO_MFR_WAIT */ {
	NULL,
	drm_mfr_vid_state_vhandler_hr_video_mfr_wait,
	-1,},

	/*HR_VIDEO_MFR_REFRESH*/ {
	drm_mfr_vid_state_pvhandler_hr_video_mfr_refresh,
	NULL,
	1,},

	/*HR_VIDEO_MFR_MAX*/ {
	NULL,
	NULL,
	-1,},

	/* SUSPEND */ {
	NULL,
	drm_mfr_vid_state_vhandler_suspend,
	-1,},

	/* COMMIT */ {
	NULL,
	drm_mfr_vid_state_vhandler_commit,
	1,},

	/* PRE_COMMIT */ {
	NULL,
	drm_mfr_vid_state_vhandler_pre_commit,
	8,},

	/* POWER_OFF */{
	NULL,
	NULL,
	-1,},
};

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_notify_power_cb(struct drm_mfr_ctx *mfr_ctx, int en)
{
	unsigned long flags_mfr = 0;
	unsigned long flags_swt = 0;
	struct drm_vid_ctx *vid_ctx = NULL;
	struct drm_psv_req psv_req;
	int ret = 0;

	DRMMFR_DBG("is called en = %d", en);

	if (en && !mfr_ctx->mfr_ctrl) {
		mfr_ctx->mfr_ctrl = drm_get_mfr_controller();
		if (!drm_mfr_validate_interface(mfr_ctx->mfr_ctrl)) {
			mfr_ctx->mfr_ctrl = NULL;
			goto failed_to_get_mfr_ctrl;
		}
		mfr_ctx->vid_ctx->mfr_ctrl = mfr_ctx->mfr_ctrl;
		mfr_ctx->swt_ctx->mfr_ctrl = mfr_ctx->mfr_ctrl;
	}

	memset(&psv_req, 0, sizeof(psv_req));
	psv_req.vsync_intr
		= mfr_ctx->vsync_is_requested ? PWR_ON : PWR_OFF;
	psv_req.underrun_intr
		= mfr_ctx->underrun_is_requested ? PWR_ON : PWR_OFF;
	psv_req.clocks = PWR_ON;
	psv_req.qos = PWR_OFF;

	vid_ctx = mfr_ctx->vid_ctx;

	if (en) {
		// sync resource settings state..
		ret = drm_mfr_psv_start(mfr_ctx->psv_ctx, &psv_req);
		if (ret) {
			DRMMFR_ERR("failed to start_psv_ctrl()");
			goto failed_to_start_psv_ctrl;
		}
	} else {
		// stop timer and restore - resource settings..
		drm_mfr_suspend_ctrl(true);
		drm_mfr_swt_panel_power(mfr_ctx->swt_ctx, en);

		drm_mfr_psv_stop(mfr_ctx->psv_ctx, &psv_req);
	}

	spin_lock_irqsave(&mfr_ctx->mfr_lock, flags_mfr);
	if (en) {
		// initialize states.. and start-timer..
		spin_lock_irqsave(&mfr_ctx->swt_ctx->vsync_lock, flags_swt);
		drm_mfr_vid_clear_video_start_byswvsync(vid_ctx);
		vid_ctx->cur_vid_state = POWER_ON;
		vid_ctx->cur_vid_ticks = -1; // will become 0 on 1st-hwvsync
		vid_ctx->cont_commit_ticks = 0;
		vid_ctx->vid_output_state = VID_OUTPUT_STARTED;
		vid_ctx->request_stop_vid_on_next_vsync = false;
		drm_mfr_vid_init_polarity_state(vid_ctx);
		vid_ctx->clocks_refcount = 1;
		vid_ctx->long_swvsync = false;
		spin_unlock_irqrestore(&mfr_ctx->swt_ctx->vsync_lock,
								flags_swt);
		drm_mfr_swt_panel_power(mfr_ctx->swt_ctx, en);
	}

	if (!en) {
		// set state for power-off...
		spin_lock_irqsave(&mfr_ctx->swt_ctx->vsync_lock, flags_swt);
		vid_ctx->cur_vid_state = POWER_OFF;
		vid_ctx->cur_vid_ticks = -1;
		vid_ctx->request_stop_vid_on_next_vsync = false;
		vid_ctx->clocks_refcount = 0;
		vid_ctx->long_swvsync = false;
		spin_unlock_irqrestore(&mfr_ctx->swt_ctx->vsync_lock,
								flags_swt);
	}
	spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);

	return 0;

failed_to_get_mfr_ctrl:
failed_to_start_psv_ctrl:
	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_notify_prepare_commit_cb(struct drm_mfr_ctx *mfr_ctx,
			int restart_vid/*, int cont_commit_frame_count*/)
{
	int ret = 0;
	unsigned long flags_mfr = 0;
	unsigned long flags_swt = 0;
	int vsync_time_us;
	struct drm_swt_ctx *swt_ctx = mfr_ctx->swt_ctx;
	struct drm_vid_ctx *vid_ctx = mfr_ctx->vid_ctx;

	if ((mfr_ctx->vid_ctx->cur_vid_state == POWER_OFF)
	    || (mfr_ctx->vid_ctx->cur_vid_state == HR_VIDEO_MFR_MAX)
	    || (mfr_ctx->vid_ctx->cur_vid_state == SUSPEND)) {
		return 0;
	}

	spin_lock_irqsave(&swt_ctx->vsync_lock, flags_swt);
	drm_mfr_vid_set_keep_enabled_rsc(vid_ctx, true);
	spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags_swt);
	drm_mfr_vid_enable_rsc(vid_ctx, true/*sync*/);

	ret = drm_mfr_vid_restart_video_and_wait(mfr_ctx,
				restart_vid/*, cont_commit_frame_count*/);
	if (ret) {
		spin_lock_irqsave(&mfr_ctx->mfr_lock, flags_mfr);
		spin_lock_irqsave(&mfr_ctx->swt_ctx->vsync_lock, flags_swt);
		vid_ctx->cont_commit_ticks = 1;
		spin_unlock_irqrestore(
				&mfr_ctx->swt_ctx->vsync_lock, flags_swt);
		spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);
		vsync_time_us =
			drm_mfr_swt_calc_hwvsync_ns(mfr_ctx->swt_ctx) / 1000;
		//vsync_time_us *= 2;
		usleep_range(vsync_time_us, vsync_time_us);
	}
	drm_mfr_vid_set_keep_enabled_rsc(vid_ctx, false);
	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_notify_commit_frame_cb(struct drm_mfr_ctx *mfr_ctx)
{
	unsigned long flags_mfr = 0;
	unsigned long flags_swt = 0;
	bool translate_commit = false;
	struct drm_swt_ctx *swt_ctx = mfr_ctx->swt_ctx;
	struct drm_vid_ctx *vid_ctx = mfr_ctx->vid_ctx;

	if (unlikely(mfr_ctx->vid_ctx->cur_vid_state == POWER_OFF)) {
		DRMMFR_DBG("vid_state is POWER_OFF");
	}

	spin_lock_irqsave(&swt_ctx->vsync_lock, flags_swt);
	drm_mfr_vid_set_keep_enabled_rsc(vid_ctx, true);
	spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags_swt);
	drm_mfr_vid_enable_rsc(vid_ctx, true/*sync*/);
	spin_lock_irqsave(&mfr_ctx->mfr_lock, flags_mfr);
	spin_lock_irqsave(&swt_ctx->vsync_lock, flags_swt);
	drm_mfr_vid_set_keep_enabled_rsc(vid_ctx, false);
	if (unlikely(mfr_ctx->vid_ctx->cur_vid_state == SUSPEND)) {
		DRMMFR_DBG("do nothing due to SUSPEND state");
	} else {
		DRMMFR_DBG("change vid_state from %s "
			"to %s cur_vid_ticks = %d",
				state_str[vid_ctx->cur_vid_state],
				state_str[COMMIT],
				vid_ctx->cur_vid_ticks);
		vid_ctx->cur_vid_state = COMMIT;
		vid_ctx->cur_vid_ticks = 0;
		translate_commit = true;
	}
	spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags_swt);
	spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);
	if (translate_commit) {
		drm_mfr_swt_stop_long_swvsync_and_restart(mfr_ctx);
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_notify_restart_vid_counter(struct drm_mfr_ctx *mfr_ctx,
							int count, int vtotal)
{
	unsigned long flags_swt = 0;
	struct drm_vid_ctx *vid_ctx = mfr_ctx->vid_ctx;
	struct drm_swt_ctx *swt_ctx;
	bool calc = false;
	int64_t hwvsync_ns;
	int64_t count_ns;

	DRMMFR_DBG("is called count = %d, vtotal=%d", count, vtotal);
	calc = vid_ctx->restart_vid_counter != count;
	calc |= vid_ctx->vtotal != vtotal;
	calc &= vtotal != 0;
	calc &= (count < vtotal);
	if (!calc) {
		return 0;
	}

	vid_ctx->restart_vid_counter = count;
	vid_ctx->vtotal = vtotal;

	swt_ctx = mfr_ctx->swt_ctx;
	hwvsync_ns = drm_mfr_swt_calc_hwvsync_ns(swt_ctx);
	count_ns = hwvsync_ns * count;
	count_ns /= vtotal;

	spin_lock_irqsave(&swt_ctx->vsync_lock, flags_swt);
	swt_ctx->adjust_vidstart_svvsync_us = ((count_ns + 999)/1000);
	swt_ctx->adjust_vidstart_svvsync_us += drm_mfr_adjust_vidstart_us;
	DRMMFR_DBG("adjust_vidstart us = %dus",
				swt_ctx->adjust_vidstart_svvsync_us);
	spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags_swt);

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_notify_request_vblank_by_user(
					struct drm_mfr_ctx *mfr_ctx, int en)
{
	struct drm_swt_ctx *swt_ctx = mfr_ctx->swt_ctx;

	DRMMFR_DBG("is called en = %d", en);
	swt_ctx->reqeust_vsync_by_user = en;
	if (!en) {
		return 0;
	}

	drm_mfr_swt_stop_long_swvsync_and_restart(mfr_ctx);

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static enum DRM_MFR_CTRL_BY drm_mfr_notify_vsync_enable_cb(
					struct drm_mfr_ctx *mfr_ctx, int en)
{
	unsigned long flags_mfr = 0;
	unsigned long flags_swt = 0;
	enum DRM_MFR_CTRL_BY ret = DRM_MFR_CTRL_BY_VENDOR;
	struct drm_vid_ctx *vid_ctx;

	spin_lock_irqsave(&mfr_ctx->mfr_lock, flags_mfr);
	spin_lock_irqsave(&mfr_ctx->swt_ctx->vsync_lock, flags_swt);
	if (mfr_ctx->vsync_is_requested == en) {
		spin_unlock_irqrestore(
			&mfr_ctx->swt_ctx->vsync_lock, flags_swt);
		spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);
		DRMMFR_ERR("vsync enable(%d) was requested...", en);
		return ret;
	}

	DRMMFR_DBG("is called en = %d", en);
	mfr_ctx->vsync_is_requested = en;
	vid_ctx = mfr_ctx->vid_ctx;

	if (vid_ctx->cur_vid_state == POWER_OFF) {
		ret = DRM_MFR_CTRL_BY_VENDOR;
	} else {
		ret = DRM_MFR_CTRL_BY_MFR;
	}
	spin_unlock_irqrestore(&mfr_ctx->swt_ctx->vsync_lock, flags_swt);
	spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);

	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static enum DRM_MFR_CTRL_BY drm_mfr_notify_underrun_enable_cb(
				struct drm_mfr_ctx *mfr_ctx, int en, int index)
{
	unsigned long flags_mfr = 0;
	unsigned long flags_swt = 0;
	enum DRM_MFR_CTRL_BY ret = DRM_MFR_CTRL_BY_VENDOR;
	struct drm_vid_ctx *vid_ctx;
	bool is_master = (index == 0);

	spin_lock_irqsave(&mfr_ctx->mfr_lock, flags_mfr);
	spin_lock_irqsave(&mfr_ctx->swt_ctx->vsync_lock, flags_swt);

	DRMMFR_DBG("is called en = %d index = %d", en, index);
	vid_ctx = mfr_ctx->vid_ctx;
	if (vid_ctx->cur_vid_state == POWER_OFF) {
		if (is_master) {
			mfr_ctx->underrun_is_requested = en;
		}
		goto unlock_return_l;
	}

	ret = DRM_MFR_CTRL_BY_MFR;
	if (is_master) {
		if (mfr_ctx->underrun_is_requested == en) {
			goto unlock_return_l;
		}
		mfr_ctx->underrun_is_requested = en;
	}
	spin_unlock_irqrestore(&mfr_ctx->swt_ctx->vsync_lock, flags_swt);
	spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);

	return ret;

unlock_return_l:
	spin_unlock_irqrestore(&mfr_ctx->swt_ctx->vsync_lock, flags_swt);
	spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static enum DRM_MFR_SEND_VBLANK
	drm_mfr_notify_hwvsync_cb(struct drm_mfr_ctx *mfr_ctx)
{
	struct drm_vid_ctx *vid_ctx = NULL;
	bool expect_hwvsync = true;
	bool video_starting_vsync = false;
	const struct drm_mfr_vid_stat_handler *sh = NULL;
	enum drm_mfr_vid_output_state
			last_vid_ouput_state = VID_OUTPUT_STARTED;
	enum DRM_MFR_SEND_VBLANK notify = DRM_MFR_SEND_VBLANK_NOTSEND;
	ktime_t cur_ktime;

	DRMMFR_DBG(">>>");

	vid_ctx = mfr_ctx->vid_ctx;

	expect_hwvsync = (vid_ctx->vid_output_state == VID_OUTPUT_STARTED);

	if (!mfr_ctx->swt_ctx->enabled_display) {
		DRMMFR_DBG("<<< display is off");
		notify = (mfr_ctx->vsync_is_requested) ?
			DRM_MFR_SEND_VBLANK_SEND : DRM_MFR_SEND_VBLANK_NOTSEND;
		goto end;
	}

	notify = ((mfr_ctx->vsync_is_requested) && (expect_hwvsync)) ?
			DRM_MFR_SEND_VBLANK_SEND : DRM_MFR_SEND_VBLANK_NOTSEND;
#ifdef DRMMFR_ONLYSWT
	expect_hwvsync = false;
	notify = DRM_MFR_SEND_VBLANK_NOTSEND;
#endif /* DRMMFR_ONLYSWT */

	// skip 1st-hwvsync after video_is_started_by swvsync
	// because 1st-hwvsync comes at video-stating timing
	video_starting_vsync = drm_mfr_vid_clear_video_start_byswvsync(vid_ctx);

	cur_ktime = ktime_get();
	if (expect_hwvsync) {
		drm_mfr_vid_update_last_expectvsynctype_v_ktime(
							vid_ctx, cur_ktime);

		last_vid_ouput_state =
			drm_mfr_vid_update_vid_output_status_on_vsync(vid_ctx);

		drm_mfr_vid_wakeup_waiting_video(vid_ctx);

		if (video_starting_vsync) {

		} else {
			sh = drm_mfr_vid_get_target_state_handler(
						vid_ctx->cur_vid_state);

			drm_mfr_vid_add_ticks(mfr_ctx->vid_ctx, VSYNC_TYPE_HW);

			// skip handling..
			if (!vid_ctx->request_stop_vid_on_next_vsync) {
				drm_mfr_vid_handle_onvsync_cmn(
					sh, mfr_ctx->vid_ctx, VSYNC_TYPE_HW);
			}
		}
		// calculate current-polarity-counter.
		drm_mfr_vid_update_polarity_onhwvsync(
			mfr_ctx->vid_ctx, last_vid_ouput_state);

		if ((mfr_ctx->swt_ctx->prevsync_timer.state == SWT_IDLE) &&
		    (!vid_ctx->request_stop_vid_on_next_vsync) &&
		    (vid_ctx->vid_output_state == VID_OUTPUT_STARTED) &&
		    drm_mfr_vid_need_video_stop_state(
						vid_ctx->cur_vid_state)) {
			drm_mfr_swt_prevsync_ctrl(mfr_ctx->swt_ctx, 1);
		}
	}

	if ((vid_ctx->request_stop_vid_on_next_vsync) /* &&
						(!expect_hwvsync) */) {
		vid_ctx->request_stop_vid_on_next_vsync = false;
		DRMMFR_DBG("stop video on hwvsync "
			"because pre-vsync couldn't stop video..");
		drm_mfr_vid_enable_output_video(vid_ctx, 0);

		// pre-vsync couldn't stop video(and start sw-vsync)
		// so start sw-vsync(next-timing) by hw-vsync
		// this case is failsafe..
		if (mfr_ctx->swt_ctx->swvsync_timer.state == SWT_IDLE) {
			drm_mfr_swt_swvsync_ctrl(mfr_ctx->swt_ctx, 1);
		}
	}
end:
	DRMMFR_DBG("<<< notify = %d", notify == DRM_MFR_SEND_VBLANK_SEND);

	return notify;
}


/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_pre_hw_vsync_cb(struct drm_mfr_ctx *mfr_ctx)
{
	struct drm_swt_ctx *swt_ctx;
	unsigned long flags_mfr = 0;
	unsigned long flags_swt= 0;

	DRMMFR_DBG(">>>");

	swt_ctx = mfr_ctx->swt_ctx;
	spin_lock_irqsave(&mfr_ctx->mfr_lock, flags_mfr);
	spin_lock_irqsave(&swt_ctx->vsync_lock, flags_swt);

	DRMMFR_DBG("<<<");
	return 0;
}


/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_post_hw_vsync_cb(struct drm_mfr_ctx *mfr_ctx)
{
	struct drm_swt_ctx *swt_ctx;
	unsigned long flags_mfr = 0;
	unsigned long flags_swt= 0;

	DRMMFR_DBG(">>>");

	swt_ctx = mfr_ctx->swt_ctx;
	spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags_swt);
	spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);

	DRMMFR_DBG("<<<");
	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_force_output_video_cb(struct drm_mfr_ctx *mfr_ctx)
{
	// failsafe function...
	int ret = 0;
	struct drm_swt_ctx *swt_ctx;
	struct drm_vid_ctx *vid_ctx;
	unsigned long flags_mfr = 0;
	unsigned long flags_swt= 0;
	int vsync_time_us;

	swt_ctx = mfr_ctx->swt_ctx;
	vid_ctx = mfr_ctx->vid_ctx;

	DRMMFR_DBG(">>>");

	if (vid_ctx->cur_vid_state == POWER_OFF) {
		DRMMFR_DBG("<<<");
		return ret;
	}

	spin_lock_irqsave(&mfr_ctx->mfr_lock, flags_mfr);
	spin_lock_irqsave(&swt_ctx->vsync_lock, flags_swt);

	drm_mfr_vid_failsafe_enable_output_video(vid_ctx);
	vid_ctx->cont_commit_ticks = 1;

	spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags_swt);
	spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);

	vsync_time_us =
		drm_mfr_swt_calc_hwvsync_ns(mfr_ctx->swt_ctx) / 1000;
	//vsync_time_us *= 2;
	usleep_range(vsync_time_us, vsync_time_us);
	DRMMFR_DBG("<<<");

	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_validate_interface(
				const struct drm_mfr_controller *mfr_ctrl)
{
	if (!mfr_ctrl)
		goto invalidate_interface;

	if (!mfr_ctrl->prepare_restart_vid)
		goto invalidate_interface;

	if (!mfr_ctrl->vblank_callback)
		goto invalidate_interface;

	//if (!mfr_ctrl->frame_done_callback)
	//	goto invalidate_interface;

	if (!mfr_ctrl->enable_output_vid)
		goto invalidate_interface;

	if (!mfr_ctrl->can_stop_video)
		goto invalidate_interface;

	if (!mfr_ctrl->is_video_stopped)
		goto invalidate_interface;

	return true;
invalidate_interface:
	DRMMFR_ERR(": mfr_ctrl is invalidate interface!!"
					" can't control hr_video");
	return false;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_create_context(void)
{
	if (mfr_context) {
		return 0;
	}

	mfr_context = kzalloc(sizeof(*mfr_context), GFP_KERNEL);
	if (!mfr_context) {
		DRMMFR_ERR("failed to alloc mfr_context");
		return -ENOMEM;
	}

	spin_lock_init(&mfr_context->mfr_lock);
	spin_lock_init(&mfr_context->psv_lock);

	mfr_context->vid_ctx
		= kzalloc(sizeof(*mfr_context->vid_ctx), GFP_KERNEL);

	if (!mfr_context->vid_ctx) {
		DRMMFR_ERR("failed to alloc vid_ctx");
		kfree(mfr_context);
		return -ENOMEM;
	}

	mfr_context->vid_ctx->cur_vid_state = POWER_OFF;
	mfr_context->vid_ctx->vid_output_state = VID_OUTPUT_STARTED;
	mfr_context->vid_ctx->cur_vid_ticks = 0;

	mfr_context->vid_ctx->wait_for_video_start = false;
	init_completion(&mfr_context->vid_ctx->video_started_comp);

	mfr_context->vid_ctx->mfr_ctx = mfr_context;

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_destroy_context(void)
{
	if (mfr_context) {
		kfree(mfr_context);
		mfr_context = NULL;
	}
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_create_swt_ctx(struct drm_swt_ctx **ppswt_ctx,
			struct drm_mfr_ctx *mfr_ctx,
			const int *cpvid_rate, const int *cpmfr_rate)
{
	struct drm_swt_ctx *swt_ctx = NULL;
	swt_ctx = kzalloc(sizeof(**ppswt_ctx), GFP_KERNEL);
	if (!swt_ctx) {
		DRMMFR_ERR("failed to alloc swt_ctx");
		return -ENOMEM;
	}

	hrtimer_init(&swt_ctx->prevsync_timer.hrt,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_init(&swt_ctx->swvsync_timer.hrt,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	spin_lock_init(&swt_ctx->vsync_lock);

	swt_ctx->mfr_ctx = mfr_ctx;
	swt_ctx->cpvid_rate = cpvid_rate; // shared vid_rate value;
	swt_ctx->cpmfr_rate = cpmfr_rate; // shared mfr_rate value;

	*ppswt_ctx = swt_ctx;

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_destroy_swt_ctx(struct drm_swt_ctx *swt_ctx)
{
	if (swt_ctx) {
		kfree(swt_ctx);
	}
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_create_spt_ctx(struct drm_spt_ctx **ppspt_ctx)
{
	struct drm_spt_ctx *spt_ctx = NULL;
	int supprted_value_ary[] = {1, 10, 15, 20, 30, 40, 60, 100,};


	int arylen = sizeof(supprted_value_ary)/sizeof(*supprted_value_ary);
	int alloc_size = sizeof(*spt_ctx) + sizeof(supprted_value_ary);
	int cnt = 0;

	spt_ctx = kzalloc(alloc_size, GFP_KERNEL);
	if (!spt_ctx) {
		DRMMFR_ERR("%s: failed to alloc spt_ctx", __func__);
		return -ENOMEM;
	}

	spt_ctx->mfr_spt_len = arylen;
	spt_ctx->mfr_spt_values =
		(int*)((char*)(spt_ctx) + sizeof(*spt_ctx));

	for (cnt = 0; cnt != arylen; cnt++) {
		spt_ctx->mfr_spt_values[cnt] = supprted_value_ary[cnt];
		DRMMFR_DBG("%s: supported mfr[%d] = %d",
			__func__, cnt, spt_ctx->mfr_spt_values[cnt]);
	}

	*ppspt_ctx = spt_ctx;

	return 0;
}


/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_destroy_spt_ctx(struct drm_spt_ctx *spt_ctx)
{
	if (spt_ctx) {
		kfree(spt_ctx);
	}
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_bind(void)
{
	int ret = 0;
	struct drm_mfr_callbacks mfr_cb;

	mfr_context->controllable = false;

	// register mfr_callbacks(notify update/vsync) for vendor-module
	memset(&mfr_cb, 0, sizeof(mfr_cb));
	mfr_cb.ctx                      = mfr_context;
	mfr_cb.notify_power             = drm_mfr_notify_power_cb;
	mfr_cb.notify_prepare_commit    = drm_mfr_notify_prepare_commit_cb;
	mfr_cb.notify_commit_frame      = drm_mfr_notify_commit_frame_cb;
	mfr_cb.notify_restart_vid_counter
					= drm_mfr_notify_restart_vid_counter;
	mfr_cb.notify_request_vblank_by_user
					= drm_mfr_notify_request_vblank_by_user;
	mfr_cb.notify_vsync_enable      = drm_mfr_notify_vsync_enable_cb;
	mfr_cb.notify_underrun_enable   = drm_mfr_notify_underrun_enable_cb;
	mfr_cb.notify_hw_vsync          = drm_mfr_notify_hwvsync_cb;
	mfr_cb.pre_hw_vsync             = drm_mfr_pre_hw_vsync_cb;
	mfr_cb.post_hw_vsync            = drm_mfr_post_hw_vsync_cb;
	mfr_cb.force_output_video       = drm_mfr_force_output_video_cb;
	ret = drm_register_mfr_callback(&mfr_cb);

	if (ret) {
	//	DRMMFR_ERR("failed to "
	//			"drm_register_mfr_callback mfr_controller()-n");
	//	mfr_context->controllable = false;
	} else {
		mfr_context->controllable = true;
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_unbind(void)
{
	DRMMFR_DBG("is called");
	drm_unregister_mfr_callback();

	mfr_context->controllable = false;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void drm_mfr_suspend_ctrl(int req)
{
	unsigned long flags_mfr = 0;
	unsigned long flags_swt= 0;
	int vsync_time_us;
	struct drm_vid_ctx *vid_ctx = NULL;
	struct drm_swt_ctx *swt_ctx = NULL;

	if (!mfr_context/* || !mfr_context->vid_ctx*/) {
		DRMMFR_ERR("no context");
		return;
	}
	DRMMFR_DBG("req = %s caller:%pS", (req ? "true" : "false"), __builtin_return_address(0));

	vid_ctx = mfr_context->vid_ctx;
	swt_ctx = mfr_context->swt_ctx;

	if (vid_ctx->cur_vid_state == POWER_OFF) {
		return;
	}

	spin_lock_irqsave(&swt_ctx->vsync_lock, flags_swt);
	drm_mfr_vid_set_keep_enabled_rsc(vid_ctx, true);
	spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags_swt);
	drm_mfr_vid_enable_rsc(vid_ctx, true/*sync*/);
	spin_lock_irqsave(&mfr_context->mfr_lock, flags_mfr);
	spin_lock_irqsave(&swt_ctx->vsync_lock, flags_swt);
	drm_mfr_vid_set_keep_enabled_rsc(vid_ctx, false);

	if (req) {
		vid_ctx->cur_vid_state = SUSPEND;
	} else {
		vid_ctx->cur_vid_state = COMMIT;

		if ((vid_ctx->vid_output_state != VID_OUTPUT_STARTED) &&
			(swt_ctx->swvsync_timer.state == SWT_IDLE)) {
			drm_mfr_swt_swvsync_ctrl(swt_ctx, 1);
		}

		vid_ctx->cont_commit_ticks = drm_mfr_mipiclkcng_tick;
	}
	vid_ctx->cur_vid_ticks = 0;

	spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags_swt);
	spin_unlock_irqrestore(&mfr_context->mfr_lock, flags_mfr);

	if (req) {
		 drm_mfr_swt_stop_long_swvsync_and_restart(swt_ctx->mfr_ctx);
	}

	vsync_time_us =
		drm_mfr_swt_calc_hwvsync_ns(mfr_context->swt_ctx) / 1000;
	vsync_time_us *= 2;

	// wait for mfr state change
	usleep_range(vsync_time_us, vsync_time_us);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_mfr_get_mfr(void)
{
	return vid_output_cnt;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void drm_mfr_set_mfr(int set_mfrate)
{
	drm_mfr_mfrate = set_mfrate;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_swt_panel_power(struct drm_swt_ctx *swt_ctx,
					bool en)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&swt_ctx->vsync_lock, flags);
	if (swt_ctx->enabled_display == en) {
		DRMMFR_ERR("panel power was turn on/off already.."
				" on/off state =%d", en);
		spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags);
		return;
	}
	swt_ctx->enabled_display = en;
	swt_ctx->swvsync_rate = drm_mfr_swvsync_rate;

	if (!en) {
		drm_mfr_swt_prevsync_ctrl(swt_ctx, en);
		drm_mfr_swt_swvsync_ctrl(swt_ctx, en);
	}

#ifdef DRMMFR_ONLYSWT
	if (en) {
		drm_mfr_swt_prevsync_ctrl(swt_ctx, en);
		drm_mfr_swt_swvsync_ctrl(swt_ctx, en);
	}
#endif /* DRMMFR_ONLYSWT */
	spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_swt_prevsync_ctrl(struct drm_swt_ctx *swt_ctx,
					bool en)
{
	struct swtimer *swt = &(swt_ctx->prevsync_timer);
	int ret = 0;
	u64 expire_time;
	int line_count;
	struct sde_hw_intf *intf_left;
	struct sde_hw_intf_ops *ops;
	intf_left = get_sde_hw_intf(1);
	ops = &(intf_left->ops);

	if (en) {
		if (swt->state == SWT_IDLE) {
			DRMMFR_DBG("start prevsync-timer");
			line_count = ops->get_line_count(intf_left);
			if (line_count > DRMMFR_VBLANK_AREA) {
				udelay(10);
				line_count = ops->get_line_count(intf_left);
				if (line_count > DRMMFR_VBLANK_AREA) {
					line_count = 0;
					DRMMFR_DBG("line_count set to zero\n");
				}
			}
			if (line_count > DRMMFR_STOP_WARNING_AREA) {
				DRMMFR_DBG("line_count is over to skip:%d\n",
						line_count);
				return;
			} else {
				expire_time = ((10000 *
					(DRMMFR_STOP_WARNING_AREA - line_count))
					/ 3000) * 1000;
			}
			DRMMFR_DBG("expire_time : %lld / line_count : %d\n",
						expire_time, line_count);
			swt->state  = SWT_RUNNING;
			swt->hrt.function = drm_mfr_swt_prevsync_cb;
			swt->base   = ktime_to_ns(ktime_get());
			swt->expire = expire_time;
			hrtimer_start(&swt->hrt, ns_to_ktime(expire_time),
							HRTIMER_MODE_REL);
		//} else (swt->state == SWT_RUNNING) {
			// try_to_cancel...
			//change expired time...
			//set calcurated expire time by current-mfr..
			//and move or restart hrt
		} else {
			DRMMFR_ERR("prevsync timer is running already..");
			// goto err;
		}
	} else {
		if (swt->state != SWT_IDLE) {
			DRMMFR_DBG("stop prevsync-timer");
			ret = hrtimer_try_to_cancel(&swt->hrt);
			if (ret < 0) {
				DRMMFR_ERR("failed to try_to_cancel "
						"prevsync-timer..");
				swt->state = SWT_CANCEL_PENDING;
			} else {
				swt->state = SWT_IDLE;
			}
		} else {
			DRMMFR_DBG("prevsync timer is idle already..");
			// goto err;
		}
	}


	return;
// err:
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_swt_swvsync_ctrl(struct drm_swt_ctx *swt_ctx,
					bool en)
{
	struct swtimer *swt = &(swt_ctx->swvsync_timer);
	int ret = 0;
	struct drm_vid_ctx *vid_ctx = swt_ctx->mfr_ctx->vid_ctx;

	if (en) {
		if (swt->state == SWT_IDLE) {
			swt->state  = SWT_RUNNING;
			swt->hrt.function = drm_mfr_swt_swvsync_cb;
			DRMMFR_DBG("start swvsync-timer");

			swt->base   = ktime_to_ns(ktime_get());
			swt_ctx->finish_1st_swvsync_restart = false;
			// expect next_hwvsync timing
			// because this function called by
			// did-stop-video prevsync
			// (expect 1st-swvsync timing is
			//  video-stopped-hwvsync timing)
			swt->expire =
			    drm_mfr_swt_calc_1st_swvsync_ns(
			      swt_ctx,
			      drm_mfr_vid_get_last_expectvsynctype_v_ktime(
			        vid_ctx));
			hrtimer_start(&swt->hrt, ns_to_ktime(swt->expire),
							HRTIMER_MODE_REL);
		//} else (swt->state == SWT_RUNNING) {
			// try_to_cancel...
			//change expired time...
			//set calcurated expire time by current-mfr..
			//and move or restart hrt
		} else {
			DRMMFR_ERR("swvsync timer is running already.");
			// goto err;
		}
	} else {
		if (swt->state != SWT_IDLE) {
			DRMMFR_DBG("stop swvsync-timer");
			ret = hrtimer_try_to_cancel(&swt->hrt);
			if (ret < 0) {
				DRMMFR_ERR("failed to try_to_cancel "
					"swvsync-timer..");
				swt->state = SWT_CANCEL_PENDING;
			} else {
				swt->state = SWT_IDLE;
			}
			swt_ctx->finish_1st_swvsync_restart = false;
		} else {
			DRMMFR_DBG("swvsync timer is idle already..");
			// goto err;
		}
	}
	return;
// err:
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_swt_stop_long_swvsync_ifrunning(struct drm_mfr_ctx *mfr_ctx)
{
	int stopped = false;
	bool shouldstop = false;
	bool hrtimer_is_active;
	int retry_count = 0;

	unsigned long flags_mfr = 0;
	unsigned long flags_swt = 0;
	struct drm_swt_ctx *swt_ctx = mfr_ctx->swt_ctx;
	struct drm_vid_ctx *vid_ctx = mfr_ctx->vid_ctx;
	struct swtimer *swt  = &(swt_ctx->swvsync_timer);

	spin_lock_irqsave(&mfr_ctx->mfr_lock, flags_mfr);
	spin_lock_irqsave(&swt_ctx->vsync_lock, flags_swt);
	if ((vid_ctx->long_swvsync) && (swt->state == SWT_RUNNING)) {
		shouldstop = true;
	}
	spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags_swt);
	spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);

	if (!shouldstop) {
		return stopped;
	}

	if (swt->state == SWT_RUNNING) {
		DRMMFR_DBG("try to stop long-swvsync");
		drm_mfr_swt_swvsync_ctrl(swt_ctx, 0);
		stopped = true;
	}

	if (swt->state == SWT_CANCEL_PENDING) {
		do {
			retry_count++;
			DRMMFR_DBG("try to wait for long-swvsync, count=%d\n",
								retry_count);
			usleep_range(1000*1000, 1000*1000);
			hrtimer_is_active = hrtimer_active(&swt->hrt);
		} while(retry_count <= 3 || hrtimer_is_active);
		if (hrtimer_is_active) {
			DRMMFR_ERR("failed to stop long-swvsync..");
		}
		swt->state = SWT_IDLE;
	}
	if (vid_ctx->long_swvsync) {
		drm_mfr_vid_add_ticks(mfr_ctx->vid_ctx, VSYNC_TYPE_NOTVSYNC);
	}

	return stopped;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_swt_stop_long_swvsync_and_restart(
						struct drm_mfr_ctx *mfr_ctx)
{
	bool stopped;
	struct drm_vid_ctx *vid_ctx;
	struct drm_swt_ctx *swt_ctx;

	stopped = drm_mfr_swt_stop_long_swvsync_ifrunning(mfr_ctx);
	// drm_mfr_vid_add_ticks_long_swvsync();
	if (stopped) {
		vid_ctx = mfr_ctx->vid_ctx;
		swt_ctx = mfr_ctx->swt_ctx;
		if ((vid_ctx->vid_output_state != VID_OUTPUT_STARTED) &&
				(swt_ctx->swvsync_timer.state == SWT_IDLE)) {
			drm_mfr_swt_swvsync_ctrl(swt_ctx, 1);
		}
	}
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_swt_calc_hwvsync_ns(struct drm_swt_ctx *swt_ctx)
{
	if ((swt_ctx->cpvid_rate == NULL) || ((*swt_ctx->cpvid_rate) == 0)) {
		return 16666667; //ns - 60Hz
	}
	return DRMMFR_1SEC_NS / (*swt_ctx->cpvid_rate);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_swt_calc_swvsync_ns(struct drm_swt_ctx *swt_ctx)
{
	if (swt_ctx->swvsync_rate == 0) {
		return 16666667; //ns - 60Hz
	}
	return DRMMFR_1SEC_NS / (swt_ctx->swvsync_rate);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_swt_calc_1st_swvsync_ns(struct drm_swt_ctx *swt_ctx,
						ktime_t base_vsync_ktime)
{
	int vsync_ns;
	int expect_next_vsync_ns;
	int from_base_vsync;
	int tmp;

	if ((swt_ctx->cpvid_rate == NULL) || ((*swt_ctx->cpvid_rate) == 0)) {
		expect_next_vsync_ns = 16666667; //ns - 60Hz
		return expect_next_vsync_ns;
	}

	// expect 1st-swvsync timing is video-stopped-hwvsync-timing
	vsync_ns = drm_mfr_swt_calc_hwvsync_ns(swt_ctx);

	// calculate expect next vsync timing
	// from base vsync
	from_base_vsync =
		ktime_to_ns(ktime_sub(ktime_get(), base_vsync_ktime));

	tmp =  ((from_base_vsync / vsync_ns) + 1) * vsync_ns;
	DRMMFR_DBG("vsync_ns = %d, from_base_vsync = %d, tmp = %d",
					vsync_ns, from_base_vsync, tmp);

	expect_next_vsync_ns = tmp - from_base_vsync;

	if (expect_next_vsync_ns < 200000) {
		DRMMFR_DBG("before expect_next_vsync_ns=%d", expect_next_vsync_ns);
		expect_next_vsync_ns += vsync_ns;
	}
	DRMMFR_DBG("expect_next_vsync_ns = %d (%d)",
		expect_next_vsync_ns, tmp);
	return expect_next_vsync_ns;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_swt_calc_next_swvsync(struct drm_swt_ctx *swt_ctx,
						ktime_t base_vsync_ktime,
						bool finish_restart_1st)
{
	int rtn;
	struct drm_vid_ctx *vid_ctx = swt_ctx->mfr_ctx->vid_ctx;
	bool can_long_swvsync = drm_mfr_use_long_swvsync;

	if (can_long_swvsync) {
		can_long_swvsync = drm_mfr_vid_can_use_long_swvsync(vid_ctx);
	}

	if (can_long_swvsync && (vid_ctx->cur_vid_state == HR_VIDEO_WAIT)) {
		rtn = drm_mfr_swt_calc_to_hr_video_wait_end_swvsync_ns(
			swt_ctx, base_vsync_ktime, finish_restart_1st);
		vid_ctx->long_swvsync = true;
		vid_ctx->long_swvsync_base_ktime = ktime_get();
	} else {
		rtn = drm_mfr_swt_calc_restart_swvsync_ns(
			swt_ctx, base_vsync_ktime, finish_restart_1st);
	}
	return rtn;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_swt_calc_restart_swvsync_ns(
						struct drm_swt_ctx *swt_ctx,
						ktime_t base_vsync_ktime,
						bool finish_restart_1st)
{
	int vsync_ns;
	int expect_next_vsync_ns;
	int from_base_vsync;
	int adjust_vcount_ns = 0;
	int tmp = 0;
	struct drm_vid_ctx *vid_ctx = swt_ctx->mfr_ctx->vid_ctx;

	vsync_ns = drm_mfr_swt_calc_swvsync_ns(swt_ctx);
	if (finish_restart_1st/* && swt_ctx->cpvid_rate == 100*/) {
		expect_next_vsync_ns = vsync_ns;
	} else {
		int hwvsync = drm_mfr_swt_calc_hwvsync_ns(swt_ctx);
		expect_next_vsync_ns = (vsync_ns*2) - hwvsync;
	}

	// calculate expect next vsync timing
	// from base vsync
	from_base_vsync =
		ktime_to_ns(ktime_sub(ktime_get(), base_vsync_ktime));

	if (from_base_vsync > expect_next_vsync_ns) {
		tmp = from_base_vsync - expect_next_vsync_ns;
		tmp /= vsync_ns;
		tmp += 1;
		expect_next_vsync_ns += (vsync_ns*tmp);
	}

	if (drm_mfr_vid_is_restartvideo_nextswvsync(vid_ctx)) {
		adjust_vcount_ns = (swt_ctx->adjust_vidstart_svvsync_us * 1000);
		if (expect_next_vsync_ns > adjust_vcount_ns)
			expect_next_vsync_ns -= adjust_vcount_ns;
	}

	DRMMFR_DBG("finish_restart_1st = %d, vsync_ns = %d, "
		"from_base_vsync = %d, adjust_vcount_ns = %d, tmp = %d",
		finish_restart_1st, vsync_ns,
		from_base_vsync, adjust_vcount_ns, tmp);

	// expect_next_vsync_ns -= from_base_vsync;

	DRMMFR_DBG("expect_next_vsync_ns = %d (%d)",
		expect_next_vsync_ns - from_base_vsync,
		expect_next_vsync_ns);
	return expect_next_vsync_ns - from_base_vsync;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_swt_calc_to_hr_video_wait_end_swvsync_ns(
						struct drm_swt_ctx *swt_ctx,
						ktime_t base_vsync_ktime,
						bool finish_restart_1st)
{
	u64 expire = 0;
	int swvsync_1time = 0;
	int to_ticks = 0;
	struct drm_vid_ctx *vid_ctx = swt_ctx->mfr_ctx->vid_ctx;
	to_ticks = vid_ctx->mfr_wait_ticks - DRMMFR_TRANSIT_PREPARE_TICKS;
	to_ticks -= vid_ctx->cur_vid_ticks;
	if (to_ticks <= 0) {
		to_ticks = 1;
	}
	swvsync_1time = drm_mfr_swt_calc_swvsync_ns(swt_ctx);

	expire = drm_mfr_swt_calc_restart_swvsync_ns(
		swt_ctx, base_vsync_ktime, finish_restart_1st);
	expire += (swvsync_1time * (to_ticks-1));
	DRMMFR_DBG("%s: set long-swvsync-timer(to end of %s: "
			"ticks = %d, swvsync_ns = %d",
			__func__, state_str[vid_ctx->cur_vid_state],
			to_ticks, swvsync_1time);
	return expire;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static enum hrtimer_restart drm_mfr_swt_prevsync_cb(struct hrtimer *hrtvsync)
{
	struct swtimer *swt = NULL;
	struct drm_mfr_ctx *mfr_ctx = NULL;
	struct drm_swt_ctx *swt_ctx = NULL;
	struct drm_vid_ctx *vid_ctx = NULL;
	const struct drm_mfr_vid_stat_handler *sh = NULL;
	enum hrtimer_restart rtn = HRTIMER_RESTART;
	unsigned long flags = 0;
	ktime_t basetime;
	ktime_t nowtime;
	long long delay_time = 0;

	DRMMFR_DBG(">>>");

	swt = container_of(hrtvsync, struct swtimer, hrt);
	if (swt == NULL) {
		DRMMFR_ERR("failed to get swt");
		rtn = HRTIMER_NORESTART;
		goto err;
	}

	swt_ctx = container_of(swt, struct drm_swt_ctx, prevsync_timer);
	if (swt_ctx == NULL) {
		DRMMFR_ERR("failed to get swt_ctx");
		rtn = HRTIMER_NORESTART;
		goto err;
	}

	basetime.tv64 = swt->base + swt->expire;
	nowtime = ktime_get();
	delay_time = (ktime_to_ns(ktime_sub(nowtime, basetime)) / 1000);
	if (delay_time > 1500) {	// move next HWvsync for delay time upper 1.5ms
		spin_lock_irqsave(&swt_ctx->vsync_lock, flags);
		swt->state = SWT_IDLE;
		spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags);
		rtn = HRTIMER_NORESTART;
		DRMMFR_DBG("restart = %d expire = %llu", rtn, swt->expire);
		goto skip_current_prevsync;
	}

	mfr_ctx = swt_ctx->mfr_ctx;
	vid_ctx = mfr_ctx->vid_ctx;

	if (vid_ctx == NULL) {
		DRMMFR_ERR("failed to get vid_ctx");
		rtn = HRTIMER_NORESTART;
		goto err;
	}

	spin_lock_irqsave(&swt_ctx->vsync_lock, flags);

	if ((!(swt_ctx->enabled_display)) && (swt->state == SWT_RUNNING)) {
		swt->state = SWT_IDLE;
		spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags);
		DRMMFR_ERR("display was off");
		rtn = HRTIMER_NORESTART;
	} else {
		sh = drm_mfr_vid_get_target_state_handler(
						vid_ctx->cur_vid_state);
		drm_mfr_vid_cur_vid_state_prevhandler(
						sh, vid_ctx, VSYNC_TYPE_SW);

		swt->state = SWT_IDLE;
		if ((vid_ctx->vid_output_state != VID_OUTPUT_STARTED) &&
		    (swt_ctx->swvsync_timer.state == SWT_IDLE)) {
			drm_mfr_swt_swvsync_ctrl(swt_ctx, 1);
		}
		spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags);

		// hrtimer_forward_now(hrtvsync, ns_to_ktime(swt->expire));
		rtn = HRTIMER_NORESTART;

	}

	//DRMMFR_DBG("<<< restart = %d expire = %llu", rtn, swt->expire);
	DRMMFR_DBG("<<<");

skip_current_prevsync:
err:
	return rtn;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static enum hrtimer_restart drm_mfr_swt_swvsync_cb(struct hrtimer *hrtvsync)
{
	struct swtimer *swt = NULL;
	struct drm_mfr_ctx *mfr_ctx = NULL;
	struct drm_swt_ctx *swt_ctx = NULL;
	struct drm_vid_ctx *vid_ctx = NULL;
	const struct drm_mfr_vid_stat_handler *sh = NULL;
	enum hrtimer_restart rtn = HRTIMER_RESTART;
	unsigned long flags = 0;
	enum drm_mfr_vid_output_state last_vid_ouput_state;
	bool expect_swvsync = false;
	bool skip_handling = false;

	DRMMFR_DBG(">>>");

	swt = container_of(hrtvsync, struct swtimer, hrt);
	if (swt == NULL) {
		DRMMFR_ERR("failed to get swt");
		rtn = HRTIMER_NORESTART;
		goto err;
	}

	swt_ctx = container_of(swt, struct drm_swt_ctx, swvsync_timer);
	if (swt_ctx == NULL) {
		DRMMFR_ERR("failed to get swt_ctx");
		rtn = HRTIMER_NORESTART;
		goto err;
	}

	mfr_ctx = swt_ctx->mfr_ctx;
	vid_ctx = mfr_ctx->vid_ctx;

	if (vid_ctx == NULL) {
		DRMMFR_ERR("failed to get vid_ctx");
		rtn = HRTIMER_NORESTART;
		goto err;
	}

	spin_lock_irqsave(&swt_ctx->vsync_lock, flags);

	if ((!(swt_ctx->enabled_display)) && (swt->state == SWT_RUNNING)) {
		swt->state = SWT_IDLE;
		swt_ctx->finish_1st_swvsync_restart = false;
		spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags);
		DRMMFR_ERR("display was off");
		rtn = HRTIMER_NORESTART;
	} else {

		//expect_swvsync = (vid_ctx->vblank_refcount < 1);
		expect_swvsync =
			(vid_ctx->vid_output_state != VID_OUTPUT_STARTED);

#ifdef DRMMFR_ONLYSWT
		expect_swvsync = true;
#endif /* DRMMFR_ONLYSWT */

		if (expect_swvsync) {
			drm_mfr_vid_update_last_expectvsynctype_v_ktime(
							vid_ctx, ktime_get());

			last_vid_ouput_state =
				drm_mfr_vid_update_vid_output_status_on_vsync(
								vid_ctx);

			sh = drm_mfr_vid_get_target_state_handler(
							vid_ctx->cur_vid_state);

			drm_mfr_vid_add_ticks(vid_ctx, VSYNC_TYPE_SW);

			skip_handling =
			    drm_mfr_vid_is_skip_swvsync_handling(
			                    vid_ctx, last_vid_ouput_state);

			if (skip_handling) {
				DRMMFR_DBG("video is stopping.. "
					"skip current-state-handler and..");
			} else {
				drm_mfr_vid_handle_onvsync_cmn(sh, vid_ctx,
								VSYNC_TYPE_SW);
				    drm_mfr_vid_disable_rsc_onswvsync_ifneeded(
							sh, vid_ctx);
			}

			drm_mfr_vid_update_video_start_byswvsync(vid_ctx,
							last_vid_ouput_state);

#ifdef DRMMFR_ONLYSWT
			if (skip_handling) {
				drm_mfr_vid_vblank_virt_callback(
						vid_ctx, VSYNC_TYPE_SW);
			if ((mfr_ctx->swt_ctx->prevsync_timer.state == SWT_IDLE)
			    (!vid_ctx->request_stop_vid_on_next_vsync) &&
			    && (vid_ctx->vid_output_state == VID_OUTPUT_STARTED)
			    && drm_mfr_vid_need_video_stop_state(
						vid_ctx->cur_vid_state)) {
				drm_mfr_swt_prevsync_ctrl(mfr_ctx->swt_ctx, 1);
			    }
			}
#else /* DRMMFR_ONLYSWT */
			// send vsync event by next hw-vsync
			// if video is while doing output.
			expect_swvsync =
			  (vid_ctx->vid_output_state != VID_OUTPUT_STARTED);

			if (expect_swvsync && mfr_ctx->vsync_is_requested) {
				if (skip_handling) {
					// skip send vsync evnet..
				} else {
					drm_mfr_vid_vblank_virt_callback(
							vid_ctx, VSYNC_TYPE_SW);
				}
			}
#endif /* DRMMFR_ONLYSWT */
		}

		if (expect_swvsync) {
			ktime_t basevsync_ktime =
				drm_mfr_vid_get_last_expectvsynctype_v_ktime(
								vid_ctx);
			swt->expire =
				drm_mfr_swt_calc_next_swvsync(
					swt_ctx,
					basevsync_ktime,
					swt_ctx->finish_1st_swvsync_restart);
				swt_ctx->finish_1st_swvsync_restart = true;
			swt->base   = ktime_to_ns(ktime_get());
		} else {
			swt->state = SWT_IDLE;
			swt->expire = 0;
			swt_ctx->finish_1st_swvsync_restart = false;
		}

		if (expect_swvsync) {
			hrtimer_forward_now(hrtvsync, ns_to_ktime(swt->expire));
			rtn = HRTIMER_RESTART;
		} else {
			rtn = HRTIMER_NORESTART;
		}
		spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags);
	}

	DRMMFR_DBG("<<< restart = %d expire = %llu", rtn, swt->expire);

err:
	return rtn;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_restart_video_and_wait(struct drm_mfr_ctx *mfr_ctx,
			int restart_vid/*, int cont_commit_frame_count*/)
{
	unsigned long flags_mfr = 0;
	unsigned long flags_swt = 0;
	bool wait_restart = false;
	int to = 1;
	int ret = 0;
	struct drm_swt_ctx *swt_ctx = mfr_ctx->swt_ctx;

	struct drm_vid_ctx *vid_ctx = mfr_ctx->vid_ctx;
	DRMMFR_DBG("%s: is called: restart_vid", __func__);

	// wait for re-started-video when video isn't started...
	spin_lock_irqsave(&mfr_ctx->mfr_lock, flags_mfr);
	spin_lock_irqsave(&swt_ctx->vsync_lock, flags_swt);
	if ((vid_ctx->cur_vid_state == POWER_OFF) ||
				(vid_ctx->cur_vid_state == HR_VIDEO_MFR_MAX) ||
				(vid_ctx->cur_vid_state == SUSPEND)) {
		DRMMFR_DBG("%s: not need video-start-wait: state = %s",
				__func__,
				state_str[vid_ctx->cur_vid_state]);
		spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags_swt);
		spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);
		return 0;
	}

	DRMMFR_DBG("%s: change vid_state from %s "
		"to %s cur_vid_ticks = %d",
			__func__,
			state_str[mfr_ctx->vid_ctx->cur_vid_state],
			state_str[PRE_COMMIT],
			mfr_ctx->vid_ctx->cur_vid_ticks);
	vid_ctx->cur_vid_state = PRE_COMMIT;
	vid_ctx->cur_vid_ticks = 0;
	vid_ctx->cont_commit_ticks = 1;

	if (vid_ctx->vid_output_state == VID_OUTPUT_STARTED) {
		wait_restart = false;
	} else  {
		wait_restart = true;
	}

	if (wait_restart) {
		if (!vid_ctx->wait_for_video_start) {
			vid_ctx->wait_for_video_start = true;
			reinit_completion(&vid_ctx->video_started_comp);
		}
		DRMMFR_DBG("%s: wait for restarted video >>>", __func__);
		spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags_swt);
		spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);
		drm_mfr_swt_stop_long_swvsync_and_restart(mfr_ctx);
		to = wait_for_completion_timeout(&vid_ctx->video_started_comp,
				msecs_to_jiffies(COMMON_WAIT_FOR_VID_START_MS));
		DRMMFR_DBG("%s: wait for restarted video <<<", __func__);
		spin_lock_irqsave(&mfr_ctx->mfr_lock, flags_mfr);
		spin_lock_irqsave(&swt_ctx->vsync_lock, flags_swt);
		if (to <= 0) {
			vid_ctx->wait_for_video_start = false;
			DRMMFR_ERR("%s: failed to wait for "
				"started video..", __func__);
			ret = 1;
		}
	}
	spin_unlock_irqrestore(&swt_ctx->vsync_lock, flags_swt);
	spin_unlock_irqrestore(&mfr_ctx->mfr_lock, flags_mfr);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_wakeup_waiting_video(struct drm_vid_ctx *vid_ctx)
{
	if (vid_ctx->wait_for_video_start) {
		DRMMFR_DBG("%s: wakeup wating-video", __func__);
		vid_ctx->wait_for_video_start = false;
		complete_all(&vid_ctx->video_started_comp);
	}
	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_state_pvhandler_updateframe(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	if (drm_mfr_vid_can_stop_video_onprevsync(vid_ctx)) {
		drm_mfr_vid_enable_output_video(vid_ctx, 0);
	} else {
		vid_ctx->request_stop_vid_on_next_vsync = true;
	}
	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_state_pvhandler_repeatframe(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	bool stop_video = false;
	int ticks_for_check = sh->ticks_for_nextstate - 1;

	if (vid_ctx->cur_vid_ticks < ticks_for_check) {
		stop_video = false;
	} else if (vid_ctx->cur_vid_ticks == ticks_for_check) {
		stop_video = !drm_mfr_vid_need_reverse_polarity(vid_ctx);
		DRMMFR_DBG("ticks = %d, "
			"cur_pol_state = %s, cur_pol_sum = %d,"
			" vtype=%s",
			vid_ctx->cur_vid_ticks,
			pol_type_str[vid_ctx->pol_state], vid_ctx->pol_sum,
			vsync_type_str[vtype]);
	} else {
		stop_video = true;
	}

	if (stop_video) {
		DRMMFR_DBG("ticks = %d, stop_video = %d, vtype=%s",
						vid_ctx->cur_vid_ticks,
						stop_video,
						vsync_type_str[vtype]);
		if (drm_mfr_vid_can_stop_video_onprevsync(vid_ctx)) {
			drm_mfr_vid_enable_output_video(vid_ctx, 0);
		} else {
			vid_ctx->request_stop_vid_on_next_vsync = true;
		}
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_state_pvhandler_hr_video_refresh(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	DRMMFR_DBG("ticks = %d vtype=%s",
					vid_ctx->cur_vid_ticks,
					vsync_type_str[vtype]);

	if (drm_mfr_vid_can_stop_video_onprevsync(vid_ctx)) {
		drm_mfr_vid_enable_output_video(vid_ctx, 0);
	} else {
		vid_ctx->request_stop_vid_on_next_vsync = true;
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_state_pvhandler_hr_video_mfr_refresh(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	if (drm_mfr_vid_can_stop_video_onprevsync(vid_ctx)) {
		drm_mfr_vid_enable_output_video(vid_ctx, 0);
	} else {
		vid_ctx->request_stop_vid_on_next_vsync = true;
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_state_vhandler_poweron(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	drm_mfr_vid_update_mfr_params(vid_ctx);

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_state_vhandler_blankframe(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	if (vtype != VSYNC_TYPE_SW) {
		return 0;
	}

	if (vid_ctx->cur_vid_ticks >=
	    sh->ticks_for_nextstate - drm_mfr_early_rsc_on_ticks) {
		drm_mfr_vid_enable_rsc(vid_ctx, false/*sync*/);
	}

	if (vid_ctx->cur_vid_ticks >= sh->ticks_for_nextstate) {
		DRMMFR_DBG("ticks = %d vtype=%s",
							vid_ctx->cur_vid_ticks,
							vsync_type_str[vtype]);
		if (!drm_mfr_vid_safe_enable_output_video(vid_ctx)) {
			vid_ctx->cur_vid_ticks--; // ...
		}
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_state_vhandler_hr_video_prepare(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	if (vtype != VSYNC_TYPE_SW) {
		return 0;
	}

	if (vid_ctx->cur_vid_ticks >=
	    sh->ticks_for_nextstate - drm_mfr_early_rsc_on_ticks) {
		drm_mfr_vid_enable_rsc(vid_ctx, false/*sync*/);
	}

	if (vid_ctx->cur_vid_ticks >= sh->ticks_for_nextstate) {
		DRMMFR_DBG("ticks = %d vtype=%s",
						vid_ctx->cur_vid_ticks,
						vsync_type_str[vtype]);
		if (!drm_mfr_vid_safe_enable_output_video(vid_ctx)) {
			vid_ctx->cur_vid_ticks--; // ...
		}
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_state_vhandler_hr_video_mfr_wait(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	DRMMFR_DBG("is called vtype = %s", vsync_type_str[vtype]);

	if (vid_ctx->cur_vid_ticks >
			vid_ctx->mfr_wait_ticks - drm_mfr_early_rsc_on_ticks) {
		drm_mfr_vid_enable_rsc(vid_ctx, false/*sync*/);
	}

	if (vid_ctx->cur_vid_ticks >= vid_ctx->mfr_wait_ticks) {
		DRMMFR_DBG("ticks = %d vtype=%s",
						vid_ctx->cur_vid_ticks,
						vsync_type_str[vtype]);
		if (!drm_mfr_vid_safe_enable_output_video(vid_ctx)) {
			vid_ctx->cur_vid_ticks--; // ...
		}
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_state_vhandler_suspend(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	if (vtype != VSYNC_TYPE_SW) {
		return 0;
	}

	if ((vid_ctx->cur_vid_ticks >= 1)
			&& (vid_ctx->vid_output_state == VID_OUTPUT_STOPPED)) {
		DRMMFR_DBG("call drm_mfr_vid_enable_output_video");
		if (!drm_mfr_vid_safe_enable_output_video(vid_ctx)) {
			vid_ctx->cur_vid_ticks--; // ...
		}
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_state_vhandler_commit(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	drm_mfr_vid_update_mfr_params(vid_ctx);

	if (vtype != VSYNC_TYPE_SW) {
		return 0;
	}

	if (!drm_mfr_vid_safe_enable_output_video(vid_ctx)) {
		vid_ctx->cur_vid_ticks--; // ...
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_state_vhandler_pre_commit(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	drm_mfr_vid_update_mfr_params(vid_ctx);

	if (vtype != VSYNC_TYPE_SW) {
		return 0;
	}

	if (vid_ctx->cur_vid_ticks != 1) {
		return 0;
	}

	if (!drm_mfr_vid_safe_enable_output_video(vid_ctx)) {
		vid_ctx->cur_vid_ticks--; // ...
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_vid_add_ticks(struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	int swvsync_1time;
	int long_vsync_ns;
	int add_ticks;

	if (!vid_ctx->long_swvsync) {
		add_ticks = 1;
	} else {
		swvsync_1time = drm_mfr_swt_calc_swvsync_ns(
						vid_ctx->mfr_ctx->swt_ctx);
		long_vsync_ns = ktime_to_ns(
					ktime_sub(ktime_get(),
					vid_ctx->long_swvsync_base_ktime));

		if (long_vsync_ns < 0) {
			DRMMFR_ERR("error.. long_swvsync ns = %d, adjust...",
								long_vsync_ns);
			long_vsync_ns = swvsync_1time;
		} else if (long_vsync_ns >= DRMMFR_1SEC_NS) {
			DRMMFR_ERR("error.. long_swvsync ns = %d, adjust...",
								long_vsync_ns);
			long_vsync_ns = DRMMFR_1SEC_NS;
		}
		add_ticks = (long_vsync_ns + (swvsync_1time/2)) / swvsync_1time;
		vid_ctx->long_swvsync = false;
	}
	vid_ctx->cur_vid_ticks += add_ticks;


	DRMMFR_DBG("current-tick = %d on %s context state = %s",
					vid_ctx->cur_vid_ticks,
					vsync_type_str[vtype],
					state_str[vid_ctx->cur_vid_state]);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_vid_handle_onvsync_cmn(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	drm_mfr_vid_cur_vid_state_vhandler(sh, vid_ctx, vtype);
	drm_mfr_vid_update_state_onvsync(sh, vid_ctx, vtype);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_vid_cur_vid_state_prevhandler(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	if (sh->prev_handler) {
		sh->prev_handler(sh, vid_ctx, vtype);
	}
	return;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_vid_cur_vid_state_vhandler(
	const struct drm_mfr_vid_stat_handler *sh, struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	if (sh->v_handler) {
		sh->v_handler(sh, vid_ctx, vtype);
	}
	return;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_vblank_virt_callback(struct drm_vid_ctx *vid_ctx,
						enum drm_mfr_vsync_type vtype)
{
	const struct drm_mfr_controller *mfr_ctrl = vid_ctx->mfr_ctrl;

	DRMMFR_DBG("send vsync_event(%d)",
					vid_ctx->mfr_ctx->vsync_is_requested);
	if (vid_ctx->mfr_ctx->vsync_is_requested == true) {
		mfr_ctrl->vblank_callback(mfr_ctrl->priv_data);
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static enum drm_mfr_hr_video_state drm_mfr_get_nextstate_fromcommit(
						struct drm_vid_ctx *vid_ctx)
{
	enum drm_mfr_hr_video_state nextstate =
					vid_ctx->cur_vid_state;

	if (vid_ctx->mfr_rate == 1) {
		nextstate = UPDATE_FRAME;
	} else if (vid_ctx->mfr_rate == vid_ctx->vid_rate) {
		nextstate = HR_VIDEO_MFR_MAX;
	} else /*if (canTranslateMFR) */{
		nextstate = HR_VIDEO_MFR_REFRESH;
	}
	return nextstate;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_update_mfr_wait_ticks_ifneeded(struct drm_vid_ctx *vid_ctx)
{
	int mfr_wait_ticks;
	struct drm_swt_ctx *swt_ctx = vid_ctx->mfr_ctx->swt_ctx;
	mfr_wait_ticks =  (swt_ctx->swvsync_rate / vid_ctx->mfr_rate) - 1;
	if (vid_ctx->mfr_wait_ticks != mfr_wait_ticks) {
		vid_ctx->mfr_wait_ticks = mfr_wait_ticks;
		DRMMFR_DBG("%s: mfr_wait_ticks = %d\n",
					__func__, vid_ctx->mfr_wait_ticks);
	}
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_update_state_onvsync(
	const struct drm_mfr_vid_stat_handler *sh,
		struct drm_vid_ctx *vid_ctx, enum drm_mfr_vsync_type vtype)
{
	int cur_vid_ticks;
	enum drm_mfr_hr_video_state willchangestate;
	int ticks_for_nextstate;

	cur_vid_ticks = vid_ctx->cur_vid_ticks;
	willchangestate = vid_ctx->cur_vid_state;
	ticks_for_nextstate = sh->ticks_for_nextstate;

	switch (vid_ctx->cur_vid_state) {
	case POWER_ON: {
		if (cur_vid_ticks >= sh->ticks_for_nextstate) {
			willchangestate = COMMIT;
		}
		break;
	}
	case UPDATE_FRAME: {
		willchangestate = BLANK_FRAME;
		break;
	}
	case BLANK_FRAME: {
		if (cur_vid_ticks >= sh->ticks_for_nextstate) {
			willchangestate = REPEAT_FRAME;
		}
		break;
	}
	case REPEAT_FRAME: {
		// we will check polarity-adjust-frame.
		if (vid_ctx->vid_output_state == VID_OUTPUT_STOPPED) {
			willchangestate = WAIT_FRAME;
		}
		break;
	}
	case WAIT_FRAME: {
		willchangestate = HR_VIDEO_WAIT;
		break;
	}
	case HR_VIDEO_WAIT: {
		if (cur_vid_ticks >= vid_ctx->mfr_wait_ticks -
				DRMMFR_TRANSIT_PREPARE_TICKS) {
			willchangestate = HR_VIDEO_PREPARE;
		}
		break;
	}
	case HR_VIDEO_PREPARE: {
		if (cur_vid_ticks >= ticks_for_nextstate) {
			willchangestate = HR_VIDEO_REFRESH;
		}
		break;
	}
	case HR_VIDEO_REFRESH: {
		willchangestate = HR_VIDEO_WAIT;
		break;
	}
	case HR_VIDEO_MFR_WAIT: {
		if (cur_vid_ticks >= vid_ctx->mfr_wait_ticks) {
			willchangestate = HR_VIDEO_MFR_REFRESH;
		}
		break;
	}
	case HR_VIDEO_MFR_REFRESH: {
		if (cur_vid_ticks >= ticks_for_nextstate) {
			willchangestate = HR_VIDEO_MFR_WAIT;
		}
		break;
	}
	case HR_VIDEO_MFR_MAX: {
		// keep current-state
		break;
	}
	case SUSPEND: {
		// keep current-state;
		break;
	}
	case COMMIT: {
		if (cur_vid_ticks >=
			ticks_for_nextstate + vid_ctx->cont_commit_ticks) {
			willchangestate =
				drm_mfr_get_nextstate_fromcommit(vid_ctx);
			drm_mfr_update_mfr_wait_ticks_ifneeded(vid_ctx);

			if (vid_ctx->cont_commit_ticks) {
				DRMMFR_DBG("clear cont_vid_ticks = %d",
					vid_ctx->cont_commit_ticks);
				vid_ctx->cont_commit_ticks = 0;
			}
		}
		break;
	}
	case PRE_COMMIT: {
		if (cur_vid_ticks >= ticks_for_nextstate) {
			DRMMFR_ERR("isn't notify commit... "
				"translate to COMMIT");
			willchangestate = COMMIT;
		}
		break;
	}
	case POWER_OFF: {
		// keep current-state;
		break;
	}
	default: {
		// err... unknown state...;
		DRMMFR_DBG("unhandleable vid_state value = %d",
						vid_ctx->cur_vid_state);
	}
	}

	if (vid_ctx->cur_vid_state != willchangestate) {
		DRMMFR_DBG("change vid_state from"
			" %s to %s :"
			" vsync_type = %s"
			" cur_vid_ticks = %d,",
			state_str[vid_ctx->cur_vid_state],
			state_str[willchangestate],
			vsync_type_str[vtype],
			vid_ctx->cur_vid_ticks);

		vid_ctx->cur_vid_state = willchangestate;
		vid_ctx->cur_vid_ticks = 0;
	}

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_vid_init_polarity_state(struct drm_vid_ctx *vid_ctx)
{
	vid_ctx->pol_state = POL_STATE_INIT;
	vid_ctx->pol_sum = 0;
	vid_ctx->pol_checked_ktime   = ktime_get();
	vid_ctx->last_expectvsynctype_v_ktime = vid_ctx->pol_checked_ktime;
	vid_ctx->unbalanced_polarity = false;
	DRMMFR_DBG("called");
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_vid_update_polarity_onhwvsync(struct drm_vid_ctx *vid_ctx,
			enum drm_mfr_vid_output_state last_vid_output_state)
{
	int diff;
	ktime_t pol_checked_ktime = ktime_get();
	int vsync_ticks = 0;
	bool unbalanced_polarity = false;
	int pol_sum = 0;

	// 1st frame
	if (vid_ctx->pol_state == POL_STATE_INIT) {
		vid_ctx->pol_state = POL_STATE_POS;
		vid_ctx->pol_sum   = 0;
		vid_ctx->pol_checked_ktime = pol_checked_ktime;
		DRMMFR_DBG("1st-vsync by power-on-output-video.");
		return;
	}

	diff = ktime_to_us(ktime_sub(pol_checked_ktime,
						vid_ctx->pol_checked_ktime));
	vsync_ticks = diff / drm_mfr_swt_calc_hwvsync_ns(
						vid_ctx->mfr_ctx->swt_ctx);
	pol_sum = vid_ctx->pol_sum;

	if (vid_ctx->pol_state == POL_STATE_POS) {
		pol_sum += diff;
		vid_ctx->pol_state = POL_STATE_NEG;
		unbalanced_polarity = (pol_sum < 0);
		DRMMFR_DBG("next output frame is neg, "
			"cur-pol_sum = %d", pol_sum);

	} else /* if (vid_ctx->pol_state = POL_STATE_NEG) */{
		pol_sum -= diff;
		vid_ctx->pol_state = POL_STATE_POS;
		unbalanced_polarity = (pol_sum > 0);
		DRMMFR_DBG("next output frame is pos, "
						"cur-pol_sum = %d", pol_sum);
	}

	vid_ctx->pol_checked_ktime = pol_checked_ktime;
	vid_ctx->pol_sum = pol_sum;
	vid_ctx->unbalanced_polarity = unbalanced_polarity;
	return;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_need_reverse_polarity(struct drm_vid_ctx *vid_ctx)
{
	return vid_ctx->unbalanced_polarity;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_enable_output_video(struct drm_vid_ctx *vid_ctx,
						bool en)
{
	const struct drm_mfr_controller *mfr_ctrl = vid_ctx->mfr_ctrl;

	// already requested start video output
	if (en && (vid_ctx->vid_output_state == VID_OUTPUT_STARTED)) {
		DRMMFR_DBG("output video was started(or staring)"
			"(vid_output_state = %s)",
			vid_output_state_str[vid_ctx->vid_output_state]);
		return true;
	}

	// already requested stop video output
	if (!en && ((vid_ctx->vid_output_state == VID_OUTPUT_STOPPED)
		|| (vid_ctx->vid_output_state == VID_OUTPUT_STOPPING))) {

		DRMMFR_DBG("stop output video was requested"
			"(vid_output_state = %s)",
			vid_output_state_str[vid_ctx->vid_output_state]);
		return true;
	}

	if (en /*&& (vid_ctx->vid_output_state != VID_OUTPUT_STARTED)*/) {

		// request starting video to mfr_controller
		if (vid_ctx->vid_output_state == VID_OUTPUT_STOPPING) {
			DRMMFR_DBG("re-enable_output_vid(1) "
					" vid_state = %s",
			vid_output_state_str[vid_ctx->vid_output_state]);
		} else {
		/*if (vid_ctx->vid_output_state != VID_OUTPUT_STOPPING) */
			//DRMMFR_DBG("call prepare_restart_vid() ");
			//mfr_ctrl->prepare_restart_vid(mfr_ctrl->priv_data);
		}
		if (mfr_ctrl->enable_output_vid(mfr_ctrl->priv_data,1)) {
			DRMMFR_DBG("failed to enable_output_vid(1)");
		}
		DRMMFR_DBG("change output_vid_status from %s to %s",
			vid_output_state_str[vid_ctx->vid_output_state],
			vid_output_state_str[VID_OUTPUT_STARTED]);
		vid_ctx->vid_output_state = VID_OUTPUT_STARTED;
		vid_output_cnt++;

	} else if (!en && vid_ctx->vid_output_state == VID_OUTPUT_STARTED) {
		// request stopping video to mfr_controller
		if (mfr_ctrl->enable_output_vid(mfr_ctrl->priv_data,0)) {
			DRMMFR_DBG("failed to enable_output_vid(0)");
		}
		DRMMFR_DBG("change output_vid_status from %s to %s",
			vid_output_state_str[vid_ctx->vid_output_state],
			vid_output_state_str[VID_OUTPUT_STOPPING]);
		vid_ctx->vid_output_state = VID_OUTPUT_STOPPING;
	}
	return true;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_safe_enable_output_video(struct drm_vid_ctx *vid_ctx)
{
	bool clocks_is_enabled;

	if (vid_ctx->vid_output_state == VID_OUTPUT_STARTED) {
		// ... video-stop timer isn't called..
		return true;
	}

	if (vid_ctx->vid_output_state != VID_OUTPUT_STOPPED) {
		goto video_isnot_stopped;
	}

	clocks_is_enabled
		= drm_mfr_psv_is_clocks_enabled(vid_ctx->mfr_ctx->psv_ctx);
	if (!clocks_is_enabled) {
		goto clock_isnot_enabled;
	}

	drm_mfr_vid_enable_output_video(vid_ctx, 1);
	return true;

video_isnot_stopped:
	DRMMFR_ERR("failed to output vid: "
		"video_isn't_stopped vid_status = %s, ticks = %d",
			state_str[vid_ctx->cur_vid_state],
			vid_ctx->cur_vid_ticks);
	return false;

clock_isnot_enabled:
	DRMMFR_ERR("failed to output vid: "
		"clocks_is_enabled = %d, vid_status = %s, ticks = %d",
			clocks_is_enabled,
			state_str[vid_ctx->cur_vid_state],
			vid_ctx->cur_vid_ticks);
	return false;

}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_vid_failsafe_enable_output_video(
						struct drm_vid_ctx *vid_ctx)
{
	struct drm_swt_ctx *swt_ctx;

	swt_ctx = vid_ctx->mfr_ctx->swt_ctx;

	DRMMFR_ERR("%s: try_enable output video.(failsafe..)", __func__);

	drm_mfr_swt_prevsync_ctrl(swt_ctx, 0);
	drm_mfr_swt_swvsync_ctrl(swt_ctx, 0);
	drm_mfr_vid_safe_enable_output_video(vid_ctx);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_keep_enabled_rsc(struct drm_vid_ctx *vid_ctx)
{
	return vid_ctx->keep_enabled_rsc;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_set_keep_enabled_rsc(struct drm_vid_ctx *vid_ctx,
								bool keep)
{
	return vid_ctx->keep_enabled_rsc = keep;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_enable_clocks(struct drm_vid_ctx *vid_ctx,
							bool en, bool needsync)
{
	int ret = 0;
	unsigned long flags_psv = 0;
	bool is_exec = false;
	enum drm_mfr_psv_state pwr = en ? PWR_ON : PWR_OFF;
	struct drm_mfr_ctx *mfr_ctx = vid_ctx->mfr_ctx;

	if (!drm_mfr_ctrl_clocks) {
		return 0;
	}

	spin_lock_irqsave(&mfr_ctx->psv_lock, flags_psv);
	if (en) {
		if (vid_ctx->clocks_refcount == 0) {
			is_exec = true;
		}
		vid_ctx->clocks_refcount = 1;
	} else {
		vid_ctx->clocks_refcount--;
		if (vid_ctx->clocks_refcount == 0) {
			is_exec = true;
		} else if (vid_ctx->clocks_refcount < 0) {
			DRMMFR_DBG("already disabled");
			vid_ctx->clocks_refcount = 0;
			spin_unlock_irqrestore(
				&mfr_ctx->psv_lock, flags_psv);
			return ret;
		}
	}

	if (!is_exec) {
		spin_unlock_irqrestore(&mfr_ctx->psv_lock, flags_psv);
		return 0;
	}

	drm_mfr_psv_req_clocks(mfr_ctx->psv_ctx, pwr);

	if (needsync) {
		spin_unlock_irqrestore(&mfr_ctx->psv_lock, flags_psv);
		drm_mfr_psv_req_commit_sync(mfr_ctx->psv_ctx);
	} else {
		spin_unlock_irqrestore(&mfr_ctx->psv_lock, flags_psv);
	}
	if (ret) {
		DRMMFR_DBG("failed to enable_clocks(%d)", en);
	}
	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_enable_hwvsync(struct drm_vid_ctx *vid_ctx,
							bool en, bool needsync)
{
	int ret = 0;
	unsigned long flags_psv = 0;
	bool is_exec = false;

	spin_lock_irqsave(&vid_ctx->mfr_ctx->psv_lock, flags_psv);
	if (en) {
		if (vid_ctx->vblank_refcount == 0) {
			is_exec = true;
		}
		vid_ctx->vblank_refcount = 1;
	} else {
		vid_ctx->vblank_refcount--;
		if (vid_ctx->vblank_refcount == 0) {
			is_exec = true;
		} else if (vid_ctx->vblank_refcount < 0) {
			DRMMFR_DBG("already disabled");
			vid_ctx->vblank_refcount = 0;
			spin_unlock_irqrestore(
				&vid_ctx->mfr_ctx->psv_lock, flags_psv);
			return ret;
		}
	}

	if (is_exec) {
		DRMMFR_DBG("enable_vsync(%d)", en);
		// will move implementation into rsc-control framework
		drm_mfr_psv_req_vsync_intr(vid_ctx->mfr_ctx->psv_ctx, en);
		if (needsync) {
			spin_unlock_irqrestore(
				&vid_ctx->mfr_ctx->psv_lock, flags_psv);
			ret = drm_mfr_psv_req_commit_sync(
				vid_ctx->mfr_ctx->psv_ctx);
		} else {
			ret = 0;
			spin_unlock_irqrestore(
				&vid_ctx->mfr_ctx->psv_lock, flags_psv);
		}
		if (ret) {
			DRMMFR_DBG("failed to enable_vsync(%d)", en);
		}
	} else {
		spin_unlock_irqrestore(&vid_ctx->mfr_ctx->psv_lock, flags_psv);
	}

	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_enable_underrun(struct drm_vid_ctx *vid_ctx,
							bool en, bool needsync)

{
	int ret = 0;
	unsigned long flags_psv = 0;
	struct drm_mfr_ctx *mfr_ctx = vid_ctx->mfr_ctx;
	struct drm_psv_ctx *psv_ctx = mfr_ctx->psv_ctx;
	bool is_exec = false;

	spin_lock_irqsave(&vid_ctx->mfr_ctx->psv_lock, flags_psv);
	if (en) {
		if (vid_ctx->underrun_refcount == 0) {
			is_exec = true;
		}
		vid_ctx->underrun_refcount = 1;
	} else {
		vid_ctx->underrun_refcount--;
		if (vid_ctx->underrun_refcount == 0) {
			is_exec = true;
		} else if (vid_ctx->underrun_refcount < 0) {
			DRMMFR_DBG("already disabled");
			vid_ctx->underrun_refcount = 0;
			spin_unlock_irqrestore(
				&mfr_ctx->psv_lock, flags_psv);
			return ret;
		}
	}

	if (!is_exec) {
		spin_unlock_irqrestore(
			&mfr_ctx->psv_lock, flags_psv);
		return 0;
	}

	drm_mfr_psv_req_underrun_intr(psv_ctx, en);

	DRMMFR_DBG("enable_underrun(%d)", en);
	if (needsync) {
		spin_unlock_irqrestore(&mfr_ctx->psv_lock, flags_psv);
		ret = drm_mfr_psv_req_commit_sync(psv_ctx);
	} else {
		ret = 0;
		spin_unlock_irqrestore(&mfr_ctx->psv_lock, flags_psv);
	}
	if (ret) {
		DRMMFR_DBG("failed to enable_underrun(%d)", en);
	}
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_vid_enable_rsc(struct drm_vid_ctx *vid_ctx, bool needsync)
{
	// drm_mfr_vid_enable_qos(vid_ctx, true, false);
	drm_mfr_vid_enable_clocks(vid_ctx, true, false/* sync */);
	drm_mfr_vid_enable_hwvsync(vid_ctx, true, false/*sync*/);
	drm_mfr_vid_enable_underrun(vid_ctx, true, false/*sync*/);
	if (needsync) {
		drm_mfr_psv_req_commit_sync(vid_ctx->mfr_ctx->psv_ctx);
	} else {
		drm_mfr_psv_req_commit(vid_ctx->mfr_ctx->psv_ctx);
	}
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_vid_disable_rsc_onswvsync_ifneeded(
				const struct drm_mfr_vid_stat_handler *sh,
						struct drm_vid_ctx *vid_ctx)
{
	bool req_commit = false;
	if (!drm_mfr_vid_can_disable_rsc_onswvsync(sh, vid_ctx)) {
		return;
	}

	if (vid_ctx->vblank_refcount > 0) {
		req_commit = true;
		drm_mfr_vid_enable_hwvsync(vid_ctx ,false, false/*sync*/);
	}

	if (vid_ctx->underrun_refcount > 0) {
		req_commit = true;
		drm_mfr_vid_enable_underrun(vid_ctx ,false, false/*sync*/);
	}

	if (vid_ctx->clocks_refcount > 0) {
		req_commit = true;
		drm_mfr_vid_enable_clocks(vid_ctx, false, false/*sync*/);
	}

	if (req_commit) {
		drm_mfr_psv_req_commit(vid_ctx->mfr_ctx->psv_ctx);
	}
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_can_use_long_swvsync(struct drm_vid_ctx *vid_ctx)
{
	bool can_use = true;

	can_use &= !vid_ctx->mfr_ctx->swt_ctx->reqeust_vsync_by_user;
	can_use &= !vid_ctx->mfr_ctx->vsync_is_requested;

	can_use &= (vid_ctx->vid_output_state == VID_OUTPUT_STOPPED);
	can_use &= (vid_ctx->vblank_refcount == 0);
	can_use &= (vid_ctx->underrun_refcount == 0);
	if (drm_mfr_ctrl_clocks) {
		can_use &= (vid_ctx->clocks_refcount == 0);
	}

	return can_use;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static enum drm_mfr_vid_output_state
		drm_mfr_vid_update_vid_output_status_on_vsync(
					struct drm_vid_ctx *vid_ctx)
{
	const struct drm_mfr_controller *mfr_ctrl = vid_ctx->mfr_ctrl;
	enum drm_mfr_vid_output_state last_state =
					vid_ctx->vid_output_state;

	if (vid_ctx->vid_output_state == VID_OUTPUT_STOPPING) {
		if (mfr_ctrl->is_video_stopped(mfr_ctrl->priv_data, true)) {
			DRMMFR_DBG("change output_vid_status from %s to %s",
				vid_output_state_str[vid_ctx->vid_output_state],
				vid_output_state_str[VID_OUTPUT_STOPPED]);

			mfr_ctrl->prepare_restart_vid(mfr_ctrl->priv_data);
			vid_ctx->vid_output_state = VID_OUTPUT_STOPPED;
		}

	}
		// send wakeup-signale for if waiting for this...
	return last_state;
}


/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static const struct drm_mfr_vid_stat_handler *
	drm_mfr_vid_get_target_state_handler(
				enum drm_mfr_hr_video_state cur_vid_state)
{
	const struct drm_mfr_vid_stat_handler *sh = NULL;
	if (POWER_ON > cur_vid_state || POWER_OFF < cur_vid_state) {
		DRMMFR_ERR("invalid vid_state = %d", cur_vid_state);
		cur_vid_state = POWER_OFF;
	}

	sh = &drm_mfr_state_handlers[cur_vid_state];
	return sh;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_update_video_start_byswvsync(struct drm_vid_ctx *vid_ctx,
			enum drm_mfr_vid_output_state last_vid_ouput_state)
{
	vid_ctx->video_start_byswvsync =
		(last_vid_ouput_state != VID_OUTPUT_STARTED) &&
		(vid_ctx->vid_output_state == VID_OUTPUT_STARTED);
	return vid_ctx->video_start_byswvsync;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_clear_video_start_byswvsync(struct drm_vid_ctx *vid_ctx)
{
	bool rtn = vid_ctx->video_start_byswvsync;
	vid_ctx->video_start_byswvsync = false;
	return rtn;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_vid_update_last_expectvsynctype_v_ktime(
			struct drm_vid_ctx *vid_ctx, ktime_t vsync_time)
{
	vid_ctx->last_expectvsynctype_v_ktime = vsync_time;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static ktime_t drm_mfr_vid_get_last_expectvsynctype_v_ktime(
						struct drm_vid_ctx *vid_ctx)
{
	return vid_ctx->last_expectvsynctype_v_ktime;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_can_stop_video_onprevsync(struct drm_vid_ctx *vid_ctx)
{
	bool canstop = true;
	// will write down check by line-ptr code...
	canstop = vid_ctx->mfr_ctrl->can_stop_video(
					vid_ctx->mfr_ctrl->priv_data);

	return canstop;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_vid_get_restart_vid_ticks(struct drm_vid_ctx *vid_ctx)
{
	int ticks_for_next;
	enum drm_mfr_hr_video_state vid_state = vid_ctx->cur_vid_state;
	const struct drm_mfr_vid_stat_handler *sh
			= drm_mfr_vid_get_target_state_handler(vid_state);

	switch(vid_state) {
	case BLANK_FRAME:
		ticks_for_next = sh->ticks_for_nextstate;
		break;
	case HR_VIDEO_PREPARE:
		ticks_for_next = sh->ticks_for_nextstate;
		break;
	case HR_VIDEO_MFR_WAIT:
		ticks_for_next = vid_ctx->mfr_wait_ticks;
		break;
	case SUSPEND:
	case COMMIT:
	case PRE_COMMIT:
		ticks_for_next = 1;
		break;
	default:
		DRMMFR_ERR("%s can't restart-video", state_str[vid_state]);
		return 0;
	}
	return ticks_for_next;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_can_disable_rsc_onswvsync(
				const struct drm_mfr_vid_stat_handler *sh,
						struct drm_vid_ctx *vid_ctx)
{

	int ticks_for_next = 0;
	int cur_vid_ticks = vid_ctx->cur_vid_ticks;
	bool canstop;

	if (drm_mfr_vid_keep_enabled_rsc(vid_ctx)) {
		DRMMFR_DBG("%s: next-commit is comming soon. keep enabled rsc "
			"state=%s, ticks = %d", __func__,
			state_str[vid_ctx->cur_vid_state], cur_vid_ticks);
		return false;
	}

	canstop = vid_ctx->vid_output_state == VID_OUTPUT_STOPPED;
	if (!canstop) {
		return canstop;
	}

	switch(vid_ctx->cur_vid_state) {
	case BLANK_FRAME:
	case WAIT_FRAME:
		ticks_for_next = sh->ticks_for_nextstate;
		break;
	case HR_VIDEO_WAIT:
		ticks_for_next = vid_ctx->mfr_wait_ticks
					- DRMMFR_TRANSIT_PREPARE_TICKS;

		break;
	case HR_VIDEO_MFR_WAIT:
		ticks_for_next = vid_ctx->mfr_wait_ticks;
		break;
	default:
		canstop = false;
	}

	if (vid_ctx->cur_vid_state != WAIT_FRAME) {
		canstop &= cur_vid_ticks <=
				ticks_for_next - drm_mfr_early_rsc_on_ticks;
	} else {
		canstop = true;
	}

	return canstop;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_is_skip_swvsync_handling(
			struct drm_vid_ctx *vid_ctx,
			enum drm_mfr_vid_output_state last_vid_ouput_state)
{
	bool skip = false;
	//const struct drm_mfr_controller *mfr_ctrl = vid_ctx->mfr_ctrl;

	//skip = (last_vid_ouput_state == VID_OUTPUT_STOPPING)
	// 	&& !mfr_ctrl->is_video_stopped(vid_ctx->mfr_ctrl->priv_data);
	skip = (last_vid_ouput_state == VID_OUTPUT_STOPPING)
		&& (vid_ctx->vid_output_state == VID_OUTPUT_STOPPING);

	return skip;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_is_restartvideo_nextswvsync(struct drm_vid_ctx *vid_ctx)
{
	bool rtn = false;
	int restart_ticks;
	if (vid_ctx->vid_output_state != VID_OUTPUT_STOPPED) {
		return rtn;
	}

	// is restart-video states..
	switch (vid_ctx->cur_vid_state) {
	case BLANK_FRAME:
	case HR_VIDEO_PREPARE:
	case HR_VIDEO_MFR_WAIT:
	//case SUSPEND:
	//case COMMIT:
	//case PRE_COMMIT:
		restart_ticks = drm_mfr_vid_get_restart_vid_ticks(vid_ctx);
		break;
	default:
		return rtn;
	}

	rtn = (restart_ticks == vid_ctx->cur_vid_ticks + 1);
	if (rtn) {
		DRMMFR_DBG("%s: ticks = %d, next-ticks timing "
				"will restart video",
				__func__, vid_ctx->cur_vid_ticks);
	}

	return rtn;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_vid_need_video_stop_state(
				enum drm_mfr_hr_video_state vid_state)
{
	int need = false;

	switch (vid_state) {
	case UPDATE_FRAME:
	case REPEAT_FRAME:
	case HR_VIDEO_REFRESH:
	case HR_VIDEO_MFR_REFRESH:
		need = true;
		break;
	default:
		need = false;
	}

	return need;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void drm_mfr_vid_update_mfr_params(struct drm_vid_ctx *vid_ctx)
{
	int mfr = 0;
	vid_ctx->vid_rate = drm_mfr_videorate;

	if (drm_mfr_spt_supported_mfr_rate(
		vid_ctx->mfr_ctx->spt_ctx, drm_mfr_mfrate)) {

		if (drm_mfr_swvsync_rate == 100) {
			mfr = drm_mfr_calcrate_mfrrate(drm_mfr_mfrate);
		} else {
			mfr = drm_mfr_mfrate;
		}
	} else {
		DRMMFR_ERR("%s: unsupport_mfr_rate(%d) is requested",
			__func__, drm_mfr_mfrate);
		if (drm_mfr_swvsync_rate == 100) {
			drm_mfr_mfrate = 40;
			mfr = drm_mfr_calcrate_mfrrate(drm_mfr_mfrate);
		} else {
			mfr = drm_mfr_mfrate = 30;
		}
	}
	DRMMFR_DBG("set mfrrate=%d", mfr);
	vid_ctx->mfr_rate = mfr;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_calcrate_mfrrate(int mfr)
{
	int calcrate_mfrrate = 33;

	switch (mfr) {
	case 1:
		calcrate_mfrrate = 1;
		break;
	case 10:
		calcrate_mfrrate = 8;
		break;
	case 15:
		calcrate_mfrrate = 12;
		break;
	case 20:
		calcrate_mfrrate = 16;
		break;
	case 30:
		calcrate_mfrrate = 25;
		break;
	case 40:
		calcrate_mfrrate = 33;
		break;
	case 60:
		calcrate_mfrrate = 50;
		break;
	case 100:
		calcrate_mfrrate = 100;
		break;
	default:
		DRMMFR_ERR("%s: invalid mfr", __func__);
		break;
	}
	DRMMFR_DBG("calcrate_mfrrate=%d",calcrate_mfrrate );
	return calcrate_mfrrate;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_spt_supported_mfr_rate(struct drm_spt_ctx *spt_ctx,
								int mfr_rate)
{
	int i = 0;
	for (; i < spt_ctx->mfr_spt_len; i++) {
		if (mfr_rate == spt_ctx->mfr_spt_values[i]) {
			break;
		}
	}
	return (i != spt_ctx->mfr_spt_len);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void drm_mfr_set_swvsync_rate(int fpslow_base, u32 bl_lvl)
{
	int swvsync_rate = 0;
	int mfr_rate = 0;
	struct drm_swt_ctx *swt_ctx = NULL;

	DRMMFR_DBG("fpslow_base = %d, bl_lvl = %d", fpslow_base, bl_lvl);

	switch(fpslow_base) {
	case DRM_BASE_FPS_30:
	case DRM_BASE_FPS_60:
		if(bl_lvl > 2569) {
			swvsync_rate = 120;
			mfr_rate = 60;
		} else if (bl_lvl <= 2569 && bl_lvl > 32) {
			swvsync_rate = 120;
			mfr_rate = 30;
		} else if (bl_lvl <= 32 ){
			swvsync_rate = 120;
			mfr_rate = 1;
		} else {
			swvsync_rate = 120;
			mfr_rate = 60;
		}
		break;
	case DRM_BASE_FPS_100:
	case DRM_BASE_FPS_120:
		if(bl_lvl > 2569) {
			swvsync_rate = 100;
			mfr_rate = 60;
		} else if (bl_lvl <= 2569 && bl_lvl > 32) {
			swvsync_rate = 100;
			mfr_rate = 40;
		} else if (bl_lvl <= 32 ){
			swvsync_rate = 100;
			mfr_rate = 1;
		} else {
			swvsync_rate = 100;
			mfr_rate = 60;
		}
		break;
	default:
		DRMMFR_ERR("%s: invalid fps_low_base", __func__);
		return;
	}

	DRMMFR_DBG("swvsync_rate = %d, mfr_rate = %d", swvsync_rate, mfr_rate);

	if (swvsync_rate == 0) {
		DRMMFR_ERR("%s: swvsync_rate isn't set", __func__);
		return;
	}

	if (!mfr_context || !mfr_context->swt_ctx) {
		DRMMFR_ERR("no context");
		return;
	}
	swt_ctx = mfr_context->swt_ctx;

	if (drm_mfr_swvsync_rate != swvsync_rate) {
		drm_mfr_suspend_ctrl(true);
		swt_ctx->swvsync_rate = drm_mfr_swvsync_rate = swvsync_rate;
		drm_mfr_mfrate = mfr_rate;
		drm_mfr_suspend_ctrl(false);
	} else if(drm_mfr_mfrate != mfr_rate) {
		drm_mfr_mfrate = mfr_rate;
	}
	DRMMFR_DBG("change drm_mfr_swvsync_rate to %d, drm_mfr_mfrate to %d",
					drm_mfr_swvsync_rate, drm_mfr_mfrate);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int __init drm_mfr_register(void)
{
	int ret = 0;

	DRMMFR_DBG("is called");

	// create mfr_ctx
	ret = drm_mfr_create_context();
	if (ret) {
		goto err;
	}

	// build swt(swvsync/prevsync hrtimers)
	ret = drm_mfr_create_swt_ctx(&mfr_context->swt_ctx,
				mfr_context,
				&mfr_context->vid_ctx->vid_rate,
				&mfr_context->vid_ctx->mfr_rate);
	if (ret) {
		goto release_mfr_ctx;
	}

	ret = drm_mfr_create_spt_ctx(&mfr_context->spt_ctx);
	if (ret) {
		goto release_swt_ctx;
	}

	mfr_context->psv_ctx = drm_mfr_psv_create_ctx();
	if (!mfr_context->psv_ctx) {
		goto release_spt_ctx;
	}
	// build debugfs tree(by drm_debugfs.c)

	ret = drm_mfr_bind();
	if (ret) {
		goto release_psv_ctx;
	}

	return 0;

release_psv_ctx:
	drm_mfr_psv_destroy_ctx(mfr_context->psv_ctx);
	mfr_context->psv_ctx = NULL;

release_spt_ctx:
	drm_mfr_destroy_spt_ctx(mfr_context->spt_ctx);
	mfr_context->spt_ctx = 0;

release_swt_ctx:
	drm_mfr_destroy_swt_ctx(mfr_context->swt_ctx);
	mfr_context->swt_ctx = 0;

release_mfr_ctx:
	drm_mfr_destroy_context();
err:
	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static void __exit drm_mfr_unregister(void)
{
	DRMMFR_DBG("is called");

	// stop mfr controll
	// (stop sw-vsync-timer)
	// (change mfr-status to suspend and enable tg)

	drm_mfr_unbind();

	// release debugfs

	drm_mfr_psv_destroy_ctx(mfr_context->psv_ctx);
	mfr_context->psv_ctx = NULL;

	drm_mfr_destroy_spt_ctx(mfr_context->spt_ctx);

	// stop sw-vsync and destroy
	drm_mfr_destroy_swt_ctx(mfr_context->swt_ctx);
	mfr_context->swt_ctx = 0;

	// destroy mfr_ctx
	drm_mfr_destroy_context();
}

int drm_mfr_chg_maxmfr_if_default_clkrate(int en, int clk_rate_hz)
{
	DRMMFR_DBG("en = %d , clk_rate_hz = %d", en, clk_rate_hz);

	if (en) {
		if (mfr_context->old_drm_mfr_mfrate == 0) {

			if (drm_cmn_get_default_clk_rate_hz() == clk_rate_hz) {
				mfr_context->old_drm_mfr_mfrate = drm_mfr_mfrate;
				drm_mfr_mfrate = 100;
				DRMMFR_DBG("drm_mfr_mfrate = %d -> %d",
					mfr_context->old_drm_mfr_mfrate,
					drm_mfr_mfrate);
			}
		}
	} else if (mfr_context->old_drm_mfr_mfrate) {
		DRMMFR_DBG("drm_mfr_mfrate = %d -> %d",
			drm_mfr_mfrate, mfr_context->old_drm_mfr_mfrate);
		drm_mfr_mfrate = mfr_context->old_drm_mfr_mfrate;
		mfr_context->old_drm_mfr_mfrate = 0;
	} else {
		return 1;
	}

	return 0;
}

module_init(drm_mfr_register);
module_exit(drm_mfr_unregister);

MODULE_AUTHOR("SHARP CORPORATION");
MODULE_DESCRIPTION("DRM MFR Driver");
MODULE_LICENSE("GPL");
