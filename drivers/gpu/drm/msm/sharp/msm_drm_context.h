/* drivers/gpu/drm/msm/sharp/msm_drm_context.h  (Display Driver)
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

#ifndef MSM_SMEM_H
#define MSM_SMEM_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include <uapi/drm/drm_smem.h>

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
/* Flicker status and min/max */
#define IS_FLICKER_ADJUSTED(param)    (((param & 0xF000) == 0x9000) ? 1 : 0)
#define VCOM_MIN	(0x0000)
#define VCOM_MAX	(0x01C7)

#define DEFAULT_VCOM    (0x42)

/* Voltage/Gamma Adjusted status */
#define DRM_GMM_ADJ_STATUS_OK          (0x96)
#define DRM_GMM_ADJ_STATUS_NOT_SET     (0x00)

enum {
	DRM_UPPER_UNIT_IS_NOT_CONNECTED = 0,
	DRM_UPPER_UNIT_IS_CONNECTED
};

enum {
	DRM_PANEL_DISPONCHK_SUCCESS,
	DRM_PANEL_DISPONCHK_STATERR,
	DRM_PANEL_DISPONCHK_READERR
};

/* ------------------------------------------------------------------------- */
/* STRUCTURE                                                                 */
/* ------------------------------------------------------------------------- */
/* msm_mdss contexts */
struct drm_vcom {
	unsigned short vcom;
	unsigned short vcom_low;
};

struct drm_flicker_ctx {
	struct drm_vcom vcom;
	unsigned short   nvram;
}__attribute__((aligned(8)));

struct drm_gmmvolt_ctx {
	unsigned char       status;
	union mdp_gmm_volt  gmm_volt __attribute__((aligned(8)));
}__attribute__((aligned(8)));

struct shdisp_boot_context {
	unsigned char            lcd_switch;
	unsigned char            panel_connected;
	unsigned char            disp_on_status;
	struct drm_gmmvolt_ctx  gmmvolt_ctx;
	struct drm_flicker_ctx  flicker_ctx;
	struct drm_panel_otp_info  panel_otp_info;
};

/* flicker structures */
struct drm_hayabusa_vcom {
	char vcom1_l;
	char vcom2_l;
	char vcom12_h;
	char lpvcom1;
	char lpvcom2;
	char vcomoff_l;
	char vcomoff_h;
};

struct drm_rosetta_vcom {
	char vcom1_l;
	char vcom2_l;
	char vcom12_h;
	char lpvcom1;
	char lpvcom2;
	char vcomoff_l;
	char vcomoff_h;
};
#endif /* MSM_SMEM_H */
/* ------------------------------------------------------------------------- */
/* END OF FILE                                                               */
/* ------------------------------------------------------------------------- */
