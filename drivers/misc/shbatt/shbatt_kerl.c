/* drivers/sharp/shbatt/shbatt_kerl.c
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

/*+-----------------------------------------------------------------------------+*/
/*| @ DEFINE COMPILE SWITCH :                                                   |*/
/*+-----------------------------------------------------------------------------+*/
/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ INCLUDE FILE :                                                            |*/
/*+-----------------------------------------------------------------------------+*/

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/power_supply.h>
#include <linux/gpio.h>
#include <linux/poll.h>
#include <linux/alarmtimer.h>	/* Timer */
#include <linux/time.h>	/* Timer */
#include <linux/qpnp/qpnp-adc.h>
#include <linux/namei.h>
#include <linux/of.h>

#include "misc/shbatt_kerl.h"
#include "shbatt_type.h"
#include "misc/shpwr_log.h"
#include "soc/qcom/sh_smem.h"
#include "soc/qcom/sharp/shdiag_smd.h"
#ifdef CONFIG_SHARP_SHTERM
#include "misc/shterm_k.h"
#endif /* CONFIG_SHARP_SHTERM */
#include <linux/notifier.h>

/*+-----------------------------------------------------------------------------+*/
/*| @ LOCAL MACRO DECLARE :                                                     |*/
/*+-----------------------------------------------------------------------------+*/
#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "SHBATT:%s: " fmt, __func__

#define SHBATT_ERROR(x...)	SHPWR_LOG(SHPWR_LOG_LEVEL_ERR,SHPWR_LOG_TYPE_BATT,x)
#define SHBATT_INFO(x...)	SHPWR_LOG(SHPWR_LOG_LEVEL_INFO,SHPWR_LOG_TYPE_BATT,x)
#define SHBATT_TRACE(x...)	SHPWR_LOG(SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,x)

#define SHBATT_DEV_NAME						"shbatt"
#define SHBATT_OF_DEV_NAME					"sharp,shbatt"
#define SHBATT_ATTR_ARRAY_END_NAME			"END_NULL"
#define SHBATT_DEPLETED_JUDGE_AVERAGE_COUNT 5

#define SHBATT_WAKE_CTL(x)										\
{																\
	do															\
	{															\
		if(x == 0)												\
		{														\
			SHBATT_TRACE("[P] %s SHBATT_WAKE_CTL(0) call shbatt_wakeup_source_num=%d \n",__FUNCTION__, atomic_read(&shbatt_wakeup_source_num));	\
			if(atomic_dec_return(&shbatt_wakeup_source_num) == 0)	\
			{													\
				if(shbatt_wakeup_source.active) 				\
				{												\
					__pm_relax(&shbatt_wakeup_source); 				\
					SHBATT_TRACE("[P] %s SHBATT_WAKE_CTL(0) done shbatt_wakeup_source_num=%d \n",__FUNCTION__, atomic_read(&shbatt_wakeup_source_num));	\
				}												\
			}													\
		}														\
		else													\
		{														\
			SHBATT_TRACE("[P] %s SHBATT_WAKE_CTL(1) call shbatt_wakeup_source_num=%d \n",__FUNCTION__, atomic_read(&shbatt_wakeup_source_num));	\
			if(atomic_inc_return(&shbatt_wakeup_source_num) == 1)	\
			{													\
				SHBATT_TRACE("[P] %s SHBATT_WAKE_CTL(1) done shbatt_wakeup_source_num=%d \n",__FUNCTION__, atomic_read(&shbatt_wakeup_source_num));	\
				__pm_stay_awake(&shbatt_wakeup_source);					\
			}													\
		}														\
	} while(0); 												\
}

#define SHBATT_ATTR_END											\
{																\
	.attr  =													\
	{															\
		.name = SHBATT_ATTR_ARRAY_END_NAME,						\
		.mode = S_IRUGO | S_IWUSR | S_IWGRP						\
	},															\
	.show  = NULL,												\
	.store = NULL,												\
}

