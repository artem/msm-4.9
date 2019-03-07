/* drivers/gpu/drm/msm/sharp/drm_diag_rosetta.h  (Display Driver)
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

#ifndef DRM_DIAG_ROSETTA_H
#define DRM_DIAG_ROSETTA_H

#ifdef DRM_DIAG_PANEL_GMM_VOLTAGE
char drm_diag_rosetta_gmm_R_data[DRM_ROSETTA_GMM_SIZE*2];

// this ary is page, addr, payload
struct drm_diag_panel_pad drm_diag_rosetta_gmm_R[] = {
	{0x20, 0xB0, 16, &drm_diag_rosetta_gmm_R_data[0]},
	{0x20, 0xB1, 16, &drm_diag_rosetta_gmm_R_data[16]},
	{0x20, 0xB2, 16, &drm_diag_rosetta_gmm_R_data[32]},
	{0x20, 0xB3, 14, &drm_diag_rosetta_gmm_R_data[48]},
};

struct drm_diag_panel_pad drm_diag_rosetta_gmm_R_nega[] = {
	{0x21, 0xB0, 16, &drm_diag_rosetta_gmm_R_data[62]},
	{0x21, 0xB1, 16, &drm_diag_rosetta_gmm_R_data[78]},
	{0x21, 0xB2, 16, &drm_diag_rosetta_gmm_R_data[94]},
	{0x21, 0xB3, 14, &drm_diag_rosetta_gmm_R_data[110]},
};

char drm_diag_rosetta_gmm_G_data[DRM_ROSETTA_GMM_SIZE*2];
struct drm_diag_panel_pad drm_diag_rosetta_gmm_G[] = {
	{0x20, 0xB4, 16, &drm_diag_rosetta_gmm_G_data[0]},
	{0x20, 0xB5, 16, &drm_diag_rosetta_gmm_G_data[16]},
	{0x20, 0xB6, 16, &drm_diag_rosetta_gmm_G_data[32]},
	{0x20, 0xB7, 14, &drm_diag_rosetta_gmm_G_data[48]},
};

struct drm_diag_panel_pad drm_diag_rosetta_gmm_G_nega[] = {
	{0x21, 0xB4, 16, &drm_diag_rosetta_gmm_G_data[62]},
	{0x21, 0xB5, 16, &drm_diag_rosetta_gmm_G_data[78]},
	{0x21, 0xB6, 16, &drm_diag_rosetta_gmm_G_data[94]},
	{0x21, 0xB7, 14, &drm_diag_rosetta_gmm_G_data[110]},
};

char drm_diag_rosetta_gmm_B_data[DRM_ROSETTA_GMM_SIZE*2];
struct drm_diag_panel_pad drm_diag_rosetta_gmm_B[] = {
	{0x20, 0xB8, 16, &drm_diag_rosetta_gmm_B_data[0]},
	{0x20, 0xB9, 16, &drm_diag_rosetta_gmm_B_data[16]},
	{0x20, 0xBA, 16, &drm_diag_rosetta_gmm_B_data[32]},
	{0x20, 0xBB, 14, &drm_diag_rosetta_gmm_B_data[48]},
};

struct drm_diag_panel_pad drm_diag_rosetta_gmm_B_nega[] = {
	{0x21, 0xB8, 16, &drm_diag_rosetta_gmm_B_data[62]},
	{0x21, 0xB9, 16, &drm_diag_rosetta_gmm_B_data[78]},
	{0x21, 0xBA, 16, &drm_diag_rosetta_gmm_B_data[94]},
	{0x21, 0xBB, 14, &drm_diag_rosetta_gmm_B_data[110]},
};

char drm_diag_rosetta_volt_data[7];
struct drm_diag_panel_pad drm_diag_rosetta_volt[] = {
	{0x20, 0x07, 1, &drm_diag_rosetta_volt_data[0]}, // VGH
	{0x20, 0x08, 1, &drm_diag_rosetta_volt_data[1]}, // VGL
	{0x20, 0x0C, 1, &drm_diag_rosetta_volt_data[2]}, // VGHO
	{0x20, 0x0D, 1, &drm_diag_rosetta_volt_data[3]}, // VGLO
	{0x20, 0x96, 1, &drm_diag_rosetta_volt_data[4]}, // CGDDP2
	{0x20, 0x97, 1, &drm_diag_rosetta_volt_data[5]}, // GVDDP
	{0x20, 0x98, 1, &drm_diag_rosetta_volt_data[6]}, // GVDDN
};

const struct drm_diag_pad_item drm_diag_rosetta_gmm_volt_pads[] = {
	{drm_diag_rosetta_volt,   ARRAY_SIZE(drm_diag_rosetta_volt)},
	{drm_diag_rosetta_gmm_R,  ARRAY_SIZE(drm_diag_rosetta_gmm_R)},
	{drm_diag_rosetta_gmm_G,  ARRAY_SIZE(drm_diag_rosetta_gmm_G)},
	{drm_diag_rosetta_gmm_B,  ARRAY_SIZE(drm_diag_rosetta_gmm_B)},
	{drm_diag_rosetta_gmm_R_nega,  ARRAY_SIZE(drm_diag_rosetta_gmm_R_nega)},
	{drm_diag_rosetta_gmm_G_nega,  ARRAY_SIZE(drm_diag_rosetta_gmm_G_nega)},
	{drm_diag_rosetta_gmm_B_nega,  ARRAY_SIZE(drm_diag_rosetta_gmm_B_nega)},
};
#endif /* DRM_DIAG_PANEL_GMM_VOLTAGE */

#endif /* DRM_DIAG_ROSETTA_H */