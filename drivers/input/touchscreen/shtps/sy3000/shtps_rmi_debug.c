/* drivers/input/touchscreen/shtps/sy3000/shtps_rmi_debug.c
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
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/time.h>

#include <linux/fs.h>

#include <linux/input/shtps_dev.h>

#include "shtps_rmi.h"
#include "shtps_param_extern.h"
#include "shtps_rmi_debug.h"
#include "shtps_log.h"

#if defined(SHTPS_DEF_FWCTL_IC_TYPE_3700)
	#include "fwctl/shtps_fwctl_s3700.h"
#else
	#include "fwctl/shtps_fwctl_s3400.h"
#endif /* SHTPS_DEF_FWCTL_IC_TYPE_3700 */

#if defined( SHTPS_DEVELOP_MODE_ENABLE )

/* -----------------------------------------------------------------------------------
 */
#if defined ( SHTPS_TOUCH_EMURATOR_ENABLE )
	SHTPS_PARAM_DEF( SHTPS_TOUCH_EMURATOR_LOG_ENABLE, 				0  );
	SHTPS_PARAM_DEF( SHTPS_TOUCH_EMURATOR_START_WAIT_TIME_SEC, 		5  );

	#define	SHTPS_TOUCH_EMU_LOG_V(...)				\
		if(SHTPS_TOUCH_EMURATOR_LOG_ENABLE != 0){	\
			DBG_PRINTK("[emu] " __VA_ARGS__);		\
		}

	#define	SHTPS_TOUCH_EMU_LOG_DBG_PRINT(...)												\
		if(((shtps_get_logflag() & 0x02) != 0) || (SHTPS_TOUCH_EMURATOR_LOG_ENABLE != 0)){	\
			DBG_PRINTK("[emu] " __VA_ARGS__);												\
		}

	#define SHTPS_TOUCH_EMU_LOG_FUNC_CALL()													\
		if(((shtps_get_logflag() & 0x02) != 0) || (SHTPS_TOUCH_EMURATOR_LOG_ENABLE != 0)){	\
			DBG_PRINTK("[emu] %s()\n", __func__);											\
		}
	#define SHTPS_TOUCH_EMU_LOG_FUNC_CALL_INPARAM(param)									\
		if(((shtps_get_logflag() & 0x02) != 0) || (SHTPS_TOUCH_EMURATOR_LOG_ENABLE != 0)){	\
			DBG_PRINTK("[emu] %s(%d)\n", __func__, param);									\
		}

	#define F12_SET_DATA_FINGERSTATE(F12_2D_DATA01, para)		(F12_2D_DATA01[0] = para)
	#define F12_SET_DATA_XPOS(F12_2D_DATA01, para)				{F12_2D_DATA01[1] = para & 0xff; F12_2D_DATA01[2] = para >> 8;}
	#define F12_SET_DATA_YPOS(F12_2D_DATA01, para)				{F12_2D_DATA01[3] = para & 0xff; F12_2D_DATA01[4] = para >> 8;}
	#define F12_SET_DATA_Z(F12_2D_DATA01, para)					(F12_2D_DATA01[5] = para)
	#define F12_SET_DATA_WX(F12_2D_DATA01, para)				(F12_2D_DATA01[6] = para)
	#define F12_SET_DATA_WY(F12_2D_DATA01, para)				(F12_2D_DATA01[7] = para)
#endif /* #if defined ( SHTPS_TOUCH_EMURATOR_ENABLE ) */


#if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE )
	SHTPS_PARAM_DEF( SHTPS_TOUCH_EVENTLOG_LOG_ENABLE, 				0  );

	#define	SHTPS_TOUCH_EVENTLOG_LOG_V(...)			\
		if(SHTPS_TOUCH_EVENTLOG_LOG_ENABLE != 0){	\
			DBG_PRINTK("[eventlog] " __VA_ARGS__);	\
		}

	#define	SHTPS_TOUCH_EVENTLOG_LOG_DBG_PRINT(...)											\
		if(((shtps_get_logflag() & 0x02) != 0) || (SHTPS_TOUCH_EVENTLOG_LOG_ENABLE != 0)){	\
			DBG_PRINTK("[eventlog] " __VA_ARGS__);											\
		}

	#define SHTPS_TOUCH_EVENTLOG_LOG_FUNC_CALL()											\
		if(((shtps_get_logflag() & 0x02) != 0) || (SHTPS_TOUCH_EVENTLOG_LOG_ENABLE != 0)){	\
			DBG_PRINTK("[eventlog] %s()\n", __func__);										\
		}
	#define SHTPS_TOUCH_EVENTLOG_LOG_FUNC_CALL_INPARAM(param)								\
		if(((shtps_get_logflag() & 0x02) != 0) || (SHTPS_TOUCH_EVENTLOG_LOG_ENABLE != 0)){	\
			DBG_PRINTK("[eventlog] %s(%d)\n", __func__, param);								\
		}
#endif /* #if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE ) */

#if defined ( SHTPS_DEBUG_MSM_GPIO_ENABLE )
	#define SHTPS_DEF_MSM_GPIO_CONFIG_ADDR(a,b)	(a + (0x1000 * (b)))
	#define SHTPS_DEF_MSM_GPIO_IN_OUT_ADDR(a,b)	(a + (0x1000 * (b)) + 0x04)

	#define SHTPS_DEF_MSM_GPIO_OE_BIT_MASK				0x0200
	#define SHTPS_DEF_MSM_GPIO_OE_SHIFT					9
	#define SHTPS_DEF_MSM_GPIO_DRV_STRENGTH_BIT_MASK	0x01c0
	#define SHTPS_DEF_MSM_GPIO_DRV_STRENGTH_SHIFT		6
	#define SHTPS_DEF_MSM_GPIO_FUNC_SEL_BIT_MASK		0x003C
	#define SHTPS_DEF_MSM_GPIO_FUNC_SEL_SHIFT			2
	#define SHTPS_DEF_MSM_GPIO_PULL_BIT_MASK			0x0003
	#define SHTPS_DEF_MSM_GPIO_PULL_SHIFT				0

	#define SHTPS_DEF_MSM_GPIO_OUT_BIT_MASK				0x02
	#define SHTPS_DEF_MSM_GPIO_OUT_SHIFT				1
	#define SHTPS_DEF_MSM_GPIO_IN_BIT_MASK				0x01
	#define SHTPS_DEF_MSM_GPIO_IN_SHIFT					0
#endif /* #if defined ( SHTPS_DEBUG_MSM_GPIO_ENABLE ) */

/* -----------------------------------------------------------------------------------
 */
#if defined ( SHTPS_TOUCH_EMURATOR_ENABLE )
	enum{
		SHTPS_TOUCH_EMURATOR_STATE_DISABLE = 0,
		SHTPS_TOUCH_EMURATOR_STATE_WAITING,
		SHTPS_TOUCH_EMURATOR_STATE_RUNNING,
		SHTPS_TOUCH_EMURATOR_STATE_RECORDING,
	};

	struct shtps_touch_emurator_list_elem{
		unsigned int	generation_time_ms;
		unsigned char	finger_id;
		unsigned short	finger_x;
		unsigned short	finger_y;
		unsigned char	finger_state;
		unsigned char	finger_wx;
		unsigned char	finger_wy;
		unsigned char	finger_z;
	};

	struct shtps_touch_emurator_info{
		int										state;
		struct timeval							emu_start_timeval;
		int										touch_list_buffering_size;
		int										touch_list_num;
		int										current_touch_index;
		struct shtps_touch_emurator_list_elem	*touch_list;
		struct delayed_work						touch_emu_read_touchevent_delayed_work;
		struct shtps_touch_info					fw_report_info;

		struct file								*touchLogFilep;
		int										rec_num;
		struct timeval							rec_start_timeval;
	};
#endif /* #if defined ( SHTPS_TOUCH_EMURATOR_ENABLE ) */


#if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE )
	enum{
		SHTPS_TOUCH_EVENTLOG_STATE_DISABLE = 0,
		SHTPS_TOUCH_EVENTLOG_STATE_RECORDING,
	};

	struct shtps_touch_eventlog_info{
		int										state;

		struct file								*touchLogFilep;
		int										rec_num;
		struct timeval							rec_start_timeval;
	};
#endif /* #if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE ) */

#if defined ( SHTPS_ACCUMULATE_FW_REPORT_INFO_ENABLE )
	#define SHTPS_ACCUMULATE_FW_REPORT_INFO_MAX			10
	#define SHTPS_ACCUMULATE_FW_REPORT_INFO_SHOW_MAX	10

	struct shtps_accumulate_fw_report_info{
		struct timeval				start_timeval;
		int							index_st;
		int							count;
		int							fw_report_info_current_id;
		int							fw_report_info_id[SHTPS_ACCUMULATE_FW_REPORT_INFO_MAX];
		unsigned long				fw_report_info_time[SHTPS_ACCUMULATE_FW_REPORT_INFO_MAX];
		struct shtps_touch_info		fw_report_info[SHTPS_ACCUMULATE_FW_REPORT_INFO_MAX];
	};
#endif /* #if defined ( SHTPS_ACCUMULATE_FW_REPORT_INFO_ENABLE ) */