#define SHPWR_LOG_INFO(fmt, ...) { \
     shpwr_add_dbg_log(pr_fmt(fmt), ##__VA_ARGS__); \
     pr_info(fmt, ##__VA_ARGS__); \
}
#define SHPWR_DUMP_REG_INFO(fmt, ...) shpwr_add_dump_reg(false, fmt, ##__VA_ARGS__)
#define SHPWR_DUMP_REG_INFO_AND_FORCESAVE(fmt, ...) shpwr_add_dump_reg(true, fmt, ##__VA_ARGS__)

/*+-----------------------------------------------------------------------------+*/
/*| @ VALUE DEFINE DECLARE :                                                    |*/
/*+-----------------------------------------------------------------------------+*/
/* wake_lock */
#define SHBATT_LOCK_FUNC_LEN				64	/* SH_PWR_DEBUG T.B.D */
#define DEFAULT_CL_HIGH_THRESH				45
#define DEFAULT_CL_LOW_THRESH				45
#define DEFAULT_CL_START_SOC				15

/*+-----------------------------------------------------------------------------+*/
/*| @ ENUMERATION DECLARE :                                                     |*/
/*+-----------------------------------------------------------------------------+*/
typedef enum
{
	SHBATT_TASK_COMMAND_LOCK,
	SHBATT_MUTEX_TYPE_NUM
}shbatt_kernel_mutex_type;

#ifdef CONFIG_PM_SUPPORT_BATT_TRACEABILITY
typedef enum {
	SHBATT_SYSFS_PARAM_TYPE_STRING,
	SHBATT_SYSFS_PARAM_TYPE_STRING_DATE,
	SHBATT_SYSFS_PARAM_TYPE_STRING_DEC,
	SHBATT_SYSFS_PARAM_TYPE_STRING_HEX,
	SHBATT_SYSFS_PARAM_TYPE_UINT8_DEC,
	SHBATT_SYSFS_PARAM_TYPE_UINT8_HEX,
	SHBATT_SYSFS_PARAM_TYPE_UINT16_DEC,
	SHBATT_SYSFS_PARAM_TYPE_UINT16_HEX,
	SHBATT_SYSFS_PARAM_TYPE_MAX
} shbatt_sysfs_parameter_type_t;
#endif /* CONFIG_PM_SUPPORT_BATT_TRACEABILITY */

/*+-----------------------------------------------------------------------------+*/
/*| @ STRUCT & UNION DECLARE :                                                  |*/
/*+-----------------------------------------------------------------------------+*/
#ifdef CONFIG_PM_SUPPORT_BATT_TRACEABILITY
typedef struct {
	const char*					name;
	char*						address;
	const unsigned int			size;
	char						str_buf[16];
	shbatt_sysfs_parameter_type_t	param_type;
} write_info_t;
#endif /* CONFIG_PM_SUPPORT_BATT_TRACEABILITY */

/*+-----------------------------------------------------------------------------+*/
/*| @ EXTERN VARIABLE :                                                         |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ PUBLIC VARIABLE :                                                         |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ STATIC VARIABLE :                                                         |*/
/*+-----------------------------------------------------------------------------+*/

static bool						shbatt_task_is_initialized = false;
static struct wakeup_source		shbatt_wakeup_source;
static atomic_t					shbatt_wakeup_source_num;

static dev_t					shbatt_dev;
static int						shbatt_major;
static int						shbatt_minor;
static struct cdev				shbatt_cdev;
static struct class*			shbatt_dev_class;

/* Timer */
static spinlock_t				shbatt_pkt_lock;
static struct mutex				shbatt_task_lock;
static struct workqueue_struct*	shbatt_task_workqueue_p;
#ifdef CONFIG_SHARP_SHTERM
static shbatt_packet_t			shbatt_pkt[16];
#endif /* CONFIG_SHARP_SHTERM */

/* wake_lock */
static struct timespec			shbatt_lock_time[SHBATT_MUTEX_TYPE_NUM];
static struct timespec			shbatt_unlock_time[SHBATT_MUTEX_TYPE_NUM];
static char						shbatt_lock_func[SHBATT_MUTEX_TYPE_NUM][SHBATT_LOCK_FUNC_LEN];

struct shbatt_chip {
	struct device                *dev;
	struct qpnp_vadc_chip        *pm_vadc_dev;
	struct qpnp_vadc_chip        *pmi_vadc_dev;
	const char                   *pa_temp_label;
	struct notifier_block        nb;
	struct work_struct           batt_psy_changed_work;
	struct work_struct           usb_psy_changed_work;
	struct work_struct           dc_psy_changed_work;
	int                          cl_high_thresh;
	int                          cl_low_thresh;
	int                          cl_start_capacity;
	int                          *thermal_mitigation;
	int                          thermal_levels;
};

struct shbatt_vadc_channel{
	const char *name;
	int channel;
};

static struct shbatt_chip *the_chip = NULL;
static int shbatt_cur_depleted_val = 100;
module_param_named(cur_depleted_val, shbatt_cur_depleted_val, int, 0644);

static int shbatt_depleted_capacity_pos = 0;
static int shbatt_depleted_capacity_array[5] = {0,};
static char shbatt_depleted_calc_ver = 2;
static int shbatt_avr_depleted_val = 0;
module_param_named(avr_depleted_val, shbatt_avr_depleted_val, int, 0644);

static struct shbatt_vadc_channel pm_vadc_channels[] =
{
	{
		.name = "ref_gnd",
		.channel = 0xFFFF,	//default
	},
	{
		.name = "ref_1250v",
		.channel = 0xFFFF,	//default
	},
	{
		.name = "die_temp",
		.channel = 0xFFFF,	//default
	},
	{
		.name = "msm_therm",
		.channel = 0xFFFF,	//default
	},
	{
		.name = "lcd_therm",
		.channel = 0xFFFF,	//default
	},
	{
		.name = "pa_therm0",
		.channel = 0xFFFF,	//default
	},
	{
		.name = "pa_therm1",
		.channel = 0xFFFF,	//default
	},
	{
		.name = "pa_therm2",
		.channel = 0xFFFF,	//default
	},
};

#ifdef CONFIG_PM_SUPPORT_BATT_TRACEABILITY
static char						pack_name[] = "0000";
module_param_string(pack_name, pack_name, sizeof(pack_name), 0600);

static char						cell_maker_name[] = "0000000000000000";
module_param_string(cell_maker_name, cell_maker_name, sizeof(cell_maker_name), 0600);

static char						maker_name[] = "0000000000000000";
module_param_string(maker_name, maker_name, sizeof(maker_name), 0600);

static char						cell_date[] = "0000000000";

module_param_string(cell_date, cell_date, sizeof(cell_date), 0644);

static char						cell_line[] = "00";
module_param_string(cell_line, cell_line, sizeof(cell_line), 0644);

static char						pack_date[] = "00000000";
module_param_string(pack_date, pack_date, sizeof(pack_date), 0644);

static char						pack_line[] = "00";
module_param_string(pack_line, pack_line, sizeof(pack_line), 0644);

static char						pack_manu_num[] = "0000";
module_param_string(pack_manu_num, pack_manu_num, sizeof(pack_manu_num), 0644);

#endif /* CONFIG_PM_SUPPORT_BATT_TRACEABILITY */

#ifdef CONFIG_PM_SUPPORT_BATT_AUTH
static int						batt_auth = -1;
module_param(batt_auth, int, 0644);
#else /* CONFIG_PM_SUPPORT_BATT_AUTH */
#ifdef CONFIG_PM_SUPPORT_BATT_TRACEABILITY
static int						batt_auth = 1;
module_param(batt_auth, int, 0644);
#endif /* CONFIG_PM_SUPPORT_BATT_TRACEABILITY */
#endif /*CONFIG_PM_SUPPORT_BATT_AUTH*/

#define NO_MOTION_RANGE 80
static int no_motion_range = NO_MOTION_RANGE;
module_param_named(
	no_motion_range, no_motion_range,
	int, S_IRUSR | S_IWUSR
);

/*+-----------------------------------------------------------------------------+*/
/*| @ EXTERN FUNCTION PROTO TYPE DECLARE :                                      |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ LOCAL FUNCTION PROTO TYPE DECLARE :                                       |*/
/*+-----------------------------------------------------------------------------+*/
static int shbatt_drv_create_device( void );

/* task */
#ifdef CONFIG_SHARP_SHTERM
static void shbatt_task(
	struct work_struct*			work_p );

static void shbatt_task_cmd_invalid(
	shbatt_packet_t*			pkt_p );

static void shbatt_task_cmd_battlog_event(
	shbatt_packet_t*			pkt_p );

static shbatt_result_t shbatt_seq_battlog_event(
	int							evt );

/* Timer */
static shbatt_packet_t* shbatt_task_get_packet( void );

static void shbatt_task_free_packet(
	shbatt_packet_t*			pkt );

/* wake_lock */
static void shbatt_seq_lock_task_mutex(
	shbatt_kernel_mutex_type	type,
	const char*					func );

static void shbatt_seq_unlock_task_mutex(
	shbatt_kernel_mutex_type	type,
	const char*					func );

static bool shbatt_check_no_motion(void);
#endif /* CONFIG_SHARP_SHTERM */

/*+-----------------------------------------------------------------------------+*/
/*| @ PLATFORM DRIVER MODULE PROTO TYPE DECLARE :                               |*/
/*+-----------------------------------------------------------------------------+*/
/* attribute store */

/* driver I/F */
static int shbatt_drv_open(
	struct inode*				in_p,
	struct file*				fi_p );

static int shbatt_drv_release(
	struct inode*				in_p,
	struct file*				fi_p );

static unsigned int shbatt_drv_poll(
	struct file*				fi_p,
	poll_table*					wait_p );

static long shbatt_drv_ioctl(
	struct file*				fi_p,
	unsigned int				cmd,
	unsigned long				arg );

static int shbatt_drv_probe(
	struct platform_device*		dev_p );

static int shbatt_drv_remove(
	struct platform_device*		dev_p );

static void shbatt_drv_shutdown(
	struct platform_device*		dev_p );

static int shbatt_drv_resume(
	struct platform_device*		dev_p);

static int shbatt_drv_suspend(
	struct platform_device*		dev_p,
	pm_message_t				state );
static int shbatt_depleted_backup_params(void);
static int shbatt_depleted_restore_params(void);

static int __init shbatt_drv_module_init( void );
static void __exit shbatt_drv_module_exit( void );

bool is_shbatt_task_initialized( void );

/*+-----------------------------------------------------------------------------+*/
/*| @ FUNCTION TABLE PROTO TYPE DECLARE :                                       |*/
/*+-----------------------------------------------------------------------------+*/
static struct file_operations shbatt_fops =
{
	.owner			= THIS_MODULE,
	.open			= shbatt_drv_open,
	.release		= shbatt_drv_release,
	.poll			= shbatt_drv_poll,
	.unlocked_ioctl	= shbatt_drv_ioctl,
	.compat_ioctl	= shbatt_drv_ioctl,
};

#ifdef CONFIG_OF
static struct of_device_id shbatt_match_table[] = {
	{ .compatible = SHBATT_OF_DEV_NAME },
	{}
};
#else  /* CONFIG_OF */
#define shbatt_match_table NULL;
#endif /* CONFIG_OF */

static struct platform_driver shbatt_platform_driver = {
	.probe		= shbatt_drv_probe,
	.remove		= shbatt_drv_remove,
	.shutdown	= shbatt_drv_shutdown,
	.driver		= {
		.name	= SHBATT_DEV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = shbatt_match_table,
	},
	.resume		= shbatt_drv_resume,
	.suspend	= shbatt_drv_suspend,
};

/* Timer */
#ifdef CONFIG_SHARP_SHTERM
static void (*const shbatt_task_cmd_func[])( shbatt_packet_t* pkt_p ) =
{
	shbatt_task_cmd_invalid,									/* SHBATT_TASK_CMD_INVALID */
	shbatt_task_cmd_battlog_event,								/* SHBATT_TASK_CMD_BATTLOG_EVENT */
};
#endif /* CONFIG_SHARP_SHTERM */

/*+-----------------------------------------------------------------------------+*/
/*| @ PUBLIC FUNCTION'S CODE AREA :                                             |*/
/*+-----------------------------------------------------------------------------+*/

/*+-----------------------------------------------------------------------------+*/
/*| @ LOCAL FUNCTION'S CODE AREA :                                              |*/
/*+-----------------------------------------------------------------------------+*/

shbatt_result_t shbatt_api_get_smem_info( shbatt_smem_info_t * p_smem_info )
{
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	sharp_smem_common_type * p_smem = 0;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	p_smem = sh_smem_get_common_address();

	if( p_smem_info && p_smem )
	{
#ifdef CONFIG_PM_SUPPORT_BATT_TRACEABILITY
		strncpy(p_smem_info->traceability_info, p_smem->shpwr_traceability_v2, sizeof(p_smem_info->traceability_info));
#endif /* CONFIG_PM_SUPPORT_BATT_TRACEABILITY */
	}
	else
	{
		SHBATT_ERROR("%s : invalid pointer, p_smem_info=0x%p, p_smem=0x%p\n",__FUNCTION__, p_smem_info, p_smem);
		result = SHBATT_RESULT_FAIL;
	}

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return result;
}


static int shbatt_drv_create_device( void )
{
	struct device*				dev_p;
	int							ret;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	ret = alloc_chrdev_region(&shbatt_dev,0,1,SHBATT_DEV_NAME);

	if(ret < 0)
	{
		SHBATT_ERROR("%s : alloc_chrdev_region failed. ret = %d\n",__FUNCTION__,ret);
		goto create_device_exit_0;
	}

	shbatt_major = MAJOR(shbatt_dev);
	shbatt_minor = MINOR(shbatt_dev);

	cdev_init(&shbatt_cdev,&shbatt_fops);

	shbatt_cdev.owner = THIS_MODULE;

	ret = cdev_add(&shbatt_cdev,shbatt_dev,1);

	if(ret < 0)
	{
		SHBATT_ERROR("%s : cdev_add failed. ret = %d\n",__FUNCTION__,ret);
		goto create_device_exit_1;
	}

	shbatt_dev_class = class_create(THIS_MODULE,SHBATT_DEV_NAME);

	if(IS_ERR(shbatt_dev_class))
	{
		ret = PTR_ERR(shbatt_dev_class);
		SHBATT_ERROR("%s : class_create failed. ret = %d\n",__FUNCTION__,ret);
		goto create_device_exit_2;
	}

	dev_p = device_create(shbatt_dev_class,NULL,shbatt_dev,&shbatt_cdev,SHBATT_DEV_NAME);

	if(IS_ERR(dev_p))
	{
		ret = PTR_ERR(dev_p);
		SHBATT_ERROR("%s : device_create failed. ret = %d\n",__FUNCTION__,ret);
		goto create_device_exit_3;
	}


	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return ret;

create_device_exit_3:
	class_destroy(shbatt_dev_class);

create_device_exit_2:
	cdev_del(&shbatt_cdev);

create_device_exit_1:
	unregister_chrdev_region(shbatt_dev,1);

create_device_exit_0:

	return ret;
}

#ifdef CONFIG_SHARP_SHTERM
static void shbatt_task(
	struct work_struct*			work_p
){
	shbatt_packet_t*			pkt_p;

	shbatt_seq_lock_task_mutex( SHBATT_TASK_COMMAND_LOCK,
								__FUNCTION__ );

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n", __FUNCTION__);

	pkt_p = (shbatt_packet_t*)work_p;

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"shbatt_task %d \n",pkt_p->hdr.cmd );

	if(pkt_p->hdr.cmd < NUM_SHBATT_TASK_CMD)
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
					"shbatt_task OK \n");
		shbatt_task_cmd_func[pkt_p->hdr.cmd](pkt_p);
	}
	else
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
					"shbatt_task NG \n");
		shbatt_task_cmd_invalid(pkt_p);
	}

	SHBATT_WAKE_CTL(0);

	shbatt_task_free_packet(pkt_p);

	shbatt_seq_unlock_task_mutex( SHBATT_TASK_COMMAND_LOCK,
								__FUNCTION__ );

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n", __FUNCTION__ );
}

static void shbatt_task_cmd_invalid(
	shbatt_packet_t*			pkt_p
){
	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n", __FUNCTION__);
	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n", __FUNCTION__ );
}

static void shbatt_task_cmd_battlog_event(
	shbatt_packet_t*			pkt_p
){
	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n", __FUNCTION__);

	shbatt_seq_battlog_event(pkt_p->prm.evt);

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n", __FUNCTION__ );
}

static shbatt_packet_t* shbatt_task_get_packet( void )
{
	int							idx;
	unsigned long				flags;
	shbatt_packet_t*			ret = NULL;

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n", __FUNCTION__);

	spin_lock_irqsave( &shbatt_pkt_lock, flags );

	for( idx = 0; idx < 16; idx++ )
	{
		if( shbatt_pkt[idx].is_used == false )
		{
			shbatt_pkt[idx].is_used = true;

			ret = &shbatt_pkt[idx];

			break;
		}
	}

	spin_unlock_irqrestore( &shbatt_pkt_lock, flags );

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n", __FUNCTION__ );

	return ret;
}

static void shbatt_task_free_packet(
	shbatt_packet_t*			pkt
){
	unsigned long				flags;

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n", __FUNCTION__);

	spin_lock_irqsave( &shbatt_pkt_lock, flags );

	pkt->is_used = false;

	spin_unlock_irqrestore( &shbatt_pkt_lock, flags );

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n", __FUNCTION__ );

	return;
}

