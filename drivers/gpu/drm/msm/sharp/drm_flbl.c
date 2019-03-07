/* drivers/gpu/drm/msm/sharp/drm_flbl.c  (Display Driver)
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
#include <linux/types.h>
#include <linux/kernel.h>
#include <video/mipi_display.h>
#include <sharp/flbl.h>
#include "../msm_drv.h"
#include "../dsi-staging/dsi_display.h"
#include "msm_drm_context.h"
#include "drm_cmn.h"

/* ------------------------------------------------------------------------- */
/* DEFINE                                                                    */
/* ------------------------------------------------------------------------- */
#define SHUB_ERR_PWM_START         (-5)

/* ------------------------------------------------------------------------- */
/* STRUCTURE                                                                 */
/* ------------------------------------------------------------------------- */
struct drm_flbl_ctx {
	unsigned int state;
	unsigned int sync1_high;
	unsigned int bl_pwm_high;
};
static struct drm_flbl_ctx flbl_ctx = {0};

/* ------------------------------------------------------------------------- */
/* FUNCTION                                                                  */
/* ------------------------------------------------------------------------- */
static int drm_flbl_validate_command(unsigned int state,
		unsigned int sync1_high, unsigned int bl_pwm_high);
static int drm_flbl_on(struct dsi_display *display, int sync1_high, int bl_pwm_high);
static int drm_flbl_off(struct dsi_display *display);
static int drm_flbl_vsout_on(struct dsi_display *display, int sync1_high);
static int drm_flbl_vsout_off(struct dsi_display *display);
/* ------------------------------------------------------------------------- */
/* EXTERN FUNCTION                                                           */
/* ------------------------------------------------------------------------- */
extern int qpnp_wled_flbl_led_on(void);
extern int qpnp_wled_flbl_bl_on(void);
extern int qpnp_wled_flbl_led_off(void);
extern int qpnp_wled_flbl_bl_off(void);

static struct flbl_cb_tbl set_cb_tbl = {0};
int flbl_register_cb(struct flbl_cb_tbl *cb_tbl)
{
	if (!cb_tbl) {
		pr_err("%s: failed to cb_tbl is NULL\n", __func__);
		return -EINVAL;
	}
	memcpy(&set_cb_tbl, cb_tbl, sizeof(struct flbl_cb_tbl));
	return 0;
}
EXPORT_SYMBOL(flbl_register_cb);

static ssize_t drm_flbl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u,%u,%u\n",
		flbl_ctx.state, flbl_ctx.sync1_high, flbl_ctx.bl_pwm_high);
}

static ssize_t drm_flbl_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int data[3] = {0};
	int ret = 0;
	struct dsi_display *display;

	display = msm_drm_get_dsi_displey();
	if (!display) {
		pr_err("%s: Invalid display data\n", __func__);
		goto exit;
	}

	ret = sscanf(buf, "%u,%u,%u", &data[0], &data[1], &data[2]);
	if (ret == 3) {
		if (!drm_flbl_validate_command(data[0], data[1], data[2])) {
			if (data[0]) {
				ret = drm_flbl_on(display, data[1], data[2]);
			} else {
				ret = drm_flbl_off(display);
			}
			if (!ret) {
				flbl_ctx.state = data[0];
				flbl_ctx.sync1_high = data[1];
				flbl_ctx.bl_pwm_high = data[2];
			}
		} else {
			pr_err("%s: parameter error\n", __func__);
		}
	} else {
		pr_err("%s: parameter number error\n", __func__);
	}

	pr_debug("%s: state=%d, sync1=%d, bl_pmw=%d\n", __func__,
			data[0], data[1], data[2]);
exit:
	return count;
}

static DEVICE_ATTR(flbl, 0664, drm_flbl_show, drm_flbl_store);
static struct attribute *flbl_attrs[] = {
	&dev_attr_flbl.attr,
	NULL
};

static const struct attribute_group flbl_attr_group = {
	.attrs = flbl_attrs,
};

void drm_flbl_init(struct device *dev)
{
	int rc;

	if (dev) {
		rc = sysfs_create_group(&dev->kobj,
					&flbl_attr_group);
		if (rc) {
			pr_err("%s: sysfs group creation failed, rc=%d\n", __func__, rc);
		}
	}

	return;
}

