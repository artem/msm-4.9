/* include/drm/drm_sharp.h  (Display Driver)
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

#ifndef _DRM_SHARP_H_
#define _DRM_SHARP_H_

#ifdef CONFIG_SHARP_DISPLAY /* CUST_ID_00040 */
extern int drm_base_fps_low_mode(void);
#else
static inline int drm_base_fps_low_mode(void)
{
	return 0;
};
#endif /* CONFIG_SHARP_DISPLAY */

#endif /* _DRM_SHARP_H */
