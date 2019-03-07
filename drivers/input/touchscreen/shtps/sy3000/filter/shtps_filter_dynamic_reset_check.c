/* drivers/input/touchscreen/shtps/sy3000/shtps_filter_dynamic_reset_check.c
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
/* -------------------------------------------------------------------------- */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>

#include <linux/input/shtps_dev.h>

#include "shtps_rmi.h"
#include "shtps_param_extern.h"
#include "shtps_log.h"

#include "shtps_filter.h"
#include "shtps_fwctl.h"
#include "shtps_rmi_devctl.h"
/* -------------------------------------------------------------------------- */
#if defined(SHTPS_DYNAMIC_RESET_CONTROL_ENABLE)

#define WAKEUP_REZERO_PROCESS	0
#define SLEEP_PROCESS			1

/* -------------------------------------------------------------------------- */
static int shtps_filter_dynamic_reset_ready_check(struct shtps_rmi_spi *ts, int process)
{
	int need_reset = 0;

	SHTPS_LOG_FUNC_CALL();
	if(SHTPS_DYNAMIC_RESET_F54_COMMAND_ENABLE != 0)
	{
		u8 buf = 0xFF;

		/* M_READ_FUNC(ts, ts->map.fn54.commandBase, &buf, 1); */
		shtps_fwctl_get_AnalogCMD(ts, &buf);
		if( (buf & 0x06) != 0 ){
			need_reset = 1;
			SHTPS_LOG_ANALYSIS("[dynamic_reset]need reset by f54 status [0x%02X]\n", buf);
		}
	}

	#if defined(SHTPS_DYNAMIC_RESET_ESD_ENABLE)
		if(process == WAKEUP_REZERO_PROCESS){
			u16 ver;
			u8 esd_detect = 0;

			#if defined(SHTPS_DYNAMIC_RESET_ESD_FLG_CHECK_ENABLE)
			{
				u8 buf[2] = {0xFF};
				/* M_READ_FUNC(ts, fc_p->map_p->fn01.dataBase, &buf, 1); */
				shtps_fwctl_get_device_status(ts, &buf[0]);
				if( (buf[0] & 0x03) == 0x03 ){
					need_reset = 2;
					esd_detect = 1;
					SHTPS_LOG_ERR_PRINT("[dynamic_reset] detect ESD_FLG\n");
				}
			}
			#endif /* SHTPS_DYNAMIC_RESET_ESD_FLG_CHECK_ENABLE */

			if(esd_detect == 0){
				#if defined(SHTPS_DYNAMIC_RESET_SPI_ERROR_ENABLE)
					u8 buf = 0x00;

					#if defined(SHTPS_DEF_FWCTL_IC_TYPE_3700)
						;
					#else
					{
						u8 productid_buf[30] = {0};
						shtps_fwctl_get_productid(ts, productid_buf);
						if( (strncmp(productid_buf, SHTPS_FWUPDATE_PRODUCTID, strlen(SHTPS_FWUPDATE_PRODUCTID)) == 0) ||
						    (strncmp(productid_buf, SHTPS_FWUPDATE_PRODUCTID2, strlen(SHTPS_FWUPDATE_PRODUCTID2)) == 0) ){
							;
						}
						else{
							SHTPS_LOG_ERR_PRINT("[dynamic_reset] detect Product ID Error\n");
							need_reset = 2;
						}
					}
					#endif /* SHTPS_DEF_FWCTL_IC_TYPE_3700 */

					ver = shtps_fwver(ts);
					if(ver==0x00 || ver==0xFF || ver==0xFFFF){
						SHTPS_LOG_ERR_PRINT("[dynamic_reset] detect FW Ver=0x%02X\n", ver);
						need_reset = 2;
					}

					if(ts->state_mgr.state == SHTPS_STATE_ACTIVE){
						shtps_fwctl_get_interrupt_enable(ts, &buf);
						if(buf != SHTPS_IRQ_ALL){
							SHTPS_LOG_ERR_PRINT("[dynamic_reset] detect Interrupt Enable Register Error(0x%02X)\n", buf);
							need_reset = 2;
						}
					}

					#if defined(SHTPS_DEF_FWCTL_PROTOCOL_I2C)
						if(shtps_get_i2c_error_detect_flg() == 1){
							SHTPS_LOG_ERR_PRINT("[dynamic_reset] detect i2c transfer error\n");
							need_reset = 2;
						}
					#else
						if(shtps_get_spi_error_detect_flg() == 1){
							SHTPS_LOG_ERR_PRINT("[dynamic_reset] detect spi transfer error\n");
							need_reset = 2;
						}
					#endif /* SHTPS_DEF_FWCTL_PROTOCOL_I2C */
				#else
					ver = shtps_fwver(ts);
					if(ver==0x0000 || ver==0x00FF){
						SHTPS_LOG_ERR_PRINT("[dynamic_reset] detect FW Ver=0x%04X\n", ver);
						need_reset = 2;
					}
				#endif /* SHTPS_DYNAMIC_RESET_SPI_ERROR_ENABLE */
			}
		}
	#endif /* SHTPS_DYNAMIC_RESET_ESD_ENABLE */

	return need_reset;
}

