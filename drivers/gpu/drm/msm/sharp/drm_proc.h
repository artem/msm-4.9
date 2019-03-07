/* drivers/gpu/drm/msm/sharp/drm_proc.h  (Display Driver)
 *
 * Copyright (C) 2016 SHARP CORPORATION
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

#ifndef DRM_PROC_H
#define DRM_PROC_H
#if defined(CONFIG_ANDROID_ENGINEERING)
extern int drm_proc_init(void);
#else
static int drm_proc_init(void)
{
    return 0;
}
#endif /* CONFIG_DEBUG_FS */
#endif /* DRM_PROC_H */