#if defined ( SHTPS_DEBUG_MSM_GPIO_ENABLE )
	struct shtps_debug_msm_gpio_param_info {
	    char *str;
	    uint8_t val;
	};

	static const struct shtps_debug_msm_gpio_param_info shtps_debug_msm_gpio_param_table[] = {
		{"func0",		0},
		{"func1",		1},
		{"func2",		2},
		{"func3",		3},
		{"func4",		4},
		{"func5",		5},
		{"func6",		6},
		{"func7",		7},
		{"func8",		8},
		{"input",		0},
		{"output",		1},
		{"no_pull",		0},
		{"pull_down",	1},
		{"pull_keeper",	2},
		{"pull_up",		3},
		{"2ma",			0},
		{"4ma",			1},
		{"6ma",			2},
		{"8ma",			3},
		{"10ma",		4},
		{"12ma",		5},
		{"14ma",		6},
		{"16ma",		7},
		{"low",			0},
		{"high",		1},
		{"End",			0xFF}
	};

	struct shtps_debug_msm_gpio_info{
		int gpio;
		uint8_t func;
		uint8_t dir;
		uint8_t pull;
		uint8_t drvstr;
		uint32_t outval;
	};

	struct shtps_debug_msm_gpio_base_addr_info {
		int gpio_min;
		int gpio_max;
		unsigned int base_addr;
	};

	struct shtps_debug_msm_gpio_base_addr_info shtps_debug_msm_gpio_base_addr_info_table[] = {
		#if defined(CONFIG_ARCH_SDM845)
			/* SDM845 V2 */
			{  4,   7, 0x03900000},
			{ 31,  34, 0x03900000},
			{ 38,  38, 0x03900000},
			{ 49,  68, 0x03900000},
			{ 79,  84, 0x03900000},
			{ 97, 121, 0x03900000},
			{127, 149, 0x03900000},
			{ 12,  26, 0x03D00000},
			{ 35,  37, 0x03D00000},
			{ 40,  40, 0x03D00000},
			{ 89,  96, 0x03D00000},
			{  0,   3, 0x03500000},
			{  8,  11, 0x03500000},
			{ 27,  30, 0x03500000},
			{ 39,  39, 0x03500000},
			{ 41,  48, 0x03500000},
			{ 69,  78, 0x03500000},
			{ 85,  88, 0x03500000},
			{122, 126, 0x03500000},
#if 0
			/* SDM845 V1 */
			{  0,  11, 0x03900000},
			{ 27,  34, 0x03900000},
			{ 38,  39, 0x03900000},
			{ 45,  84, 0x03900000},
			{ 97, 149, 0x03900000},
			{ 12,  26, 0x03D00000},
			{ 35,  37, 0x03D00000},
			{ 40,  44, 0x03D00000},
			{ 85,  96, 0x03D00000},
#endif
		#endif
			{  0,   0, 0xFFFFFFFF},
	};
#endif /* #if defined ( SHTPS_DEBUG_MSM_GPIO_ENABLE ) */

/* -----------------------------------------------------------------------------------
 */
#if defined ( SHTPS_TOUCH_EMURATOR_ENABLE )
	struct shtps_touch_emurator_info gShtps_touch_emu_info;
#endif /* #if defined ( SHTPS_TOUCH_EMURATOR_ENABLE ) */

#if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE )
	struct shtps_touch_eventlog_info gShtps_touch_eventlog_info;
#endif /* #if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE ) */

#if defined ( SHTPS_ACCUMULATE_FW_REPORT_INFO_ENABLE )
	SHTPS_PARAM_DEF( SHTPS_ACCUMULATE_FW_REPORT_INFO_ONOFF, 				0);
	static DEFINE_MUTEX(shtps_accumulate_fw_report_info_lock);
	struct shtps_accumulate_fw_report_info *gShtps_accumulate_fw_report_info_p;
#endif /* #if defined ( SHTPS_ACCUMULATE_FW_REPORT_INFO_ENABLE ) */

#if defined ( SHTPS_DEBUG_MSM_GPIO_ENABLE )
	static struct shtps_debug_msm_gpio_info shtps_debug_msm_gpio = {
		.gpio = -1,
	};
#endif /* #if defined ( SHTPS_DEBUG_MSM_GPIO_ENABLE ) */

/* -----------------------------------------------------------------------------------
 */
#if defined ( SHTPS_TOUCH_EMURATOR_ENABLE )
static int shtps_touch_emu_read_touchevent_timer_start(unsigned long delay_sec)
{
	SHTPS_TOUCH_EMU_LOG_FUNC_CALL_INPARAM((int)delay_sec);

	cancel_delayed_work(&gShtps_touch_emu_info.touch_emu_read_touchevent_delayed_work);
	schedule_delayed_work(&gShtps_touch_emu_info.touch_emu_read_touchevent_delayed_work, msecs_to_jiffies(delay_sec * 1000));

	return 0;
}

static int shtps_touch_emu_read_touchevent_timer_stop(void)
{
	SHTPS_TOUCH_EMU_LOG_FUNC_CALL();

	cancel_delayed_work(&gShtps_touch_emu_info.touch_emu_read_touchevent_delayed_work);

	return 0;
}

static void shtps_touch_emu_read_touchevent_delayed_work_function(struct work_struct *work)
{
	struct timeval current_timeval;
	unsigned long generation_time_us;
	unsigned long specified_time_us;
	unsigned long next_event_time_us;
	unsigned long next_event_time_total_us;
	int ret;
	
	int sleepcount;

	SHTPS_TOUCH_EMU_LOG_DBG_PRINT("shtps_touch_emu_read_touchevent_delayed_work_function() start\n");

	if(gShtps_touch_emu_info.state == SHTPS_TOUCH_EMURATOR_STATE_WAITING){
		gShtps_touch_emu_info.state = SHTPS_TOUCH_EMURATOR_STATE_RUNNING;

		do_gettimeofday(&gShtps_touch_emu_info.emu_start_timeval);
	}

	while(gShtps_touch_emu_info.state == SHTPS_TOUCH_EMURATOR_STATE_RUNNING){
		/* call read touchevent function */
		ret = shtps_read_touchevent_from_outside();
		if(ret < 0){
			gShtps_touch_emu_info.state = SHTPS_TOUCH_EMURATOR_STATE_DISABLE;
			SHTPS_TOUCH_EMU_LOG_DBG_PRINT("touch emurate end by state not active\n");
		}
		else if(gShtps_touch_emu_info.state == SHTPS_TOUCH_EMURATOR_STATE_DISABLE){
			SHTPS_TOUCH_EMU_LOG_DBG_PRINT("touch emurate end\n");
		}

		if(gShtps_touch_emu_info.state != SHTPS_TOUCH_EMURATOR_STATE_RUNNING){
			break;
		}

		generation_time_us = gShtps_touch_emu_info.touch_list[gShtps_touch_emu_info.current_touch_index].generation_time_ms * 1000;

		next_event_time_total_us = 0;

		sleepcount = 0;

		while(gShtps_touch_emu_info.state == SHTPS_TOUCH_EMURATOR_STATE_RUNNING){
		
			do_gettimeofday(&current_timeval);
			
			specified_time_us = ((current_timeval.tv_sec - gShtps_touch_emu_info.emu_start_timeval.tv_sec) * 1000000)
								 + ((current_timeval.tv_usec - gShtps_touch_emu_info.emu_start_timeval.tv_usec));

			if(generation_time_us > specified_time_us){
				next_event_time_us = generation_time_us - specified_time_us;
				if(next_event_time_us > (1 * 1000 * 1000)){
					next_event_time_us = (1 * 1000 * 1000);
				}
				next_event_time_total_us += next_event_time_us;

				usleep_range(next_event_time_us, next_event_time_us);

				sleepcount++;
			}
			else{
				SHTPS_TOUCH_EMU_LOG_V("generation_time_us=%lu, specified_time_us=%lu, next_event_time_total_us=%lu sleepcount=%d\n",
					generation_time_us,
					specified_time_us,
					next_event_time_total_us,
					sleepcount
				);
				break;
			}
		}
	}

	if(gShtps_touch_emu_info.touch_list != NULL){
		kfree(gShtps_touch_emu_info.touch_list);
		gShtps_touch_emu_info.touch_list = NULL;
	}
	gShtps_touch_emu_info.touch_list_num = 0;
	gShtps_touch_emu_info.touch_list_buffering_size = 0;
	gShtps_touch_emu_info.current_touch_index = 0;
	SHTPS_TOUCH_EMU_LOG_DBG_PRINT("shtps_touch_emu_read_touchevent_delayed_work_function() end\n");
}

int shtps_touch_emu_is_running(void)
{
	if(gShtps_touch_emu_info.state == SHTPS_TOUCH_EMURATOR_STATE_RUNNING){
		return 1;
	}
	return 0;
}

