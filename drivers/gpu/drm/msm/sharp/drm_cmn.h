/* drivers/gpu/drm/msm/sharp/drm_cmn.h  (Display Driver)
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

#ifndef DRM_CMN_H
#define DRM_CMN_H

/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#include "../dsi-staging/dsi_display.h"

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* EXTERN                                                                    */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* FUNCTION                                                                  */
/* ------------------------------------------------------------------------- */
int drm_cmn_video_transfer_ctrl(u8 onoff);
int drm_cmn_stop_video(void);
int drm_cmn_start_video(void);
int drm_cmn_panel_cmds_transfer(struct dsi_display *pdisp,
			struct dsi_cmd_desc cmds[], int cmd_cnt);
int drm_cmn_panel_cmds_transfer_videoctrl(struct dsi_display *pdisp,
			struct dsi_cmd_desc cmds[], int cmd_cnt);
int drm_cmn_panel_dcs_write0(struct dsi_display *pdisp, char addr);
int drm_cmn_panel_dcs_write1(struct dsi_display *pdisp,
			char addr, char data);
int drm_cmn_panel_dcs_read(struct dsi_display *pdisp, char addr,
			int rlen, char *rbuf);
int drm_cmn_set_default_clk_rate_hz(void);
int drm_cmn_get_default_clk_rate_hz(void);

#endif /* DRM_CMN_H */
