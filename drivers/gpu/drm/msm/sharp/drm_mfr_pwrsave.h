/* drivers/gpu/drm/msm/sharp/drm_mfr_pwrsave.h  (Display Driver)
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

#ifndef DRM_MFR_PWRSAVE_H
#define DRM_MFR_PWRSAVE_H


enum drm_mfr_psv_state {
	PWR_OFF = 0,
	PWR_ON = 1,
};

//enum drm_mfr_psv_dsi_lane_state {
//	LPM = 0,
//	HSM = 1,
//};

struct drm_psv_req {
	// enum drm_mfr_psv_state intrs;
	enum drm_mfr_psv_state vsync_intr;
	enum drm_mfr_psv_state underrun_intr;
	enum drm_mfr_psv_state clocks;
	enum drm_mfr_psv_state qos;
	// enum drm_mfr_psv_dsi_lane_state dsi;
};


// struct drm_psv_ctx - shared psv-ctx ptr for drm_mfr
struct drm_psv_ctx;

/** struct psv_ctrl_priv_data - vendor private data ptr.
 *  members are implemented by vendor modules,
 *  and closed for drm_mfr modules.
 */
struct psv_ctrl_priv_data;  // implemented by vendor module
struct drm_mfr_psv_controller {
	struct psv_ctrl_priv_data *priv_data;
	int (*vsync_intr_ctrl)(struct psv_ctrl_priv_data *priv, int en);
	int (*underrun_intr_ctrl)(struct psv_ctrl_priv_data *priv, int en);

	/* clocks, power, iommu, dsi_clklane_state */
	int (*clocks_ctrl)(struct psv_ctrl_priv_data *priv, int en);
};

/**
 * They are implemented by vendor modules,
 * and called by drm_mdr_pwrsave.
 */
extern const struct drm_mfr_psv_controller *drm_get_psv_controller(void);


/**
 * they are implemented by drm_mfr_pwrsave.
 * and called by drm_mfr
 */
extern struct drm_psv_ctx *drm_mfr_psv_create_ctx(void);
extern void drm_mfr_psv_destroy_ctx(struct drm_psv_ctx *psv_ctx);
extern int drm_mfr_psv_start(struct drm_psv_ctx *psv_ctx,
					struct drm_psv_req *init_states);
void drm_mfr_psv_stop(struct drm_psv_ctx *psv_ctx,
					struct drm_psv_req *stop_states);

extern void drm_mfr_psv_req_vsync_intr(struct drm_psv_ctx *psv_ctx, int en);
extern void drm_mfr_psv_req_underrun_intr(struct drm_psv_ctx *psv_ctx, int en);
extern void drm_mfr_psv_req_clocks(struct drm_psv_ctx *psv_ctx, int en);
extern void drm_mfr_psv_req_qos(struct drm_psv_ctx *psv_ctx, int en);
extern int drm_mfr_psv_req_commit(struct drm_psv_ctx *psv_ctx);
extern int drm_mfr_psv_req_commit_sync(struct drm_psv_ctx *psv_ctx);
extern int drm_mfr_psv_is_clocks_enabled(struct drm_psv_ctx *psv_ctx);

/**
 * enable this code when isn't implemented by vendor-modules side...
 */
/*
static inline const struct drm_mfr_psv_controller *drm_get_psv_controller(void)
{
	return NULL;
}
*/
#endif /* DRM_MFR_PWRSAVE_H */