int shtps_touch_emu_set_finger_info(u8 *buf, int bufsize)
{
	int				i;
	struct shtps_touch_emurator_list_elem	*touch_list;
	int				set_data_flg = 0;
	unsigned long	set_data_time = 0;
	struct timeval current_timeval;
	unsigned long specified_time_ms;
	struct fingers *fw_repot_finger_tmp;
	
	SHTPS_TOUCH_EMU_LOG_V("shtps_touch_emu_set_finger_info() start\n");
	for(i = 0; i < SHTPS_FINGER_MAX; i++){
		u8 *F12_2D_DATA01 = buf + 2 + (8 * i);
		fw_repot_finger_tmp = &gShtps_touch_emu_info.fw_report_info.fingers[i];
		F12_SET_DATA_FINGERSTATE(	F12_2D_DATA01,	fw_repot_finger_tmp->state);
		F12_SET_DATA_XPOS(			F12_2D_DATA01,	fw_repot_finger_tmp->x);
		F12_SET_DATA_YPOS(			F12_2D_DATA01,	fw_repot_finger_tmp->y);
		#if defined(SHTPS_COORDINATES_POINT_SYMMETRY_ENABLE)
			if(fw_repot_finger_tmp->state != SHTPS_TOUCH_STATE_NO_TOUCH){
				struct shtps_rmi_spi *ts = gShtps_rmi_spi;
				F12_SET_DATA_XPOS(			F12_2D_DATA01,	(shtps_fwctl_get_maxXPosition(ts) - fw_repot_finger_tmp->x));
				F12_SET_DATA_YPOS(			F12_2D_DATA01,	(shtps_fwctl_get_maxYPosition(ts) - fw_repot_finger_tmp->y));
			}
		#endif /* SHTPS_COORDINATES_POINT_SYMMETRY_ENABLE */
		F12_SET_DATA_Z(				F12_2D_DATA01,	fw_repot_finger_tmp->z);
		F12_SET_DATA_WX(			F12_2D_DATA01,	fw_repot_finger_tmp->wx);
		F12_SET_DATA_WY(			F12_2D_DATA01,	fw_repot_finger_tmp->wy);
	}

	do_gettimeofday(&current_timeval);
	
	specified_time_ms = ((current_timeval.tv_sec - gShtps_touch_emu_info.emu_start_timeval.tv_sec) * 1000)
						 + ((current_timeval.tv_usec - gShtps_touch_emu_info.emu_start_timeval.tv_usec) / 1000);

	for(i = gShtps_touch_emu_info.current_touch_index; i < gShtps_touch_emu_info.touch_list_num; i++){
		touch_list = &gShtps_touch_emu_info.touch_list[i];
		if(touch_list->generation_time_ms <= specified_time_ms){
			if((set_data_flg != 0) && (set_data_time != touch_list->generation_time_ms)){
				break;
			}
			else{
				u8 *F12_2D_DATA01 = buf + 2 + (8 * touch_list->finger_id);
				F12_SET_DATA_FINGERSTATE(	F12_2D_DATA01,	touch_list->finger_state);
				F12_SET_DATA_XPOS(			F12_2D_DATA01,	touch_list->finger_x);
				F12_SET_DATA_YPOS(			F12_2D_DATA01,	touch_list->finger_y);
				#if defined(SHTPS_COORDINATES_POINT_SYMMETRY_ENABLE)
					if(touch_list->finger_state != SHTPS_TOUCH_STATE_NO_TOUCH){
						struct shtps_rmi_spi *ts = gShtps_rmi_spi;
						F12_SET_DATA_XPOS(			F12_2D_DATA01,	(shtps_fwctl_get_maxXPosition(ts) - touch_list->finger_x));
						F12_SET_DATA_YPOS(			F12_2D_DATA01,	(shtps_fwctl_get_maxYPosition(ts) - touch_list->finger_y));
					}
				#endif /* SHTPS_COORDINATES_POINT_SYMMETRY_ENABLE */
				F12_SET_DATA_Z(				F12_2D_DATA01,	touch_list->finger_z);
				F12_SET_DATA_WX(			F12_2D_DATA01,	touch_list->finger_wx);
				F12_SET_DATA_WY(			F12_2D_DATA01,	touch_list->finger_wy);

				fw_repot_finger_tmp = &gShtps_touch_emu_info.fw_report_info.fingers[touch_list->finger_id];
				fw_repot_finger_tmp->state	= touch_list->finger_state;
				fw_repot_finger_tmp->x		= touch_list->finger_x;
				fw_repot_finger_tmp->y		= touch_list->finger_y;
				fw_repot_finger_tmp->z		= touch_list->finger_z;
				fw_repot_finger_tmp->wx		= touch_list->finger_wx;
				fw_repot_finger_tmp->wy		= touch_list->finger_wy;

				set_data_flg = 1;

				set_data_time = touch_list->generation_time_ms;

				gShtps_touch_emu_info.current_touch_index++;
				SHTPS_TOUCH_EMU_LOG_V("touch emu set time=%u [%d] state=%d x=%d y=%d wx=%d wy=%d z=%d\n",
										touch_list->generation_time_ms,
										touch_list->finger_id,
										touch_list->finger_state,
										touch_list->finger_x,
										touch_list->finger_y,
										touch_list->finger_wx,
										touch_list->finger_wy,
										touch_list->finger_z
										);
			}
		}
		else{
			break;
		}
	}

	if(gShtps_touch_emu_info.current_touch_index >= gShtps_touch_emu_info.touch_list_num){
		gShtps_touch_emu_info.state = SHTPS_TOUCH_EMURATOR_STATE_DISABLE;
	}
	SHTPS_TOUCH_EMU_LOG_V("shtps_touch_emu_set_finger_info() end current index=(%d/%d)\n", gShtps_touch_emu_info.current_touch_index, gShtps_touch_emu_info.touch_list_num);
	return 0;
}


int shtps_touch_emu_is_recording(void)
{
	if(gShtps_touch_emu_info.state == SHTPS_TOUCH_EMURATOR_STATE_RECORDING){
		return 1;
	}
	return 0;
}

static int shtps_touch_emu_recording_data(unsigned char *buffer, ssize_t size)
{
	int nr_write = 0;

	if(gShtps_touch_emu_info.touchLogFilep){
		nr_write = vfs_write(gShtps_touch_emu_info.touchLogFilep, buffer, size, &gShtps_touch_emu_info.touchLogFilep->f_pos);
	}
	return nr_write;
}

int shtps_touch_emu_rec_finger_info(u8 *buf)
{
	int				i;
	struct timeval	current_timeval;
	unsigned long	recording_time_ms;
	u8				fingerNum = SHTPS_FINGER_MAX;
	u8				*fingerInfo;
	struct shtps_touch_info	fw_report_info_tmp;
	u8				strbuf[100];
	int				strsize;
	int				ret;
	

	if(gShtps_touch_emu_info.state != SHTPS_TOUCH_EMURATOR_STATE_RECORDING){
		SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "touch is not recording.\n" );
		return -1;
	}

	SHTPS_TOUCH_EMU_LOG_V("shtps_touch_emu_rec_finger_info() start\n");
	
	if(gShtps_touch_emu_info.rec_num == 0){
		do_gettimeofday(&gShtps_touch_emu_info.rec_start_timeval);
	}

	do_gettimeofday(&current_timeval);
	recording_time_ms = ((current_timeval.tv_sec - gShtps_touch_emu_info.rec_start_timeval.tv_sec) * 1000)
						 + ((current_timeval.tv_usec - gShtps_touch_emu_info.rec_start_timeval.tv_usec) / 1000);

	
	memcpy(&fw_report_info_tmp, &gShtps_touch_emu_info.fw_report_info, sizeof(fw_report_info_tmp));

	for(i = 0; i < fingerNum; i++){
		fingerInfo = &buf[i * 8];

		fw_report_info_tmp.fingers[i].state	= F12_DATA_FINGERSTATE(fingerInfo);
		fw_report_info_tmp.fingers[i].x		= F12_DATA_XPOS(fingerInfo);
		fw_report_info_tmp.fingers[i].y		= F12_DATA_YPOS(fingerInfo);
		#if defined(SHTPS_COORDINATES_POINT_SYMMETRY_ENABLE)
			if(fw_report_info_tmp.fingers[i].state != SHTPS_TOUCH_STATE_NO_TOUCH){
				struct shtps_rmi_spi *ts = gShtps_rmi_spi;
				fw_report_info_tmp.fingers[i].x		= shtps_fwctl_get_maxXPosition(ts) - F12_DATA_XPOS(fingerInfo);
				fw_report_info_tmp.fingers[i].y		= shtps_fwctl_get_maxYPosition(ts) - F12_DATA_YPOS(fingerInfo);
			}
		#endif /* SHTPS_COORDINATES_POINT_SYMMETRY_ENABLE */
		fw_report_info_tmp.fingers[i].wx	= F12_DATA_WX(fingerInfo);
		fw_report_info_tmp.fingers[i].wy	= F12_DATA_WY(fingerInfo);
		fw_report_info_tmp.fingers[i].z		= F12_DATA_Z(fingerInfo);
	}

	for(i = 0; i < fingerNum; i++){
		if(	(fw_report_info_tmp.fingers[i].state	!= gShtps_touch_emu_info.fw_report_info.fingers[i].state) ||
			(fw_report_info_tmp.fingers[i].x		!= gShtps_touch_emu_info.fw_report_info.fingers[i].x	) ||
			(fw_report_info_tmp.fingers[i].y		!= gShtps_touch_emu_info.fw_report_info.fingers[i].y	) ||
			(fw_report_info_tmp.fingers[i].wx		!= gShtps_touch_emu_info.fw_report_info.fingers[i].wx	) ||
			(fw_report_info_tmp.fingers[i].wy		!= gShtps_touch_emu_info.fw_report_info.fingers[i].wy	) ||
			(fw_report_info_tmp.fingers[i].z		!= gShtps_touch_emu_info.fw_report_info.fingers[i].z	) ){

			// time,state,id,x,y,wx,wy,z
			strsize = sprintf(strbuf, "%lu,%d,%d,%d,%d,%d,%d,%d\n",
								recording_time_ms,
								(fw_report_info_tmp.fingers[i].state),
								i,
								(fw_report_info_tmp.fingers[i].x	),
								(fw_report_info_tmp.fingers[i].y	),
								(fw_report_info_tmp.fingers[i].wx	),
								(fw_report_info_tmp.fingers[i].wy	),
								(fw_report_info_tmp.fingers[i].z	) );

			ret = shtps_touch_emu_recording_data(strbuf, strsize);
			SHTPS_TOUCH_EMU_LOG_V("shtps_touch_emu_rec_finger_info() ret=%d add %s", ret, strbuf);

			gShtps_touch_emu_info.rec_num++;
		}
	}

	memcpy(&gShtps_touch_emu_info.fw_report_info, &fw_report_info_tmp, sizeof(fw_report_info_tmp));

	SHTPS_TOUCH_EMU_LOG_V("shtps_touch_emu_rec_finger_info() end current rectime=%lu ms\n", recording_time_ms);
	return 0;
}