/* wake_lock */
static void shbatt_seq_lock_task_mutex(
	shbatt_kernel_mutex_type	type,
	const char*					func
){
	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n",__FUNCTION__);

	strncpy( &shbatt_lock_func[type][0], func, SHBATT_LOCK_FUNC_LEN - 1 );
	get_monotonic_boottime( &shbatt_lock_time[type] );

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG, SHPWR_LOG_TYPE_BATT,
				"[P] %s() lock start\n", &shbatt_lock_func[type][0] );

	mutex_lock(&shbatt_task_lock);

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n",__FUNCTION__);
}

static void shbatt_seq_unlock_task_mutex(
	shbatt_kernel_mutex_type	type,
	const char*					func
){
	struct timespec						diff;

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n",__FUNCTION__);

	mutex_unlock(&shbatt_task_lock);
	get_monotonic_boottime( &shbatt_unlock_time[type] );

	memset(&diff, 0x00, sizeof( diff ) );
	diff = timespec_sub( shbatt_unlock_time[type], shbatt_lock_time[type] );

	SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE, SHPWR_LOG_TYPE_BATT,
				"[P] %s() locktime:%lu.%09lu\n", &shbatt_lock_func[type][0], diff.tv_sec, diff.tv_nsec );

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n",__FUNCTION__);
}

shbatt_result_t shbatt_api_battlog_event(
	shbattlog_event_num			evt
){
	shbatt_packet_t*			pkt_p;

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		return SHBATT_RESULT_REJECTED;
	}

	pkt_p = shbatt_task_get_packet();

	if(pkt_p == NULL)
	{
		return SHBATT_RESULT_REJECTED;
	}

	SHBATT_WAKE_CTL(1);

	pkt_p->hdr.cmd		= SHBATT_TASK_CMD_BATTLOG_EVENT;
	pkt_p->hdr.cmp_p	= NULL;
	pkt_p->hdr.ret_p	= NULL;
	pkt_p->prm.evt		= evt;

	INIT_WORK((struct work_struct*)pkt_p,shbatt_task);

	queue_work(shbatt_task_workqueue_p,(struct work_struct*)pkt_p);

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_battlog_charge_status(
	int			status
){
	int charge_status_event = SHBATTLOG_EVENT_NONE;
	static int pre_charge_status_event = SHBATTLOG_EVENT_NONE;

	switch (status) {
	case POWER_SUPPLY_STATUS_CHARGING:
		charge_status_event = SHBATTLOG_EVENT_CHG_START;
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
		charge_status_event = SHBATTLOG_EVENT_CHG_END;
		break;
	case POWER_SUPPLY_STATUS_FULL:
		charge_status_event = SHBATTLOG_EVENT_CHG_COMP;
		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		charge_status_event = SHBATTLOG_EVENT_CHG_ERROR;
		break;
	default:
		charge_status_event = SHBATTLOG_EVENT_NONE;
	}

	if(charge_status_event != SHBATTLOG_EVENT_NONE && charge_status_event != pre_charge_status_event) {
		SHPWR_LOG_INFO("pre_charge_status_event = %d, charge_status_event = %d\n", pre_charge_status_event, charge_status_event);

		shbatt_api_battlog_event(charge_status_event);
		pre_charge_status_event = charge_status_event;
	}

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_battlog_charge_error(
	int			charge_error_status
){

	static int pre_charge_error_event = SHBATTLOG_EVENT_NONE;
	int charge_error_event = SHBATTLOG_EVENT_NONE;

	if (charge_error_status == POWER_SUPPLY_HEALTH_OVERVOLTAGE)
		charge_error_event = SHBATTLOG_EVENT_CHG_ERR_BD_CHG_UNUSUAL_ST;
	else if (charge_error_status == POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE)
		charge_error_event = SHBATTLOG_EVENT_CHG_ERR_BD_BAT_UNUSUAL_ST;

	if(charge_error_event != SHBATTLOG_EVENT_NONE && charge_error_event != pre_charge_error_event) {
		SHPWR_LOG_INFO("pre_charge_error_event = %d, charge_error_event = %d\n", pre_charge_error_event, charge_error_event);

		shbatt_api_battlog_event(charge_error_event);
		pre_charge_error_event = charge_error_event;
	}

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_battlog_jeita_status(
	int			jeita_cur_status
){

	static int jeita_pre_status = POWER_SUPPLY_HEALTH_UNKNOWN;

	if (jeita_cur_status  != jeita_pre_status) {
		SHPWR_LOG_INFO("jeita_pre_status = %d,jeita_cur_status  = %d\n", jeita_pre_status , jeita_cur_status );

		switch (jeita_cur_status){
		case POWER_SUPPLY_HEALTH_OVERHEAT:
			shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_HOT_STOP_ST);
			break;
		case POWER_SUPPLY_HEALTH_COLD:
			shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_COLD_STOP_ST);
			break;
		case POWER_SUPPLY_HEALTH_WARM:
			shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_HOT_FAST_ST);
			break;
		case POWER_SUPPLY_HEALTH_COOL:
			shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_COLD_FAST_ST);
			break;
		case POWER_SUPPLY_HEALTH_GOOD:
			shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_FAST_ST);
			break;
		default:
			break;
		}
	}
	jeita_pre_status = jeita_cur_status;

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_battlog_capacity(
	int			cur_capacity
){
	static int pre_capacity = -1;
	static const shbattlog_event_num event_tbl[11] =
	{
		SHBATTLOG_EVENT_FGIC_EX0,
		SHBATTLOG_EVENT_FGIC_EX10,
		SHBATTLOG_EVENT_FGIC_EX20,
		SHBATTLOG_EVENT_FGIC_EX30,
		SHBATTLOG_EVENT_FGIC_EX40,
		SHBATTLOG_EVENT_FGIC_EX50,
		SHBATTLOG_EVENT_FGIC_EX60,
		SHBATTLOG_EVENT_FGIC_EX70,
		SHBATTLOG_EVENT_FGIC_EX80,
		SHBATTLOG_EVENT_FGIC_EX90,
		SHBATTLOG_EVENT_FGIC_EX100
	};

	if( cur_capacity < 0 || cur_capacity > 100 ){
		return SHBATT_RESULT_REJECTED;
	}

	if( pre_capacity != cur_capacity ){
		pr_debug("pre_capacity = %d cur_capacity  = %d\n", pre_capacity, cur_capacity );

		if(cur_capacity % 10 == 0)
		{
			pr_debug("event cur_cap  = %d tbl:%d\n", cur_capacity, event_tbl[cur_capacity/10] );
			shbatt_api_battlog_event(event_tbl[cur_capacity/10]);
		}
		pre_capacity = cur_capacity;
	}

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_battlog_usb_type(
	int			usb_type
){

	static int pre_usb_type_event = SHBATTLOG_EVENT_NONE;
	int usb_type_event = SHBATTLOG_EVENT_NONE;

	switch (usb_type) {
	case POWER_SUPPLY_TYPE_USB:
		usb_type_event = SHBATTLOG_EVENT_CHG_TYPE_SDP;
		break;
	case POWER_SUPPLY_TYPE_USB_DCP:
		usb_type_event = SHBATTLOG_EVENT_CHG_TYPE_DCP;
		break;
	case POWER_SUPPLY_TYPE_USB_CDP:
		usb_type_event = SHBATTLOG_EVENT_CHG_TYPE_CDP;
		break;
	case POWER_SUPPLY_TYPE_USB_HVDCP:
	case POWER_SUPPLY_TYPE_USB_HVDCP_3:
		usb_type_event = SHBATTLOG_EVENT_CHG_TYPE_HVDCP;
		break;
	case POWER_SUPPLY_TYPE_USB_PD:
		usb_type_event = SHBATTLOG_EVENT_CHG_TYPE_PD;
		break;
	default:
		usb_type_event = SHBATTLOG_EVENT_CHG_TYPE_OTHER;
	}

	// Notify if the event is different from the previous one.
	if ( usb_type_event != pre_usb_type_event ) {
		SHPWR_LOG_INFO("change usb_type = %d, usb_type_event = %d -> %d\n", usb_type, pre_usb_type_event, usb_type_event);
		shbatt_api_battlog_event(usb_type_event);

		pre_usb_type_event = usb_type_event;
	}

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_battlog_typec_mode(
	int			typec_mode
){

	static int pre_typec_mode_event = POWER_SUPPLY_TYPEC_NONE;
	int typec_mode_event = POWER_SUPPLY_TYPEC_NONE;

	switch (typec_mode) {
	case POWER_SUPPLY_TYPEC_NONE:
		typec_mode_event = SHBATTLOG_EVENT_TYPEC_MODE_NONE;
		break;
	case POWER_SUPPLY_TYPEC_SOURCE_DEFAULT:
		typec_mode_event = SHBATTLOG_EVENT_TYPEC_MODE_SOURCE_DEFAULT;
		break;
	case POWER_SUPPLY_TYPEC_SOURCE_MEDIUM:
		typec_mode_event = SHBATTLOG_EVENT_TYPEC_MODE_SOURCE_MEDIUM;
		break;
	case POWER_SUPPLY_TYPEC_SOURCE_HIGH:
		typec_mode_event = SHBATTLOG_EVENT_TYPEC_MODE_SOURCE_HIGH;
		break;
	case POWER_SUPPLY_TYPEC_NON_COMPLIANT:
		typec_mode_event = SHBATTLOG_EVENT_TYPEC_MODE_NON_COMPLIANT;
		break;
	case POWER_SUPPLY_TYPEC_SINK:
		typec_mode_event = SHBATTLOG_EVENT_TYPEC_MODE_SINK;
		break;
	case POWER_SUPPLY_TYPEC_SINK_POWERED_CABLE:
		typec_mode_event = SHBATTLOG_EVENT_TYPEC_MODE_SINK_POWERED_CABLE;
		break;
	case POWER_SUPPLY_TYPEC_SINK_DEBUG_ACCESSORY:
		typec_mode_event = SHBATTLOG_EVENT_TYPEC_MODE_SINK_DEBUG_ACCESSORY;
		break;
	case POWER_SUPPLY_TYPEC_SINK_AUDIO_ADAPTER:
		typec_mode_event = SHBATTLOG_EVENT_TYPEC_MODE_SINK_AUDIO_ADAPTER;
		break;
	case POWER_SUPPLY_TYPEC_POWERED_CABLE_ONLY:
		typec_mode_event = SHBATTLOG_EVENT_TYPEC_MODE_POWERED_CABLE_ONLY;
		break;
	default:
		typec_mode_event = POWER_SUPPLY_TYPEC_NONE;
	}

	// Notify if the event is different from the previous one.
	if ( typec_mode_event != pre_typec_mode_event ) {
		SHPWR_LOG_INFO("change typec_mode = %d, typec_mode_event = %d -> %d\n", typec_mode, pre_typec_mode_event, typec_mode_event);
		shbatt_api_battlog_event(typec_mode_event);

		pre_typec_mode_event = typec_mode_event;
	}

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_battlog_usb_present(
	int			usb_present
){
	static int pre_usb_present = 0;

	// Notify if the event is different from the previous one.
	if (pre_usb_present != usb_present) {
		SHPWR_LOG_INFO("change usb_present : %d -> %d\n",
						pre_usb_present, usb_present);

		if (usb_present)
			shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_INSERT_USB);
		else
			shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_REMOVE_USB);

		pre_usb_present = usb_present;
	}

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_battlog_dc_present(
	int			dc_present
){
	static int pre_dc_present = 0;

	// Notify if the event is different from the previous one.
	if (pre_dc_present != dc_present) {
		SHPWR_LOG_INFO("change dc_present : %d -> %d\n",
						pre_dc_present, dc_present);

		if (dc_present)
			shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_PUT_CRADLE);
		else
			shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_REMOVE_CRADLE);

		pre_dc_present = dc_present;
	}

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_battlog_input_suspend(
	int			input_suspend
){
	static int pre_input_suspend = 0;

	// Notify if the event is different from the previous one.
	if (pre_input_suspend != input_suspend) {
		SHPWR_LOG_INFO("change input_suspend : %d -> %d\n",
						pre_input_suspend, input_suspend);

		if (input_suspend)
			shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_INPUT_SUSPEND_ON);
		else
			shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_INPUT_SUSPEND_OFF);

		pre_input_suspend = input_suspend;
	}

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_battlog_system_temp_level(
	int			system_temp_level
){

	if (system_temp_level == the_chip->thermal_levels) {
		SHPWR_LOG_INFO("system_temp_level = thermal_levels = %d, system_temp_level_event = %d\n",
						the_chip->thermal_levels, SHBATTLOG_EVENT_CHG_COUNT_OVER_STOP_ST);
		shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_COUNT_OVER_STOP_ST);
	}

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_battlog_shortage_st(
	int			storming_status
){
	static int pre_storming_status = 0;

	// Notify if the event is different from the previous one.
	if (pre_storming_status != storming_status) {
		SHPWR_LOG_INFO("change storming status : %d -> %d\n",
						pre_storming_status, storming_status);

		if (storming_status)
			shbatt_api_battlog_event(SHBATTLOG_EVENT_CHG_ERR_CHG_POWER_SHORTAGE_ST);

		pre_storming_status = storming_status;
	}

	return SHBATT_RESULT_SUCCESS;
}

