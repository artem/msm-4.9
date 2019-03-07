/* include/sharp/shrlog_restart.h
 *
 * Copyright (C) 2010 Sharp Corporation
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

#ifndef __SHARP_SHRLOG_RESTART_H
#define __SHARP_SHRLOG_RESTART_H

#include <soc/qcom/restart.h>

#if defined(CONFIG_SHARP_HANDLE_PANIC)
struct sharp_msm_restart_callback {
	int (*preset_reason)(int);
	bool (*warm_reset_ow)(bool, const char *cmd, bool, int, int);
	void (*restart_reason_ow)(const char *, bool, int, int);
	int (*enabled_fn)(void);
	void (*poweroff_ow)(void);
	void (*restart_addr_set)(void *);
};
int sharp_msm_set_restart_callback(struct sharp_msm_restart_callback *cb);
#endif /* CONFIG_SHARP_HANDLE_PANIC */

#endif /* __SHARP_SHRLOG_RESTART_H */