static int shtps_touch_emu_start_recording(u8 *fileName)
{
	int ret = 0;
	if(!gShtps_touch_emu_info.touchLogFilep){
		gShtps_touch_emu_info.touchLogFilep = filp_open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0660);
		if(IS_ERR(gShtps_touch_emu_info.touchLogFilep)){
			SHTPS_TOUCH_EMU_LOG_DBG_PRINT("touch record file open [%s] error!\n", fileName);
			gShtps_touch_emu_info.touchLogFilep = NULL;
			ret = -EINVAL;
		}
		else{
			SHTPS_TOUCH_EMU_LOG_DBG_PRINT("touch record file open [%s]\n", fileName);

			gShtps_touch_emu_info.state = SHTPS_TOUCH_EMURATOR_STATE_RECORDING;
			memset(&gShtps_touch_emu_info.fw_report_info, 0, sizeof(gShtps_touch_emu_info.fw_report_info));

			gShtps_touch_emu_info.rec_num = 0;
			ret = 0;
		}
	}
	else{
		SHTPS_TOUCH_EMU_LOG_DBG_PRINT("touch record file already open!\n");
		ret = -1;
	}
	return ret;
}
static int shtps_touch_emu_stop_recording(void)
{
	if(gShtps_touch_emu_info.touchLogFilep){
		filp_close(gShtps_touch_emu_info.touchLogFilep, NULL);
		gShtps_touch_emu_info.touchLogFilep = NULL;

		gShtps_touch_emu_info.state = SHTPS_TOUCH_EMURATOR_STATE_DISABLE;
		SHTPS_TOUCH_EMU_LOG_DBG_PRINT("touch record file close. rec_num=%d\n", gShtps_touch_emu_info.rec_num);
	}
	return 0;
}




static int shtps_get_arguments(
	char	*argStr,		/* [I/O] arguments strings (processed in function) */
	char	**argList,		/* [I/O] arguments pointer output buffer */
	int		argListMaxSize	/* [I/ ] arguments list size */
)
{
	int i;
	int argListNum = 0;
	int isParam;

	if((argStr == NULL) || (argList == NULL) || (argListMaxSize < 1)){
		return 0;
	}

	isParam = 0;

	for(i = 0; i < PAGE_SIZE; i++){
		if( (argStr[i] == '\0') ){
			if(isParam == 1){
				argListNum++;
			}
			break;
		}
		else if( (argStr[i] == '\n') || (argStr[i] == ',') || (argStr[i] == ' ') ){
			argStr[i] = '\0';
			if(isParam == 1){
				argListNum++;
			}
			isParam = 0;
			if(argListNum >= argListMaxSize){
				break;
			}
			continue;
		}
		else{
			if(isParam == 0){
				isParam = 1;
				argList[argListNum] = &argStr[i];
			}
		}
	}

	return argListNum;
}

static
int shtps_sysfs_start_touch_emurator(const char *buf, size_t count)
{
	u8 *data;

	if(NULL == buf || 0 == count){
		if(gShtps_touch_emu_info.touch_list == NULL){
			SHTPS_TOUCH_EMU_LOG_DBG_PRINT("touch emu no list data! please set data.\n");
			return -EINVAL;
		}
	}
	else{
		data = (u8*)kmalloc(count, GFP_KERNEL);
		if(data == NULL){
			return -ENOMEM;
		}
		memcpy(data, buf, count);

		gShtps_touch_emu_info.touch_list_num = count / sizeof(struct shtps_touch_emurator_list_elem);
		if(gShtps_touch_emu_info.touch_list != NULL){
			kfree(gShtps_touch_emu_info.touch_list);
		}
		gShtps_touch_emu_info.touch_list = (struct shtps_touch_emurator_list_elem *)data;
	}

	gShtps_touch_emu_info.current_touch_index = 0;
	gShtps_touch_emu_info.state = SHTPS_TOUCH_EMURATOR_STATE_WAITING;

	{
		int i;
		struct shtps_touch_emurator_list_elem *list_elem;
		for(i = 0; i < gShtps_touch_emu_info.touch_list_num; i++){
			list_elem = &gShtps_touch_emu_info.touch_list[i];
			SHTPS_TOUCH_EMU_LOG_DBG_PRINT("time=%u state[%d] Touch Info[%d] x=%d, y=%d, wx=%d, wy=%d, z=%d\n",
					list_elem->generation_time_ms,
					list_elem->finger_state,
					list_elem->finger_id,
					list_elem->finger_x,
					list_elem->finger_y,
					list_elem->finger_wx,
					list_elem->finger_wy,
					list_elem->finger_z);
		}
	}
	SHTPS_TOUCH_EMU_LOG_DBG_PRINT("touch emu start list_num=%d\n",
							gShtps_touch_emu_info.touch_list_num);

	memset(&gShtps_touch_emu_info.fw_report_info, 0, sizeof(gShtps_touch_emu_info.fw_report_info));

	shtps_touch_emu_read_touchevent_timer_stop();
	shtps_touch_emu_read_touchevent_timer_start(SHTPS_TOUCH_EMURATOR_START_WAIT_TIME_SEC);

	return 0;
}

static
int shtps_sysfs_stop_touch_emurator(void)
{
	SHTPS_TOUCH_EMU_LOG_DBG_PRINT("stop touch emurator. state(%d => %d)\n", gShtps_touch_emu_info.state, SHTPS_TOUCH_EMURATOR_STATE_DISABLE);
	shtps_touch_emu_read_touchevent_timer_stop();
	gShtps_touch_emu_info.state = SHTPS_TOUCH_EMURATOR_STATE_DISABLE;

	return 0;
}


static
ssize_t show_sysfs_emurator_ctrl(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	sprintf(buf, "emurator state=%d\n", gShtps_touch_emu_info.state);
	
	return( strlen(buf) );
}

static
ssize_t store_sysfs_emurator_ctrl(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	char *argbuf;
	int argc;
	char *argv[10];

	
	argbuf = (char*)kmalloc(count, GFP_KERNEL);
	if(argbuf == NULL){
		SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "argbuf alloc error.\n" );
		return( count );
	}
	memcpy(argbuf, buf, count);

	argc = shtps_get_arguments( argbuf, argv, sizeof(argv)/sizeof(char *) );

	SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "store_sysfs_emurator_ctrl() call start command = %s\n", argv[0] );

	if(strcmp(argv[0], "start") == 0){
		if(gShtps_touch_emu_info.state == SHTPS_TOUCH_EMURATOR_STATE_DISABLE){
			// call emurator start function
			ret = shtps_sysfs_start_touch_emurator(NULL, 0);
		}
		else{
			SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "emurator is already running.\n" );
		}
	}
	else if(strcmp(argv[0], "stop") == 0){
		shtps_sysfs_stop_touch_emurator();
	}
	else if(strcmp(argv[0], "clear") == 0){
		if(gShtps_touch_emu_info.state == SHTPS_TOUCH_EMURATOR_STATE_RUNNING){
			SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "emurator is running. stop emurator and clear buffer.\n" );
			shtps_sysfs_stop_touch_emurator();
		}
		else{
			SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "clear emurator data buffer. %d byte(list num = %d) => 0\n",
								gShtps_touch_emu_info.touch_list_buffering_size,
								gShtps_touch_emu_info.touch_list_num );
			if(gShtps_touch_emu_info.touch_list != NULL){
				kfree(gShtps_touch_emu_info.touch_list);
				gShtps_touch_emu_info.touch_list = NULL;
			}
			gShtps_touch_emu_info.touch_list_num = 0;
			gShtps_touch_emu_info.touch_list_buffering_size = 0;
		}
	}
	else if(strcmp(argv[0], "start_wait_time") == 0){
		if(argc >= 2){
			SHTPS_TOUCH_EMURATOR_START_WAIT_TIME_SEC = simple_strtol(argv[1], NULL, 0);
			SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "set emurator start wait time = %d(sec)\n", SHTPS_TOUCH_EMURATOR_START_WAIT_TIME_SEC );
		}
		else{
			SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "few parameters!\n" );
		}
	}
	else{
		SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "command [%s] is not support!\n", argv[0] );
	}

	SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "store_sysfs_emurator_ctrl() call end\n" );
	kfree(argbuf);

	if(ret < 0){
		return ret;
	}
	return( count );
}


static
ssize_t show_sysfs_emurator_data(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	sprintf(buf, "emurator_data_size = %d byte(list num = %d)\n",
							gShtps_touch_emu_info.touch_list_buffering_size,
							gShtps_touch_emu_info.touch_list_num );
	
	return( strlen(buf) );
}

static
ssize_t store_sysfs_emurator_data(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	char *data;
	int copy_size;

	SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "store_sysfs_emurator_data() call start count = %d\n", (int)count );

	copy_size = count;

	if(gShtps_touch_emu_info.state == SHTPS_TOUCH_EMURATOR_STATE_DISABLE){
		// buffering data
		if(gShtps_touch_emu_info.touch_list == NULL){
			gShtps_touch_emu_info.touch_list_num = 0;
			gShtps_touch_emu_info.touch_list_buffering_size = 0;

			data = (char *)kmalloc(copy_size, GFP_KERNEL);
			if(data == NULL){
				return -ENOMEM;
			}
			memcpy(data, buf, copy_size);
		}
		else{
			data = (char *)kmalloc(gShtps_touch_emu_info.touch_list_buffering_size + copy_size, GFP_KERNEL);
			if(data == NULL){
				return -ENOMEM;
			}
			memcpy(data, gShtps_touch_emu_info.touch_list, gShtps_touch_emu_info.touch_list_buffering_size);
			memcpy(data + gShtps_touch_emu_info.touch_list_buffering_size, buf, copy_size);
			kfree(gShtps_touch_emu_info.touch_list);
		}
		gShtps_touch_emu_info.touch_list = (struct shtps_touch_emurator_list_elem *)data;
		gShtps_touch_emu_info.touch_list_buffering_size += copy_size;
		gShtps_touch_emu_info.touch_list_num = gShtps_touch_emu_info.touch_list_buffering_size / sizeof(struct shtps_touch_emurator_list_elem);

		SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "store_sysfs_emurator_data() call end current size = %d byte(list num = %d)\n",
								gShtps_touch_emu_info.touch_list_buffering_size,
								gShtps_touch_emu_info.touch_list_num );
	}
	else{
		SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "emurator is running. not set data.\n" );
	}

	return( copy_size );
}