shbatt_result_t shbatt_api_battlog_typec_overheat_st(
	int			overheat_status
){

	static int pre_overheat_status = 0;

	// Notify if the event is different from the previous one.
	if (pre_overheat_status != overheat_status) {
		SHPWR_LOG_INFO("change typec overheat status : %d -> %d\n",
						pre_overheat_status, overheat_status);

		if (overheat_status)
			shbatt_api_battlog_event(SHBATTLOG_EVENT_DETECT_USB_HIGH_TEMP);
		else
			shbatt_api_battlog_event(SHBATTLOG_EVENT_RELEASE_USB_HIGH_TEMP);

		pre_overheat_status = overheat_status;
	}

	return SHBATT_RESULT_SUCCESS;
}

enum qpnp_vadc_channels shbatt_api_get_pm_vadc_chan_by_name(const char* name)
{
	enum qpnp_vadc_channels channel = ADC_MAX_NUM;

	int index;

	for (index = 0; index < sizeof(pm_vadc_channels)/sizeof(struct shbatt_vadc_channel); index++)
	{
		if (strcmp(pm_vadc_channels[index].name, name) == 0)
		{
			channel = pm_vadc_channels[index].channel;
			break;
		}
	}

	if (channel == ADC_MAX_NUM)
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
			"[E] %s() label:%s channel:0x%x is not found.\n",__FUNCTION__, name, channel);

	return channel;
}

shbatt_result_t shbatt_api_get_battery_log_info(
	shbattlog_info_t*		bli_p
){
	int		ret;
	union power_supply_propval	val = {0,};
	struct power_supply*		batt_psy = NULL;
	struct power_supply*		bms_psy = NULL;
	struct power_supply*		usb_psy = NULL;
	struct qpnp_vadc_result		vadc_val;
	int channel;

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n",__FUNCTION__);

	if(shbatt_task_is_initialized == false)
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
					"[E] %s() shbatt_task_is_initialized is false. \n", __FUNCTION__ );
		return SHBATT_RESULT_REJECTED;
	}
	batt_psy = power_supply_get_by_name("battery");
	if( batt_psy == NULL )
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
					"[E] %s() batt_psy is NULL.\n", __FUNCTION__ );
		return SHBATT_RESULT_REJECTED;
	}
	SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
				"[P] %s() power_supply_get_by_name()\n",__FUNCTION__);

	bms_psy = power_supply_get_by_name("bms");
	if( bms_psy == NULL )
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
					"[E] %s() bms_psy is NULL.\n", __FUNCTION__ );
		return SHBATT_RESULT_REJECTED;
	}
	SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
				"[P] %s() power_supply_get_by_name()\n",__FUNCTION__);

	usb_psy = power_supply_get_by_name("usb");
	if( usb_psy == NULL )
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
					"[E] %s() usb_psy is NULL.\n", __FUNCTION__ );
		return SHBATT_RESULT_REJECTED;
	}
	SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
				"[P] %s() power_supply_get_by_name()\n",__FUNCTION__);

	ret = batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val );
	if( ret < 0 )
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
					"[E] %s() ret:%d POWER_SUPPLY_PROP_VOLTAGE_NOW is failed.\n",__FUNCTION__, ret );
		return SHBATT_RESULT_FAIL;
	}
	bli_p->bat_vol = val.intval / 1000;
	SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
				"[P] %s()val.intval:%d. bat_vol:%d.\n",__FUNCTION__,
				val.intval, bli_p->bat_vol );
	val.intval = 0;

	ret = batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_TEMP, &val );
	if( ret < 0 )
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
					"[E] %s() ret:%d POWER_SUPPLY_PROP_TEMP is failed.\n",__FUNCTION__, ret );
		return SHBATT_RESULT_FAIL;
	}
	bli_p->bat_temp = val.intval / 10;
	SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
				"[P] %s()val.intval:%d. bat_temp:%d.\n",__FUNCTION__,
				val.intval, bli_p->bat_temp );
	val.intval = 0;

	ret = batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val );
	if( ret < 0 )
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
					"[E] %s() ret:%d POWER_SUPPLY_PROP_CAPACITY is failed.\n",__FUNCTION__, ret );
		return SHBATT_RESULT_FAIL;
	}
	bli_p->vol_per = val.intval;
	SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
				"[P] %s()val.intval:%d. vol_per:%d.\n",__FUNCTION__,
				val.intval, bli_p->vol_per );
	val.intval = 0;

	ret = usb_psy->desc->get_property(usb_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val );
	if( ret < 0 )
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
					"[E] %s() ret:%d POWER_SUPPLY_PROP_VOLTAGE_NOW is failed.\n",__FUNCTION__, ret );
		return SHBATT_RESULT_FAIL;
	}
	bli_p->chg_vol = val.intval / 1000;
	SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
				"[P] %s()val.intval:%d. chg_vol:%d.\n",__FUNCTION__,
				val.intval, bli_p->chg_vol );

	if (the_chip->pmi_vadc_dev) {
		/* pmi vadc dev has no item */
	}

	if (the_chip->pm_vadc_dev) {
		channel = shbatt_api_get_pm_vadc_chan_by_name("msm_therm");
		if (channel != ADC_MAX_NUM) {
			ret = qpnp_vadc_read(the_chip->pm_vadc_dev, channel, &vadc_val);
			if ( ret < 0 ) {
				SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
							"[E] %s() ret:%d qpnp_vadc_read() channel:0x%x is failed.\n",__FUNCTION__, ret, channel );
				return SHBATT_RESULT_FAIL;
			}
			bli_p->cpu_temp = (int)vadc_val.physical;
			bli_p->chg_temp = (int)vadc_val.physical;
			SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
							"[P] %s()vadc_val.physical:%ld. cpu_temp:%d. chg_temp:%d.\n",__FUNCTION__,
							(long)vadc_val.physical, bli_p->cpu_temp, bli_p->chg_temp );
		}

		channel = shbatt_api_get_pm_vadc_chan_by_name(the_chip->pa_temp_label);
		if (channel != ADC_MAX_NUM) {
			ret = qpnp_vadc_read(the_chip->pm_vadc_dev, channel, &vadc_val);
			if ( ret < 0 ) {
				SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
							"[E] %s() ret:%d qpnp_vadc_read() channel:0x%x is failed.\n",__FUNCTION__, ret, channel );
				return SHBATT_RESULT_FAIL;
			}
			bli_p->pa_temp = (int)vadc_val.physical;
			SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
							"[P] %s()vadc_val.physical:%ld. pa_temp:%d.\n",__FUNCTION__,
							(long)vadc_val.physical, bli_p->pa_temp );
		}

		channel = shbatt_api_get_pm_vadc_chan_by_name("lcd_therm");
		if (channel != ADC_MAX_NUM) {
			ret = qpnp_vadc_read(the_chip->pm_vadc_dev, channel, &vadc_val);
			if ( ret < 0 ) {
				SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
							"[E] %s() ret:%d qpnp_vadc_read() channel:0x%x is failed.\n",__FUNCTION__, ret, channel );
				return SHBATT_RESULT_FAIL;
			}
			bli_p->lcd_temp = (int)vadc_val.physical;
			SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
							"[P] %s()vadc_val.physical:%ld. lcd_temp:%d.\n",__FUNCTION__,
							(long)vadc_val.physical, bli_p->lcd_temp );
		}
	} else {
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR,SHPWR_LOG_TYPE_BATT,
			"[E] %s() PM dev is not exist \n", __FUNCTION__ );