/* -------------------------------------------------------------------------- */
static int shtps_filter_dynamic_reset_process(struct shtps_rmi_spi *ts, int process)
{
	int rc = 0;

	shtps_mutex_lock_ctrl();
	rc = shtps_filter_dynamic_reset_ready_check(ts, process);
	shtps_mutex_unlock_ctrl();

	#if defined(SHTPS_DYNAMIC_RESET_ESD_ENABLE)
	if(rc == 2){
		SHTPS_LOG_ERR_PRINT("[ESD_regulator_reset] start\n");
		#if defined(SHTPS_POWER_OFF_IN_SLEEP_ENABLE)
			shtps_shutdown(ts);

			msleep(SHTPS_ESD_REGULATOR_RESET_SLEEPTIME);

			#if defined(SHTPS_DEF_FWCTL_PROTOCOL_I2C)
				shtps_clr_i2c_error_detect_flg();
			#else
				shtps_clr_spi_error_detect_flg();
			#endif /* SHTPS_DEF_FWCTL_PROTOCOL_I2C */

			shtps_start(ts);
			shtps_wait_startup(ts);
		#else
			shtps_irq_disable(ts);
			shtps_esd_regulator_get();
			shtps_esd_regulator_reset();
			shtps_esd_regulator_put();

			#if defined(SHTPS_DEF_FWCTL_PROTOCOL_I2C)
				shtps_clr_i2c_error_detect_flg();
			#else
				shtps_clr_spi_error_detect_flg();
			#endif /* SHTPS_DEF_FWCTL_PROTOCOL_I2C */

			shtps_shutdown(ts);
			shtps_start(ts);
			shtps_wait_startup(ts);
		#endif /* SHTPS_POWER_OFF_IN_SLEEP_ENABLE */
		SHTPS_LOG_ERR_PRINT("[ESD_regulator_reset] end\n");
	}else if(rc == 1){
	#else
	if(rc != 0){
	#endif /* SHTPS_DYNAMIC_RESET_ESD_ENABLE */
		SHTPS_LOG_ANALYSIS("[dynamic_reset] start\n");
		shtps_shutdown(ts);
		shtps_start(ts);
		shtps_wait_startup(ts);
		SHTPS_LOG_ANALYSIS("[dynamic_reset] end\n");
	}

	return rc;
}

/* -------------------------------------------------------------------------- */
void shtps_filter_dynamic_reset_sleep_process(struct shtps_rmi_spi *ts)
{
	SHTPS_LOG_FUNC_CALL();
	shtps_filter_dynamic_reset_process(ts, SLEEP_PROCESS);
}

/* -------------------------------------------------------------------------- */
int shtps_filter_dynamic_reset_wakeup_rezero_process(struct shtps_rmi_spi *ts)
{
	int ret = 0;

	SHTPS_LOG_FUNC_CALL();
	ret = shtps_filter_dynamic_reset_process(ts, WAKEUP_REZERO_PROCESS);

	return ret;
}

/* -------------------------------------------------------------------------- */
#endif /* SHTPS_DYNAMIC_RESET_CONTROL_ENABLE */
