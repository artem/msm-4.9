/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __LEDS_QPNP_HAPTICS_H
#define __LEDS_QPNP_HAPTICS_H

#ifdef CONFIG_LEDS_SHARP_QPNP_HAPTIC
enum {
	QPNP_HAP_VIB_STOP,
	QPNP_HAP_VIB_START
};
int qpnp_hap_register_notifier(struct notifier_block *nb);
int qpnp_hap_unregister_notifier(struct notifier_block *nb);
#else
static inline int qpnp_hap_register_notifier(struct notifier_block *nb)
{
	return 0;
}
static inline int qpnp_hap_unregister_notifier(struct notifier_block *nb)
{
	return 0;
}
#endif /* CONFIG_LEDS_SHARP_QPNP_HAPTIC */
#endif /* __LEDS_QPNP_HAPTICS_H */