//		return SHBATT_RESULT_REJECTED;
	}

	val.intval = 0;

	ret = batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val );
	if( ret < 0 )
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
					"[E] %s() ret:%d POWER_SUPPLY_PROP_CURRENT_NOW is failed.\n",__FUNCTION__, ret );
		return SHBATT_RESULT_FAIL;
	}
	bli_p->latest_cur = -val.intval / 1000;
	SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
				"[P] %s()val.intval:%d. latest_cur:%d.\n",__FUNCTION__,
				-val.intval, bli_p->latest_cur );

	val.intval = 0;

	ret = bms_psy->desc->get_property(bms_psy, POWER_SUPPLY_PROP_CURRENT_AVG, &val );
	if( ret < 0 )
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
					"[E] %s() ret:%d POWER_SUPPLY_PROP_CURRENT_AVG is failed.\n",__FUNCTION__, ret );
		return SHBATT_RESULT_FAIL;
	}
	bli_p->avg_cur = -val.intval / 1000;
	SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
				"[P] %s()val.intval:%d. avg_cur:%d.\n",__FUNCTION__,
				-val.intval, bli_p->avg_cur );

	val.intval = 0;

	ret = usb_psy->desc->get_property(usb_psy, POWER_SUPPLY_PROP_INPUT_CURRENT_NOW, &val );
	if( ret < 0 )
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
					"[E] %s() ret:%d POWER_SUPPLY_PROP_INPUT_CURRENT_NOW is failed.\n",__FUNCTION__, ret );
		return SHBATT_RESULT_FAIL;
	}
	bli_p->chg_cur = val.intval / 1000;
	SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
				"[P] %s()val.intval:%d. chg_cur:%d.\n",__FUNCTION__,
				val.intval, bli_p->chg_cur );
	val.intval = 0;

	ret = bms_psy->desc->get_property(bms_psy, POWER_SUPPLY_PROP_SKIN_TEMP, &val );
	if( ret < 0 )
	{
		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR, SHPWR_LOG_TYPE_BATT,
					"[E] %s() ret:%d POWER_SUPPLY_PROP_SKIN_TEMP is failed.\n",__FUNCTION__, ret );
		return SHBATT_RESULT_FAIL;
	}
	bli_p->cam_temp = val.intval / 1000;
	SHPWR_LOG( SHPWR_LOG_LEVEL_NOTICE,SHPWR_LOG_TYPE_BATT,
				"[P] %s()val.intval:%d. cam_temp(aux_therm):%d.\n",__FUNCTION__,
				val.intval, bli_p->cam_temp );
	val.intval = 0;

	bli_p->cur_dep_per = shbatt_cur_depleted_val;
	bli_p->avg_dep_per = shbatt_avr_depleted_val;

//	if (bli_p->event_num == SHBATTLOG_EVENT_NONE)	// SHBATTLOG_EVENT_BATT_REPORT_NORM or SHBATTLOG_EVENT_BATT_REPORT_CHG
//		sh_dump_regs();

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}
EXPORT_SYMBOL(shbatt_api_get_battery_log_info);
#endif /* CONFIG_SHARP_SHTERM */

#ifdef CONFIG_PM_SUPPORT_BATT_TRACEABILITY
static int shbatt_api_chk_smem_str( write_info_t *file )
{
	unsigned int	chk_cntr;
	char*			tmp_buf = NULL;

	SHBATT_TRACE( "[S] %s str:%s\n", __FUNCTION__, file->str_buf );

	tmp_buf = file->str_buf;
	for( chk_cntr = 0; chk_cntr < file->size; chk_cntr++ ) {
		if( ( tmp_buf[chk_cntr] >= '0' ) &&
			( tmp_buf[chk_cntr] <= '9' ) ) {
			;
		}
		else if( ( ( tmp_buf[chk_cntr] >= 'A' ) &&
				( tmp_buf[chk_cntr] <= 'F' ) ) ||
				( ( tmp_buf[chk_cntr] >= 'a' ) &&
				( tmp_buf[chk_cntr] <= 'f' ) ) ) {
			switch( file->param_type ) {
			case SHBATT_SYSFS_PARAM_TYPE_STRING_DATE:
			case SHBATT_SYSFS_PARAM_TYPE_STRING_DEC:
			case SHBATT_SYSFS_PARAM_TYPE_UINT8_DEC:
			case SHBATT_SYSFS_PARAM_TYPE_UINT16_DEC:
				SHBATT_ERROR( "[E]A-F a-f found in str_buff[%d]:%c. type:%d\n", chk_cntr, tmp_buf[chk_cntr], file->param_type );
				snprintf( file->str_buf, file->size+1, "000000000000000" );
				return 1;
			default:
				break;
			}
		}
		else if( ( ( tmp_buf[chk_cntr] >= 'G' ) &&
				( tmp_buf[chk_cntr] <= 'Z' ) ) ||
				( ( tmp_buf[chk_cntr] >= 'g' ) &&
				( tmp_buf[chk_cntr] <= 'z' ) ) ) {
			switch( file->param_type ) {
			case SHBATT_SYSFS_PARAM_TYPE_STRING:
				break;
			default:
				SHBATT_ERROR( "[E]G-Z g-z found in str_buff[%d]:%c. type:%d\n", chk_cntr, tmp_buf[chk_cntr], file->param_type );
				snprintf( file->str_buf, file->size+1, "000000000000000" );
				return 1;
			}
		}
		else if( tmp_buf[chk_cntr] == '\0' ) {
			SHBATT_ERROR( "[E]str_buf[%d] is NULL. type:%d\n",chk_cntr, file->param_type );
			snprintf( file->str_buf, file->size+1, "000000000000000" );
			return 1;
		}
		else {
			SHBATT_ERROR( "[E]str_buf[%d]:%c. type:%d\n",chk_cntr, tmp_buf[chk_cntr], file->param_type );
			snprintf( file->str_buf, file->size+1, "000000000000000" );
			return 1;
		}
	}

	SHBATT_TRACE( "[E] %s\n", __FUNCTION__ );
	return 0;
}