static
ssize_t store_sysfs_emurator_record_start(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	char *argbuf;
	int argc;
	char *argv[10];

	argbuf = (char*)kmalloc(count, GFP_KERNEL);
	if(argbuf == NULL){
		SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "argbuf alloc error.\n" );
		return -ENOMEM;
	}
	memcpy(argbuf, buf, count);

	argc = shtps_get_arguments( argbuf, argv, sizeof(argv)/sizeof(char *) );

	SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "store_sysfs_emurator_record_start() call start\n" );

	if(gShtps_touch_emu_info.state == SHTPS_TOUCH_EMURATOR_STATE_DISABLE){
		if(argc >= 1){
			ret = shtps_touch_emu_start_recording(argv[0]);
		}
		else{
			SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "touch recording not start because few paramters.\n" );
			ret = -EINVAL;
		}
	}
	else{
		SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "touch recording not start because emurator is running.\n" );
		ret = -1;
	}

	kfree(argbuf);
	if(ret < 0){
		return ret;
	}
	return( count );
}

static
ssize_t store_sysfs_emurator_record_stop(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	SHTPS_TOUCH_EMU_LOG_DBG_PRINT( "store_sysfs_emurator_record_stop() call start\n" );
	ret = shtps_touch_emu_stop_recording();

	if(ret < 0){
		return ret;
	}
	return( count );
}

struct shtps_debug_param_regist_info emurator_sysfs_regist_info[] = {
	{"emurator", SHTPS_DEBUG_PARAM_NAME_LOG, &SHTPS_TOUCH_EMURATOR_LOG_ENABLE, NULL, NULL},
	{"emurator", "ctrl", NULL, show_sysfs_emurator_ctrl, store_sysfs_emurator_ctrl},
	{"emurator", "data", NULL, show_sysfs_emurator_data, store_sysfs_emurator_data},
	{"emurator", "start_wait_time", &SHTPS_TOUCH_EMURATOR_START_WAIT_TIME_SEC, NULL, NULL},
	{"emurator", "record_start", NULL, NULL, store_sysfs_emurator_record_start},
	{"emurator", "record_stop", NULL, NULL, store_sysfs_emurator_record_stop},

	{NULL, NULL, NULL, NULL, NULL}
};
#endif /* #if defined ( SHTPS_TOUCH_EMURATOR_ENABLE ) */


#if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE )
int shtps_touch_eventlog_is_recording(void)
{
	if(gShtps_touch_eventlog_info.state == SHTPS_TOUCH_EVENTLOG_STATE_RECORDING){
		return 1;
	}
	return 0;
}

static int shtps_touch_eventlog_recording_data(unsigned char *buffer, ssize_t size)
{
	int nr_write = 0;

	if(gShtps_touch_eventlog_info.touchLogFilep){
		nr_write = vfs_write(gShtps_touch_eventlog_info.touchLogFilep, buffer, size, &gShtps_touch_eventlog_info.touchLogFilep->f_pos);
	}
	return nr_write;
}

int shtps_touch_eventlog_rec_event_info(int state, int finger, int x, int y, int w, int wx, int wy, int z)
{
	struct timeval	current_timeval;
	unsigned long	recording_time_ms;
	u8				strbuf[100];
	int				strsize;
	int				ret;

	if(gShtps_touch_eventlog_info.state != SHTPS_TOUCH_EVENTLOG_STATE_RECORDING){
		SHTPS_TOUCH_EVENTLOG_LOG_DBG_PRINT( "touch eventlog is not recording.\n" );
		return -1;
	}

	if(gShtps_touch_eventlog_info.rec_num == 0){
		do_gettimeofday(&gShtps_touch_eventlog_info.rec_start_timeval);
	}

	do_gettimeofday(&current_timeval);
	recording_time_ms = ((current_timeval.tv_sec - gShtps_touch_eventlog_info.rec_start_timeval.tv_sec) * 1000)
						 + ((current_timeval.tv_usec - gShtps_touch_eventlog_info.rec_start_timeval.tv_usec) / 1000);

	
	// time,state,id,x,y,w,wx,wy,z
	strsize = sprintf(strbuf, "%lu,%d,%d,%d,%d,%d,%d,%d,%d\n",
						recording_time_ms,
						state,
						finger,
						x,
						y,
						w,
						wx,
						wy,
						z);

	ret = shtps_touch_eventlog_recording_data(strbuf, strsize);
	SHTPS_TOUCH_EVENTLOG_LOG_V("shtps_touch_eventlog_recording_data() ret=%d add %s", ret, strbuf);

	gShtps_touch_eventlog_info.rec_num++;

	SHTPS_TOUCH_EVENTLOG_LOG_V("shtps_touch_eventlog_rec_event_info() current rectime=%lu ms\n", recording_time_ms);
	return 0;
}

static int shtps_eventlog_start_recording(u8 *fileName)
{
	int ret = 0;
	if(!gShtps_touch_eventlog_info.touchLogFilep){
		gShtps_touch_eventlog_info.touchLogFilep = filp_open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0660);
		if(IS_ERR(gShtps_touch_eventlog_info.touchLogFilep)){
			SHTPS_TOUCH_EVENTLOG_LOG_DBG_PRINT("touch eventlog record file open [%s] error!\n", fileName);
			gShtps_touch_eventlog_info.touchLogFilep = NULL;
			ret = -EINVAL;
		}
		else{
			SHTPS_TOUCH_EVENTLOG_LOG_DBG_PRINT("touch eventlog record file open [%s]\n", fileName);

			gShtps_touch_eventlog_info.state = SHTPS_TOUCH_EVENTLOG_STATE_RECORDING;

			gShtps_touch_eventlog_info.rec_num = 0;
			ret = 0;
		}
	}
	else{
		SHTPS_TOUCH_EVENTLOG_LOG_DBG_PRINT("touch eventlog record file already open!\n");
		ret = -1;
	}
	return ret;
}
static int shtps_eventlog_stop_recording(void)
{
	if(gShtps_touch_eventlog_info.touchLogFilep){
		filp_close(gShtps_touch_eventlog_info.touchLogFilep, NULL);
		gShtps_touch_eventlog_info.touchLogFilep = NULL;

		gShtps_touch_eventlog_info.state = SHTPS_TOUCH_EVENTLOG_STATE_DISABLE;
		SHTPS_TOUCH_EVENTLOG_LOG_DBG_PRINT("touch eventlog record file close. rec_num=%d\n", gShtps_touch_eventlog_info.rec_num);
	}
	return 0;
}

static
ssize_t store_sysfs_eventlog_record_start(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	char *argbuf;
	int argc;
	char *argv[10];

	argbuf = (char*)kmalloc(count, GFP_KERNEL);
	if(argbuf == NULL){
		SHTPS_TOUCH_EVENTLOG_LOG_DBG_PRINT( "argbuf alloc error.\n" );
		return -ENOMEM;
	}
	memcpy(argbuf, buf, count);

	argc = shtps_get_arguments( argbuf, argv, sizeof(argv)/sizeof(char *) );

	SHTPS_TOUCH_EVENTLOG_LOG_DBG_PRINT( "store_sysfs_eventlog_record_start() call start\n" );

	if(gShtps_touch_eventlog_info.state == SHTPS_TOUCH_EVENTLOG_STATE_DISABLE){
		if(argc >= 1){
			ret = shtps_eventlog_start_recording(argv[0]);
		}
		else{
			SHTPS_TOUCH_EVENTLOG_LOG_DBG_PRINT( "touch eventlog recording not start because few paramters.\n" );
			ret = -EINVAL;
		}
	}
	else{
		SHTPS_TOUCH_EVENTLOG_LOG_DBG_PRINT( "touch eventlog recording not start because already running.\n" );
		ret = -1;
	}

	kfree(argbuf);
	if(ret < 0){
		return ret;
	}
	return( count );
}

static
ssize_t store_sysfs_eventlog_record_stop(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;

	SHTPS_TOUCH_EVENTLOG_LOG_DBG_PRINT( "store_sysfs_eventlog_record_stop() call start\n" );
	ret = shtps_eventlog_stop_recording();

	if(ret < 0){
		return ret;
	}
	return( count );
}

struct shtps_debug_param_regist_info eventlog_rec_sysfs_regist_info[] = {
	{"eventlog", SHTPS_DEBUG_PARAM_NAME_LOG, &SHTPS_TOUCH_EVENTLOG_LOG_ENABLE, NULL, NULL},
	{"eventlog", "record_start", NULL, NULL, store_sysfs_eventlog_record_start},
	{"eventlog", "record_stop", NULL, NULL, store_sysfs_eventlog_record_stop},

	{NULL, NULL, NULL, NULL, NULL}
};
#endif /* #if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE ) */


#if defined ( SHTPS_ACCUMULATE_FW_REPORT_INFO_ENABLE )
static int shtps_accumulate_fw_report_info_push(struct shtps_accumulate_fw_report_info *mng_p, struct shtps_touch_info *touch_info, unsigned long time_us)
{
	int index_target;

	mutex_lock(&shtps_accumulate_fw_report_info_lock);

	index_target = mng_p->index_st + mng_p->count;
	index_target %= SHTPS_ACCUMULATE_FW_REPORT_INFO_MAX;

	if(mng_p->count > 0){
		if(index_target == mng_p->index_st){
			mng_p->index_st++;
			mng_p->index_st %= SHTPS_ACCUMULATE_FW_REPORT_INFO_MAX;
		}
	}

	if(mng_p->count < SHTPS_ACCUMULATE_FW_REPORT_INFO_MAX){
		mng_p->count++;
	}

	memcpy(&mng_p->fw_report_info[index_target], touch_info, sizeof(struct shtps_touch_info));
	mng_p->fw_report_info_id[index_target] = mng_p->fw_report_info_current_id;
	mng_p->fw_report_info_time[index_target] = time_us;
	mng_p->fw_report_info_current_id++;

	mutex_unlock(&shtps_accumulate_fw_report_info_lock);

	return 0;
}

