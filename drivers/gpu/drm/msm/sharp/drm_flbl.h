/* drivers/gpu/drm/msm/sharp/drm_flbl.h  (Display Driver)
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
#ifndef DRM_FLBL_H
#define DRM_FLBL_H

#if defined(CONFIG_SHARP_DISPLAY) && defined(CONFIG_LEDS_QPNP_WLED)
extern void drm_flbl_init(struct device *dev);
#else
static inline void drm_flbl_init(struct device *dev)
{
	return;
};
#endif
#endif /* DRM_FLBL_H */