static shbatt_result_t shbatt_api_write_traceability( void )
{
	static write_info_t	write_attr[] =
	{
		{
			.name = "cell_date",
			.address = &cell_date[0],
			.size = sizeof("AAAAYYMMDD")-1,
			.str_buf = "",
			.param_type = SHBATT_SYSFS_PARAM_TYPE_STRING
		},
		{
			.name = "cell_line",
			.address = &cell_line[0],
			.size = sizeof("CC")-1,
			.str_buf = "",
			.param_type = SHBATT_SYSFS_PARAM_TYPE_STRING_HEX
		},
		{
			.name = "pack_date",
			.address = &pack_date[0],
			.size = sizeof("yymmddpp")-1,
			.str_buf = "",
			.param_type = SHBATT_SYSFS_PARAM_TYPE_STRING_HEX
		},
		{
			.name = "pack_manu_num",
			.address = &pack_manu_num[0],
			.size = sizeof("XXXX")-1,
			.str_buf = "",
			.param_type = SHBATT_SYSFS_PARAM_TYPE_UINT16_DEC
		},
	};
	shbatt_smem_info_t			smem_info;
	char*						write_p;
	shbatt_result_t				shbatt_result;
	unsigned int				write_cntr;

	SHBATT_TRACE("[S] %s\n", __FUNCTION__);

	memset(&smem_info, 0x00, sizeof(shbatt_smem_info_t));
	shbatt_result = shbatt_api_get_smem_info(&smem_info);
	if (shbatt_result != SHBATT_RESULT_SUCCESS) {
		SHBATT_ERROR("[E] shbatt_api_get_smem_info failed:%d.\n", shbatt_result );
		return shbatt_result;
	}

	write_p = smem_info.traceability_info;

	for( write_cntr = 0; write_cntr < sizeof(write_attr)/sizeof(write_attr[0]); write_cntr++ ) {

		write_info_t *w = &write_attr[write_cntr];
		snprintf( w->str_buf, w->size+1, "%s", write_p );

		if( shbatt_api_chk_smem_str( w ) == 1 ) {
			SHBATT_ERROR("smem check %s. end fail-safe.\n", w->name);
		}

		SHBATT_TRACE("[P] %s. size:%d str:%s\n", w->name, w->size, w->str_buf );

		strncpy(w->address, w->str_buf, w->size);

		write_p += w->size;
	}

	SHBATT_TRACE("[E] %s\n", __FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}
#endif /* CONFIG_PM_SUPPORT_BATT_TRACEABILITY */

#define SHBATT_DEPLETED_CACPCITY		("/durable/shpwr/shbatt_depleted_calc_ver")
static int shbatt_depleted_backup_params(void)
{
	struct file *fp = NULL;
	int count = 0;
	int average_depleted_val = 0;
	char write_data[50];
	mm_segment_t old_fs;
	int res = -EINVAL;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	SHBATT_TRACE("[S] %s\n", __FUNCTION__);
	for(count = 0; count < shbatt_depleted_capacity_pos; count++)
	{
		average_depleted_val += shbatt_depleted_capacity_array[count];
	}
	average_depleted_val = average_depleted_val / shbatt_depleted_capacity_pos;
	shbatt_avr_depleted_val = average_depleted_val;

	fp = filp_open(SHBATT_DEPLETED_CACPCITY, O_RDWR | O_CREAT, 0644);
	if (IS_ERR_OR_NULL(fp)) {
		SHBATT_ERROR("%s : Cannot open file: %s err=%p\n", __FUNCTION__, SHBATT_DEPLETED_CACPCITY, fp);
		result = SHBATT_RESULT_FAIL;
	} else {
		old_fs = get_fs();
		set_fs(get_ds());
		count = snprintf(write_data, sizeof(write_data), "%d\n%d\n%d\n",
				shbatt_depleted_calc_ver, average_depleted_val, shbatt_depleted_capacity_pos);
		if((res = (int)__vfs_write(fp, write_data, count, &fp->f_pos)) == 0) {
			SHBATT_ERROR("%s : fwrite result: %d\n", __FUNCTION__, res);
			result = SHBATT_RESULT_FAIL;
		}

		if((res = fp->f_op->fsync(fp, 0, LLONG_MAX, 0)) != 0) {
			SHBATT_ERROR("%s : fsync result: %d\n", __FUNCTION__, res);
			result = SHBATT_RESULT_FAIL;
		}
		set_fs(old_fs);
		filp_close(fp, NULL);
	}
	SHBATT_TRACE("[E] %s\n", __FUNCTION__);
	return result;
}

#define SHBATT_READ_DATA_BUF_LEN 50
static int shbatt_depleted_restore_params(void)
{
	struct file *fp = NULL;
	int res = -EINVAL;
	int ver;
	int average_depleted_val = 0;
	int depleted_capacity_pos = 0;
	int count = 0;
	mm_segment_t old_fs;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	struct path  path;
	char read_data[50];
	loff_t fpos = 0;
	int lvl_index = 0;
	int i =0;
	int buff[3];
	char* val_start;
	int ret = 0;

	SHBATT_TRACE("[S] %s\n", __FUNCTION__);

	res = kern_path(SHBATT_DEPLETED_CACPCITY, LOOKUP_OPEN, &path);
	if (res == 0) {
		path_put(&path);
		fp = filp_open(SHBATT_DEPLETED_CACPCITY, O_RDONLY, 0644);
		if (!IS_ERR_OR_NULL(fp)) {
			old_fs = get_fs();
			set_fs(get_ds());

			if ((res = (int)__vfs_read(fp, read_data, SHBATT_READ_DATA_BUF_LEN, &fpos)) != 0) {
				read_data[res] = '\0';

				for (lvl_index = 0; lvl_index < res && i < res; lvl_index++) {
					while(read_data[i] == ' ' && i < res) i++; /* skip the blank space of head */
					if (i == res) {
						break;
					}
					val_start = &read_data[i];
					while(read_data[i] != ' ' && read_data[i] != '\0' && read_data[i] != '\n'&& i < res) i++; /* to the next blank space */
					if (read_data[i] == '\n') {
						read_data[i] = '\0';
						i++;
					}
					if(lvl_index > 2){
						break;
					}
					ret = kstrtoint(val_start, 0, &buff[lvl_index]);
					if(ret) {
						break;
					}
					if (read_data[i] == '\0') {
						break;
					}
				}
				ver = buff[0];
				average_depleted_val = buff[1];
				depleted_capacity_pos = buff[2];
				for(count = 0; count < depleted_capacity_pos; count++)
				{
					shbatt_depleted_capacity_array[count] = average_depleted_val;
				}
				shbatt_depleted_capacity_pos = depleted_capacity_pos;
			}

			set_fs(old_fs);
			filp_close(fp, NULL);
		} else {
			SHBATT_ERROR("%s : Cannot open file: %s err=%p\n", __FUNCTION__, SHBATT_DEPLETED_CACPCITY, fp);
			if (!shpwr_is_initialized()) SHBATT_ERROR("%s : --> file system not ready\n", __FUNCTION__);
			result = SHBATT_RESULT_FAIL;
		}
	} else {
		SHBATT_TRACE("[P] %s : Not Exist file: %s err=%p\n", __FUNCTION__, SHBATT_DEPLETED_CACPCITY, fp);
		if (!shpwr_is_initialized()) SHBATT_ERROR("%s : --> file system not ready\n", __FUNCTION__);
		result = SHBATT_RESULT_FAIL;
	}

	SHBATT_TRACE("[E] %s\n", __FUNCTION__);
	return result;
}
#define SHBATT_DEP_RESULT_FILE		("/durable/shpwr/shbatt")
#define SHBATT_DEP_RESULT_BUF_LEN	1
shbatt_result_t shbatt_api_store_fg_cap_learning_result(
	int64_t learned_cc_uah,
	int nom_mah,
	int high_thresh,
	int low_thresh
) {
	int dep_per;
	int dep_result;
	static int prev_dep_result = -1; /* Uninitialized : -1 */
	struct file *fp = NULL;
	char *bufp;
	mm_segment_t old_fs;
	int res = -EINVAL;
	shbatt_result_t result = SHBATT_RESULT_SUCCESS;
	struct path  path;
	loff_t fpos = 0;
	int count;
	static bool call_once = false;

	if(!call_once)
	{
		result = shbatt_depleted_restore_params();
		if(result == SHBATT_RESULT_SUCCESS)
			call_once = true;
	}
	dep_per = div64_s64(learned_cc_uah * 100, nom_mah * 1000);
	if(shbatt_depleted_capacity_pos < SHBATT_DEPLETED_JUDGE_AVERAGE_COUNT)
	{
		shbatt_depleted_capacity_array[shbatt_depleted_capacity_pos] = dep_per;
		shbatt_depleted_capacity_pos++;
	} else {
		for(count = 1; count < shbatt_depleted_capacity_pos; count++)
		{
			shbatt_depleted_capacity_array[count-1] = shbatt_depleted_capacity_array[count];
		}
		shbatt_depleted_capacity_array[count-1] = dep_per;
	}
	shbatt_depleted_backup_params();

	shbatt_cur_depleted_val = dep_per;

	SHBATT_TRACE("[S] %s\n", __FUNCTION__);

	/* Good: 0, Depleted: 1, Acceptable: 2 */
	if(shbatt_avr_depleted_val >= high_thresh)
		dep_result = 0;
	else if(shbatt_avr_depleted_val <= low_thresh)
		dep_result = 1;
	else /* low_thresh < dep_per < high_thresh*/
		dep_result = 2;

	SHBATT_TRACE("[P] %s : prev_dep_result=%d dep_result=%d\n", __FUNCTION__, prev_dep_result, dep_result);

	bufp = kmalloc(SHBATT_DEP_RESULT_BUF_LEN + 1, GFP_KERNEL);
	if (!bufp) return SHBATT_RESULT_FAIL;

	memset(bufp, 0, SHBATT_DEP_RESULT_BUF_LEN + 1);
	if (prev_dep_result == -1) {
		res = kern_path(SHBATT_DEP_RESULT_FILE, LOOKUP_OPEN, &path);
		if (res == 0) {
			path_put(&path);
			fp = filp_open(SHBATT_DEP_RESULT_FILE, O_RDONLY, 0644);
			if (!IS_ERR_OR_NULL(fp)) {
				old_fs = get_fs();
				set_fs(get_ds());

				if ((res = (int)__vfs_read(fp, bufp, SHBATT_DEP_RESULT_BUF_LEN, &fpos)) != 0) {
					bufp[res] = '\0';
					if (kstrtoint(bufp, 10, &prev_dep_result) != 0)
						prev_dep_result = -1;
					SHBATT_TRACE("[P] %s : read file: %s bufp:%s --> prev_dep_result=%d \n", __FUNCTION__, SHBATT_DEP_RESULT_FILE, bufp, prev_dep_result);
				}
				set_fs(old_fs);

				filp_close(fp, NULL);
			} else {
				SHBATT_ERROR("%s : Cannot open file: %s err=%p\n", __FUNCTION__, SHBATT_DEP_RESULT_FILE, fp);
				if (!shpwr_is_initialized()) SHBATT_ERROR("%s : --> file system not ready\n", __FUNCTION__);
			}
		}
		 else {
			SHBATT_TRACE("[P] %s : Not Exist file: %s err=%p\n", __FUNCTION__, SHBATT_DEP_RESULT_FILE, fp);
			if (!shpwr_is_initialized()) SHBATT_ERROR("%s : --> file system not ready\n", __FUNCTION__);
		}
	}
	if(dep_result != prev_dep_result)
	{
		fp = filp_open(SHBATT_DEP_RESULT_FILE, O_RDWR | O_CREAT, 0644);
		if (IS_ERR_OR_NULL(fp)) {
			SHBATT_ERROR("%s : Cannot open file: %s err=%p\n", __FUNCTION__, SHBATT_DEP_RESULT_FILE, fp);
			if (!shpwr_is_initialized()) SHBATT_ERROR("%s : --> file system not ready\n", __FUNCTION__);
			result = SHBATT_RESULT_FAIL;
			if (prev_dep_result != -1) prev_dep_result = dep_result;
		} else {
			sprintf(bufp, "%d", dep_result);
			old_fs = get_fs();
			set_fs(get_ds());

			if((res = (int)__vfs_write(fp, bufp, SHBATT_DEP_RESULT_BUF_LEN, &fp->f_pos)) == 0) {
				SHBATT_ERROR("%s : fwrite result: %d\n", __FUNCTION__, res);
				result = SHBATT_RESULT_FAIL;
			}

			if((res = fp->f_op->fsync(fp, 0, LLONG_MAX, 0)) != 0) {
				SHBATT_ERROR("%s : fsync result: %d\n", __FUNCTION__, res);
				result = SHBATT_RESULT_FAIL;
			}
			set_fs(old_fs);

			filp_close(fp, NULL);
			prev_dep_result = dep_result;
		}
	}
	kfree(bufp);
	SHBATT_TRACE("[E] %s\n", __FUNCTION__);
	return result;
}

#ifdef CONFIG_SHARP_SHTERM
static shbatt_result_t shbatt_seq_battlog_event(
	int		evt
) {
	shbattlog_info_t	shterm_bli;

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n",__FUNCTION__);

	memset(&shterm_bli, 0, sizeof(shterm_bli));

    shterm_bli.event_num = evt;

    switch( evt )
    {
    case SHBATTLOG_EVENT_CHG_INSERT_USB:
        if(shbatt_check_no_motion()) {
            shterm_bli.event_num = SHBATTLOG_EVENT_CHG_INSERT_USB_NOMOTION;
        }
        break;
    case SHBATTLOG_EVENT_CHG_REMOVE_USB:
        if(shbatt_check_no_motion()) {
            shterm_bli.event_num = SHBATTLOG_EVENT_CHG_REMOVE_USB_NOMOTION;
        }
        break;
    case SHBATTLOG_EVENT_CHG_PUT_CRADLE:
        if(shbatt_check_no_motion()) {
            shterm_bli.event_num = SHBATTLOG_EVENT_CHG_PUT_CRADLE_NOMOTION;
        }
        break;
    case SHBATTLOG_EVENT_CHG_REMOVE_CRADLE:
        if(shbatt_check_no_motion()) {
            shterm_bli.event_num = SHBATTLOG_EVENT_CHG_REMOVE_CRADLE_NOMOTION;
        }
        break;
    default:
        break;
    }

//	if( shbatt_api_get_battery_log_info(&shterm_bli) != SHBATT_RESULT_SUCCESS )
//	{
//		SHPWR_LOG( SHPWR_LOG_LEVEL_ERR,SHPWR_LOG_TYPE_BATT,
//				"[P] %s() event:%d. shbatt_api_get_battery_log_info() failed.\n", __FUNCTION__,
//				shterm_bli.event_num );
//	}

	shterm_k_set_event(&shterm_bli);

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n",__FUNCTION__);

	return SHBATT_RESULT_SUCCESS;
}

static bool shbatt_check_no_motion(void)
{
    bool ret = false;
#if 0
    struct shub_input_acc_info info;
    static int pre_nX = 0;
    static int pre_nY = 0;
    static int pre_nZ = 0;
    unsigned int sub_nX, sub_nY, sub_nZ;

    SHBATT_TRACE("[S] %s \n",__FUNCTION__);

    memset(&info, 0, sizeof(struct shub_input_acc_info));

    if(shub_api_get_acc_info(&info) != 0) {
        SHPWR_LOG( SHPWR_LOG_LEVEL_ERR,SHPWR_LOG_TYPE_BATT,
            "[E] %s(): shub_api_get_face_check_info failed.\n",__FUNCTION__);
        return false;
    }

    if(info.nStat != 0) {
        sub_nX = abs(pre_nX - info.nX);
        sub_nY = abs(pre_nY - info.nY);
        sub_nZ = abs(pre_nZ - info.nZ);

        SHPWR_LOG_INFO("pre_nX=%d, pre_nY=%d, pre_nZ=%d\n",
            pre_nX, pre_nY, pre_nZ);
        SHPWR_LOG_INFO("last_nX=%d, last_nX=%d, last_nX=%d\n",
            info.nX, info.nY, info.nZ);
        SHPWR_LOG_INFO("sub_nX=%d, sub_nY=%d, sub_nZ=%d\n",
        sub_nX, sub_nY, sub_nZ);

        if(sub_nX < no_motion_range &&
           sub_nY < no_motion_range &&
           sub_nZ < no_motion_range) {
            SHPWR_LOG_INFO("no motion\n");
            ret = true;
        } else {
            SHPWR_LOG_INFO("motion\n");
            ret = false;
        }
    } else {
        SHPWR_LOG_INFO("skip\n");
        ret = false;
    }

    pre_nX = info.nX;
    pre_nY = info.nY;
    pre_nZ = info.nZ;
#endif
    SHBATT_TRACE("[E] %s \n",__FUNCTION__);
    return ret;
}
#endif /* CONFIG_SHARP_SHTERM */

/*+-----------------------------------------------------------------------------+*/
/*| @ PLATFORM DRIVER MODULE CODE AREA :                                        |*/
/*+-----------------------------------------------------------------------------+*/
static int shbatt_drv_open(
	struct inode*				in_p,
	struct file*				fi_p
){
	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n", __FUNCTION__);
	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n", __FUNCTION__ );
	return 0;
}

static int shbatt_drv_release(
	struct inode*				in_p,
	struct file*				fi_p
){
	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n", __FUNCTION__);
	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n", __FUNCTION__ );
	return 0;
}

static unsigned int shbatt_drv_poll(
	struct file*				fi_p,
	poll_table*					wait_p
){
	unsigned					int mask = 0;

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n", __FUNCTION__);

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n", __FUNCTION__ );

	return mask;
}

static long shbatt_drv_ioctl(
	struct file*				fi_p,
	unsigned int				cmd,
	unsigned long				arg
){
	int							ret = -EPERM;

	SHBATT_TRACE("[S] %s() cmd=0x%x\n",__FUNCTION__,cmd);

	SHBATT_TRACE("[E] %s() ret=%d \n",__FUNCTION__,ret);

	return ret;
}

static void batt_psy_changed_work(struct work_struct *work)
{
	int ret;
	struct shbatt_chip *chip = container_of(work,
			struct shbatt_chip, batt_psy_changed_work);
	union power_supply_propval	val = {0,};
	struct power_supply*		batt_psy = NULL;
	struct power_supply*		usb_psy = NULL;
	struct power_supply*		dc_psy = NULL;
	struct power_supply*		bms_psy = NULL;
	bool usb_online = false, dc_online = false;
	static bool cap_learning_active = false;
	static int cl_init_capacity = 0;
	int charge_status = -1;
	int capacity;
	int learned_cc_uah = 0;
	int nom_batt_cap_mah = 0;

	dev_dbg(chip->dev, "batt_psy_changed_work start\n");

	if (!is_shbatt_task_initialized()) {
		dev_err(chip->dev, "shbatt_task is not initialize\n");
		goto out;
	}

	batt_psy = power_supply_get_by_name("battery");
	if (!batt_psy) {
		dev_err(chip->dev, "batt_psy is null\n");
		goto out;
	}

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_err(chip->dev, "usb_psy is null\n");
		goto out;
	}

	dc_psy = power_supply_get_by_name("dc");
	if (!dc_psy) {
		dev_err(chip->dev, "dc_psy is null\n");
		goto out;
	}

	bms_psy = power_supply_get_by_name("bms");
	if (!bms_psy) {
		dev_err(chip->dev, "bms_psy is null\n");
		goto out;
	}

	ret = usb_psy->desc->get_property(usb_psy, POWER_SUPPLY_PROP_ONLINE, &val);
	if (!ret)
		usb_online = (bool)val.intval;

	ret = dc_psy->desc->get_property(dc_psy, POWER_SUPPLY_PROP_ONLINE, &val);
	if (!ret)
		dc_online = (bool)val.intval;

	ret = batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_INPUT_SUSPEND, &val);
