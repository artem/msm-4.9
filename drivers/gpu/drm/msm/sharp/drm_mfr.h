/* drivers/gpu/drm/msm/sharp/drm_mfr.h  (Display Driver)
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

//#ifdef CONFIG_SHARP_DRM_HR_VID /* CUST_ID_00015 */
//#endif /* CONFIG_SHARP_DRM_HR_VID */

#ifndef DRM_MFR_H
#define DRM_MFR_H

/**
 *
 */
enum DRM_MFR_SEND_VBLANK {
	DRM_MFR_SEND_VBLANK_SEND,
	DRM_MFR_SEND_VBLANK_NOTSEND,
};

enum DRM_MFR_CTRL_BY {
	DRM_MFR_CTRL_BY_MFR,
	DRM_MFR_CTRL_BY_VENDOR,
};

/**
 *  struct drm_mfr_ctx - drm_ctrl_handle.
 *  members are implemented by drm_mfr modules,
 *  and closed for other modules.
 *  (struct drm_mfr_callbacks too.)
 */
struct drm_mfr_ctx;
struct drm_mfr_callbacks {
	struct drm_mfr_ctx *ctx;
	int (*notify_power)(struct drm_mfr_ctx *ctx, int en);
	int (*notify_prepare_commit)(struct drm_mfr_ctx *ctx,
				int restart_vid
				/*, int cont_commit_frame_count*/);
	int (*notify_commit_frame)(struct drm_mfr_ctx *ctx);
	int (*notify_request_vblank_by_user)(struct drm_mfr_ctx *ctx,
						int en);
	int (*notify_restart_vid_counter)(struct drm_mfr_ctx *ctx,
						int count, int vtotal);
	enum DRM_MFR_CTRL_BY
	    (*notify_vsync_enable)(struct drm_mfr_ctx *ctx, int en);
	enum DRM_MFR_CTRL_BY
	    (*notify_underrun_enable)(struct drm_mfr_ctx *ctx,
	    					int en, int index);
	enum DRM_MFR_SEND_VBLANK
	    (*notify_hw_vsync)(struct drm_mfr_ctx *ctx);
	int (*pre_hw_vsync)(struct drm_mfr_ctx *ctx);
	int (*post_hw_vsync)(struct drm_mfr_ctx *ctx);
	int (*force_output_video)(struct drm_mfr_ctx *ctx);
};


/**
 *  struct mfr_ctrl_priv_data - vendor private data ptr.
 *  members are implemented by vendor modules,
 *  and closed for drm_mfr modules.
 * (struct drm_mfr_controller members too.)
 */
struct mfr_ctrl_priv_data;
struct drm_mfr_controller {
	struct mfr_ctrl_priv_data *priv_data;
	int (*prepare_restart_vid)(struct mfr_ctrl_priv_data *priv);
	int (*vblank_callback)(struct mfr_ctrl_priv_data *priv);
	// int (*frame_done_callback)(struct mfr_ctrl_priv_data *priv);
	int (*enable_output_vid)(struct mfr_ctrl_priv_data *priv, int en);
	int (*can_stop_video)(struct mfr_ctrl_priv_data *priv);
	int (*is_video_stopped)(struct mfr_ctrl_priv_data *priv,
					bool wait_ifstopsoon);
	// int (*get_vrefresh)(struct mfr_ctrl_priv_data *priv);
};


/**
 * They are implemented by vendor modules,
 * and called by drm_mfr.
 */
extern int drm_register_mfr_callback(struct drm_mfr_callbacks *cb);
extern int drm_unregister_mfr_callback(void);
extern const struct drm_mfr_controller *drm_get_mfr_controller(void);

/**
 * They are implemented by vendor modules,
 * and called by vendor modules too,
 */
extern const struct drm_mfr_callbacks *drm_mfr_get_mfr_callbacks(void);

/**
 * called by sharp other modules..
 */
extern void drm_mfr_suspend_ctrl(int req);
extern int drm_mfr_get_mfr(void);
extern void drm_mfr_set_mfr(int set_mfrate);
extern void drm_mfr_set_swvsync_rate(int fpslow_base, u32 bl_lvl);
extern int drm_mfr_chg_maxmfr_if_default_clkrate(int en, int clk_rate_hz);

/**
 * enable these code when isn't implemented by vendor-modules side...
 */
/*
static int drm_register_mfr_callback(struct drm_mfr_callbacks *cb)
{
	return 0;
}
static int drm_unregister_mfr_callback(void)
{
	return 0;
}
static inline const struct drm_mfr_controller *drm_get_mfr_controller(void)
{
	return NULL;
}
*/
#endif /* DRM_MFR_H */