static int drm_flbl_validate_command(unsigned int state,
		unsigned int sync1_high, unsigned int bl_pwm_high)
{
	if (state) {
		if (flbl_ctx.state && (flbl_ctx.sync1_high == sync1_high) &&
			(flbl_ctx.bl_pwm_high == bl_pwm_high)) {
			pr_info("%s: same request\n", __func__);
			return -EINVAL;
		}

		if ((sync1_high < 1) || (sync1_high > 6500)) {
			return -EINVAL;
		}

		if ((bl_pwm_high < 2) || (bl_pwm_high > 7999)) {
			return -EINVAL;
		}
	} else {
		if (!flbl_ctx.state) {
			pr_info("%s: same request\n", __func__);
			return -EINVAL;
		}

		if (sync1_high != 0 || bl_pwm_high != 0) {
			return -EINVAL;
		}
	}

	return 0;
}

static int drm_flbl_on(struct dsi_display *display, int sync1_high, int bl_pwm_high)
{
	int rc = 0;
	struct flbl_bkl_pwm_param param;

	pr_debug("%s: in sync1=%d, bl_pwm=%d \n", __func__, sync1_high, bl_pwm_high);

	if (display->ctrl[0].ctrl->current_state.power_state
					== DSI_CTRL_POWER_VREG_OFF) {
		pr_err("%s: display's power state is OFF\n", __func__);
		return -EINVAL;
	}

	rc = qpnp_wled_flbl_bl_on();
	if (rc) {
		pr_err("%s: failed to qpnp_wled_flbl_bl_on(), rc = %d\n", __func__, rc);
		return -EINVAL;
	}

	rc = drm_flbl_vsout_on(display, sync1_high);

	param.logic       = 1;
	param.defaultStat = 1;
	param.pulseNum    = 1;
	param.high1       = bl_pwm_high;
	param.total1      = 8000;
	param.high2       = 40;
	param.total2      = 81;
	param.pulse2Cnt   = 1;

	if (set_cb_tbl.set_param_bkl_pwm) {
		rc = set_cb_tbl.set_param_bkl_pwm(&param);
	} else {
		pr_err("%s: set_param_bkl_pwm Is NULL", __func__);
		return -EINVAL;
	}
	pr_debug("%s: call shub_api_set_param_bkl_pwm(), rc = %d\n", __func__, rc);
	if (rc) {
		pr_err("%s: failed to shub_api_set_param_bkl_pwm(), rc = %d\n", __func__, rc);
		return -EINVAL;
	}

	if (set_cb_tbl.enable_bkl_pwm) {
		rc = set_cb_tbl.enable_bkl_pwm(1);
	} else {
		pr_err("%s: enable_bkl_pwm Is NULL", __func__);
		return -EINVAL;
	}
	pr_debug("%s: call shub_api_enable_bkl_pwm(), rc = %d\n", __func__, rc);
	if (rc && rc != SHUB_ERR_PWM_START) {
		pr_err("%s: failed to shub_api_enable_bkl_pwm(), rc = %d\n", __func__, rc);
		return -EINVAL;
	}

	if (set_cb_tbl.set_port_bkl_pwm) {
		rc = set_cb_tbl.set_port_bkl_pwm(1);
	} else {
		pr_err("%s: set_port_bkl_pwm Is NULL", __func__);
		return -EINVAL;
	}
	pr_debug("%s: call shub_api_set_port_bkl_pwm(), rc = %d\n", __func__, rc);
	if (rc) {
		pr_err("%s: failed to shub_api_set_port_bkl_pwm(), rc = %d\n", __func__, rc);
		return -EINVAL;
	}

	rc = qpnp_wled_flbl_led_on();
	if (rc) {
		pr_err("%s: failed to qpnp_wled_flbl_led_on(), rc = %d\n", __func__, rc);
		return -EINVAL;
	}

	return rc;
}