#ifdef CONFIG_SHARP_SHTERM
	if (!ret)
		shbatt_api_battlog_input_suspend(val.intval);
#endif /* CONFIG_SHARP_SHTERM */

	ret = batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &val);
#ifdef CONFIG_SHARP_SHTERM
	if (!ret)
		shbatt_api_battlog_system_temp_level(val.intval);
#endif /* CONFIG_SHARP_SHTERM */

	ret = batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_STORMING_STATUS, &val);
#ifdef CONFIG_SHARP_SHTERM
	if (!ret)
		shbatt_api_battlog_shortage_st(val.intval);
#endif /* CONFIG_SHARP_SHTERM */

	ret = batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_TYPEC_OVERHEAT_STATUS, &val);
#ifdef CONFIG_SHARP_SHTERM
	if (!ret)
		shbatt_api_battlog_typec_overheat_st(val.intval);
#endif /* CONFIG_SHARP_SHTERM */

	charge_status = -1;
	ret = batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (!ret) {
		charge_status = val.intval;
#ifdef CONFIG_SHARP_SHTERM
		shbatt_api_battlog_charge_status(charge_status);
#endif /* CONFIG_SHARP_SHTERM */
		if (charge_status == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			cap_learning_active = false;
			//OVP and Safty Timer Expired check
			ret = batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_CHARGER_ERROR_STATUS, &val);
#ifdef CONFIG_SHARP_SHTERM
			if (!ret)
				shbatt_api_battlog_charge_error(val.intval);
#endif /* CONFIG_SHARP_SHTERM */
		}
	}

	ret = batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_HEALTH, &val);
#ifdef CONFIG_SHARP_SHTERM
	if ((!ret) && (usb_online || dc_online))
			shbatt_api_battlog_jeita_status(val.intval);
#endif /* CONFIG_SHARP_SHTERM */

	ret = batt_psy->desc->get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (!ret) {
		capacity = val.intval;
#ifdef CONFIG_SHARP_SHTERM
		shbatt_api_battlog_capacity(capacity);
#endif /* CONFIG_SHARP_SHTERM */

		if ((charge_status == POWER_SUPPLY_STATUS_CHARGING)
			&&(val.intval <= chip->cl_start_capacity)) {
			cap_learning_active = true;
			cl_init_capacity = capacity;
		}
		else if (cap_learning_active && (charge_status == POWER_SUPPLY_STATUS_FULL)) {
			cap_learning_active = false;

			/* If the delta is < 30%, then skip processing full data */
			if ((capacity - cl_init_capacity) >= 30) {
				ret = bms_psy->desc->get_property(bms_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &val);
				if (!ret) learned_cc_uah = val.intval;

				ret = bms_psy->desc->get_property(bms_psy, POWER_SUPPLY_PROP_BATTERY_FULL_DESIGN, &val);
				if (!ret) nom_batt_cap_mah = val.intval;

				if ((learned_cc_uah > 0) && (nom_batt_cap_mah > 0)) {
						shbatt_api_store_fg_cap_learning_result(
							learned_cc_uah,
							nom_batt_cap_mah,
							chip->cl_high_thresh,
							chip->cl_low_thresh);
				}
			}
		}
	}

out:
	pm_relax(chip->dev);
}

static void usb_psy_changed_work(struct work_struct *work)
{
	int ret;
	struct shbatt_chip *chip = container_of(work,
			struct shbatt_chip, usb_psy_changed_work);
	union power_supply_propval	val = {0,};
	struct power_supply*		usb_psy = NULL;

	dev_dbg(chip->dev, "usb_psy_changed_work start\n");

	if(!is_shbatt_task_initialized()) {
		dev_err(chip->dev, "shbatt_task is not initialize\n");
		goto out;
	}

	usb_psy = power_supply_get_by_name("usb");
	if(!usb_psy) {
		dev_err(chip->dev, "usb_psy is null\n");
		goto out;
	}

	ret = usb_psy->desc->get_property(usb_psy, POWER_SUPPLY_PROP_PRESENT, &val);
#ifdef CONFIG_SHARP_SHTERM
	if (!ret)
		shbatt_api_battlog_usb_present(val.intval);
#endif /* CONFIG_SHARP_SHTERM */

	ret = usb_psy->desc->get_property(usb_psy, POWER_SUPPLY_PROP_REAL_TYPE, &val);
#ifdef CONFIG_SHARP_SHTERM
	if (!ret)
		shbatt_api_battlog_usb_type(val.intval);
#endif /* CONFIG_SHARP_SHTERM */

	ret = usb_psy->desc->get_property(usb_psy, POWER_SUPPLY_PROP_TYPEC_MODE, &val);
#ifdef CONFIG_SHARP_SHTERM
	if (!ret)
		shbatt_api_battlog_typec_mode(val.intval);
#endif /* CONFIG_SHARP_SHTERM */

out:
	pm_relax(chip->dev);
}

static void dc_psy_changed_work(struct work_struct *work)
{
	int ret;
	struct shbatt_chip *chip = container_of(work,
			struct shbatt_chip, dc_psy_changed_work);
	union power_supply_propval	val = {0,};
	struct power_supply*		dc_psy = NULL;

	dev_dbg(chip->dev, "dc_psy_changed_work start\n");

	if(!is_shbatt_task_initialized()) {
		dev_err(chip->dev, "shbatt_task is not initialize\n");
		goto out;
	}

	dc_psy = power_supply_get_by_name("dc");
	if(!dc_psy) {
		dev_err(chip->dev, "dc_psy is null\n");
		goto out;
	}

	ret = dc_psy->desc->get_property(dc_psy, POWER_SUPPLY_PROP_PRESENT, &val);
#ifdef CONFIG_SHARP_SHTERM
	if (!ret)
		shbatt_api_battlog_dc_present(val.intval);
#endif /* CONFIG_SHARP_SHTERM */

out:
	pm_relax(chip->dev);
}

static int shbatt_notifier_cb(struct notifier_block *nb, unsigned long event, void *data)
{
	struct power_supply *psy = data;
	struct shbatt_chip *chip = container_of(nb, struct shbatt_chip, nb);

	dev_dbg(chip->dev, "notifier call back :%s\n", psy->desc->name);

	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	if (strcmp(psy->desc->name, "battery") == 0) {
		if (work_pending(&chip->batt_psy_changed_work))
			return NOTIFY_OK;

		pm_stay_awake(chip->dev);
		schedule_work(&chip->batt_psy_changed_work);
	}
	else if (strcmp(psy->desc->name, "usb") == 0) {
		if (work_pending(&chip->usb_psy_changed_work))
			return NOTIFY_OK;

		pm_stay_awake(chip->dev);
		schedule_work(&chip->usb_psy_changed_work);
	}
	else if (strcmp(psy->desc->name, "dc") == 0) {
		if (work_pending(&chip->dc_psy_changed_work))
			return NOTIFY_OK;

		pm_stay_awake(chip->dev);
		schedule_work(&chip->dc_psy_changed_work);
	}

	return NOTIFY_OK;
}