static struct shtps_touch_info* shtps_accumulate_fw_report_info_pop(struct shtps_accumulate_fw_report_info *mng_p, int *id, unsigned long *time_us)
{
	struct shtps_touch_info *touch_info = NULL;

	mutex_lock(&shtps_accumulate_fw_report_info_lock);

	if(mng_p->count > 0){
		touch_info = &mng_p->fw_report_info[mng_p->index_st];
		if(id != NULL){
			*id = mng_p->fw_report_info_id[mng_p->index_st];
		}
		if(time_us != NULL){
			*time_us = mng_p->fw_report_info_time[mng_p->index_st];
		}
		mng_p->count--;
		mng_p->index_st++;
		mng_p->index_st %= SHTPS_ACCUMULATE_FW_REPORT_INFO_MAX;
	}

	mutex_unlock(&shtps_accumulate_fw_report_info_lock);

	return touch_info;
}

void shtps_accumulate_fw_report_info_add(struct shtps_touch_info *touch_info)
{
	struct timeval current_timeval;
	unsigned long specified_time_us;
	if(gShtps_accumulate_fw_report_info_p == NULL){
		return;
	}
	if(SHTPS_ACCUMULATE_FW_REPORT_INFO_ONOFF != 1){
		return;
	}
	if(touch_info == NULL){
		return;
	}

	do_gettimeofday(&current_timeval);
	specified_time_us = ((current_timeval.tv_sec - gShtps_accumulate_fw_report_info_p->start_timeval.tv_sec) * 1000000)
						 + ((current_timeval.tv_usec - gShtps_accumulate_fw_report_info_p->start_timeval.tv_usec));

	shtps_accumulate_fw_report_info_push(gShtps_accumulate_fw_report_info_p, touch_info, specified_time_us);
	return;
}

static
ssize_t show_sysfs_fw_report_info(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i;
	int cnt;
	static char workbuf[512];
	struct shtps_touch_info* info;
	int fingerMax = SHTPS_FINGER_MAX;
	int fw_report_info_id;
	int loopmax;
	unsigned long time_us;

	loopmax = gShtps_accumulate_fw_report_info_p->count;
	if(loopmax < SHTPS_ACCUMULATE_FW_REPORT_INFO_SHOW_MAX){
		loopmax = SHTPS_ACCUMULATE_FW_REPORT_INFO_SHOW_MAX;
	}

	buf[0] = '\0';
	for(cnt = 0; cnt < loopmax; cnt++){
		info = shtps_accumulate_fw_report_info_pop(gShtps_accumulate_fw_report_info_p, &fw_report_info_id, &time_us);
		if(info != NULL){
			for (i = 0; i < fingerMax; i++) {
				sprintf(workbuf, "[%d] time=%lu,id=%d,state=%d,x=%d,y=%d,wx=%d,wy=%d,z=%d\n",
									fw_report_info_id,
									time_us,
									i,
									info->fingers[i].state,
									info->fingers[i].x,
									info->fingers[i].y,
									info->fingers[i].wx,
									info->fingers[i].wy,
									info->fingers[i].z);
				strcat(buf, workbuf);
			}
		}
	}

	return( strlen(buf) );
}

struct shtps_debug_param_regist_info fw_report_info_sysfs_regist_info[] = {
	{"fw_report_info", SHTPS_DEBUG_PARAM_NAME_ONOFF, &SHTPS_ACCUMULATE_FW_REPORT_INFO_ONOFF, NULL, NULL},
	{"fw_report_info", "fw_report_info", NULL, show_sysfs_fw_report_info, NULL},

	{NULL, NULL, NULL, NULL, NULL}
};
#endif /* #if defined ( SHTPS_ACCUMULATE_FW_REPORT_INFO_ENABLE ) */

/* -----------------------------------------------------------------------------------
 */
#if defined ( SHTPS_DEBUG_MSM_GPIO_ENABLE )
static uint32_t shtps_msm_gpio_reg_read(unsigned int physical_address)
{
	int ret = 0;
	void __iomem *reg_address = ioremap_nocache(physical_address, 4);

	if (reg_address == NULL) {
		SHTPS_LOG_ERR_PRINT("[DEBUG_MSM_GPIO]msm_gpio_reg_read: ioremap_nocache error\n");
		return 0;
	}

	ret = ioread32(reg_address);
	iounmap(reg_address);

	return ret;
}

static void shtps_msm_gpio_reg_write(unsigned int physical_address, uint32_t config)
{
    void __iomem *reg_address = ioremap_nocache(physical_address, 4);

	if (reg_address == NULL) {
		SHTPS_LOG_ERR_PRINT("[DEBUG_MSM_GPIO]msm_gpio_reg_write: ioremap_nocache error\n");
		return;
	}

	iowrite32(config, reg_address);
	iounmap(reg_address);

	return;
}

static unsigned int shtps_msm_gpio_get_config_addr(int gpio)
{
	int i;
	unsigned int ret_addr = 0;

	for(i = 0; shtps_debug_msm_gpio_base_addr_info_table[i].base_addr != 0xFFFFFFFF; i++){
		if( (shtps_debug_msm_gpio_base_addr_info_table[i].gpio_min <= gpio) &&
			(gpio <= shtps_debug_msm_gpio_base_addr_info_table[i].gpio_max) )
		{
			ret_addr = SHTPS_DEF_MSM_GPIO_CONFIG_ADDR(shtps_debug_msm_gpio_base_addr_info_table[i].base_addr, gpio);
		}
	}

	return ret_addr;
}

static unsigned int shtps_msm_gpio_get_in_out_addr(int gpio)
{
	int i;
	unsigned int ret_addr = 0;

	for(i = 0; shtps_debug_msm_gpio_base_addr_info_table[i].base_addr != 0xFFFFFFFF; i++){
		if( (shtps_debug_msm_gpio_base_addr_info_table[i].gpio_min <= gpio) &&
			(gpio <= shtps_debug_msm_gpio_base_addr_info_table[i].gpio_max) )
		{
			ret_addr = SHTPS_DEF_MSM_GPIO_IN_OUT_ADDR(shtps_debug_msm_gpio_base_addr_info_table[i].base_addr, gpio);
		}
	}

	return ret_addr;
}

static int shtps_msm_gpio_info_get(struct shtps_debug_msm_gpio_info *info)
{
    unsigned int gpio_address;
	uint32_t gpio_config;
	uint32_t gpio_outval;

    gpio_address = shtps_msm_gpio_get_config_addr(info->gpio);
	if(gpio_address == 0){
		SHTPS_LOG_ERR_PRINT("[DEBUG_MSM_GPIO]get config addr error\n");
		return -1;
	}

	gpio_config  = shtps_msm_gpio_reg_read(gpio_address);
	info->func   = ((gpio_config & SHTPS_DEF_MSM_GPIO_FUNC_SEL_BIT_MASK) >> SHTPS_DEF_MSM_GPIO_FUNC_SEL_SHIFT);
	info->dir    = ((gpio_config & SHTPS_DEF_MSM_GPIO_OE_BIT_MASK) >> SHTPS_DEF_MSM_GPIO_OE_SHIFT);
	info->pull   = ((gpio_config & SHTPS_DEF_MSM_GPIO_PULL_BIT_MASK) >> SHTPS_DEF_MSM_GPIO_PULL_SHIFT);
	info->drvstr = ((gpio_config & SHTPS_DEF_MSM_GPIO_DRV_STRENGTH_BIT_MASK) >> SHTPS_DEF_MSM_GPIO_DRV_STRENGTH_SHIFT);

    gpio_address = shtps_msm_gpio_get_in_out_addr(info->gpio);
	if(gpio_address == 0){
		SHTPS_LOG_ERR_PRINT("[DEBUG_MSM_GPIO]get in/out addr error\n");
		return -1;
	}

	gpio_outval = shtps_msm_gpio_reg_read(gpio_address);
	if(info->dir == 0){
		info->outval = ((gpio_outval & SHTPS_DEF_MSM_GPIO_IN_BIT_MASK) >> SHTPS_DEF_MSM_GPIO_IN_SHIFT);
	}else{
		info->outval = ((gpio_outval & SHTPS_DEF_MSM_GPIO_OUT_BIT_MASK) >> SHTPS_DEF_MSM_GPIO_OUT_SHIFT);
	}

	return 0;
}

static int shtps_msm_gpio_info_set(struct shtps_debug_msm_gpio_info *info)
{
    unsigned int gpio_address;
	uint32_t gpio_config = 0;

	gpio_config |= ((info->func << SHTPS_DEF_MSM_GPIO_FUNC_SEL_SHIFT) & SHTPS_DEF_MSM_GPIO_FUNC_SEL_BIT_MASK);
	gpio_config |= ((info->dir << SHTPS_DEF_MSM_GPIO_OE_SHIFT) & SHTPS_DEF_MSM_GPIO_OE_BIT_MASK);
	gpio_config |= ((info->pull << SHTPS_DEF_MSM_GPIO_PULL_SHIFT) & SHTPS_DEF_MSM_GPIO_PULL_BIT_MASK);
	gpio_config |= ((info->drvstr << SHTPS_DEF_MSM_GPIO_DRV_STRENGTH_SHIFT) & SHTPS_DEF_MSM_GPIO_DRV_STRENGTH_BIT_MASK);

    gpio_address = shtps_msm_gpio_get_config_addr(info->gpio);
	if(gpio_address == 0){
		SHTPS_LOG_ERR_PRINT("[DEBUG_MSM_GPIO]get config addr error\n");
		return -1;
	}
	shtps_msm_gpio_reg_write(gpio_address, gpio_config);

	gpio_config = ((info->outval << SHTPS_DEF_MSM_GPIO_OUT_SHIFT) & SHTPS_DEF_MSM_GPIO_OUT_BIT_MASK);

    gpio_address = shtps_msm_gpio_get_in_out_addr(info->gpio);
	if(gpio_address == 0){
		SHTPS_LOG_ERR_PRINT("[DEBUG_MSM_GPIO]get in/out addr error\n");
		return -1;
	}
	shtps_msm_gpio_reg_write(gpio_address, gpio_config);

	return 0;
}

