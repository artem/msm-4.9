/* include/uapi/drm/sharp_drm.h  (Display Driver)
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

#ifndef _SHARP_DRM_H_
#define _SHARP_DRM_H_

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include "drm_smem.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define DRM_REG_WRITE       (0x0001)
#define DRM_SAVE_VALUE      (0x0002)
#define DRM_SAVE_VALUE_LOW  (0x0004)
#define DRM_RESET_VALUE     (0x0008)
#define DRM_GET_VALUE       (0x0100)
#define DRM_GET_VALUE_LOW   (0x0200)

#define DRM_MIPICHK_RESULT_OK 1
#define DRM_MIPICHK_RESULT_NG 0
#define DRM_DSI_DSIPHY_REGULATOR_CTRL_0	(0x00)

#define DRM_MIPICHK_AMP_NUM             (8)
#define DRM_MIPICHK_SENSITIV_NUM        (16)

#define DRM_GMMVOLT_REQ_ADJUST          (1)
#define DRM_GMMVOLT_REQ_UNADJUST        (2)

#define DRM_BASE_FPS_30                 (30)
#define DRM_BASE_FPS_60                 (60)
#define DRM_BASE_FPS_100                (100)
#define DRM_BASE_FPS_120                (120)

#if defined(CONFIG_ARCH_DIO) || defined(FEATURE_SH_DISPLAY_MODEL_DIO)
    #define DRM_BASE_FPS_DEFAULT        DRM_BASE_FPS_100
#elif defined(CONFIG_ARCH_JOHNNY) || defined(FEATURE_SH_DISPLAY_MODEL_JOHNNY)
    #define DRM_BASE_FPS_DEFAULT        DRM_BASE_FPS_60
#elif defined(CONFIG_ARCH_PUCCI) || defined(FEATURE_SH_DISPLAY_MODEL_PUCCI)
    #define DRM_BASE_FPS_DEFAULT        DRM_BASE_FPS_120
#else
    #define DRM_BASE_FPS_DEFAULT        DRM_BASE_FPS_60
#endif

#define DRM_BASE_MAX_FPS                DRM_BASE_FPS_DEFAULT

enum {
    MDP_INTERNAL_OSC_TYPE_A,
    MDP_INTERNAL_OSC_TYPE_B,
    MDP_INTERNAL_OSC_TYPE_C,
    NUM_MDP_INTERNAL_OSC_TYPE
};

struct mdp_mipi_clkchg_host {
	unsigned char frame_rate;
	unsigned int clock_rate;
	unsigned short display_width;
	unsigned short display_height;
	unsigned short hsync_pulse_width;
	unsigned short h_back_porch;
	unsigned short h_front_porch;
	unsigned short vsync_pulse_width;
	unsigned short v_back_porch;
	unsigned short v_front_porch;
	unsigned char t_clk_post;
	unsigned char t_clk_pre;
	unsigned char timing_ctrl[12];
};

typedef struct mdp_mipi_clkchg_panel_tag {
	unsigned char mipi_vbp_hf[2];
	unsigned char mipi_vfp_hf[2];
	unsigned char vbp_norm_hf[2];
	unsigned char vfp_norm_hf[2];
	unsigned char rtn_v_hf[2];
	unsigned char rtn_hf[2];
	unsigned char stv_delay_hf;
	unsigned char stv_adv_hf;
	unsigned char gck_delay_hf;
	unsigned char gck_adv_hf;
	unsigned char soeht_hf;
	unsigned char muxs_hf;
	unsigned char muxw_hf;
	unsigned char muxs_v_hf;
	unsigned char muxw_v_hf;
	unsigned char oscset1;
	unsigned char oscset2;
	unsigned char oscscope;
	unsigned char osc_fine_trim;
} mdp_mipi_clkchg_panel_t;

struct mdp_mipi_clkchg_param {
    struct mdp_mipi_clkchg_host host;
    mdp_mipi_clkchg_panel_t panel;
    int internal_osc;
};

struct drm_flicker_param {
	unsigned short	request;
	unsigned short	vcom;
};

struct mdp_mipi_check_param {
	uint8_t frame_cnt;
	uint8_t amp;
	uint8_t sensitiv;
	uint8_t result_master;
	uint8_t result_slave;
};

#define DRM_MIPI_DSI_CLKCHG            0x50
#define DRM_SET_FLICKER_PARAM          0x51
#define DRM_GET_FLICKER_PARAM          0x52
#define DRM_MIPI_DSI_CHECK             0x53
#define DRM_SET_GMMTABLE_AND_VOLTAGE   0x54
#define DRM_GET_GMMTABLE_AND_VOLTAGE   0x55
#define DRM_GET_PANEL_OTP_INFO         0x56
#define DRM_CHANGE_BASE_FPS_LOW        0x57

#define DRM_IOCTL_MIPI_DSI_CLKCHG      DRM_IOW (DRM_COMMAND_BASE + \
			DRM_MIPI_DSI_CLKCHG, struct mdp_mipi_clkchg_param)
#define DRM_IOCTL_SET_FLICKER_PARAM DRM_IOW((DRM_COMMAND_BASE + \
			DRM_SET_FLICKER_PARAM), struct drm_flicker_param)
#define DRM_IOCTL_GET_FLICKER_PARAM DRM_IOWR((DRM_COMMAND_BASE + \
			DRM_GET_FLICKER_PARAM), struct drm_flicker_param)
#define DRM_IOCTL_MIPI_DSI_CHECK DRM_IOWR((DRM_COMMAND_BASE + \
			DRM_MIPI_DSI_CHECK), struct mdp_mipi_check_param)
#define DRM_IOCTL_SET_GMMTABLE_AND_VOLTAGE DRM_IOW ((DRM_COMMAND_BASE + \
			DRM_SET_GMMTABLE_AND_VOLTAGE), struct mdp_gmm_volt_info)
#define DRM_IOCTL_GET_GMMTABLE_AND_VOLTAGE DRM_IOWR((DRM_COMMAND_BASE + \
			DRM_GET_GMMTABLE_AND_VOLTAGE), struct mdp_gmm_volt_info)
#define DRM_IOCTL_GET_PANEL_OTP_INFO DRM_IOWR((DRM_COMMAND_BASE + \
			DRM_GET_PANEL_OTP_INFO), struct drm_panel_otp_info)
#define DRM_IOCTL_CHANGE_BASE_FPS_LOW DRM_IOWR((DRM_COMMAND_BASE + \
			DRM_CHANGE_BASE_FPS_LOW), int)

#endif /* _SHARP_DRM_H_ */
