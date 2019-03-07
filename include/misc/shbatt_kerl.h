/*
 * Copyright (C) 2017 SHARP CORPORATION All rights reserved.
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

#ifndef SHBATT_KERL_H
#define SHBATT_KERL_H

/*+-----------------------------------------------------------------------------+*/
/*| @ INCLUDE FILE :                                                            |*/
/*+-----------------------------------------------------------------------------+*/
#ifdef CONFIG_SHARP_SHTERM
#include <misc/shterm_k.h>
#endif /* CONFIG_SHARP_SHTERM */

//#include <misc/shub_driver.h>

/*+-----------------------------------------------------------------------------+*/
/*| @ VALUE DEFINE DECLARE :                                                    |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ ENUMERATION DECLARE :                                                     |*/
/*+-----------------------------------------------------------------------------+*/

typedef enum shbatt_result_tag
{
	SHBATT_RESULT_SUCCESS,
	SHBATT_RESULT_FAIL,
	SHBATT_RESULT_REJECTED,
	NUM_SHBATT_RESULT

} shbatt_result_t;

/*+-----------------------------------------------------------------------------+*/
/*| @ STRUCT & UNION DECLARE :                                                  |*/
/*+-----------------------------------------------------------------------------+*/
typedef struct shbatt_smem_info_tag
{
	unsigned char			traceability_info[24];
} shbatt_smem_info_t;

/*+-----------------------------------------------------------------------------+*/
/*| @ PUBLIC VARIABLE :                                                         |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ PUBLIC FUNCTION PROTO TYPE DECLARE :                                      |*/
/*+-----------------------------------------------------------------------------+*/
#ifdef CONFIG_SHARP_SHTERM
shbatt_result_t shbatt_api_get_battery_log_info(
	shbattlog_info_t*		bli_p );

shbatt_result_t shbatt_api_battlog_event(
	shbattlog_event_num			evt);

shbatt_result_t shbatt_api_battlog_charge_status(
	int			status);

shbatt_result_t shbatt_api_battlog_charge_error(
	int			charge_error_event);

shbatt_result_t shbatt_api_battlog_jeita_status(
	int			jeita_cur_status);

shbatt_result_t shbatt_api_battlog_capacity(
	int			cur_capacity);

shbatt_result_t shbatt_api_battlog_usb_type(
	int			usb_type);

shbatt_result_t shbatt_api_battlog_typec_mode(
	int			typec_mode);
#endif /* CONFIG_SHARP_SHTERM */

bool is_shbatt_task_initialized( void );

shbatt_result_t shbatt_api_store_fg_cap_learning_result(
	int64_t learned_cc_uah,
	int nom_cap_uah,
	int high_thresh,
	int low_thresh
);
/*+-----------------------------------------------------------------------------+*/
/*| @ PRIVATE FUNCTION PROTO TYPE DECLARE :                                     |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ THIS FILE END :                                                           |*/
/*+-----------------------------------------------------------------------------+*/

#endif /* SHBATT_KERL_H */
