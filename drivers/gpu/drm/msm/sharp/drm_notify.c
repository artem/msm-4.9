/* drivers/gpu/drm/msm/sharp/drm_notify.c  (Display Driver)
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
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/notifier.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include "drm_notify.h"

static int show_blank_event_val = 1; /* panel_power_off = 1 */
static ssize_t drm_notify_show_blank_event(struct device *dev,
		struct device_attribute *attr, char *buf);

/**
 * sysfs attribute
 */
static DEVICE_ATTR(show_blank_event, S_IRUGO, drm_notify_show_blank_event, NULL);
static struct attribute *drm_notify_attrs[] = {
	&dev_attr_show_blank_event.attr,
	NULL
};

static struct attribute_group drm_notify_attr_group = {
    .name = "display",
	.attrs = drm_notify_attrs,
};

/**
 * sysfs create file
 */
int drm_notify_create_sysfs(struct device *dev)
{
	int rc = 0;
	pr_debug("%s: device_name = [%s]\n", __func__, dev->kobj.name);

	if (dev) {
		rc = sysfs_create_group(&dev->kobj,
					&drm_notify_attr_group);
		if (rc) {
			pr_err("%s: sysfs group creation failed, rc=%d\n", __func__, rc);
		}
	}

	/* display blank*/
	show_blank_event_val = 1;
	return rc;
}

/**
 * sysfs remove file
 */
void drm_notify_remove_sysfs(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &drm_notify_attr_group);
}

/**
 * sysfs notifier
 */
void drm_sysfs_notifier(struct device *dev, int blank)
{
	pr_debug("%s: blank = %d start\n", __func__, blank);

	show_blank_event_val = blank;
	sysfs_notify(&dev->kobj, "display", "show_blank_event");
}

/**
 * sysfs notifier - sysfs update
 */
static ssize_t drm_notify_show_blank_event(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	pr_debug("%s: panel_power_on = %d\n", __func__, show_blank_event_val);

	ret = scnprintf(buf, PAGE_SIZE, "panel_power_on = %d\n",
						show_blank_event_val);
	return ret;
}