static
ssize_t show_sysfs_msm_gpio_dump(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int rc = 0;
	ssize_t count;
	uint8_t gpio_func;
	uint8_t gpio_dir;
	uint8_t gpio_pull;
	uint8_t gpio_drvstr;
	uint32_t gpio_outval;

	rc = shtps_msm_gpio_info_get(&shtps_debug_msm_gpio);

	if(rc != 0){
		count = snprintf(buf, PAGE_SIZE, "[msm_gpio] gpio dump error\n");
	}else{
		gpio_func = shtps_debug_msm_gpio.func;
		gpio_dir = shtps_debug_msm_gpio.dir;
		gpio_pull = shtps_debug_msm_gpio.pull;
		gpio_drvstr = shtps_debug_msm_gpio.drvstr;
		gpio_outval = shtps_debug_msm_gpio.outval;

		count = snprintf(buf, PAGE_SIZE, "[msm_gpio][%d.] [FS]0x%X [DIR]%s(%d) [PULL]%s(%d) [DRV]%s(%d) [VAL]%s(%d)\n",
											shtps_debug_msm_gpio.gpio,
											gpio_func,
											(gpio_dir == 0) ? "IN" : "OUT",
											gpio_dir,
											(gpio_pull == 0) ? "NO" : (gpio_pull == 1) ? "DOWN" :
												(gpio_pull == 2) ? "KEEPER" : (gpio_pull == 3) ? "UP" : "Unknown",
											gpio_pull,
											(gpio_drvstr == 0) ? "2mA" : (gpio_drvstr == 1) ? "4mA" : (gpio_drvstr == 2) ? "6mA" :
												(gpio_drvstr == 3) ? "8mA" : (gpio_drvstr == 4) ? "10mA" : (gpio_drvstr == 5) ? "12mA" :
												(gpio_drvstr == 6) ? "14mA" : (gpio_drvstr == 7) ? "16mA" : "Unknown",
											gpio_drvstr,
											(gpio_outval == 0) ? "Low" : "High",
											gpio_outval);
	}

	return count;
}

static
ssize_t store_sysfs_msm_gpio_dump(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val;

	sscanf(buf,"%d", &val);

	shtps_debug_msm_gpio.gpio = val;

	return count;
}

static
ssize_t show_sysfs_msm_gpio_config(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	ssize_t count;

	count = snprintf(buf, PAGE_SIZE,
						"Usage\n"
						"<gpio | func | dir | pull | drvstr | outval>\n"
						"   <gpio>    : 0-xx\n"
						"   <func>    : func0 / func1 / func2 / func3 / func4 / func5 / func6 / func7 / func8\n"
						"   <dir>     : input / output\n"
						"   <pull>    : no_pull / pull_down / pull_up\n"
						"   <drvstr>  : 2ma / 4ma / 6ma / 8ma / 10ma / 12ma / 14ma / 16ma\n"
						"   <outval>  : low / high\n");

	return count;

}

static
ssize_t store_sysfs_msm_gpio_config(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int i;
	int gpio_num;
	char gpio_func_str[6];
	char gpio_dir_str[7];
	char gpio_pull_str[12];
	char gpio_drvstr_str[5];
	char gpio_outval_str[5];

	sscanf(buf,"%d %s %s %s %s %s", &gpio_num, gpio_func_str, gpio_dir_str, gpio_pull_str, gpio_drvstr_str, gpio_outval_str);

	shtps_debug_msm_gpio.gpio = gpio_num;

	for(i = 0; shtps_debug_msm_gpio_param_table[i].val != 0xFF; i++){
		if( strcmp(gpio_func_str, shtps_debug_msm_gpio_param_table[i].str) == 0 ){
			shtps_debug_msm_gpio.func = shtps_debug_msm_gpio_param_table[i].val;
		}
	}

	for(i = 0; shtps_debug_msm_gpio_param_table[i].val != 0xFF; i++){
		if( strcmp(gpio_dir_str, shtps_debug_msm_gpio_param_table[i].str) == 0 ){
			shtps_debug_msm_gpio.dir = shtps_debug_msm_gpio_param_table[i].val;
		}
	}

	for(i = 0; shtps_debug_msm_gpio_param_table[i].val != 0xFF; i++){
		if( strcmp(gpio_pull_str, shtps_debug_msm_gpio_param_table[i].str) == 0 ){
			shtps_debug_msm_gpio.pull = shtps_debug_msm_gpio_param_table[i].val;
		}
	}

	for(i = 0; shtps_debug_msm_gpio_param_table[i].val != 0xFF; i++){
		if( strcmp(gpio_drvstr_str, shtps_debug_msm_gpio_param_table[i].str) == 0 ){
			shtps_debug_msm_gpio.drvstr = shtps_debug_msm_gpio_param_table[i].val;
		}
	}

	for(i = 0; shtps_debug_msm_gpio_param_table[i].val != 0xFF; i++){
		if( strcmp(gpio_outval_str, shtps_debug_msm_gpio_param_table[i].str) == 0 ){
			shtps_debug_msm_gpio.outval = shtps_debug_msm_gpio_param_table[i].val;
		}
	}

	shtps_msm_gpio_info_set(&shtps_debug_msm_gpio);

	return count;
}

struct shtps_debug_param_regist_info shtps_debug_msm_gpio_info[] = {
	{"msm_gpio_debug", "msm_gpio_dump", NULL, show_sysfs_msm_gpio_dump, store_sysfs_msm_gpio_dump},
	{"msm_gpio_debug", "msm_gpio_config", NULL, show_sysfs_msm_gpio_config, store_sysfs_msm_gpio_config},
	{NULL, NULL, NULL, NULL, NULL}
};
#endif /* #if defined ( SHTPS_DEBUG_MSM_GPIO_ENABLE ) */

/* -----------------------------------------------------------------------------------
 */
#define SHTPS_DEBUG_PARAM_DIR_NAME_ROOT		"debug"
#define SHTPS_DEBUG_PARAM_DIR_NAME_PARAM	"parameter"

struct shtps_debug_info{
	struct list_head	kobj_list;
	struct list_head	attr_list;

};

static struct shtps_debug_info *gDegbuInfo;


struct shtps_debug_kobj_list_info{
	struct list_head	kobj_list;
	struct kobject		*parent_kobj_p;
	struct kobject		*kobj_p;
};

struct shtps_debug_attr_list_info{
	struct list_head		attr_list;
	struct kobject			*parent_kobj_p;
	struct kobj_attribute	*kobj_attr_p;
	struct kobj_attribute	kobj_attr;
	int						*param_p;
};

static struct kobject* shtps_debug_search_kobj(struct kobject *parent_kobj_p, const char *search_name)
{
	struct list_head *list_p;
	struct shtps_debug_kobj_list_info *item_info_p;
	struct kobject *find_kobj_p = NULL;

	list_for_each(list_p, &(gDegbuInfo->kobj_list)){
		item_info_p = list_entry(list_p, struct shtps_debug_kobj_list_info, kobj_list);

		if( strcmp(kobject_name(item_info_p->kobj_p), search_name) == 0 ){
			if(parent_kobj_p == NULL){
				find_kobj_p = item_info_p->kobj_p;
				break;
			}else{
				if(item_info_p->parent_kobj_p == parent_kobj_p){
					find_kobj_p = item_info_p->kobj_p;
					break;
				}
			}
		}
	}

	return find_kobj_p;
}

static struct shtps_debug_attr_list_info* shtps_debug_search_attr(struct kobject *parent_kobj_p, const char *search_name)
{
	struct list_head *list_p;
	struct shtps_debug_attr_list_info *item_info_p;
	struct shtps_debug_attr_list_info *find_attr_p = NULL;

	list_for_each(list_p, &(gDegbuInfo->attr_list)){
		item_info_p = list_entry(list_p, struct shtps_debug_attr_list_info, attr_list);

		if( strcmp(item_info_p->kobj_attr.attr.name, search_name) == 0 ){
			if(parent_kobj_p == NULL){
				find_attr_p = item_info_p;
				break;
			}else{
				if(item_info_p->parent_kobj_p == parent_kobj_p){
					find_attr_p = item_info_p;
					break;
				}
			}
		}
	}

	return find_attr_p;
}

static ssize_t shtps_debug_common_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	ssize_t count;
	struct shtps_debug_attr_list_info *find_attr_p = NULL;

	find_attr_p = shtps_debug_search_attr(kobj, attr->attr.name);

	if(find_attr_p == NULL){
		count = snprintf(buf, PAGE_SIZE, "-\n");
	}else if(find_attr_p->param_p == NULL){
		count = snprintf(buf, PAGE_SIZE, "-\n");
	}else{
		count = snprintf(buf, PAGE_SIZE, "%d\n", *(find_attr_p->param_p));
	}

	return count;
}

static ssize_t shtps_debug_common_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct shtps_debug_attr_list_info *find_attr_p = NULL;
	int val;

	sscanf(buf,"%d", &val);

	find_attr_p = shtps_debug_search_attr(kobj, attr->attr.name);

	if( (find_attr_p != NULL) && (find_attr_p->param_p != NULL) ){
		*(find_attr_p->param_p) = val;
	}

	return count;
}

static struct kobject* shtps_debug_create_kobj(struct kobject *parent_kobj_p, const char *name)
{
	struct shtps_debug_kobj_list_info *list_info_p = NULL;
	struct kobject *create_kobj_p = NULL;

