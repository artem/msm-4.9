/* drivers/sharp/shbatt/shbatt_type.h
 *
 * Copyright (C) 2014 SHARP CORPORATION All rights reserved.
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

#ifndef SHBATT_TYPE_H
#define SHBATT_TYPE_H

/*+-----------------------------------------------------------------------------+*/
/*| @ INCLUDE FILE :                                                            |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ VALUE DEFINE DECLARE :                                                    |*/
/*+-----------------------------------------------------------------------------+*/
#define SHBATT_IOC_MAGIC						's'

/*+-----------------------------------------------------------------------------+*/
/*| @ ENUMERATION DECLARE :                                                     |*/
/*+-----------------------------------------------------------------------------+*/
/*attributes.*/
/*cable(ac,usb)status attributes.*/

/* Timer */
typedef enum shbatt_api_to_tsk_command_tag
{
	SHBATT_TASK_CMD_INVALID,
	SHBATT_TASK_CMD_BATTLOG_EVENT,
/*| TODO: New API add point */
	NUM_SHBATT_TASK_CMD,
} shbatt_api_to_tsk_command_t;

typedef enum
{
	SHBATT_CMD_INVALID,
	NUM_SHBATT_CMD,
} shbatt_kernel_to_user_command_t;

/*+-----------------------------------------------------------------------------+*/
/*| @ STRUCT & UNION DECLARE :                                                  |*/
/*+-----------------------------------------------------------------------------+*/

typedef struct shbatt_packet_hdr_tag
{
	shbatt_api_to_tsk_command_t	cmd;
	struct completion*			cmp_p;
	shbatt_result_t* 			ret_p;
} shbatt_packet_hdr_t;

typedef union shbatt_usse_packet_prm_tag
{
	int							evt;
} shbatt_usse_packet_prm_t;

typedef union shbatt_packet_prm_tag
{
	int							evt;
	void*						param_p;
} shbatt_packet_prm_t;

typedef struct shbatt_packet_tag
{
	struct work_struct			work;
	shbatt_packet_hdr_t			hdr;
	shbatt_packet_prm_t			prm;
	bool						is_used;
} shbatt_packet_t;

/*+-----------------------------------------------------------------------------+*/
/*| @ CALLBACK FUNCTION TYPE DECLARE :                                          |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ THIS FILE END :                                                           |*/
/*+-----------------------------------------------------------------------------+*/

#endif /* SHBATT_TYPE_H */