static int drm_flbl_off(struct dsi_display *display)
{
	int rc = 0;

	pr_debug("%s: in\n", __func__);
	rc = qpnp_wled_flbl_led_off();
	if (rc) {
		pr_err("%s: failed to qpnp_wled_flbl_led_off(), rc = %d\n", __func__, rc);
		return -EINVAL;
	}


	if (set_cb_tbl.enable_bkl_pwm) {
		rc = set_cb_tbl.enable_bkl_pwm(0);
	} else {
		pr_err("%s: enable_bkl_pwm Is NULL", __func__);
		return -EINVAL;
	}
	pr_debug("%s: call shub_api_enable_bkl_pwm(), rc = %d\n", __func__, rc);
	if (rc && rc != SHUB_ERR_PWM_START) {
		pr_err("%s: failed to shub_api_enable_bkl_pwm(), rc = %d\n", __func__, rc);
		return -EINVAL;
	}

	if (set_cb_tbl.set_port_bkl_pwm) {
		rc = set_cb_tbl.set_port_bkl_pwm(0);
	} else {
		pr_err("%s: enable_bkl_pwm Is NULL", __func__);
		return -EINVAL;
	}
	pr_debug("%s: call shub_api_set_port_bkl_pwm(), rc = %d\n", __func__, rc);
	if (rc) {
		pr_err("%s: failed to shub_api_set_port_bkl_pwm(), rc = %d\n", __func__, rc);
		return -EINVAL;
	}

	rc = drm_flbl_vsout_off(display);

	rc = qpnp_wled_flbl_bl_off();
	if (rc) {
		pr_err("%s: failed to qpnp_wled_flbl_bl_off(), rc = %d\n", __func__, rc);
		return -EINVAL;
	}

	return rc;
}

static int drm_flbl_vsout_on(struct dsi_display *display, int sync1_high)
{
	int rc = 0;
	unsigned int sync1_active;

	unsigned char flbl_proc_cmd_addr_value[][2] = {
		{0xFF, 0x24},
		{0xB9, 0x20},
		{0xBC, 0x0C},
		{0xBD, 0x00}, /* sync1_active */
		{0xB4, 0x04},
		{0xB6, 0x17},
		{0xFF, 0x10},
		{0xBC, 0x00},
	};

	struct dsi_cmd_desc vsout_on_cmd[] = {
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[0], 0, NULL}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[1], 0, NULL}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[2], 0, NULL}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[3], 0, NULL}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[4], 0, NULL}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[5], 0, NULL}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[6], 0, NULL}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[7], 0, NULL}, 1, 0},
	};

	sync1_active = max((sync1_high * 10 / 128),1); // sync1_high/3.2/4
	if (sync1_active > 255) {
		sync1_active = 255;
	}
	flbl_proc_cmd_addr_value[3][1] = (unsigned char)sync1_active;

	pr_debug("%s: sync1_high=%d, sync1_active=0x%02X(%d)\n", __func__, sync1_high, flbl_proc_cmd_addr_value[3][1], sync1_active);

	rc = drm_cmn_panel_cmds_transfer_videoctrl(display, vsout_on_cmd, ARRAY_SIZE(vsout_on_cmd));
	if (rc < 0) {
		pr_err("%s: faild to drm_cmn_panel_cmds_transfer_videoctrl rc = %d.\n", __func__, rc);
		return -EINVAL;
	}
	return rc;
}

static int drm_flbl_vsout_off(struct dsi_display *display)
{
	int rc = 0;

	unsigned char flbl_proc_cmd_addr_value[][2] = {
		{0xFF, 0x24},
		{0xB4, 0x06},
		{0xB6, 0x11},
		{0xFF, 0x10},
		{0xBC, 0x1A},
	};

	struct dsi_cmd_desc vsout_off_cmd[] = {
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[0], 0, NULL}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[1], 0, NULL}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[2], 0, NULL}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[3], 0, NULL}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, MIPI_DSI_MSG_USE_LPM, 0, 0, 2, flbl_proc_cmd_addr_value[4], 0, NULL}, 1, 0},
	};

	if (display->ctrl[0].ctrl->current_state.power_state
					== DSI_CTRL_POWER_VREG_OFF) {
		pr_err("%s: display's power state is OFF\n", __func__);
		return -EINVAL;
	}

	rc = drm_cmn_panel_cmds_transfer_videoctrl(display, vsout_off_cmd, ARRAY_SIZE(vsout_off_cmd));
	if (rc < 0) {
		pr_err("%s: faild to drm_cmn_panel_cmds_transfer_videoctrl rc = %d.\n", __func__, rc);
		return -EINVAL;
	}

	return rc;
}
