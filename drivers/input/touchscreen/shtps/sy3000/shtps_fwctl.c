/* drivers/input/touchscreen/shtps/sy3000/shtps_fwctl.c
 *
 * Copyright (c) 2017, Sharp. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/input/shtps_dev.h>

#include "shtps_rmi.h"
#include "shtps_fwctl.h"
#include "shtps_log.h"
#include "shtps_param_extern.h"

/* -------------------------------------------------------------------------- */
extern struct shtps_fwctl_functbl		shtps_fwctl_s3400_function_table;
extern struct shtps_fwctl_functbl		shtps_fwctl_s3700_function_table;

struct shtps_fwctl_functbl	*shtps_fwctl_function[]={
	&shtps_fwctl_s3400_function_table,
	&shtps_fwctl_s3700_function_table,
	NULL,
};

/* -------------------------------------------------------------------------- */
int shtps_fwctl_init(struct shtps_rmi_spi *ts_p, void *tps_ctrl_p, struct shtps_ctrl_functbl *func_p)
{
	ts_p->fwctl_p = kzalloc(sizeof(struct shtps_fwctl_info), GFP_KERNEL);
	if(ts_p->fwctl_p == NULL){
		PR_ERROR("memory allocation error:%s()\n", __func__);
		return -ENOMEM;
	}

	#if defined(SHTPS_DEF_FWCTL_IC_TYPE_3700)
		ts_p->fwctl_p->fwctl_ic_type = 1;
	#else
		ts_p->fwctl_p->fwctl_ic_type = 0;
	#endif /* SHTPS_DEF_FWCTL_IC_TYPE_3700 */
	ts_p->fwctl_p->fwctl_func_p = shtps_fwctl_function[ ts_p->fwctl_p->fwctl_ic_type ];
	ts_p->fwctl_p->devctrl_func_p = func_p;	/* func.table spi */
	ts_p->fwctl_p->tps_ctrl_p = tps_ctrl_p;	/* struct device */

	ts_p->fwctl_p->map_p = shtps_fwctl_ic_init(ts_p);
	if(ts_p->fwctl_p->map_p == NULL){
		PR_ERROR("map memory allocation error:%s()\n", __func__);
		kfree(ts_p->fwctl_p);
		return -ENOMEM;
	}

	return 0;
}

/* -------------------------------------------------------------------------- */
void shtps_fwctl_deinit(struct shtps_rmi_spi *ts_p)
{
	if(ts_p->fwctl_p->map_p)	kfree(ts_p->fwctl_p->map_p);
	ts_p->fwctl_p->map_p = NULL;

	if(ts_p->fwctl_p)	kfree(ts_p->fwctl_p);
	ts_p->fwctl_p = NULL;
}

/* -------------------------------------------------------------------------- */
