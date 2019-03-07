/* include/sharp/flbl.h  (Display Driver)
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
#ifndef FLBL_H
#define FLBL_H

#include <linux/types.h>

/* ------------------------------------------------------------------------- */
/* STRUCT                                                                    */
/* ------------------------------------------------------------------------- */
struct flbl_bkl_pwm_param {
	uint8_t logic;
	uint8_t defaultStat;
	uint8_t pulseNum;
	uint16_t high1;
	uint16_t total1;
	uint16_t high2;
	uint16_t total2;
	uint8_t pulse2Cnt;
};

struct flbl_cb_tbl {
	int (*enable_bkl_pwm)(int enable);
	int (*set_param_bkl_pwm)(struct flbl_bkl_pwm_param *param);
	int (*set_port_bkl_pwm)(int port);
};

/* ------------------------------------------------------------------------- */
/* PROTOTYPES                                                                */
/* ------------------------------------------------------------------------- */
#if defined(CONFIG_SHARP_DISPLAY) && defined(CONFIG_LEDS_QPNP_WLED)
int flbl_register_cb(struct flbl_cb_tbl *cb_tbl);
#else
static inline int flbl_register_cb(struct flbl_cb_tbl *cb_tbl)
{
	return 0;
};
#endif
#endif /* FLBL_H */
