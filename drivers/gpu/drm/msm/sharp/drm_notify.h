/* drivers/gpu/drm/msm/sharp/drm_notify.h  (Display Driver)
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

#ifndef _DRM_NOTIFY_H_
#define _DRM_NOTIFY_H_

#include <drm/drm_sharp.h>

/*
 * DRM notifier interfaces
 */
extern int drm_notify_create_sysfs(struct device *dev);
extern void drm_notify_remove_sysfs(struct device *dev);
extern void drm_sysfs_notifier(struct device *dev, int blank);

#endif /* _DRM_NOTIFY_H */
