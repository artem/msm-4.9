/* drivers/gpu/drm/msm/sharp/drm_cmn.c  (Display Driver)
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
#include <linux/iopoll.h>
#include <video/mipi_display.h>
#include "../sde/sde_hw_intf.h"
#include "../dsi-staging/dsi_ctrl_reg.h"
#include "../dsi-staging/dsi_hw.h"
#include "../msm_drv.h"
#include "drm_cmn.h"
#include "drm_mfr.h"
/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* STRUCTURE                                                                 */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* FUNCTION                                                                  */
/* ------------------------------------------------------------------------- */
static int drm_cmn_ctrl_timing_generator(u8 onoff);
static int drm_cmn_ctrl_video_engine(u8 onoff);
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_cmn_video_transfer_ctrl(u8 onoff)
{
	int ret = 0;

	pr_debug("%s: in\n", __func__);

	if (onoff) {
		ret = drm_cmn_start_video();
		if (ret) {
			goto error;
		}
#ifdef CONFIG_SHARP_DRM_HR_VID
		drm_mfr_suspend_ctrl(false);
#endif /* CONFIG_SHARP_DRM_HR_VID */
	} else {
#ifdef CONFIG_SHARP_DRM_HR_VID
		drm_mfr_suspend_ctrl(true);
#endif /* CONFIG_SHARP_DRM_HR_VID */
		ret = drm_cmn_stop_video();
		if (ret) {
			goto error;
		}
	}

	pr_debug("%s: succeed %s video\n", __func__,
					(onoff ? "starting" : "stopping"));
	return 0;

error:
	pr_err("%s: failed to %s video\n", __func__,
					(onoff ? "starting" : "stopping"));
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_cmn_stop_video(void)
{
	int ret = 0;

	pr_debug("%s: in\n", __func__);

	ret = drm_cmn_ctrl_timing_generator(false);
	if (ret) {
		pr_err("%s: failed drm_cmn_ctrl_timing_generator\n",
								 __func__);
		return ret;
	}

	pr_debug("%s: wait 20msec\n", __func__);
	usleep_range(20000, 20000);

	ret = drm_cmn_ctrl_video_engine(false);
	if (ret) {
		pr_err("%s: failed drm_cmn_ctrl_video_engine\n", __func__);
		return ret;
	}

	pr_debug("%s: out\n", __func__);
	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_cmn_start_video(void)
{
	int ret = 0;

	pr_debug("%s: in\n", __func__);

	ret = drm_cmn_ctrl_video_engine(true);
	if (ret) {
		pr_err("%s: failed drm_cmn_ctrl_video_engine\n", __func__);
		return ret;
	}

	ret = drm_cmn_ctrl_timing_generator(true);
	if (ret) {
		pr_err("%s: failed drm_cmn_ctrl_timing_generator\n",
								__func__);
		return ret;
	}

	pr_debug("%s: out\n", __func__);
	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_cmn_ctrl_timing_generator(u8 onoff)
{
	struct sde_hw_intf *intf_left;
	struct sde_hw_intf *intf_right;

	pr_debug("%s: %s timing generator\n", __func__,
						(onoff ? "start" : "stop"));

	intf_left = get_sde_hw_intf(1);
	intf_right = get_sde_hw_intf(2);
	if (!intf_left || !intf_right) {
		pr_err("%s: null sde_hw_intf\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: intf_left = %p, intf_right = %p\n", __func__,
							intf_left, intf_right);
	if (!intf_left->ops.enable_timing || !intf_right->ops.enable_timing) {
		pr_err("%s: enable_timing function is Null\n", __func__);
		return -EINVAL;
	}

	intf_left->ops.enable_timing(intf_left, onoff);
	intf_right->ops.enable_timing(intf_right, onoff);

	pr_debug("%s: out\n", __func__);

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_cmn_ctrl_video_engine(u8 onoff)
{
	struct dsi_display *display = msm_drm_get_dsi_displey();
	struct dsi_ctrl *dsi_ctrl_left;
	struct dsi_ctrl *dsi_ctrl_right;
	int ret = 0;
	int ctrl_engine;

	pr_debug("%s: %s video engine\n", __func__,
						(onoff ? "start" : "stop"));

	if (!display) {
		pr_err("%s: no display\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: display= %p\n", __func__, display);

	dsi_ctrl_left = display->ctrl[0].ctrl;
	dsi_ctrl_right = display->ctrl[1].ctrl;
	if (!dsi_ctrl_left || !dsi_ctrl_right) {
		pr_err("%s: no dsi_ctrl\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: dsi_ctrl_left = %p, dsi_ctrl_right = %p\n", __func__,
						dsi_ctrl_left, dsi_ctrl_right);

	ctrl_engine = onoff ? DSI_CTRL_ENGINE_ON : DSI_CTRL_ENGINE_OFF;

	ret = dsi_ctrl_check_vid_engine_state(dsi_ctrl_left, ctrl_engine);
	if (!ret) {
		ret = dsi_ctrl_set_vid_engine_state(dsi_ctrl_left, ctrl_engine);
		if (ret) {
			goto error;
		}
	}
	ret = dsi_ctrl_check_vid_engine_state(dsi_ctrl_right, ctrl_engine);
	if (!ret) {
		ret = dsi_ctrl_set_vid_engine_state(dsi_ctrl_right, ctrl_engine);
		if (ret) {
			goto error;
		}
	}

	pr_debug("%s: out\n", __func__);

	return 0;
error:
	pr_err("%s: failed dsi_ctrl_set_vid_engine_state\n", __func__);
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_cmn_panel_cmds_transfer(struct dsi_display *pdisp, struct dsi_cmd_desc cmds[], int cmd_cnt)
{
	int ret = 0;
	int ret2 = 0;
	int i;
	u32 flags = 0;

	if (!pdisp || !pdisp->panel) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = dsi_display_clk_ctrl(pdisp->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_ON);
	if (ret) {
		pr_err("%s: failed to enable all DSI clocks, rc=%d\n",
		       pdisp->name, ret);
		goto error_enable_vid_transfer;
	}

	/* acquire panel_lock to make sure no commands are in progress */
	dsi_panel_acquire_panel_lock(pdisp->panel);

	ret = dsi_display_cmd_engine_ctrl(pdisp, true);
	if (ret) {
		pr_err("%s: set cmd engine enable err ret=%d\n",
							__func__, ret);
		goto error_disable_panel_lock;
	}

	for (i = 0; i < cmd_cnt; i++) {
		flags = DSI_CTRL_CMD_FETCH_MEMORY;
		if (cmds[i].msg.type == MIPI_DSI_DCS_READ) {
			flags |= DSI_CTRL_CMD_READ;
		}
		if (cmds[i].last_command) {
			cmds[i].msg.flags |=  MIPI_DSI_MSG_LASTCOMMAND;
			flags |= DSI_CTRL_CMD_LAST_COMMAND;
		}

		ret = dsi_ctrl_cmd_transfer(pdisp->ctrl[0].ctrl, &cmds[i].msg,
								  flags);
		if (ret < 0) {
			pr_err("%s: cmd transfer failed ret=%d\n",
								__func__, ret);
			break;
		}
		if ((cmds[i].msg.type == MIPI_DSI_DCS_READ) &&
						(cmds[i].msg.rx_len != ret)) {
			pr_err("%s: cmd transfer failed "
						"read size req=%ld read=%d\n",
					__func__, cmds[i].msg.rx_len, ret);
			break;
		}
		if (cmds[i].post_wait_ms) {
			msleep(cmds[i].post_wait_ms);
		}
	}

	if (ret > 0) {
		ret = 0;
	}

	ret2 = dsi_display_cmd_engine_ctrl(pdisp, false);
	if (ret2) {
		pr_err("%s: set cmd engine disable err ret=%d\n",
							__func__, ret2);
		ret = ret2;
	}

error_disable_panel_lock:
	/* release panel_lock */
	dsi_panel_release_panel_lock(pdisp->panel);

	ret2 = dsi_display_clk_ctrl(pdisp->dsi_clk_handle,
			DSI_ALL_CLKS, DSI_CLK_OFF);
	if (ret2) {
		pr_err("%s: failed to disable all DSI clocks, rc=%d\n",
		       pdisp->name, ret2);
		ret = ret2;
	}

error_enable_vid_transfer:
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_cmn_panel_cmds_transfer_videoctrl(struct dsi_display *pdisp, struct dsi_cmd_desc cmds[], int cmd_cnt)
{
	int ret = 0;
	int ret2 = 0;


	if (!pdisp || !pdisp->panel) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	ret = drm_cmn_video_transfer_ctrl(false);
	if (ret) {
		pr_err("%s: failed stop_video\n", __func__);
		return ret;
	}


	ret = drm_cmn_panel_cmds_transfer(pdisp, cmds, cmd_cnt);

	ret2 = drm_cmn_video_transfer_ctrl(true);
	if (ret2) {
		pr_err("%s: failed start_video\n", __func__);
		ret = ret2;
	}
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_cmn_panel_dcs_write0(struct dsi_display *pdisp, char addr)
{
	int ret = 0;
	int msg_flags;
	char payload[2];
	struct dsi_cmd_desc drm_cmds;

//	msg_flags = MIPI_DSI_MSG_UNICAST;
	msg_flags = MIPI_DSI_MSG_USE_LPM | MIPI_DSI_MSG_UNICAST;

	memset(&payload, 0, sizeof(payload));
	payload[0] = addr;
//	payload[1] = data;

	memset(&drm_cmds, 0, sizeof(drm_cmds));
	drm_cmds.msg.channel  = 0;
	drm_cmds.msg.type = MIPI_DSI_DCS_SHORT_WRITE;
	drm_cmds.msg.flags    = msg_flags;
	drm_cmds.msg.ctrl     = 0;	/* 0 = dsi-master */
	drm_cmds.msg.tx_len   = 1;
	drm_cmds.msg.tx_buf   = payload;
	drm_cmds.last_command = 1;
	drm_cmds.post_wait_ms = 0;

	ret = drm_cmn_panel_cmds_transfer(pdisp, &drm_cmds, 1);

	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_cmn_panel_dcs_write1(struct dsi_display *pdisp,
				char addr,
				char data)
{
	int ret = 0;
	int msg_flags;
	char payload[2];
	struct dsi_cmd_desc drm_cmds;

//	msg_flags = MIPI_DSI_MSG_UNICAST;
	msg_flags = MIPI_DSI_MSG_USE_LPM | MIPI_DSI_MSG_UNICAST;

	memset(&payload, 0, sizeof(payload));
	payload[0] = addr;
	payload[1] = data;

	memset(&drm_cmds, 0, sizeof(drm_cmds));
	drm_cmds.msg.channel  = 0;
	drm_cmds.msg.type = MIPI_DSI_DCS_LONG_WRITE;
	drm_cmds.msg.flags    = msg_flags;
	drm_cmds.msg.ctrl     = 0;	/* 0 = dsi-master */
	drm_cmds.msg.tx_len   = 2;
	drm_cmds.msg.tx_buf   = payload;
	drm_cmds.last_command = 1;
	drm_cmds.post_wait_ms = 0;

	ret = drm_cmn_panel_cmds_transfer(pdisp, &drm_cmds, 1);

	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_cmn_panel_dcs_read(struct dsi_display *pdisp, char addr,
				int rlen, char *rbuf)
{
	int ret = 0;
	unsigned char addr_value[2] = {addr, 0x00};
	struct dsi_cmd_desc read_cmd[] = {
		{{0, MIPI_DSI_DCS_READ,
			MIPI_DSI_MSG_USE_LPM | MIPI_DSI_MSG_UNICAST,
			0, 0, 1, addr_value, rlen, rbuf}, 1, 0},
	};

	if (!pdisp) {
		return -EINVAL;
	}

	ret = drm_cmn_panel_cmds_transfer(pdisp, read_cmd, ARRAY_SIZE(read_cmd));
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_cmn_set_default_clk_rate_hz(void)
{
	struct dsi_display *display = NULL;
	struct msm_drm_private *priv = NULL;
	int default_clk_rate_hz;

	display = msm_drm_get_dsi_displey();
	if (!display) {
		pr_err("%s: Invalid dsi_display\n", __func__);
		return -EINVAL;
	}

	if (!display->modes) {
		pr_err("%s: Invalid dsi_display->modes\n", __func__);
		return -EINVAL;
	}

	default_clk_rate_hz = display->modes[0].priv_info->clk_rate_hz;

	priv = display->drm_dev->dev_private;
	if (!priv) {
		pr_err("%s: Invalid dev_private\n", __func__);
		return -EINVAL;
	}

	priv->default_clk_rate_hz = default_clk_rate_hz;

	pr_debug("%s: default_clk_rate_hz = %d\n", __func__, default_clk_rate_hz);

	return 0;
}

int drm_cmn_get_default_clk_rate_hz(void)
{
	struct dsi_display *display = NULL;
	struct msm_drm_private *priv = NULL;
	int default_clk_rate_hz = 0;

	display = msm_drm_get_dsi_displey();
	if (!display) {
		pr_err("%s: Invalid dsi_display\n", __func__);
		return 0;
	}

	priv = display->drm_dev->dev_private;
	if (!priv) {
		pr_err("%s: Invalid dev_private\n", __func__);
		return 0;
	}

	default_clk_rate_hz = priv->default_clk_rate_hz;

	pr_debug("%s: default_clk_rate_hz = %d\n", __func__, default_clk_rate_hz);

	return default_clk_rate_hz;
}
