/* include/uapi/drm/drm_smem.h  (Display Driver)
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

#ifndef _DRM_SMEM_H_
#define _DRM_SMEM_H_

#define DRM_ROSETTA_GMM_SIZE      (62)
#define DRM_HAYABUSA_GMM_SIZE     (60)
#define DRM_HAYABUSA_ADV_GMM_SIZE (30)
#define DRM_SAZABI_GMM_SIZE       (60)
#define DRM_RM69350_GMM_SIZE      (28)

enum panel_type {
	PANEL_TYPE_HAYABUSA,
	PANEL_TYPE_ROSETTA,
	PANEL_TYPE_SAZABI,
	PANEL_TYPE_RM69350,
};

struct drm_panel_otp_info {
    unsigned char status;
    signed char a;
    signed char b;
}__attribute__((aligned(8)));

struct hayabusa_gmm_volt {
	unsigned short gmmR[DRM_HAYABUSA_GMM_SIZE];
	unsigned short gmmG[DRM_HAYABUSA_GMM_SIZE];
	unsigned short gmmB[DRM_HAYABUSA_GMM_SIZE];
	unsigned char vgh;
	unsigned char vgl;
	unsigned char gvddp;
	unsigned char gvddn;
	unsigned char gvddp2;
	unsigned char vgho;
	unsigned char vglo;
	unsigned char adv_gmm[DRM_HAYABUSA_ADV_GMM_SIZE];
};

struct rosetta_gmm_volt {
	unsigned short gmmR[DRM_ROSETTA_GMM_SIZE];
	unsigned short gmmG[DRM_ROSETTA_GMM_SIZE];
	unsigned short gmmB[DRM_ROSETTA_GMM_SIZE];
	unsigned char vgh;
	unsigned char vgl;
	unsigned char gvddp;
	unsigned char gvddn;
	unsigned char gvddp2;
	unsigned char vgho;
	unsigned char vglo;
};

struct sazabi_gmm_volt {
	unsigned short gmmR[DRM_SAZABI_GMM_SIZE];
	unsigned short gmmG[DRM_SAZABI_GMM_SIZE];
	unsigned short gmmB[DRM_SAZABI_GMM_SIZE];
	unsigned char vgh;
	unsigned char vgl;
	unsigned char vgho1;
	unsigned char vgho2;
	unsigned char vglo1;
	unsigned char vglo2;
	unsigned char vgmp2;
	unsigned char vgmp1;
	unsigned char vgsp;
	unsigned char vgmn2;
	unsigned char vgmn1;
	unsigned char vgsn;
};
struct rm69350_gmm_volt {
	unsigned short gmmR[DRM_RM69350_GMM_SIZE];
	unsigned short gmmG[DRM_RM69350_GMM_SIZE];
	unsigned short gmmB[DRM_RM69350_GMM_SIZE];
	unsigned char vglr;
	unsigned char vghr;
	unsigned char vgsp;
	unsigned char vgmp1;
	unsigned char vgmp2;
};
union mdp_gmm_volt {
	struct rosetta_gmm_volt rosetta;
	struct hayabusa_gmm_volt hayabusa;
	struct sazabi_gmm_volt sazabi;
	struct rm69350_gmm_volt rm69350;
}__attribute__((aligned(8)));

struct mdp_gmm_volt_info {
	int request;
	enum panel_type panel_type;
	union mdp_gmm_volt gmm_volt;
}__attribute__((aligned(8)));

#endif /* _DRM_SMEM_H_ */
