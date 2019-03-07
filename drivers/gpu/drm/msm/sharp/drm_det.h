/* drivers/gpu/drm/msm/sharp/drm_det.h  (Display Driver)
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

#ifndef DRM_DET_H
#define DRM_DET_H

extern int drm_det_init(struct dsi_display *display);
extern int drm_det_disponchk(struct dsi_display *display);
extern int drm_det_post_panel_on(void);
extern int drm_det_pre_panel_off(void);
extern bool drm_det_is_retry_over(void);
extern int drm_det_chk_panel_on(void);
extern int drm_det_mipierr_clear(struct dsi_display *display);
extern void drm_det_esd_chk_ng(void);

#endif /* DRM_DET_H */
