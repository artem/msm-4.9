/* drivers/gpu/drm/msm/sharp/drm_diag_rm69350.h  (Display Driver)
 *
 * Copyright (C) 2018 SHARP CORPORATION
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

#ifndef DRM_DIAG_RM69350_H
#define DRM_DIAG_RM69350_H

#ifdef DRM_DIAG_PANEL_GMM_VOLTAGE
char drm_diag_rm69350_gmm_R_data[DRM_RM69350_GMM_SIZE*2];

// this ary is page, addr, payload
struct drm_diag_panel_pad drm_diag_rm69350_gmm_R[] = {
	{0x50, 0x00, 1, &drm_diag_rm69350_gmm_R_data[0]},
	{0x50, 0x01, 1, &drm_diag_rm69350_gmm_R_data[1]},
	{0x50, 0x02, 1, &drm_diag_rm69350_gmm_R_data[2]},
	{0x50, 0x03, 1, &drm_diag_rm69350_gmm_R_data[3]},
	{0x50, 0x04, 1, &drm_diag_rm69350_gmm_R_data[4]},
	{0x50, 0x05, 1, &drm_diag_rm69350_gmm_R_data[5]},
	{0x50, 0x06, 1, &drm_diag_rm69350_gmm_R_data[6]},
	{0x50, 0x07, 1, &drm_diag_rm69350_gmm_R_data[7]},
	{0x50, 0x08, 1, &drm_diag_rm69350_gmm_R_data[8]},
	{0x50, 0x09, 1, &drm_diag_rm69350_gmm_R_data[9]},
	{0x50, 0x0A, 1, &drm_diag_rm69350_gmm_R_data[10]},
	{0x50, 0x0B, 1, &drm_diag_rm69350_gmm_R_data[11]},
	{0x50, 0x0C, 1, &drm_diag_rm69350_gmm_R_data[12]},
	{0x50, 0x0D, 1, &drm_diag_rm69350_gmm_R_data[13]},
	{0x50, 0x0E, 1, &drm_diag_rm69350_gmm_R_data[14]},
	{0x50, 0x0F, 1, &drm_diag_rm69350_gmm_R_data[15]},
	{0x50, 0x10, 1, &drm_diag_rm69350_gmm_R_data[16]},
	{0x50, 0x11, 1, &drm_diag_rm69350_gmm_R_data[17]},
	{0x50, 0x12, 1, &drm_diag_rm69350_gmm_R_data[18]},
	{0x50, 0x13, 1, &drm_diag_rm69350_gmm_R_data[19]},
	{0x50, 0x14, 1, &drm_diag_rm69350_gmm_R_data[20]},
	{0x50, 0x15, 1, &drm_diag_rm69350_gmm_R_data[21]},
	{0x50, 0x16, 1, &drm_diag_rm69350_gmm_R_data[22]},
	{0x50, 0x17, 1, &drm_diag_rm69350_gmm_R_data[23]},
	{0x50, 0x18, 1, &drm_diag_rm69350_gmm_R_data[24]},
	{0x50, 0x19, 1, &drm_diag_rm69350_gmm_R_data[25]},
	{0x50, 0x1A, 1, &drm_diag_rm69350_gmm_R_data[26]},
	{0x50, 0x1B, 1, &drm_diag_rm69350_gmm_R_data[27]},
	{0x50, 0x1C, 1, &drm_diag_rm69350_gmm_R_data[28]},
	{0x50, 0x1D, 1, &drm_diag_rm69350_gmm_R_data[29]},
	{0x50, 0x1E, 1, &drm_diag_rm69350_gmm_R_data[30]},
	{0x50, 0x1F, 1, &drm_diag_rm69350_gmm_R_data[31]},
	{0x50, 0x20, 1, &drm_diag_rm69350_gmm_R_data[32]},
	{0x50, 0x21, 1, &drm_diag_rm69350_gmm_R_data[33]},
	{0x50, 0x22, 1, &drm_diag_rm69350_gmm_R_data[34]},
	{0x50, 0x23, 1, &drm_diag_rm69350_gmm_R_data[35]},
	{0x50, 0x24, 1, &drm_diag_rm69350_gmm_R_data[36]},
	{0x50, 0x25, 1, &drm_diag_rm69350_gmm_R_data[37]},
	{0x50, 0x26, 1, &drm_diag_rm69350_gmm_R_data[38]},
	{0x50, 0x27, 1, &drm_diag_rm69350_gmm_R_data[39]},
	{0x50, 0x28, 1, &drm_diag_rm69350_gmm_R_data[40]},
	{0x50, 0x29, 1, &drm_diag_rm69350_gmm_R_data[41]},
	{0x50, 0x2A, 1, &drm_diag_rm69350_gmm_R_data[42]},
	{0x50, 0x2B, 1, &drm_diag_rm69350_gmm_R_data[43]},
	{0x50, 0x30, 1, &drm_diag_rm69350_gmm_R_data[44]},
	{0x50, 0x31, 1, &drm_diag_rm69350_gmm_R_data[45]},
	{0x50, 0x32, 1, &drm_diag_rm69350_gmm_R_data[46]},
	{0x50, 0x33, 1, &drm_diag_rm69350_gmm_R_data[47]},
	{0x50, 0x34, 1, &drm_diag_rm69350_gmm_R_data[48]},
	{0x50, 0x35, 1, &drm_diag_rm69350_gmm_R_data[49]},
	{0x50, 0x36, 1, &drm_diag_rm69350_gmm_R_data[50]},
	{0x50, 0x37, 1, &drm_diag_rm69350_gmm_R_data[51]},
	{0x50, 0x38, 1, &drm_diag_rm69350_gmm_R_data[52]},
	{0x50, 0x39, 1, &drm_diag_rm69350_gmm_R_data[53]},
	{0x50, 0x3A, 1, &drm_diag_rm69350_gmm_R_data[54]},
	{0x50, 0x3B, 1, &drm_diag_rm69350_gmm_R_data[55]},
};

char drm_diag_rm69350_gmm_G_data[DRM_RM69350_GMM_SIZE*2];
struct drm_diag_panel_pad drm_diag_rm69350_gmm_G[] = {
	{0x50, 0x40, 1, &drm_diag_rm69350_gmm_G_data[0]},
	{0x50, 0x41, 1, &drm_diag_rm69350_gmm_G_data[1]},
	{0x50, 0x42, 1, &drm_diag_rm69350_gmm_G_data[2]},
	{0x50, 0x43, 1, &drm_diag_rm69350_gmm_G_data[3]},
	{0x50, 0x44, 1, &drm_diag_rm69350_gmm_G_data[4]},
	{0x50, 0x45, 1, &drm_diag_rm69350_gmm_G_data[5]},
	{0x50, 0x46, 1, &drm_diag_rm69350_gmm_G_data[6]},
	{0x50, 0x47, 1, &drm_diag_rm69350_gmm_G_data[7]},
	{0x50, 0x48, 1, &drm_diag_rm69350_gmm_G_data[8]},
	{0x50, 0x49, 1, &drm_diag_rm69350_gmm_G_data[9]},
	{0x50, 0x4A, 1, &drm_diag_rm69350_gmm_G_data[10]},
	{0x50, 0x4B, 1, &drm_diag_rm69350_gmm_G_data[11]},
	{0x50, 0x4C, 1, &drm_diag_rm69350_gmm_G_data[12]},
	{0x50, 0x4D, 1, &drm_diag_rm69350_gmm_G_data[13]},
	{0x50, 0x4E, 1, &drm_diag_rm69350_gmm_G_data[14]},
	{0x50, 0x4F, 1, &drm_diag_rm69350_gmm_G_data[15]},
	{0x50, 0x50, 1, &drm_diag_rm69350_gmm_G_data[16]},
	{0x50, 0x51, 1, &drm_diag_rm69350_gmm_G_data[17]},
	{0x50, 0x52, 1, &drm_diag_rm69350_gmm_G_data[18]},
	{0x50, 0x53, 1, &drm_diag_rm69350_gmm_G_data[19]},
	{0x50, 0x58, 1, &drm_diag_rm69350_gmm_G_data[20]},
	{0x50, 0x59, 1, &drm_diag_rm69350_gmm_G_data[21]},
	{0x50, 0x5A, 1, &drm_diag_rm69350_gmm_G_data[22]},
	{0x50, 0x5B, 1, &drm_diag_rm69350_gmm_G_data[23]},
	{0x50, 0x5C, 1, &drm_diag_rm69350_gmm_G_data[24]},
	{0x50, 0x5D, 1, &drm_diag_rm69350_gmm_G_data[25]},
	{0x50, 0x5E, 1, &drm_diag_rm69350_gmm_G_data[26]},
	{0x50, 0x5F, 1, &drm_diag_rm69350_gmm_G_data[27]},
	{0x50, 0x60, 1, &drm_diag_rm69350_gmm_G_data[28]},
	{0x50, 0x61, 1, &drm_diag_rm69350_gmm_G_data[29]},
	{0x50, 0x62, 1, &drm_diag_rm69350_gmm_G_data[30]},
	{0x50, 0x63, 1, &drm_diag_rm69350_gmm_G_data[31]},
	{0x50, 0x64, 1, &drm_diag_rm69350_gmm_G_data[32]},
	{0x50, 0x65, 1, &drm_diag_rm69350_gmm_G_data[33]},
	{0x50, 0x66, 1, &drm_diag_rm69350_gmm_G_data[34]},
	{0x50, 0x67, 1, &drm_diag_rm69350_gmm_G_data[35]},
	{0x50, 0x68, 1, &drm_diag_rm69350_gmm_G_data[36]},
	{0x50, 0x69, 1, &drm_diag_rm69350_gmm_G_data[37]},
	{0x50, 0x6A, 1, &drm_diag_rm69350_gmm_G_data[38]},
	{0x50, 0x6B, 1, &drm_diag_rm69350_gmm_G_data[39]},
	{0x50, 0x6C, 1, &drm_diag_rm69350_gmm_G_data[40]},
	{0x50, 0x6D, 1, &drm_diag_rm69350_gmm_G_data[41]},
	{0x50, 0x6E, 1, &drm_diag_rm69350_gmm_G_data[42]},
	{0x50, 0x6F, 1, &drm_diag_rm69350_gmm_G_data[43]},
	{0x50, 0x70, 1, &drm_diag_rm69350_gmm_G_data[44]},
	{0x50, 0x71, 1, &drm_diag_rm69350_gmm_G_data[45]},
	{0x50, 0x72, 1, &drm_diag_rm69350_gmm_G_data[46]},
	{0x50, 0x73, 1, &drm_diag_rm69350_gmm_G_data[47]},
	{0x50, 0x74, 1, &drm_diag_rm69350_gmm_G_data[48]},
	{0x50, 0x75, 1, &drm_diag_rm69350_gmm_G_data[49]},
	{0x50, 0x76, 1, &drm_diag_rm69350_gmm_G_data[50]},
	{0x50, 0x77, 1, &drm_diag_rm69350_gmm_G_data[51]},
	{0x50, 0x78, 1, &drm_diag_rm69350_gmm_G_data[52]},
	{0x50, 0x79, 1, &drm_diag_rm69350_gmm_G_data[53]},
	{0x50, 0x7A, 1, &drm_diag_rm69350_gmm_G_data[54]},
	{0x50, 0x7B, 1, &drm_diag_rm69350_gmm_G_data[55]},
};

char drm_diag_rm69350_gmm_B_data[DRM_RM69350_GMM_SIZE*2];
struct drm_diag_panel_pad drm_diag_rm69350_gmm_B[] = {
	{0x50, 0x7C, 1, &drm_diag_rm69350_gmm_B_data[0]},
	{0x50, 0x7D, 1, &drm_diag_rm69350_gmm_B_data[1]},
	{0x50, 0x7E, 1, &drm_diag_rm69350_gmm_B_data[2]},
	{0x50, 0x7F, 1, &drm_diag_rm69350_gmm_B_data[3]},
	{0x50, 0x80, 1, &drm_diag_rm69350_gmm_B_data[4]},
	{0x50, 0x81, 1, &drm_diag_rm69350_gmm_B_data[5]},
	{0x50, 0x82, 1, &drm_diag_rm69350_gmm_B_data[6]},
	{0x50, 0x83, 1, &drm_diag_rm69350_gmm_B_data[7]},
	{0x50, 0x84, 1, &drm_diag_rm69350_gmm_B_data[8]},
	{0x50, 0x85, 1, &drm_diag_rm69350_gmm_B_data[9]},
	{0x50, 0x86, 1, &drm_diag_rm69350_gmm_B_data[10]},
	{0x50, 0x87, 1, &drm_diag_rm69350_gmm_B_data[11]},
	{0x50, 0x88, 1, &drm_diag_rm69350_gmm_B_data[12]},
	{0x50, 0x89, 1, &drm_diag_rm69350_gmm_B_data[13]},
	{0x50, 0x8A, 1, &drm_diag_rm69350_gmm_B_data[14]},
	{0x50, 0x8B, 1, &drm_diag_rm69350_gmm_B_data[15]},
	{0x50, 0x8C, 1, &drm_diag_rm69350_gmm_B_data[16]},
	{0x50, 0x8D, 1, &drm_diag_rm69350_gmm_B_data[17]},
	{0x50, 0x8E, 1, &drm_diag_rm69350_gmm_B_data[18]},
	{0x50, 0x8F, 1, &drm_diag_rm69350_gmm_B_data[19]},
	{0x50, 0x90, 1, &drm_diag_rm69350_gmm_B_data[20]},
	{0x50, 0x91, 1, &drm_diag_rm69350_gmm_B_data[21]},
	{0x50, 0x92, 1, &drm_diag_rm69350_gmm_B_data[22]},
	{0x50, 0x93, 1, &drm_diag_rm69350_gmm_B_data[23]},
	{0x50, 0x94, 1, &drm_diag_rm69350_gmm_B_data[24]},
	{0x50, 0x95, 1, &drm_diag_rm69350_gmm_B_data[25]},
	{0x50, 0x96, 1, &drm_diag_rm69350_gmm_B_data[26]},
	{0x50, 0x97, 1, &drm_diag_rm69350_gmm_B_data[27]},
	{0x50, 0x98, 1, &drm_diag_rm69350_gmm_B_data[28]},
	{0x50, 0x99, 1, &drm_diag_rm69350_gmm_B_data[29]},
	{0x50, 0x9A, 1, &drm_diag_rm69350_gmm_B_data[30]},
	{0x50, 0x9B, 1, &drm_diag_rm69350_gmm_B_data[31]},
	{0x50, 0x9C, 1, &drm_diag_rm69350_gmm_B_data[32]},
	{0x50, 0x9D, 1, &drm_diag_rm69350_gmm_B_data[33]},
	{0x50, 0x9E, 1, &drm_diag_rm69350_gmm_B_data[34]},
	{0x50, 0x9F, 1, &drm_diag_rm69350_gmm_B_data[35]},
	{0x50, 0xA4, 1, &drm_diag_rm69350_gmm_B_data[36]},
	{0x50, 0xA5, 1, &drm_diag_rm69350_gmm_B_data[37]},
	{0x50, 0xA6, 1, &drm_diag_rm69350_gmm_B_data[38]},
	{0x50, 0xA7, 1, &drm_diag_rm69350_gmm_B_data[39]},
	{0x50, 0xAC, 1, &drm_diag_rm69350_gmm_B_data[40]},
	{0x50, 0xAD, 1, &drm_diag_rm69350_gmm_B_data[41]},
	{0x50, 0xAE, 1, &drm_diag_rm69350_gmm_B_data[42]},
	{0x50, 0xAF, 1, &drm_diag_rm69350_gmm_B_data[43]},
	{0x50, 0xB0, 1, &drm_diag_rm69350_gmm_B_data[44]},
	{0x50, 0xB1, 1, &drm_diag_rm69350_gmm_B_data[45]},
	{0x50, 0xB2, 1, &drm_diag_rm69350_gmm_B_data[46]},
	{0x50, 0xB3, 1, &drm_diag_rm69350_gmm_B_data[47]},
	{0x50, 0xB4, 1, &drm_diag_rm69350_gmm_B_data[48]},
	{0x50, 0xB5, 1, &drm_diag_rm69350_gmm_B_data[49]},
	{0x50, 0xB6, 1, &drm_diag_rm69350_gmm_B_data[50]},
	{0x50, 0xB7, 1, &drm_diag_rm69350_gmm_B_data[51]},
	{0x50, 0xB8, 1, &drm_diag_rm69350_gmm_B_data[52]},
	{0x50, 0xB9, 1, &drm_diag_rm69350_gmm_B_data[53]},
	{0x50, 0xBA, 1, &drm_diag_rm69350_gmm_B_data[54]},
	{0x50, 0xBB, 1, &drm_diag_rm69350_gmm_B_data[55]},
};

char drm_diag_rm69350_volt_data[5];
struct drm_diag_panel_pad drm_diag_rm69350_volt[] = {
	{0x40, 0x38, 1, &drm_diag_rm69350_volt_data[0]}, // VGLR
	{0x40, 0x3A, 1, &drm_diag_rm69350_volt_data[1]}, // VGHR
	{0xD0, 0x93, 1, &drm_diag_rm69350_volt_data[2]}, // VGHSP
	{0x50, 0xBD, 1, &drm_diag_rm69350_volt_data[3]}, // VGMP1
	{0x50, 0xBE, 1, &drm_diag_rm69350_volt_data[4]}, // VGMP2
};

const struct drm_diag_pad_item drm_diag_rm69350_gmm_volt_pads[] = {
	{drm_diag_rm69350_volt,   ARRAY_SIZE(drm_diag_rm69350_volt)},
	{drm_diag_rm69350_gmm_R,  ARRAY_SIZE(drm_diag_rm69350_gmm_R)},
	{drm_diag_rm69350_gmm_G,  ARRAY_SIZE(drm_diag_rm69350_gmm_G)},
	{drm_diag_rm69350_gmm_B,  ARRAY_SIZE(drm_diag_rm69350_gmm_B)},
};
#endif /* DRM_DIAG_PANEL_GMM_VOLTAGE */

#endif /* DRM_DIAG_RM69350_H */