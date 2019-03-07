/* include/sharp/shpwr_log.h
 *
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

#ifndef SHPWR_LOG_H
#define SHPWR_LOG_H
/*+-----------------------------------------------------------------------------+*/
/*| @ DEFINE COMPILE SWITCH :													|*/
/*+-----------------------------------------------------------------------------+*/

#define SHPWR_DURABLE_LOG_ENABLED
#define SHPWR_DUMP_REG_ENABLED

/*+-----------------------------------------------------------------------------+*/
/*| @ INCLUDE FILE :                                                            |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ VALUE DEFINE DECLARE :                                                    |*/
/*+-----------------------------------------------------------------------------+*/
#ifdef CONFIG_BATTERY_SHARP

#define SHPWR_LOG_PREFIX_BATT		"[SHBATT]"

#define SHPWR_LOG(level,type,x...)								\
{																\
	switch( type )												\
	{															\
	case SHPWR_LOG_TYPE_BATT:									\
		SHPWR_LOG_OUT( level, shpwr_log_current_level(type), SHPWR_LOG_PREFIX_BATT, x );\
		break;													\
	default:													\
		SHPWR_LOG_OUT( level, shpwr_log_current_level(type), SHPWR_LOG_PREFIX_BATT, x );\
		break;													\
	}															\
}																\

#define SHPWR_LOG_OUT(level,setting,prefix,x...)				\
{																\
	if( level <= setting )										\
	{															\
		switch(level)											\
		{														\
		case SHPWR_LOG_LEVEL_EMERG:								\
			printk(KERN_EMERG prefix x);						\
			break;												\
		case SHPWR_LOG_LEVEL_ALERT:								\
			printk(KERN_ALERT prefix x);						\
			break;												\
		case SHPWR_LOG_LEVEL_CRIT:								\
			printk(KERN_CRIT prefix x);							\
			break;												\
		case SHPWR_LOG_LEVEL_ERR:								\
			printk(KERN_ERR prefix x);							\
			break;												\
		case SHPWR_LOG_LEVEL_WARNING:							\
			printk(KERN_WARNING prefix x);						\
			break;												\
		case SHPWR_LOG_LEVEL_NOTICE:							\
			printk(KERN_NOTICE prefix x);						\
			break;												\
		case SHPWR_LOG_LEVEL_INFO:								\
			printk(KERN_INFO prefix x);							\
			break;												\
		case SHPWR_LOG_LEVEL_DEBUG:								\
			printk(KERN_DEBUG prefix x);						\
			break;												\
		default:												\
			printk(KERN_INFO prefix x);							\
			break;												\
		}														\
	}															\
}
#else
#define SHPWR_LOG(level,type,x...)		do {} while(0)
#endif /* CONFIG_BATTERY_SHARP */
/*+-----------------------------------------------------------------------------+*/
/*| @ ENUMERATION DECLARE :                                                     |*/
/*+-----------------------------------------------------------------------------+*/

typedef enum {
	SHPWR_LOG_LEVEL_EMERG = 1,		/* [ 1]system is unusable				*/
	SHPWR_LOG_LEVEL_ALERT,			/* [ 2]action must be taken immediately	*/
	SHPWR_LOG_LEVEL_CRIT,			/* [ 3]critical conditions				*/
	SHPWR_LOG_LEVEL_ERR,			/* [ 4]error conditions					*/
	SHPWR_LOG_LEVEL_WARNING,		/* [ 5]warning conditions				*/
	SHPWR_LOG_LEVEL_NOTICE,			/* [ 6]normal but significant condition	*/
	SHPWR_LOG_LEVEL_INFO,			/* [ 7]informational					*/
	SHPWR_LOG_LEVEL_DEBUG			/* [ 8]debug-level messages				*/
}shpwr_log_level;

typedef enum {
	SHPWR_LOG_TYPE_BATT = 0,
	SHPWR_LOG_TYPE_MAX
}shpwr_log_type;

typedef enum shpwr_result_tag
{
	SHPWR_RESULT_SUCCESS,
	SHPWR_RESULT_FAIL,
	SHPWR_RESULT_REJECTED,
	NUM_SHPWR_RESULT

} shpwr_result_t;

/*+-----------------------------------------------------------------------------+*/
/*| @ STRUCT & UNION DECLARE :                                                  |*/
/*+-----------------------------------------------------------------------------+*/


#define SHPWR_DBG_KERNEL_FILE_CHECK_POINTER_ALWAYS_OK ((void *)1)

/*+-----------------------------------------------------------------------------+*/
/*| @ PUBLIC VARIABLE :                                                         |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ PUBLIC FUNCTION PROTO TYPE DECLARE :                                      |*/
/*+-----------------------------------------------------------------------------+*/
bool shpwr_is_initialized(void);

void shpwr_log_out(int level, char *fmt, ...);

#ifdef CONFIG_BATTERY_SHARP
shpwr_log_level shpwr_log_current_level( shpwr_log_type type );

#ifdef SHPWR_DURABLE_LOG_ENABLED
void shpwr_dbg_log_init(void);
void shpwr_add_dbg_log(char *fmt, ...);
#endif /* SHPWR_DURABLE_LOG_ENABLED */

#ifdef SHPWR_DUMP_REG_ENABLED
void shpwr_dump_reg_init(void);
void shpwr_add_dump_reg(bool force_save_flg, char *fmt, ...);
#endif /* SHPWR_DUMP_REG_ENABLED */
#endif /* CONFIG_BATTERY_SHARP */

/*+-----------------------------------------------------------------------------+*/
/*| @ PRIVATE FUNCTION PROTO TYPE DECLARE :                                     |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ THIS FILE END :                                                           |*/
/*+-----------------------------------------------------------------------------+*/

#endif /* SHPWR_LOG_H */