	list_info_p = kzalloc(sizeof(struct shtps_debug_kobj_list_info), GFP_KERNEL);
	if(list_info_p != NULL){
		create_kobj_p = kobject_create_and_add(name, parent_kobj_p);
		if(create_kobj_p != NULL){
			list_info_p->kobj_p = create_kobj_p;
			list_info_p->parent_kobj_p = parent_kobj_p;
			list_add_tail(&(list_info_p->kobj_list), &(gDegbuInfo->kobj_list));
		}
		else{
			kfree(list_info_p);
		}
	}

	return create_kobj_p;
}

static void shtps_debug_delete_kobj_all(void)
{
	struct shtps_debug_kobj_list_info *item_info_p;

	while( list_empty( &(gDegbuInfo->kobj_list) ) == 0 ){
		item_info_p = list_entry(gDegbuInfo->kobj_list.next, struct shtps_debug_kobj_list_info, kobj_list);

		if(item_info_p != NULL){
			kobject_put(item_info_p->kobj_p);
			list_del_init(&(item_info_p->kobj_list));
			kfree(item_info_p);
		}
	}

	list_del_init(&(gDegbuInfo->kobj_list));
}

static int shtps_debug_create_attr(struct kobject *parent_kobj_p, struct kobj_attribute attr, int *param_p)
{
	struct shtps_debug_attr_list_info *list_info_p = NULL;
	int rc;

	list_info_p = kzalloc(sizeof(struct shtps_debug_attr_list_info), GFP_KERNEL);
	if(list_info_p != NULL){
		list_info_p->parent_kobj_p = parent_kobj_p;
		list_info_p->kobj_attr_p = &(list_info_p->kobj_attr);
		list_info_p->kobj_attr = attr;
		if(param_p == NULL){
			list_info_p->param_p = NULL;
		}else{
			list_info_p->param_p = param_p;
		}
		rc = sysfs_create_file(parent_kobj_p, &(list_info_p->kobj_attr.attr));
		if(rc == 0){
			list_add_tail(&(list_info_p->attr_list), &(gDegbuInfo->attr_list));
			#if defined(SHTPS_ENGINEER_BUILD_ENABLE)
				rc = sysfs_chmod_file(parent_kobj_p, &(list_info_p->kobj_attr.attr), (S_IRUGO | S_IWUGO));
			#endif /* SHTPS_ENGINEER_BUILD_ENABLE */
		}else{
			kfree(list_info_p);
		}
	}

	return 0;
}

static void shtps_debug_delete_attr_all(void)
{
	struct shtps_debug_attr_list_info *item_info_p;

	while( list_empty( &(gDegbuInfo->attr_list) ) == 0 ){
		item_info_p = list_entry(gDegbuInfo->attr_list.next, struct shtps_debug_attr_list_info, attr_list);

		if(item_info_p != NULL){
			sysfs_remove_file(item_info_p->parent_kobj_p, &(item_info_p->kobj_attr_p->attr));
			list_del_init(&(item_info_p->attr_list));
			kfree(item_info_p);
		}
	}

	list_del_init(&(gDegbuInfo->attr_list));
}

static int shtps_debug_param_obj_init(struct kobject *shtps_root_kobj)
{
	struct kobject *kobj_p;

	gDegbuInfo = (struct shtps_debug_info*)kmalloc(sizeof(struct shtps_debug_info), GFP_KERNEL);
	if(gDegbuInfo == NULL){
		return -1;
	}

	INIT_LIST_HEAD(&(gDegbuInfo->kobj_list));
	INIT_LIST_HEAD(&(gDegbuInfo->attr_list));

	if(shtps_root_kobj == NULL){
		return -1;
	}

	kobj_p = shtps_debug_create_kobj(shtps_root_kobj, SHTPS_DEBUG_PARAM_DIR_NAME_ROOT);
	if(kobj_p == NULL){
		return -1;
	}

	return 0;
}

static void shtps_debug_param_obj_deinit(void)
{
	shtps_debug_delete_attr_all();
	shtps_debug_delete_kobj_all();
	if(gDegbuInfo != NULL){
		kfree(gDegbuInfo);
		gDegbuInfo = NULL;
	}
}

int shtps_debug_param_add(struct shtps_debug_param_regist_info *info_p)
{
	int i;
	struct kobject *shtps_root_kobj_p = NULL;

	shtps_root_kobj_p = shtps_debug_search_kobj(NULL, SHTPS_DEBUG_PARAM_DIR_NAME_ROOT);

	if(shtps_root_kobj_p == NULL){
		return -1;
	}

	for(i = 0; info_p[i].param_name != NULL; i++)
	{
		struct kobject *parent_kobj_p = NULL;
		struct kobject *param_kobj_p = NULL;
		struct kobj_attribute kobj_attr;

		parent_kobj_p = shtps_debug_search_kobj(shtps_root_kobj_p, info_p[i].parent_name);
		if(parent_kobj_p == NULL){
			parent_kobj_p = shtps_debug_create_kobj(shtps_root_kobj_p, info_p[i].parent_name);
			if(parent_kobj_p == NULL){
				return -1;
			}
		}

		param_kobj_p = shtps_debug_search_kobj(parent_kobj_p, SHTPS_DEBUG_PARAM_DIR_NAME_PARAM);
		if(param_kobj_p == NULL){
			param_kobj_p = shtps_debug_create_kobj(parent_kobj_p, SHTPS_DEBUG_PARAM_DIR_NAME_PARAM);
			if(param_kobj_p == NULL){
				return -1;
			}
		}

		kobj_attr.attr.name = info_p[i].param_name;
		kobj_attr.attr.mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);

		if(info_p[i].show == NULL){
			kobj_attr.show = shtps_debug_common_show;
		}else{
			kobj_attr.show = info_p[i].show;
		}

		if(info_p[i].store == NULL){
			kobj_attr.store = shtps_debug_common_store;
		}else{
			kobj_attr.store = info_p[i].store;
		}

		if( (strcmp(info_p[i].param_name, SHTPS_DEBUG_PARAM_NAME_ONOFF) == 0) ||
			(strcmp(info_p[i].param_name, SHTPS_DEBUG_PARAM_NAME_LOG) == 0) )
		{
			shtps_debug_create_attr(parent_kobj_p, kobj_attr, info_p[i].param_p);
		}else{
			shtps_debug_create_attr(param_kobj_p, kobj_attr, info_p[i].param_p);
		}
	}

	return 0;
}
/* -----------------------------------------------------------------------------------
 */
int shtps_debug_init(struct shtps_debug_init_param *param)
{
	if(!param){
		return -1;
	}

	/*  */
	shtps_debug_param_obj_init(param->shtps_root_kobj);

	#if defined ( SHTPS_TOUCH_EMURATOR_ENABLE )
		memset(&gShtps_touch_emu_info, 0, sizeof(gShtps_touch_emu_info));
		gShtps_touch_emu_info.state = SHTPS_TOUCH_EMURATOR_STATE_DISABLE;
		INIT_DELAYED_WORK(&gShtps_touch_emu_info.touch_emu_read_touchevent_delayed_work, shtps_touch_emu_read_touchevent_delayed_work_function);
		shtps_debug_param_add(emurator_sysfs_regist_info);
	#endif /* #if defined ( SHTPS_TOUCH_EMURATOR_ENABLE ) */

	#if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE )
		memset(&gShtps_touch_eventlog_info, 0, sizeof(gShtps_touch_eventlog_info));
		gShtps_touch_eventlog_info.state = SHTPS_TOUCH_EVENTLOG_STATE_DISABLE;
		shtps_debug_param_add(eventlog_rec_sysfs_regist_info);
	#endif /* #if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE ) */

	#if defined ( SHTPS_ACCUMULATE_FW_REPORT_INFO_ENABLE )
		gShtps_accumulate_fw_report_info_p = kzalloc(sizeof(struct shtps_accumulate_fw_report_info), GFP_KERNEL);
		if(gShtps_accumulate_fw_report_info_p != NULL){
			do_gettimeofday(&gShtps_accumulate_fw_report_info_p->start_timeval);
			shtps_debug_param_add(fw_report_info_sysfs_regist_info);
		}
	#endif /* #if defined ( SHTPS_ACCUMULATE_FW_REPORT_INFO_ENABLE ) */

	#if defined ( SHTPS_DEBUG_MSM_GPIO_ENABLE )
		shtps_debug_param_add(shtps_debug_msm_gpio_info);
	#endif /* #if defined ( SHTPS_DEBUG_MSM_GPIO_ENABLE ) */

	return 0;
}

void shtps_debug_deinit(void)
{
	#if defined ( SHTPS_TOUCH_EMURATOR_ENABLE )
		if(gShtps_touch_emu_info.touch_list != NULL){
			kfree(gShtps_touch_emu_info.touch_list);
		}
	#endif /* #if defined ( SHTPS_TOUCH_EMURATOR_ENABLE ) */

	/*  */
	shtps_debug_param_obj_deinit();
}

void shtps_debug_sleep_enter(void)
{
	#if defined ( SHTPS_TOUCH_EMURATOR_ENABLE )
		if(shtps_touch_emu_is_running() != 0){
			shtps_sysfs_stop_touch_emurator();
		}
		else if(shtps_touch_emu_is_recording() != 0){
			shtps_touch_emu_stop_recording();
		}
	#endif /* #if defined ( SHTPS_TOUCH_EMURATOR_ENABLE ) */

	#if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE )
		if(shtps_touch_eventlog_is_recording() != 0){
			shtps_eventlog_stop_recording();
		}
	#endif /* #if defined ( SHTPS_TOUCH_EVENTLOG_ENABLE ) */
}

#endif /* #if defined( SHTPS_DEVELOP_MODE_ENABLE ) */

MODULE_DESCRIPTION("SHARP TOUCHPANEL DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");
