/*
 *  cxd224x-i2c.c - cxd224x NFC driver
 *
 * Copyright (C) 2012-2015 Sony Corporation.
 * Copyright (C) 2012 Broadcom Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _CXD224X_H
#define _CXD224X_H

#include "nfc.h"

#define CXDNFC_MAGIC 'S'
/*
 * CXDNFC power control via ioctl
 * CXDNFC_POWER_CTL(0): power off
 * CXDNFC_POWER_CTL(1): power on
 * CXDNFC_WAKE_CTL(0): PON HIGH (normal power mode)
 * CXDNFC_WAKE_CTL(1): PON LOW (low power mode)
 * CXDNFC_WAKE_RST():  assert XRST
 */
#define CXDNFC_POWER_CTL		_IO(CXDNFC_MAGIC, 0x01)
#define CXDNFC_WAKE_CTL			_IO(CXDNFC_MAGIC, 0x02)
#define CXDNFC_RST_CTL			_IO(CXDNFC_MAGIC, 0x03)
#define CXDNFC_GET_PON_CTL		_IOR(CXDNFC_MAGIC, 0x04, unsigned int)
#ifdef CONFIG_NFC_CXD224X_TST1	// for TIMPANI
#define CXDNFC_TST1_CTL			_IO(CXDNFC_MAGIC, 0x05)
#endif	// CONFIG_NFC_CXD224X_TST1

#define CXDNFC_RST_ACTIVE 1            /* ActiveHi = 1, ActiveLow = 0 */

struct cxd224x_platform_data {
	unsigned int irq_gpio;
	unsigned int en_gpio;
	unsigned int wake_gpio;
	unsigned int rst_gpio;
#ifdef CONFIG_NFC_CXD224X_TST1	// for TIMPANI
	unsigned int tst1_gpio;
#endif	// CONFIG_NFC_CXD224X_TST1
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pin_default_int;
	struct pinctrl_state	*pin_default_wake;
	struct pinctrl_state	*pin_default_rst;
};

#endif