static int shbatt_drv_probe(
	struct platform_device*		dev_p
){
	int rc = 0;
	int byte_len;
	u32 temp;
	static struct shbatt_chip *chip;
	struct qpnp_vadc_chip *pm_vadc_dev = NULL;
	struct qpnp_vadc_chip *pmi_vadc_dev = NULL;
	struct device_node *batt_node = NULL;
	struct device_node *bms_node = NULL;
	struct device_node *pm_vadc_node = NULL;
	const char *name;
	const char *pa_temp_label;
	int index;
	struct device_node *node;

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	if (of_find_property(dev_p->dev.of_node, "qcom,shbatt-pm-vadc", NULL)) {
		pm_vadc_dev = qpnp_get_vadc(&dev_p->dev, "shbatt-pm");
		if (IS_ERR(pm_vadc_dev)) {
		rc = PTR_ERR(pm_vadc_dev);
		if (rc != -EPROBE_DEFER)
			SHPWR_LOG( SHPWR_LOG_LEVEL_ERR,SHPWR_LOG_TYPE_BATT,
					"[E] %s() Couldn't get pm vadc rc =%d.\n", __FUNCTION__ ,rc );
			return rc;
		}
		if(of_property_read_string(dev_p->dev.of_node, "sharp,pa_temp_label", &pa_temp_label))
		{
			pr_info("sharp,pa_temp_label is not found, set to pa_therm0 label\n");
			pa_temp_label = "pa_therm0";
		}
	}

	if (of_find_property(dev_p->dev.of_node, "qcom,shbatt-pmi-vadc", NULL)) {
		pmi_vadc_dev = qpnp_get_vadc(&dev_p->dev, "shbatt-pmi");
		if (IS_ERR(pmi_vadc_dev)) {
		rc = PTR_ERR(pmi_vadc_dev);
		if (rc != -EPROBE_DEFER)
			SHPWR_LOG( SHPWR_LOG_LEVEL_ERR,SHPWR_LOG_TYPE_BATT,
					"[E] %s() Couldn't get pmi vadc rc =%d.\n", __FUNCTION__ ,rc );
			return rc;
		}
	}

#ifndef CONFIG_PM_SUPPORT_BATT_AUTH
#ifdef CONFIG_PM_SUPPORT_BATT_TRACEABILITY
	if(of_property_read_bool(dev_p->dev.of_node, "sharp,show-batt-auth"))
		batt_auth = 1;
	else
		batt_auth = -1;
#endif /* CONFIG_PM_SUPPORT_BATT_TRACEABILITY */
#endif /* CONFIG_PM_SUPPORT_BATT_AUTH */

	chip = devm_kzalloc(&dev_p->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&dev_p->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32(dev_p->dev.of_node, "sharp,cl-high-thresh", &temp);
	if (rc < 0)
		chip->cl_high_thresh = DEFAULT_CL_HIGH_THRESH;
	else
		chip->cl_high_thresh = temp;

	rc = of_property_read_u32(dev_p->dev.of_node, "sharp,cl-low-thresh", &temp);
	if (rc < 0)
		chip->cl_low_thresh = DEFAULT_CL_LOW_THRESH;
	else
		chip->cl_low_thresh = temp;

	bms_node = of_find_compatible_node(NULL, NULL, "qcom,fg-gen3");
	if (bms_node) {
		rc = of_property_read_u32(bms_node, "qcom,cl-start-capacity", &temp);
		if (rc < 0)
			chip->cl_start_capacity = DEFAULT_CL_START_SOC;
		else
			chip->cl_start_capacity = temp;
	}

	batt_node = of_find_compatible_node(NULL, NULL, "qcom,qpnp-smb2");
	if (batt_node) {
		if (of_find_property(batt_node, "qcom,thermal-mitigation", &byte_len)) {
			chip->thermal_mitigation = devm_kzalloc(&dev_p->dev, byte_len,
				GFP_KERNEL);

			if (chip->thermal_mitigation != NULL) {
				chip->thermal_levels = byte_len / sizeof(u32);
				rc = of_property_read_u32_array(batt_node,
						"qcom,thermal-mitigation",
						chip->thermal_mitigation,
						chip->thermal_levels);
				if (rc < 0) {
					dev_err(&dev_p->dev,
						"Couldn't read threm limits rc = %d\n", rc);
				}
			}
		}
	}
	else {
		dev_err(&dev_p->dev, "%s: qcom,fg-gen3 not found, initialize to default value\n", __func__);
		chip->cl_high_thresh = DEFAULT_CL_HIGH_THRESH;
		chip->cl_low_thresh = DEFAULT_CL_LOW_THRESH;
		chip->cl_start_capacity = DEFAULT_CL_START_SOC;
		chip->thermal_mitigation = NULL;
		chip->thermal_levels = 0;
	}

	dev_dbg(&dev_p->dev, "%s: qcom,cl-high-thresh = %d\n", __func__, chip->cl_high_thresh);
	dev_dbg(&dev_p->dev, "%s: qcom,cl-low-thresh = %d\n", __func__, chip->cl_low_thresh);
	dev_dbg(&dev_p->dev, "%s: qcom,cl-start-capacity = %d\n", __func__, chip->cl_start_capacity);
	dev_dbg(&dev_p->dev, "%s: qcom,thermal-mitigation num = %d\n", __func__, chip->thermal_levels);

	chip->dev = &dev_p->dev;
	chip->pm_vadc_dev = pm_vadc_dev;
	chip->pmi_vadc_dev = pmi_vadc_dev;
	chip->pa_temp_label = pa_temp_label;

	if(chip->pm_vadc_dev)
	{
		pm_vadc_node = of_find_compatible_node(NULL, NULL, "qcom,qpnp-vadc-hc");
		if (pm_vadc_node != NULL)
		{
			pr_info("\"qcom,qpnp-vadc-hc\" compatible node is found.\n");
			for_each_child_of_node(pm_vadc_node, node)
			{
				if (node == NULL)
				{
					pr_err("pm_vadc node is null\n");
					break;
				}
				if(of_property_read_string(node, "label", &name))
					continue;

				for (index = 0; index < sizeof(pm_vadc_channels)/sizeof(struct shbatt_vadc_channel); index++)
				{
					if (strcmp(pm_vadc_channels[index].name, name) != 0)
						continue;

					of_property_read_u32(node,
										"reg", &pm_vadc_channels[index].channel);
				}
			}
		} else {
			pr_info("\"qcom,qpnp-vadc-hc\" compatible node is not found.\n");
		}

		for (index = 0; index < sizeof(pm_vadc_channels)/sizeof(struct shbatt_vadc_channel); index++)
			pr_info("pm_vadc label:%s, chan:0x%x\n", pm_vadc_channels[index].name, pm_vadc_channels[index].channel);
	} else {
		pr_info("pm_vadc is null\n");
	}
	pr_info("pa_temp_label:%s chan:0x%x\n", chip->pa_temp_label, shbatt_api_get_pm_vadc_chan_by_name(chip->pa_temp_label));

	the_chip = chip;

	INIT_WORK(&chip->batt_psy_changed_work, batt_psy_changed_work);
	INIT_WORK(&chip->usb_psy_changed_work, usb_psy_changed_work);
	INIT_WORK(&chip->dc_psy_changed_work, dc_psy_changed_work);

	chip->nb.notifier_call = shbatt_notifier_cb;
	rc = power_supply_reg_notifier(&chip->nb);
	if (rc < 0) {
		dev_err(chip->dev, "Failed register notifier_block rc:%d\n", rc);
		return rc;
	}

	if(shbatt_drv_create_device() < 0)
	{
		SHBATT_ERROR("%s : create device failed.\n",__FUNCTION__);
		return -EPERM;
	}

/*
 * If this is not comment out, the debugboard can
 * not boot normally. This is seems related
 * to share memeory
 */
	if (1) {
		shbatt_task_is_initialized = true;
	}

#ifdef CONFIG_PM_SUPPORT_BATT_TRACEABILITY
	shbatt_api_write_traceability();
#endif /* CONFIG_PM_SUPPORT_BATT_TRACEABILITY */

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return 0;
}

static int shbatt_drv_remove(
	struct platform_device*		dev_p
){
	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n", __FUNCTION__);

	the_chip = NULL;

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n", __FUNCTION__ );
	return 0;
}

static void shbatt_drv_shutdown(
	struct platform_device*		dev_p
){
	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[S] %s()\n", __FUNCTION__);

	shbatt_task_is_initialized = false;

	SHPWR_LOG( SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,
				"[E] %s()\n", __FUNCTION__ );
}

static int shbatt_drv_resume(
	struct platform_device*		dev_p
){
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return 0;
}

static int shbatt_drv_suspend(
	struct platform_device*		dev_p,
	pm_message_t				state
){
	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
	return 0;

}

static int __init shbatt_drv_module_init( void )
{
	SHBATT_TRACE( "[S] %s \n", __FUNCTION__ );

	shbatt_task_workqueue_p = create_singlethread_workqueue("shbatt_task");

	mutex_init(&shbatt_task_lock);

	spin_lock_init(&shbatt_pkt_lock);

	wakeup_source_init( &shbatt_wakeup_source, "shbatt_wake" );

	atomic_set( &shbatt_wakeup_source_num, 0 );

	platform_driver_register( &shbatt_platform_driver );

	/* wake_lock */
	memset( &shbatt_lock_time, 0x00, sizeof(shbatt_lock_time) );
	memset( &shbatt_unlock_time, 0x00, sizeof(shbatt_unlock_time) );
	memset( &shbatt_lock_func, 0x00, sizeof(shbatt_lock_func) );

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);

	return 0;
}

static void __exit shbatt_drv_module_exit( void )
{

	SHBATT_TRACE("[S] %s \n",__FUNCTION__);

	platform_driver_unregister(&shbatt_platform_driver);

	SHBATT_TRACE("[E] %s \n",__FUNCTION__);
}

bool is_shbatt_task_initialized( void ) {
	return shbatt_task_is_initialized;
}
EXPORT_SYMBOL(is_shbatt_task_initialized);

module_init(shbatt_drv_module_init);
module_exit(shbatt_drv_module_exit);

MODULE_DESCRIPTION("SH Battery Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.0");
/*+-----------------------------------------------------------------------------+*/
/*| @ THIS FILE END :                                                           |*/
/*+-----------------------------------------------------------------------------+*/
