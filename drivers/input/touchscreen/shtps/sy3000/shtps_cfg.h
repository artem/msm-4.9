/* drivers/input/touchscreen/shtps/sy3000/shtps_cfg.h
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
#ifndef __SHTPS_CFG_H__
#define __SHTPS_CFG_H__
/* --------------------------------------------------------------------------- */

#include <linux/input/shtps_dev.h>
/* --------------------------------------------------------------------------- */
#if defined(CONFIG_SHARP_TPS_SY3000_PRJ_001860)
	#include "prj-001860/shtps_cfg_prj-001860.h"

#elif defined(CONFIG_SHARP_TPS_SY3700_CUSPRJ_1846)
	#include "cusprj-1846/shtps_cfg_cusprj-1846.h"

#elif defined(CONFIG_SHARP_TPS_SY3700_CUSPRJ_2295)
	#include "cusprj-2295/shtps_cfg_cusprj-2295.h"

#else
	#include "prj-001860/shtps_cfg_prj-001860.h"

#endif

/* --------------------------------------------------------------------------- */
#endif	/* __SHTPS_CFG_H__ */

