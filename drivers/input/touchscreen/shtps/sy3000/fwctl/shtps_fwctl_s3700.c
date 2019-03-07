/* drivers/input/touchscreen/shtps/sy3000/shtps_fwctl_s3700.c
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
#include <linux/jiffies.h>
#include <linux/slab.h>

#include <linux/input/shtps_dev.h>

#include "shtps_rmi.h"
#include "shtps_rmi_sub.h"
#include "shtps_log.h"

#include "shtps_fwctl.h"
#include "shtps_fwctl_s3700.h"
#include "shtps_param_extern.h"

/* -------------------------------------------------------------------------- */
#if defined( SHTPS_LOG_ERROR_ENABLE )
	#define SHTPS_LOG_FWCTL_FUNC_CALL()		// SHTPS_LOG_FUNC_CALL()
#else
	#define SHTPS_LOG_FWCTL_FUNC_CALL()
#endif

#define SHTPS_READ_SERIAL_WAIT_US		10
#define SHTPS_SERIAL_NUMBER_ALL_SIZE	0x58

/* -------------------------------------------------------------------------- */
#if defined( SHTPS_LOG_DEBUG_ENABLE )
	static char sensor_log_tmp[16];
	static char sensor_log_outstr[256];
#endif /* SHTPS_LOG_DEBUG_ENABLE */

/* -------------------------------------------------------------------------- */
static struct rmi_map* shtps_fwctl_s3700_ic_init(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_init_writeconfig(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_cmd_get_partition_table(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_get_partition_table(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_get_config_blocknum(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_get_firm_blocknum(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_get_blocksize(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_get_result_writeconfig(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_get_result_writeimage(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_get_result_erase(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_write_config(struct shtps_fwctl_info *fc_p, u8 *fwdata_p, int blockSize);
static int shtps_fwctl_s3700_loader_write_image(struct shtps_fwctl_info *fc_p, u8 *fwdata_p, int blockSize);
static int shtps_fwctl_s3700_loader_cmd(struct shtps_fwctl_info *fc_p, u8 cmd, u8 isLockdown);
static int shtps_fwctl_s3700_loader_cmd_erase(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_cmd_erase_config(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_cmd_writeimage(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_cmd_writeconfig(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_cmd_enterbl(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_loader_exit(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_check_crc_error(struct shtps_fwctl_info *fc_p, u8 *status_p);
static int shtps_fwctl_s3700_get_device_status(struct shtps_fwctl_info *fc_p, u8 *status_p);
static int shtps_fwctl_s3700_soft_reset(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_irqclear_get_irqfactor(struct shtps_fwctl_info *fc_p, u8 *status_p);
static int shtps_fwctl_s3700_rezero(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_get_pageselect(struct shtps_fwctl_info *fc_p, u8 *buf);
static int shtps_fwctl_s3700_map_construct(struct shtps_fwctl_info *fc_p, int func_check);
static int shtps_fwctl_s3700_is_sleeping(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_set_doze(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_set_doze_param(struct shtps_fwctl_info *fc_p, u8 *param_p, u8 param_size);
static int shtps_fwctl_s3700_set_active(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_set_sleepmode_on(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_set_sleepmode_off(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_set_lpwg_mode_on(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_set_lpwg_mode_off(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_set_lpwg_mode_cal(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_get_fingermax(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_get_fingerinfo(struct shtps_fwctl_info *fc_p, u8 *buf_p, int read_cnt, u8 *irqsts_p, u8 *extsts_p, u8 **finger_pp);
static int shtps_fwctl_s3700_get_one_fingerinfo(struct shtps_fwctl_info *fc_p, int id, u8 *buf_p, u8 **finger_pp);
static u8* shtps_fwctl_s3700_get_finger_info_buf(struct shtps_fwctl_info *fc_p, int fingerid, int fingerMax, u8 *buf_p);
static int shtps_fwctl_s3700_get_finger_state(struct shtps_fwctl_info *fc_p, int fingerid, int fingerMax, u8 *buf_p);
static int shtps_fwctl_s3700_get_finger_pos_x(struct shtps_fwctl_info *fc_p, u8 *buf_p);
static int shtps_fwctl_s3700_get_finger_pos_y(struct shtps_fwctl_info *fc_p, u8 *buf_p);
static int shtps_fwctl_s3700_get_finger_wx(struct shtps_fwctl_info *fc_p, u8 *buf_p);
static int shtps_fwctl_s3700_get_finger_wy(struct shtps_fwctl_info *fc_p, u8 *buf_p);
static int shtps_fwctl_s3700_get_finger_z(struct shtps_fwctl_info *fc_p, u8 *buf_p);
static void shtps_fwctl_s3700_get_gesture(struct shtps_fwctl_info *fc_p, int fingerMax, u8 *buf_p, u8 *gs1_p, u8 *gs2_p);
static int shtps_fwctl_s3700_get_gesturetype(struct shtps_fwctl_info *fc_p, u8 *status_p);
static int shtps_fwctl_s3700_get_fwdate(struct shtps_fwctl_info *fc_p, u8 *year_p, u8 *month_p);
static int shtps_fwctl_s3700_get_serial_number(struct shtps_fwctl_info *fc_p, u8 *buf_p);
static int shtps_fwctl_s3700_get_fwver(struct shtps_fwctl_info *fc_p, u16 *ver_p);
static int shtps_fwctl_s3700_get_tm_mode(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_get_tm_rxsize(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_get_tm_txsize(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_get_tm_frameline(struct shtps_fwctl_info *fc_p, u8 tm_mode, u8 *tm_data_p);
static int shtps_fwctl_s3700_get_tm_baseline(struct shtps_fwctl_info *fc_p, u8 tm_mode, u8 *tm_data_p);
static int shtps_fwctl_s3700_get_tm_baseline_raw(struct shtps_fwctl_info *fc_p, u8 tm_mode, u8 *tm_data_p);
static int shtps_fwctl_s3700_cmd_tm_frameline(struct shtps_fwctl_info *fc_p, u8 tm_mode);
static int shtps_fwctl_s3700_cmd_tm_baseline(struct shtps_fwctl_info *fc_p, u8 tm_mode);
static int shtps_fwctl_s3700_cmd_tm_baseline_raw(struct shtps_fwctl_info *fc_p, u8 tm_mode);
static int shtps_fwctl_s3700_initparam(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_initparam_activemode(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_initparam_dozemode(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_initparam_lpwgmode(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_start_testmode(struct shtps_fwctl_info *fc_p, u8 tm_mode);
static int shtps_fwctl_s3700_stop_testmode(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_baseline_offset_disable(struct shtps_fwctl_info *fc_p);
static void shtps_fwctl_s3700_set_dev_state(struct shtps_fwctl_info *fc_p, u8 state);
static u8 shtps_fwctl_s3700_get_dev_state(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_get_maxXPosition(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_get_maxYPosition(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_get_AnalogCMD(struct shtps_fwctl_info *fc_p, u8 *buf_p);
static int shtps_fwctl_s3700_get_ObjectAttention(struct shtps_fwctl_info *fc_p, u8 *buf_p);
static int shtps_fwctl_s3700_get_lpwg_def_settings(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_command_force_update(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_command_force_cal(struct shtps_fwctl_info *fc_p);
static int shtps_fwctl_s3700_f54_command_completion_wait(struct shtps_fwctl_info *fc_p, int max, int interval);
static int shtps_fwctl_s3700_set_custom_report_rate(struct shtps_fwctl_info *fc_p, u8 rate);
static int shtps_fwctl_s3700_set_lpwg_sweep_on(struct shtps_fwctl_info *fc_p, u8 enable);
static int shtps_fwctl_s3700_set_lpwg_double_tap(struct shtps_fwctl_info *fc_p, u8 enable);

/* -------------------------------------------------------------------------- */
static struct rmi_map* shtps_fwctl_s3700_ic_init(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return kzalloc(sizeof(struct rmi_map), GFP_KERNEL);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_init_writeconfig(struct shtps_fwctl_info *fc_p)
{
	u8 data[2];

	SHTPS_LOG_FWCTL_FUNC_CALL();
	data[0] = 0;
	data[1] = 0;
	return M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 2, data, 2);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_cmd_get_partition_table(struct shtps_fwctl_info *fc_p)
{
	u8 data[6];
	int rc;
	u8 buf;
	
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn34.dataBase, &buf, 1);
	if(rc) {
		goto err_exit;
	} else if((0x80 && buf) == 0) {
		rc = 1;
		goto err_exit;
	}
	
	memset(data, 0, sizeof(data));
	data[0] = PARTITION_ID_FLASHCONFIG;
	data[3] = PARTITION_TABLE_READ_SIZE;
	data[4] = 0x00;
	data[5] = FLASH_COMMAND_READ;
	
	rc = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 1, data, 6);
	if(rc) {
		goto err_exit;
	}
	
	return 0;
	
err_exit:
	return rc;
}
/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_get_partition_table(struct shtps_fwctl_info *fc_p)
{
	u8 partition_table[PARTITION_TABLE_READ_SIZE * F34_QUERY_BLOCKSIZE(fc_p->map_p->fn34.query.data)];
	int i;
	int rc;
	int localCoreCodeLength;
	int localCoreConfigLength;
	
	SHTPS_LOG_FWCTL_FUNC_CALL();
	localCoreCodeLength = 0;
	localCoreConfigLength = 0;
	
	rc = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 5, partition_table, (PARTITION_TABLE_READ_SIZE * F34_QUERY_BLOCKSIZE(fc_p->map_p->fn34.query.data)));
	if(rc) {
		goto err_exit;
	}
	
	for(i = 0; ((8 * i) + 2) < (PARTITION_TABLE_READ_SIZE * F34_QUERY_BLOCKSIZE(fc_p->map_p->fn34.query.data)); i++) {
		if(partition_table[(8 * i) + 2] == PARTITION_ID_CORECODE) {
			localCoreCodeLength = partition_table[(8 * i) + 4];
			localCoreCodeLength = localCoreCodeLength | (0xFF00 & (partition_table[(8 * i) + 5] << 8));
			fc_p->map_p->fn34.pl.coreCodeLength = localCoreCodeLength;
		}
		if(partition_table[(8 * i) + 2] == PARTITION_ID_CORECONFIG) {
			localCoreConfigLength = partition_table[(8 * i) + 4];
			localCoreConfigLength = localCoreConfigLength | (0xFF00 & (partition_table[(8 * i) + 5] << 8));
			fc_p->map_p->fn34.pl.coreConfigLength = localCoreConfigLength;
		}
	}
	
	return 0;
	
err_exit:
	return rc;
}
/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_get_config_blocknum(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return fc_p->map_p->fn34.pl.coreConfigLength;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_get_firm_blocknum(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return fc_p->map_p->fn34.pl.coreCodeLength;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_get_blocksize(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return F34_QUERY_BLOCKSIZE(fc_p->map_p->fn34.query.data);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_get_result_writeconfig(struct shtps_fwctl_info *fc_p)
{
	int rc;
	u8 status;

	SHTPS_LOG_FWCTL_FUNC_CALL();

	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn34.dataBase, &status, 1);
	status = status & 0x1f;

	if(status != FLASH_STATUS_SUCCESS){
		SHTPS_LOG_DBG_PRINT("%s:%d,%d\n", __func__, rc, status);
		return -1;
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_get_result_writeimage(struct shtps_fwctl_info *fc_p)
{
	int rc;
	u8 status;

	SHTPS_LOG_FWCTL_FUNC_CALL();

	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn34.dataBase, &status, 1);
	status = status & 0x1f;

	if(status != FLASH_STATUS_SUCCESS){
		SHTPS_LOG_DBG_PRINT("%s:%d,%d\n", __func__, rc, status);
		return -1;
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_get_result_erase(struct shtps_fwctl_info *fc_p)
{
	int rc;
	u8 status;
	
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn34.dataBase, &status, 1);
	if(rc) {
		return rc;
	}
	if(status == FLASH_STATUS_NOT_ERASED) {
		SHTPS_LOG_DBG_PRINT("%s:%d,%d\n", __func__, rc, status);
		return -1;
	}
	
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_write_config(struct shtps_fwctl_info *fc_p, u8 *fwdata_p, int blockSize)
{
	int i;
	int rc;
	u8 data[3];
	u8 tx_buf[blockSize];
	
	SHTPS_LOG_FWCTL_FUNC_CALL();
	
	data[0] = PARTITION_ID_CORECONFIG;
	
	rc = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 1, data, 1);
	if(rc) {
		goto err_exit;
	}
	
	data[0] = FLASH_BLOCK_SIZE;
	data[1] = 0;
	data[2] = FLASH_COMMAND_WRITE;
	rc = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 3, data, 3);
	if(rc) {
		goto err_exit;
	}
	
	for(i = 0; i < blockSize; i++){
		tx_buf[i] = fwdata_p[i];
	}
	
	return M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 5, tx_buf, F34_QUERY_BLOCKSIZE(fc_p->map_p->fn34.query.data));

err_exit:
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_write_image(struct shtps_fwctl_info *fc_p, u8 *fwdata_p, int blockSize)
{
	int i;
	int rc;
	u8 data[3];
	u8 tx_buf[blockSize];
	
	SHTPS_LOG_FWCTL_FUNC_CALL();
	
	data[0] = PARTITION_ID_CORECODE;
	
	rc = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 1, data, 1);
	if(rc) {
		goto err_exit;
	}
	
	data[0] = FLASH_BLOCK_SIZE;
	data[1] = 0;
	data[2] = FLASH_COMMAND_WRITE;
	rc = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 3, data, 3);
	if(rc) {
		goto err_exit;
	}
	
	for(i = 0; i < blockSize; i++){
		tx_buf[i] = fwdata_p[i];
	}
	
	return M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 5, tx_buf, F34_QUERY_BLOCKSIZE(fc_p->map_p->fn34.query.data));
	
err_exit:
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_cmd(struct shtps_fwctl_info *fc_p, u8 cmd, u8 isLockdown)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_cmd_erase(struct shtps_fwctl_info *fc_p)
{
	int rc;
	u8 data[8];
	SHTPS_LOG_FWCTL_FUNC_CALL();

	memset(data, 0, sizeof(data));
	data[0] = PARTITION_ID_CORECODE;
	data[5] = FLASH_COMMAND_ERASE;
	data[6] = F34_QUERY_BOOTLOADERID0(fc_p->map_p->fn34.query.data);
	data[7] = F34_QUERY_BOOTLOADERID1(fc_p->map_p->fn34.query.data);
	
	rc = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 1, data, 8);
	if(rc) {
		goto err_exit;
	}
	
err_exit:
	return rc;
}
/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_cmd_erase_config(struct shtps_fwctl_info *fc_p)
{
	int rc;
	u8 data[8];
	SHTPS_LOG_FWCTL_FUNC_CALL();
	
	memset(data, 0, sizeof(data));
	data[0] = PARTITION_ID_CORECONFIG;
	data[5] = FLASH_COMMAND_ERASE;
	data[6] = F34_QUERY_BOOTLOADERID0(fc_p->map_p->fn34.query.data);
	data[7] = F34_QUERY_BOOTLOADERID1(fc_p->map_p->fn34.query.data);
	
	rc = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 1, data, 8);
	if(rc) {
		goto err_exit;
	}
	
err_exit:
	return rc;
}
/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_cmd_writeimage(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_cmd_writeconfig(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_cmd_enterbl(struct shtps_fwctl_info *fc_p)
{
	u8 data[8];
	SHTPS_LOG_FWCTL_FUNC_CALL();

	memset(data, 0, sizeof(data));
	data[0] = PARTITION_ID_BOOTLOADER;
	data[5] = FLASH_COMMAND_ENTER_BOOTLOADER;
	data[6] = F34_QUERY_BOOTLOADERID0(fc_p->map_p->fn34.query.data);
	data[7] = F34_QUERY_BOOTLOADERID1(fc_p->map_p->fn34.query.data);
	
	return M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 1, data, 8);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_loader_exit(struct shtps_fwctl_info *fc_p)
{
	int rc;
	u8 status = 0;
	
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = shtps_fwctl_s3700_soft_reset(fc_p);
	if(rc == 0)	{
		msleep(SHTPS_RESET_BOOTLOADER_WAIT_MS);
		rc = shtps_fwctl_s3700_check_crc_error(fc_p, &status);
		if(rc == 0 && status != 0) {
			rc = (-1);
		}
	}
	return rc;
}
/* -------------------------------------------------------------------------- */

static int shtps_fwctl_s3700_check_crc_error(struct shtps_fwctl_info *fc_p, u8 *status_p)
{
	int rc;
	u8 status = 0;

	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn34.dataBase, &status, 1);
	if(rc == 0)	{
		if((status & 0x80) == 0x80) {
			if((status & 0x0F) == 0x09) {
				SHTPS_LOG_DBG_PRINT("%s() error operation status = 0x%02x\n", __func__, (status & 0x0F));
				*status_p = 1;
			}
			else {
				*status_p = 0;
			}
		}
		else {
			rc = shtps_fwctl_s3700_get_device_status(fc_p, &status);
			if( (rc == 0) && (
				((status & 0x40) != 0) ||
				((status & 0x0F) == 4) ||
				((status & 0x0F) == 5) ||
				((status & 0x0F) == 6)) ){
				SHTPS_LOG_DBG_PRINT("%s() error device status = 0x%02x\n", __func__, (status & 0x0F));
				*status_p = 1;
			}
			else {
				*status_p = 0;
			}
		}
	}
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_device_status(struct shtps_fwctl_info *fc_p, u8 *status_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return M_READ_FUNC(fc_p, fc_p->map_p->fn01.dataBase, status_p, 1);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_soft_reset(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.commandBase, 0x01);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_irqclear_get_irqfactor(struct shtps_fwctl_info *fc_p, u8 *status_p)
{
	int rc;
	u8 buf[2]={0,0};

	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn01.dataBase, buf, 2);
	if(rc == 0){
		*status_p = buf[1];
	}
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_rezero(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	shtps_fwctl_s3700_command_force_cal(fc_p);
	shtps_fwctl_s3700_f54_command_completion_wait(fc_p, SHTPS_F54_COMMAND_WAIT_POLL_COUNT, SHTPS_F54_COMMAND_WAIT_POLL_INTERVAL);

	return 0;	// no error check
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_pageselect(struct shtps_fwctl_info *fc_p, u8 *buf)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return M_READ_FUNC(fc_p, SHTPS_PAGESELECT_ADR, buf, 1);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_map_construct(struct shtps_fwctl_info *fc_p, int func_check)
{
	struct rmi_pdt	pdt;
	int				i;
	int				rc;
	int				page;
	int				page_max;
	u8				maxFinger[2];
	u8				maxPosition[4];
	char			productID[11];

	SHTPS_LOG_FWCTL_FUNC_CALL();

	msleep(3);
	memset(fc_p->map_p, 0, sizeof(struct rmi_map));

	if(fc_p->dev_state == SHTPS_DEV_STATE_LOADER){
		page_max = 0;
	}
	else{
		page_max = SHTPS_PDT_PAGE_SIZE_MAX;
	}

	/* Read the PDT */
	for(page = 0; page <= page_max; page++){
		rc = M_WRITE_FUNC(fc_p, 0xFF, page);
		if(rc)	goto err_exit;
		for(i = 0xE9;i > 0x0a;i-=sizeof(pdt)){
			rc = M_READ_FUNC(fc_p, ((page & 0x0f) << 8) | i, (u8*)&pdt, sizeof(pdt));
			if(rc)	goto err_exit;

			if(!pdt.functionNumber){
				/** End of PDT */
				break;
			}

			SHTPS_LOG_DBG_PRINT("\n");
			switch(pdt.functionNumber){
			case 0x01:
				SHTPS_LOG_DBG_PRINT("Found: RMI Device Control\n");
				fc_p->map_p->fn01.enable		= 1;
				fc_p->map_p->fn01.queryBase  = ((page & 0x0f) << 8) | pdt.queryBaseAddr;
				fc_p->map_p->fn01.ctrlBase   = ((page & 0x0f) << 8) | pdt.controlBaseAddr;
				fc_p->map_p->fn01.dataBase   = ((page & 0x0f) << 8) | pdt.dataBaseAddr;
				fc_p->map_p->fn01.commandBase= ((page & 0x0f) << 8) | pdt.commandBaseAddr;

				rc = M_READ_FUNC(fc_p, fc_p->map_p->fn01.queryBase,
									fc_p->map_p->fn01.query.data, sizeof(fc_p->map_p->fn01.query.data));
				if(rc)	goto err_exit;

				memcpy(productID, &F01_QUERY_PRODUCTID(fc_p->map_p->fn01.query.data), sizeof(productID));
				productID[10] = '\0';
				SHTPS_LOG_DBG_PRINT("F01 Query Data\n");
				SHTPS_LOG_DBG_PRINT("-------------------------------------------------\n");
				SHTPS_LOG_DBG_PRINT("Manufacturer ID       : 0x%02x\n", F01_QUERY_MANUFACTURERID(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("CutomMap              : 0x%02x\n", F01_QUERY_CUSTOMMAP(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("NonCompliant          : 0x%02x\n", F01_QUERY_NONCOMPLIANT(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("HasSensorID           : 0x%02x\n", F01_QUERY_HASSENSORID(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("HasAdjustableDoze     : 0x%02x\n", F01_QUERY_HASAJUSTABLEDOZE(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("HasAdjDozeHoldoff     : 0x%02x\n", F01_QUERY_HASADJDOZEHOLDOFF(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("HasProductProperties2 : 0x%02x\n", F01_QUERY_HASPRODUCTPROPERTIES2(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("productInfo0          : 0x%02x\n", F01_QUERY_PRODUCTINFO0(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("productInfo1          : 0x%02x\n", F01_QUERY_PRODUCTINFO1(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("Year                  : 0x%02x\n", F01_QUERY_DATECODEYEAR(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("Month                 : 0x%02x\n", F01_QUERY_DATECODEMONTH(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("Day                   : 0x%02x\n", F01_QUERY_DATECODEDAY(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("CP1                   : 0x%02x\n", F01_QUERY_CP1(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("CP2                   : 0x%02x\n", F01_QUERY_CP2(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("WaferLotID0           : 0x%04x\n", F01_QUERY_WAFERLOTID0(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("WaferLotID1           : 0x%04x\n", F01_QUERY_WAFERLOTID1(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("WaferLotID2           : 0x%02x\n", F01_QUERY_WAFERLOTID2(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("ProductID             : %s\n", productID);
				SHTPS_LOG_DBG_PRINT("HasDS4Queries         : 0x%02x\n", F01_QUERY_HASDS4QUERIES(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("DS4 Length            : 0x%02x\n", F01_QUERY_DS4_LENGTH(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("DS4 HasPackageQuery   : 0x%02x\n", F01_QUERY_DS4_HASPACKAGEIDQUERY(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("DS4 HasPackratQuery   : 0x%02x\n", F01_QUERY_DS4_HASPACKRATQUERY(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("DS4 HasResetQuery     : 0x%02x\n", F01_QUERY_DS4_HASRESETQUERY(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("DS4 HasMaskRevQuery   : 0x%02x\n", F01_QUERY_DS4_HASMASKREVQUERY(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("DS4 HasI2CControl     : 0x%02x\n", F01_QUERY_DS4_HASI2CCONTROL(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("DS4 HasSPIControl     : 0x%02x\n", F01_QUERY_DS4_HASSPICONTROL(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("DS4 HasATTNControl    : 0x%02x\n", F01_QUERY_DS4_HASATTNCONTROL(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("DS4 HasToolIDPacketQuery             : 0x%02x\n", F01_QUERY_DS4_HASTOOLIDPACKETQUERY(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("DS4 HasFirmwareRevisionIdPacketQuery : 0x%02x\n", F01_QUERY_DS4_HASFIRMWAREREVISIONIDPACKETQUERY(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("RESET Enabled         : 0x%02x\n", F01_QUERY_RESET_ENABLED(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("RESET Polarity        : 0x%02x\n", F01_QUERY_RESET_POLARITY(fc_p->map_p->fn01.query.data));
				SHTPS_LOG_DBG_PRINT("RESET Pull-upEnabled  : 0x%02x\n", F01_QUERY_RESET_PULLUPENABLED(fc_p->map_p->fn01.query.data));
				break;

			case 0x05:
				SHTPS_LOG_DBG_PRINT("Found: Image Reporting\n");
				fc_p->map_p->fn05.enable		= 1;
				fc_p->map_p->fn05.queryBase  = ((page & 0x0f) << 8) | pdt.queryBaseAddr;
				fc_p->map_p->fn05.ctrlBase   = ((page & 0x0f) << 8) | pdt.controlBaseAddr;
				fc_p->map_p->fn05.dataBase   = ((page & 0x0f) << 8) | pdt.dataBaseAddr;
				fc_p->map_p->fn05.commandBase= ((page & 0x0f) << 8) | pdt.commandBaseAddr;

				rc = M_READ_FUNC(fc_p, fc_p->map_p->fn05.queryBase,
									fc_p->map_p->fn05.query.data, sizeof(fc_p->map_p->fn05.query.data));
				if(rc)	goto err_exit;

				SHTPS_LOG_DBG_PRINT("F05 Query Data\n");
				SHTPS_LOG_DBG_PRINT("-------------------------------------------------\n");
				SHTPS_LOG_DBG_PRINT("NumberOfReceiverElectrodes    : 0x%02x\n", F05_QUERY_NUMOFRCVEL(fc_p->map_p->fn05.query.data));
				SHTPS_LOG_DBG_PRINT("NumberOfTransmitterElectrodes : 0x%02x\n", F05_QUERY_NUMOFTRANSEL(fc_p->map_p->fn05.query.data));
				SHTPS_LOG_DBG_PRINT("Has15bitDelta                 : 0x%02x\n", F05_QUERY_HAS16BIT(fc_p->map_p->fn05.query.data));
				SHTPS_LOG_DBG_PRINT("SizeOfF05ImageWindow          : 0x%02x\n", F05_QUERY_IMAGEWINDOWSIZE(fc_p->map_p->fn05.query.data));
				break;

			case 0x11:
				SHTPS_LOG_DBG_PRINT("Found: 2-D Sensors\n");
				fc_p->map_p->fn11.enable		= 1;
				fc_p->map_p->fn11.queryBase  = ((page & 0x0f) << 8) | pdt.queryBaseAddr;
				fc_p->map_p->fn11.ctrlBase   = ((page & 0x0f) << 8) | pdt.controlBaseAddr;
				fc_p->map_p->fn11.dataBase   = ((page & 0x0f) << 8) | pdt.dataBaseAddr;
				fc_p->map_p->fn11.commandBase= ((page & 0x0f) << 8) | pdt.commandBaseAddr;

				rc = M_READ_FUNC(fc_p, fc_p->map_p->fn11.queryBase,
									fc_p->map_p->fn11.query.data, sizeof(fc_p->map_p->fn11.query.data));
				if(rc)	goto err_exit;

				rc = M_READ_FUNC(fc_p, fc_p->map_p->fn11.ctrlBase + 0x06, maxPosition, 4);
				if(rc)	goto err_exit;

				fc_p->map_p->fn11.ctrl.maxXPosition = maxPosition[0] | ((maxPosition[1] & 0x0F) << 0x08);
				fc_p->map_p->fn11.ctrl.maxYPosition = maxPosition[2] | ((maxPosition[3] & 0x0F) << 0x08);

				SHTPS_LOG_DBG_PRINT("F11 Query Data\n");
				SHTPS_LOG_DBG_PRINT("-------------------------------------------------\n");
				SHTPS_LOG_DBG_PRINT("NumberOf2DSensors              : 0x%02x\n", F11_QUERY_NUMOFSENSORS(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasQuery9                      : 0x%02x\n", F11_QUERY_HASQUERY9(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasQuery11                     : 0x%02x\n", F11_QUERY_HASQUERY11(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasQuery12                     : 0x%02x\n", F11_QUERY_HASQUERY12(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("NumberOfFingers                : 0x%02x\n", F11_QUERY_NUMOFFINGERS(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasRelMode                     : 0x%02x\n", F11_QUERY_HASREL(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasAbsMode                     : 0x%02x\n", F11_QUERY_HASABS(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasGestures                    : 0x%02x\n", F11_QUERY_HASGESTURES(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasSensitivityAdjust           : 0x%02x\n", F11_QUERY_HASSENSADJUST(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("Configurable                   : 0x%02x\n", F11_QUERY_CONFIGURABLE(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("NumberOfXElectrodes            : 0x%02x\n", F11_QUERY_NUMOFXELEC(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("NumberOfYElectrodes            : 0x%02x\n", F11_QUERY_NUMOFYELEC(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("MaximumElectrodes              : 0x%02x\n", F11_QUERY_MAXELEC(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("AbsoluteDataSize               : 0x%02x\n", F11_QUERY_ABSOLUTEDATASIZE(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("AnchoredFinger                 : 0x%02x\n", F11_QUERY_ANCHOREDFINGER(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasDribble                     : 0x%02x\n", F11_QUERY_HASDRIBBLE(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasBendingCorrection           : 0x%02x\n", F11_QUERY_HASBENDINGCORRECTION(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasLargeObjectSuppression      : 0x%02x\n", F11_QUERY_HASLARGEOBJECTSUPPRESSION(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasJitterFilter                : 0x%02x\n", F11_QUERY_HASJITTERFILTER(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasPen                         : 0x%02x\n", F11_QUERY_HASPEN(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasFingerProximity             : 0x%02x\n", F11_QUERY_HASFINGERPROXIMITY(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasLargeObjectSensitivity      : 0x%02x\n", F11_QUERY_HASLARGEOBJECTSENSITIVITY(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasTwoPenThresholds            : 0x%02x\n", F11_QUERY_HASTWOPENTHRESHOLDS(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasPenHoverDiscrimination      : 0x%02x\n", F11_QUERY_HASPENHOVERDISCRIMINATION(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasNewPenRegisters             : 0x%02x\n", F11_QUERY_HASNEWPENREGISTERS(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasZTuning                     : 0x%02x\n", F11_QUERY_HASZTUNING(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasPositionInterpolationTuning : 0x%02x\n", F11_QUERY_HASPOSITIONIPTUNING(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasWTuning                     : 0x%02x\n", F11_QUERY_HASWTUNING(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasPitchInfo                   : 0x%02x\n", F11_QUERY_HASPITCHINFO(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasDefaultFingerWidth          : 0x%02x\n", F11_QUERY_HASDEFAULTFINGERWIDTH(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasSegmentationAggressiveness  : 0x%02x\n", F11_QUERY_HASSEGAGGRESSIVENESS(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasTxRxClip                    : 0x%02x\n", F11_QUERY_HASTXRXCLIP(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("HasDrummingCorrection          : 0x%02x\n", F11_QUERY_HASDRUMMINGCORRECTION(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("Has8bitW                       : 0x%02x\n", F11_QUERY_HAS8BITW(fc_p->map_p->fn11.query.data));
				SHTPS_LOG_DBG_PRINT("Has2DAjustableMapping          : 0x%02x\n", F11_QUERY_HAS2DAJSTMAPPING(fc_p->map_p->fn11.query.data));
				break;

			case 0x12:
				SHTPS_LOG_DBG_PRINT("Found: 2-D Sensors\n");
				fc_p->map_p->fn12.enable		= 1;
				fc_p->map_p->fn12.queryBase  = ((page & 0x0f) << 8) | pdt.queryBaseAddr;
				fc_p->map_p->fn12.ctrlBase   = ((page & 0x0f) << 8) | pdt.controlBaseAddr;
				fc_p->map_p->fn12.dataBase   = ((page & 0x0f) << 8) | pdt.dataBaseAddr;
				fc_p->map_p->fn12.commandBase= ((page & 0x0f) << 8) | pdt.commandBaseAddr;

				{
					int j, k;
					u8 presence_size;
					u8 subpacket[0xFF];
					u16 base_addr;
					u8 reg_type_num;

					for(j = 0; j < sizeof(fc_p->map_p->fn12.query.data); j++){
						rc = M_READ_FUNC(fc_p, fc_p->map_p->fn12.queryBase + j, &fc_p->map_p->fn12.query.data[j], 1);
						DEVICE_ERR_CHECK(rc, err_exit);
					}

					presence_size = 0x00;
					memset(subpacket, 0, sizeof(subpacket));
					reg_type_num = (sizeof(fc_p->map_p->fn12.ctrl.num) / sizeof(fc_p->map_p->fn12.ctrl.num[0]));
					base_addr = fc_p->map_p->fn12.ctrlBase;

					rc = M_READ_FUNC(fc_p, fc_p->map_p->fn12.queryBase + 4, &presence_size, 1);
					DEVICE_ERR_CHECK(rc, err_exit);
					rc = M_READ_FUNC(fc_p, fc_p->map_p->fn12.queryBase + 5, subpacket, presence_size);
					DEVICE_ERR_CHECK(rc, err_exit);

					if(presence_size != 0){
						presence_size--;
					}

					for(j = 0; j < presence_size; j++){
						for(k = 0; k < 8; k++){
							if( ((j * 8) + k) < reg_type_num){
								if( (subpacket[j + 1] & (1 << k)) != 0){
									fc_p->map_p->fn12.ctrl.num[(j * 8) + k].enable = 1;
									fc_p->map_p->fn12.ctrl.num[(j * 8) + k].addr = base_addr;
									base_addr++;
								}else{
									fc_p->map_p->fn12.ctrl.num[(j * 8) + k].enable = 0;
									fc_p->map_p->fn12.ctrl.num[(j * 8) + k].addr = 0x00;
								}
							}
						}
					}

					{
						u8 buff_num = 0;
						presence_size = subpacket[0];
						rc = M_READ_FUNC(fc_p, fc_p->map_p->fn12.queryBase + 6, subpacket, presence_size);
						DEVICE_ERR_CHECK(rc, err_exit);

						for(j = 0; j < reg_type_num; j++)
						{
							u8 sub_num = 0;

							if(fc_p->map_p->fn12.ctrl.num[j].enable != 0){
								fc_p->map_p->fn12.ctrl.num[j].sub_size = subpacket[buff_num];
								buff_num++;

								for(k = 0; k < 7; k++){
									fc_p->map_p->fn12.ctrl.num[j].sub_enable[sub_num] = ((subpacket[buff_num] >> k) & 0x01);
									sub_num++;
								}
								while(buff_num < presence_size){
									if((subpacket[buff_num] & 0x80) == 0){
										buff_num++;
										break;
									}
									else{
										buff_num++;
										for(k = 0; k < 7; k++){
											fc_p->map_p->fn12.ctrl.num[j].sub_enable[sub_num] = ((subpacket[buff_num] >> k) & 0x01);
											sub_num++;
										}
									}
								}
							}
						}
					}

					/* ----- */
					presence_size = 0x00;
					memset(subpacket, 0, sizeof(subpacket));
					reg_type_num = (sizeof(fc_p->map_p->fn12.data.num) / sizeof(fc_p->map_p->fn12.data.num[0]));
					base_addr = fc_p->map_p->fn12.dataBase;

					rc = M_READ_FUNC(fc_p, fc_p->map_p->fn12.queryBase + 7, &presence_size, 1);
					DEVICE_ERR_CHECK(rc, err_exit);
					rc = M_READ_FUNC(fc_p, fc_p->map_p->fn12.queryBase + 8, subpacket, presence_size);
					DEVICE_ERR_CHECK(rc, err_exit);

					if(presence_size != 0){
						presence_size--;
					}

					for(j = 0; j < presence_size; j++){
						for(k = 0; k < 8; k++){
							if( ((j * 8) + k) < reg_type_num){
								if( (subpacket[j + 1] & (1 << k)) != 0){
									fc_p->map_p->fn12.data.num[(j * 8) + k].enable = 1;
									fc_p->map_p->fn12.data.num[(j * 8) + k].addr = base_addr;
									base_addr++;
								}else{
									fc_p->map_p->fn12.data.num[(j * 8) + k].enable = 0;
									fc_p->map_p->fn12.data.num[(j * 8) + k].addr = 0x00;
								}
							}
						}
					}

					{
						u8 buff_num = 0;
						presence_size = subpacket[0];
						rc = M_READ_FUNC(fc_p, fc_p->map_p->fn12.queryBase + 9, subpacket, presence_size);
						DEVICE_ERR_CHECK(rc, err_exit);

						for(j = 0; j < reg_type_num; j++)
						{
							u8 sub_num = 0;

							if(fc_p->map_p->fn12.data.num[j].enable != 0){
								fc_p->map_p->fn12.data.num[j].sub_size = subpacket[buff_num];
								buff_num++;

								for(k = 0; k < 7; k++){
									fc_p->map_p->fn12.data.num[j].sub_enable[sub_num] = ((subpacket[buff_num] >> k) & 0x01);
									sub_num++;
								}
								while(buff_num < presence_size){
									if((subpacket[buff_num] & 0x80) == 0){
										buff_num++;
										break;
									}
									else{
										buff_num++;
										for(k = 0; k < 7; k++){
											fc_p->map_p->fn12.data.num[j].sub_enable[sub_num] = ((subpacket[buff_num] >> k) & 0x01);
											sub_num++;
										}
									}
								}
							}
						}
					}

					rc = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[8].addr, maxPosition, 4);
					DEVICE_ERR_CHECK(rc, err_exit);

					fc_p->map_p->fn12.ctrl.maxXPosition = maxPosition[0] | ((maxPosition[1] & 0x0F) << 0x08);
					fc_p->map_p->fn12.ctrl.maxYPosition = maxPosition[2] | ((maxPosition[3] & 0x0F) << 0x08);

					rc = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[23].addr, maxFinger, 2);
					DEVICE_ERR_CHECK(rc, err_exit);

					fc_p->map_p->fn12.ctrl.maxFingerNum = maxFinger[1];

					fc_p->map_p->fn12.ctrl.support_object_glove = ((F12_QUERY_SUPPORTEDOBJECTTYPES(fc_p->map_p->fn12.query.data) >> 5) & 0x01);
				}

				SHTPS_LOG_DBG_PRINT("F12 Query Data\n");
				SHTPS_LOG_DBG_PRINT("-------------------------------------------------\n");
				SHTPS_LOG_DBG_PRINT("HasRegisterDescriptor          : 0x%02x\n", F12_QUERY_HASREGISTERDESCRIPTOR(fc_p->map_p->fn12.query.data));
				SHTPS_LOG_DBG_PRINT("SizeOfQueryPresence            : 0x%02x\n", F12_QUERY_SIZEOFQUERYPRESENCE(fc_p->map_p->fn12.query.data));
				SHTPS_LOG_DBG_PRINT("SizeOfQueryStructure           : 0x%02x\n", F12_QUERY_SIZEOFQUERYSTRUCTURE(fc_p->map_p->fn12.query.data));
				SHTPS_LOG_DBG_PRINT("QueryStructure                 : 0x%02x\n", F12_QUERY_QUERYSTRUCTURE(fc_p->map_p->fn12.query.data));
				SHTPS_LOG_DBG_PRINT("SizeOfControlPresence          : 0x%02x\n", F12_QUERY_SIZEOFCONTROLPRESENCE(fc_p->map_p->fn12.query.data));
				SHTPS_LOG_DBG_PRINT("SizeOfControlStructure         : 0x%02x\n", F12_QUERY_SIZEOFCONTROLSTRUCTURE(fc_p->map_p->fn12.query.data));
				SHTPS_LOG_DBG_PRINT("ControlStructure               : 0x%02x\n", F12_QUERY_CONTROLSTRUCTURE(fc_p->map_p->fn12.query.data));
				SHTPS_LOG_DBG_PRINT("SizeOfDataPresence             : 0x%02x\n", F12_QUERY_SIZEOFDATAPRESENCE(fc_p->map_p->fn12.query.data));
				SHTPS_LOG_DBG_PRINT("SizeOfDataStructure            : 0x%02x\n", F12_QUERY_SIZEOFDATASTRUCTURE(fc_p->map_p->fn12.query.data));
				SHTPS_LOG_DBG_PRINT("DataStructure                  : 0x%02x\n", F12_QUERY_DATASTRUCTURE(fc_p->map_p->fn12.query.data));
				SHTPS_LOG_DBG_PRINT("MaxPosition                    : (%d, %d)\n", fc_p->map_p->fn12.ctrl.maxXPosition, fc_p->map_p->fn12.ctrl.maxYPosition);
				SHTPS_LOG_DBG_PRINT("MaxFingerNum                   : %d\n", fc_p->map_p->fn12.ctrl.maxFingerNum);
				SHTPS_LOG_DBG_PRINT("SupportObject Glove            : %d\n", fc_p->map_p->fn12.ctrl.support_object_glove);
				break;

			case 0x34:
				SHTPS_LOG_DBG_PRINT("Found: Flash memory management\n");
				fc_p->map_p->fn34.enable		= 1;
				fc_p->map_p->fn34.queryBase = ((page & 0x0f) << 8) | pdt.queryBaseAddr;
				fc_p->map_p->fn34.dataBase  = ((page & 0x0f) << 8) | pdt.dataBaseAddr;
				fc_p->map_p->fn34.ctrlBase   = ((page & 0x0f) << 8) | pdt.controlBaseAddr;

				rc = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn34.queryBase,
									fc_p->map_p->fn34.query.data, sizeof(fc_p->map_p->fn34.query.data));
				if(rc)	goto err_exit;

				SHTPS_LOG_DBG_PRINT("F34 Query Data\n");
				SHTPS_LOG_DBG_PRINT("-------------------------------------------------\n");
				SHTPS_LOG_DBG_PRINT("bootLoaderID0        : 0x%02x\n", F34_QUERY_BOOTLOADERID0(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("bootLoaderID1        : 0x%02x\n", F34_QUERY_BOOTLOADERID1(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("firmwareID0          : 0x%02x\n", F34_QUERY_FIRMWAREID0(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("firmwareID1          : 0x%02x\n", F34_QUERY_FIRMWAREID1(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("firmwareID2          : 0x%02x\n", F34_QUERY_FIRMWAREID2(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("firmwareID3          : 0x%02x\n", F34_QUERY_FIRMWAREID3(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("MinimumWriteSize     : 0x%02x\n", F34_QUERY_MINIMUMWRITESIZE(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("BlockSize            : 0x%04x\n", F34_QUERY_BLOCKSIZE(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("FlashPageSize        : 0x%04x\n", F34_QUERY_FLASHPAGESIZE(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("AdjustPartitionSize  : 0x%04x\n", F34_QUERY_ADJUSTPARTITIONSIZE(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("FlashConfigLength    : 0x%04x\n", F34_QUERY_FLASHCONFIGLENGTH(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("PayLoadLength        : 0x%04x\n", F34_QUERY_PAYLOAD_LENGTH(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("GuestCodeSerialzation: 0x%02x\n", F34_QUERY_GUESTSERIALIZATION(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("DeviceConfig         : 0x%02x\n", F34_QUERY_DEVICECONFIG(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("DisplayConfig        : 0x%02x\n", F34_QUERY_DISPLAYCONFIG(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("GuestCode            : 0x%02x\n", F34_QUERY_GUESTCODE(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("CoreCode             : 0x%02x\n", F34_QUERY_CORECODE(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("CoreConfig           : 0x%02x\n", F34_QUERY_CORECONFIG(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("FlashConfig          : 0x%02x\n", F34_QUERY_FLASHCONFIG(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("MfgBlk               : 0x%02x\n", F34_QUERY_MFGBLK(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("Bootloader           : 0x%02x\n", F34_QUERY_BOOTLOADER(fc_p->map_p->fn34.query.data));
				SHTPS_LOG_DBG_PRINT("GlobalParams         : 0x%02x\n", F34_QUERY_GLOBALPARAMS(fc_p->map_p->fn34.query.data));
				break;

			case 0x54:
				SHTPS_LOG_DBG_PRINT("Found: Specification Addendum\n");
				fc_p->map_p->fn54.enable		= 1;
				fc_p->map_p->fn54.queryBase  = ((page & 0x0f) << 8) | pdt.queryBaseAddr;
				fc_p->map_p->fn54.ctrlBase   = ((page & 0x0f) << 8) | pdt.controlBaseAddr;
				fc_p->map_p->fn54.dataBase   = ((page & 0x0f) << 8) | pdt.dataBaseAddr;
				fc_p->map_p->fn54.commandBase= ((page & 0x0f) << 8) | pdt.commandBaseAddr;

				rc = M_READ_FUNC(fc_p, fc_p->map_p->fn54.queryBase,
									fc_p->map_p->fn54.query.data, sizeof(fc_p->map_p->fn54.query.data));
				if(rc)	goto err_exit;

				SHTPS_LOG_DBG_PRINT("F54 Query Data\n");
				SHTPS_LOG_DBG_PRINT("-------------------------------------------------\n");
				SHTPS_LOG_DBG_PRINT("NumberOfReceiverElectrodes    : 0x%02x\n", F54_QUERY_NUMOFRCVEL(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("NumberOfTransmitterElectrodes : 0x%02x\n", F54_QUERY_NUMOFTRANSEL(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasBaseLine                   : 0x%02x\n", F54_QUERY_HASBASELINE(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasImage8                     : 0x%02x\n", F54_QUERY_HAS8BIT(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasImage16                    : 0x%02x\n", F54_QUERY_HAS16BIT(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("ClockRate                     : 0x%02x\n", F54_QUERY_CLOCKRATE(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("AnalogHardwareFamily          : 0x%02x\n", F54_QUERY_ANALOGHARDWAREFAMILY(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasPixelTouchThresholdTuning  : 0x%02x\n", F54_QUERY_HASPIXELTOUCHTHRESHOLDTUNING(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasArbitrarySensorAssignment  : 0x%02x\n", F54_QUERY_HASARBITRARYSENSORASSIGNMENT(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasInterferenceMetric         : 0x%02x\n", F54_QUERY_HASINTERFERENCEMETRIC(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasSenseFrequencyControl      : 0x%02x\n", F54_QUERY_HASSENSEFREQCONTROL(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasFirmwareNoiseMitigation    : 0x%02x\n", F54_QUERY_HASFWNOISEMITIGATION(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasLowPowerCtrl               : 0x%02x\n", F54_QUERY_HASLOWPOERCTRL(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasTwoByteReportRateReporting : 0x%02x\n", F54_QUERY_HASTWOBYTEREPORTRATEREPORTING(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasOneByteReportRateReporting : 0x%02x\n", F54_QUERY_HASONEBYTEREPORTRATEREPORTING(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasRelaxationCtrl             : 0x%02x\n", F54_QUERY_HASRELAXATIONCTRL(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("AxisCompensationMode          : 0x%02x\n", F54_QUERY_AXISCOMPENSATIONMODE(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasIIRFilter                  : 0x%02x\n", F54_QUERY_HASIIRFILTER(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasCMNRemoval                 : 0x%02x\n", F54_QUERY_HASCMNREMOVAL(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasCMNCapScaleFactor          : 0x%02x\n", F54_QUERY_HASCMNCAPSCALEFACTOR(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasPixelThresholdHysteresis   : 0x%02x\n", F54_QUERY_HASPIXCELTHRESHHYSTERESIS(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasEdgeCompensation           : 0x%02x\n", F54_QUERY_HASEDGECOMPENSATION(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasPerFrequencyNoiseControl   : 0x%02x\n", F54_QUERY_HASPERFREQNOISECTRL(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasForceFastRelaxation        : 0x%02x\n", F54_QUERY_HASFORCEFASTRELAXATION(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasMMStateMitigation          : 0x%02x\n", F54_QUERY_HASMMSTATEMITIGATION(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasCDM4                       : 0x%02x\n", F54_QUERY_HASCDM4(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("HasVarianceMetric             : 0x%02x\n", F54_QUERY_HASVARIANCEMETRIC(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("Has0DRelaxation               : 0x%02x\n", F54_QUERY_HAS0DRELAXATIONCTRL(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("Has0DAcquisitionCtrl          : 0x%02x\n", F54_QUERY_HAS0DACQUISITIONCTRL(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("MaxNumOfSensingFrequencies    : 0x%02x\n", F54_QUERY_MAXNUMOFSENSINGFREQ(fc_p->map_p->fn54.query.data));
				SHTPS_LOG_DBG_PRINT("BurstsPerCluster              : 0x%02x\n", F54_QUERY_BURSTSPERCLUSTER(fc_p->map_p->fn54.query.data));
				break;

			case 0x19:
				SHTPS_LOG_DBG_PRINT("Found: 0-D Capacitivve Buttons\n");
				fc_p->map_p->fn19.enable		= 1;
				fc_p->map_p->fn19.queryBase  = ((page & 0x0f) << 8) | pdt.queryBaseAddr;
				fc_p->map_p->fn19.ctrlBase   = ((page & 0x0f) << 8) | pdt.controlBaseAddr;
				fc_p->map_p->fn19.dataBase   = ((page & 0x0f) << 8) | pdt.dataBaseAddr;
				fc_p->map_p->fn19.commandBase= ((page & 0x0f) << 8) | pdt.commandBaseAddr;

				rc = M_READ_FUNC(fc_p, fc_p->map_p->fn19.queryBase,
									fc_p->map_p->fn19.query.data, sizeof(fc_p->map_p->fn19.query.data));
				if(rc)	goto err_exit;

				SHTPS_LOG_DBG_PRINT("F19 Query Data\n");
				SHTPS_LOG_DBG_PRINT("-------------------------------------------------\n");
				SHTPS_LOG_DBG_PRINT("HasSensitivityAdjust   : 0x%02x\n", F19_QUERY_HASSENSITIVITYADJUST(fc_p->map_p->fn19.query.data));
				SHTPS_LOG_DBG_PRINT("HasHysteresisThreshold : 0x%02x\n", F19_QUERY_HASHYSTERESISTHRESHOLD(fc_p->map_p->fn19.query.data));
				SHTPS_LOG_DBG_PRINT("ButtonCount            : 0x%02x\n", F19_QUERY_BUTTONCOUNT(fc_p->map_p->fn19.query.data));
				break;

			case 0x1A:
				SHTPS_LOG_DBG_PRINT("Found: 0-D Capacitivve Buttons\n");
				fc_p->map_p->fn1A.enable		= 1;
				fc_p->map_p->fn1A.queryBase  = ((page & 0x0f) << 8) | pdt.queryBaseAddr;
				fc_p->map_p->fn1A.ctrlBase   = ((page & 0x0f) << 8) | pdt.controlBaseAddr;
				fc_p->map_p->fn1A.dataBase   = ((page & 0x0f) << 8) | pdt.dataBaseAddr;
				fc_p->map_p->fn1A.commandBase= ((page & 0x0f) << 8) | pdt.commandBaseAddr;

				rc = M_READ_FUNC(fc_p, fc_p->map_p->fn1A.queryBase,
									fc_p->map_p->fn1A.query.data, sizeof(fc_p->map_p->fn1A.query.data));
				if(rc)	goto err_exit;

				SHTPS_LOG_DBG_PRINT("F1A Query Data\n");
				SHTPS_LOG_DBG_PRINT("-------------------------------------------------\n");
				SHTPS_LOG_DBG_PRINT("MaxButtonCount               : 0x%02x\n", F1A_QUERY_MAX_BUTTONCOUNT(fc_p->map_p->fn1A.query.data));
				SHTPS_LOG_DBG_PRINT("HasGeneralControle           : 0x%02x\n", F1A_QUERY_HASGENERALCONTROL(fc_p->map_p->fn1A.query.data));
				SHTPS_LOG_DBG_PRINT("HasInterruptEnable           : 0x%02x\n", F1A_QUERY_HASINTERRUPTENABLE(fc_p->map_p->fn1A.query.data));
				SHTPS_LOG_DBG_PRINT("HasMultiButtonSelect         : 0x%02x\n", F1A_QUERY_HASMULTIBUTTONSELECT(fc_p->map_p->fn1A.query.data));
				SHTPS_LOG_DBG_PRINT("HasTxRxMapping               : 0x%02x\n", F1A_QUERY_HASTXRXMAPPING(fc_p->map_p->fn1A.query.data));
				SHTPS_LOG_DBG_PRINT("HasPerButtonThreshold        : 0x%02x\n", F1A_QUERY_HASPERBUTTONTHRESHOLD(fc_p->map_p->fn1A.query.data));
				SHTPS_LOG_DBG_PRINT("HasReleaseThreshold          : 0x%02x\n", F1A_QUERY_HASRELEASETHRESHOLD(fc_p->map_p->fn1A.query.data));
				SHTPS_LOG_DBG_PRINT("HasStrongestButtonHysteresis : 0x%02x\n", F1A_QUERY_HASSTRONGESTBUTTONHYSTERESIS(fc_p->map_p->fn1A.query.data));
				SHTPS_LOG_DBG_PRINT("HasFilterStrength            : 0x%02x\n", F1A_QUERY_HASFILTERSTRENGTH(fc_p->map_p->fn1A.query.data));
				break;

			case 0x51:
				SHTPS_LOG_DBG_PRINT("Found: Custom\n");
				fc_p->map_p->fn51.enable		= 1;
				fc_p->map_p->fn51.queryBase  = ((page & 0x0f) << 8) | pdt.queryBaseAddr;
				fc_p->map_p->fn51.ctrlBase   = ((page & 0x0f) << 8) | pdt.controlBaseAddr;
				fc_p->map_p->fn51.dataBase   = ((page & 0x0f) << 8) | pdt.dataBaseAddr;
				fc_p->map_p->fn51.commandBase= ((page & 0x0f) << 8) | pdt.commandBaseAddr;

				rc = M_READ_FUNC(fc_p, fc_p->map_p->fn51.queryBase,
									fc_p->map_p->fn51.query.data, sizeof(fc_p->map_p->fn51.query.data));
				if(rc)	goto err_exit;

				SHTPS_LOG_DBG_PRINT("F51 Query Data\n");
				SHTPS_LOG_DBG_PRINT("-------------------------------------------------\n");
				SHTPS_LOG_DBG_PRINT("QueryRegisterCount   : 0x%02x\n", F51_QUERY_QUERYREGISTERCOUNT(fc_p->map_p->fn51.query.data));
				SHTPS_LOG_DBG_PRINT("DataRegisterCount    : 0x%02x\n", F51_QUERY_DATAREGISTERCOUNT(fc_p->map_p->fn51.query.data));
				SHTPS_LOG_DBG_PRINT("ControlRegisterCount : 0x%02x\n", F51_QUERY_CONTROLREGISTERCOUNT(fc_p->map_p->fn51.query.data));
				SHTPS_LOG_DBG_PRINT("CommandRegisterCount : 0x%02x\n", F51_QUERY_COMMANDREGISTERCOUNT(fc_p->map_p->fn51.query.data));
				break;

			default:
				break;
			}
		}
	}
	rc = M_WRITE_FUNC(fc_p, 0xFF, 0x00);
	if(rc)	goto err_exit;

	if(0 == fc_p->map_p->fn01.enable){
		SHTPS_LOG_ERR_PRINT("map construct error. fn01=disable\n");
		rc = -1;
		goto err_exit;
	}

	if(func_check){
		u8 buf;
		rc = shtps_fwctl_s3700_check_crc_error(fc_p, &buf);
		if(rc)	goto err_exit;
		if(buf != 0){
			SHTPS_LOG_DBG_PRINT("FW CRC error detect\n");
			if(	fc_p->map_p->fn34.enable == 0){
				rc = -1;
				SHTPS_LOG_ERR_PRINT("map construct error. / fn34=disable\n");
				goto err_exit;
			}
		}else{
			if(	fc_p->map_p->fn12.enable == 0 ||
				fc_p->map_p->fn54.enable == 0)
			{
				rc = -1;
				SHTPS_LOG_ERR_PRINT("map construct error. fw status = 0x%02X / fn12=%s, fn54=%s\n",
					buf,
					(fc_p->map_p->fn12.enable == 1)? "enable" : "disable",
					(fc_p->map_p->fn54.enable == 1)? "enable" : "disable");
				goto err_exit;
			}
		}
	}
	
	#if defined(SHTPS_LPWG_MODE_ENABLE)
		if(rc == 0)	rc = shtps_fwctl_s3700_get_lpwg_def_settings(fc_p);
	#endif /* SHTPS_LPWG_MODE_ENABLE */

	#if defined(SHTPS_SPI_AVOID_BLOCKREAD_FAIL)
		if(rc == 0){
			rc = M_READ_PACKET_FUNC(fc_p,
						fc_p->map_p->fn12.ctrl.num[11].addr,
						fc_p->map_p->reg_F12_CTRL11_val,
						sizeof(fc_p->map_p->reg_F12_CTRL11_val));
		}
	#endif /* SHTPS_SPI_AVOID_BLOCKREAD_FAIL */

	#if defined(SHTPS_CTRL_FW_REPORT_RATE)
		M_READ_FUNC(fc_p,
					fc_p->map_p->fn51.ctrlBase + F51_QUERY_CUSTOMREPORTRATEREGISTERADDRESS(fc_p->map_p->fn51.query.data) + 1,
					&(fc_p->map_p->fn51_custom_report_rate_def), sizeof(fc_p->map_p->fn51_custom_report_rate_def));
	#endif /* SHTPS_CTRL_FW_REPORT_RATE */

	#if defined(SHTPS_DEF_GLOVE_CHANGE_THERMAL_UPDATE_INTERVAL_ENABLE)
		if(rc == 0){
			u8 buf[8];
			rc = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL133_OFFSET, buf, sizeof(buf));
			if(rc == 0){
				fc_p->map_p->reg_fn54_ctrl133_TX_thermal_update_interval_def[0] = buf[4];
				fc_p->map_p->reg_fn54_ctrl133_TX_thermal_update_interval_def[1] = buf[5];
				fc_p->map_p->reg_fn54_ctrl133_RX_thermal_update_interval_def[0] = buf[6];
				fc_p->map_p->reg_fn54_ctrl133_RX_thermal_update_interval_def[1] = buf[7];
			}
		}
	#endif /* SHTPS_DEF_GLOVE_CHANGE_THERMAL_UPDATE_INTERVAL_ENABLE */

	#if defined(SHTPS_DEF_GLOVE_CHANGE_FREQ_SCAN_THRESHOLD_ENABLE)
		if(rc == 0){
			u8 buf[6];
			rc = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL89_OFFSET, buf, sizeof(buf));
			if(rc == 0){
				fc_p->map_p->reg_fn54_ctrl89_freq_scan_threshold_def[0] = buf[4];
				fc_p->map_p->reg_fn54_ctrl89_freq_scan_threshold_def[1] = buf[5];
			}
		}
	#endif /* SHTPS_DEF_GLOVE_CHANGE_FREQ_SCAN_THRESHOLD_ENABLE */

	#if defined(SHTPS_DEF_GLOVE_DISABLE_MOISTURE_ENABLE)
		if(rc == 0){
			u8 buf[1];
			rc = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[33].addr, buf, sizeof(buf));
			if(rc == 0){
				fc_p->map_p->reg_F12_CTRL33_def = buf[0];
			}
		}
	#endif /* SHTPS_DEF_GLOVE_DISABLE_MOISTURE_ENABLE */

	if(rc == 0){
		rc = M_READ_PACKET_FUNC(fc_p,
					fc_p->map_p->fn12.ctrl.num[23].addr,
					&fc_p->map_p->reg_F12_CTRL23_object_report_enable,
					1);
	}

err_exit:
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_is_sleeping(struct shtps_fwctl_info *fc_p)
{
	int rc;
	u8 val;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, &val, 1);

	if(rc == 0){
		return (val & 0x03);
	}else{
		return (-1);
	}
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_set_doze(struct shtps_fwctl_info *fc_p)
{
#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
	int rc;
	u8 val;
	SHTPS_LOG_FWCTL_FUNC_CALL();

	if(shtps_check_set_doze_enable() == 1){
		rc = M_READ_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, &val, 1);
		if(rc == 0){
			val &= ~0x04;
			rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, val);
			if(rc == 0)	shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_DOZE);
		}
	}else{
		return 0;
	}
	return rc;
#else /* SHTPS_LOW_POWER_MODE_ENABLE */
	return 0;
#endif  /* SHTPS_LOW_POWER_MODE_ENABLE */
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_set_doze_param(struct shtps_fwctl_info *fc_p, u8 *param_p, u8 param_size)
{
	int rc;

	if(param_size >= 1){
		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase + 0x02, param_p[0]);
		if(rc != 0)	goto err_exit;
	}
	if(param_size >= 2){
		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase + 0x03, param_p[1]);
		if(rc != 0)	goto err_exit;
	}
	if(param_size >= 3){
		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase + 0x04, param_p[2]);
		if(rc != 0)	goto err_exit;
	}

err_exit:
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_set_active(struct shtps_fwctl_info *fc_p)
{
	int rc;
	u8 val;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, &val, 1);
	if(rc == 0){
		val |= 0x04;
		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, val);
		if(rc == 0)	shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_ACTIVE);
	}
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_set_sleepmode_on(struct shtps_fwctl_info *fc_p)
{
	int rc;
	u8 val;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, &val, 1);
	if(rc == 0){
		val = (val & (~0x03)) | 0x01;	/* 0xfc | 0x01 */
		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, val);
		if(rc == 0)	shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_SLEEP);
	}
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_set_sleepmode_off(struct shtps_fwctl_info *fc_p)
{
	int rc;
	u8 val;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, &val, 1);
	if(rc == 0){
		val &= (~0x03);	/* 0xfc */
		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, val);
	}
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_set_lpwg_mode_on(struct shtps_fwctl_info *fc_p)
{
	u8 data;

	SHTPS_LOG_FWCTL_FUNC_CALL();

	if(SHTPS_LPWG_CLIP_SET_TYPE == 1){
		M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[8].addr, 
									fc_p->map_p->fn12_ctrl8_enable_settings, 
									sizeof(fc_p->map_p->fn12_ctrl8_enable_settings));

		#if defined(SHTPS_LPWG_F51_REPORT_BEYOND_ACTIVE_AREA_ENABLE)
			M_WRITE_FUNC(fc_p, SHTPS_F51_REPORT_BEYOND_ACTIVE_AREA_ADDR,
										fc_p->map_p->fn51_report_beyond_active_area_enable_settings);
		#endif /* SHTPS_LPWG_F51_REPORT_BEYOND_ACTIVE_AREA_ENABLE */
	}

	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[27].addr, 
								fc_p->map_p->fn12_ctrl27_enable_settings, 
								sizeof(fc_p->map_p->fn12_ctrl27_enable_settings));

	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[18].addr, 
								fc_p->map_p->fn12_ctrl18_enable_settings, 
								sizeof(fc_p->map_p->fn12_ctrl18_enable_settings));

	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[15].addr,
								fc_p->map_p->fn12_ctrl15_enable_settings,
								sizeof(fc_p->map_p->fn12_ctrl15_enable_settings));

	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[22].addr,
								fc_p->map_p->fn12_ctrl22_enable_settings,
								sizeof(fc_p->map_p->fn12_ctrl22_enable_settings));

	#if defined( SHTPS_HOST_LPWG_MODE_ENABLE )
		if(shtps_check_host_lpwg_enable() == 0){
			M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[20].addr, 
										fc_p->map_p->fn12_ctrl20_enable_settings, 
										sizeof(fc_p->map_p->fn12_ctrl20_enable_settings));
		}
	#else /* SHTPS_HOST_LPWG_MODE_ENABLE */
		M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[20].addr, 
									fc_p->map_p->fn12_ctrl20_enable_settings, 
									sizeof(fc_p->map_p->fn12_ctrl20_enable_settings));
	#endif /* SHTPS_HOST_LPWG_MODE_ENABLE */

	M_READ_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, &data, 1);
	if(SHTPS_LPWG_DOZE_ENABLE == 0){
		M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, ((data & ~0x03) | 0x04));
	}else{
		M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, data & ~0x04);
		M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, data & ~0x07);
		SHTPS_LOG_DBG_PRINT("[LPWG] device set doze [0x%02X]", data & ~0x07);
	}

	#if defined( SHTPS_HOST_LPWG_MODE_ENABLE )
		if(shtps_check_host_lpwg_enable() != 0){
			if(SHTPS_LPWG_DOZE_ENABLE == 0){
				shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_ACTIVE);
			}else{
				shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_DOZE);
			}
		}else{
			shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_LPWG);
		}
	#else
		shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_LPWG);
	#endif /* SHTPS_HOST_LPWG_MODE_ENABLE */

	#if defined( SHTPS_ACTIVE_SLEEP_WAIT_ALWAYS_ENABLE )
		if(data & 0x01){
			if(SHTPS_SLEEP_OUT_WAIT_MS > 0){
				msleep(SHTPS_SLEEP_OUT_WAIT_MS);
			}
		}
	#endif /* SHTPS_ACTIVE_SLEEP_WAIT_ALWAYS_ENABLE */

	if(SHTPS_LPWG_CLIP_SET_TYPE == 2){
		shtps_fwctl_s3700_command_force_update(fc_p);
		shtps_fwctl_s3700_f54_command_completion_wait(fc_p, SHTPS_F54_COMMAND_WAIT_POLL_COUNT, SHTPS_F54_COMMAND_WAIT_POLL_INTERVAL);
	}

	return 0;	// no error check
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_set_lpwg_mode_off(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();

	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[18].addr, 
								fc_p->map_p->fn12_ctrl18_disable_settings, 
								sizeof(fc_p->map_p->fn12_ctrl18_disable_settings));

	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[27].addr, 
								fc_p->map_p->fn12_ctrl27_disable_settings, 
								sizeof(fc_p->map_p->fn12_ctrl27_disable_settings));
	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[20].addr, 
								fc_p->map_p->fn12_ctrl20_disable_settings, 
								sizeof(fc_p->map_p->fn12_ctrl20_disable_settings));

	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[15].addr,
								fc_p->map_p->fn12_ctrl15_disable_settings,
								sizeof(fc_p->map_p->fn12_ctrl15_disable_settings));

	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[22].addr,
								fc_p->map_p->fn12_ctrl22_disable_settings,
								sizeof(fc_p->map_p->fn12_ctrl22_disable_settings));

	if(SHTPS_LPWG_CLIP_SET_TYPE == 1){
		#if defined(SHTPS_LPWG_F51_REPORT_BEYOND_ACTIVE_AREA_ENABLE)
			M_WRITE_FUNC(fc_p, SHTPS_F51_REPORT_BEYOND_ACTIVE_AREA_ADDR,
										fc_p->map_p->fn51_report_beyond_active_area_disable_settings);
		#endif /* SHTPS_LPWG_F51_REPORT_BEYOND_ACTIVE_AREA_ENABLE */

		M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[8].addr, 
									fc_p->map_p->fn12_ctrl8_disable_settings, 
									sizeof(fc_p->map_p->fn12_ctrl8_disable_settings));
	}
	else if(SHTPS_LPWG_CLIP_SET_TYPE == 2){
		shtps_fwctl_s3700_command_force_update(fc_p);
		shtps_fwctl_s3700_f54_command_completion_wait(fc_p, SHTPS_F54_COMMAND_WAIT_POLL_COUNT, SHTPS_F54_COMMAND_WAIT_POLL_INTERVAL);
	}

	return 0;	// no error check
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_set_lpwg_mode_cal(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	if(SHTPS_LPWG_MODE_ON_AFTER_SLEEP_ENABLE){
		shtps_fwctl_s3700_set_sleepmode_on(fc_p);
		if(SHTPS_SLEEP_IN_WAIT_MS > 0){
			mdelay(SHTPS_SLEEP_IN_WAIT_MS);
		}
		if(SHTPS_LPWG_DOZE_ENABLE == 0){
			shtps_fwctl_s3700_set_active(fc_p);
		}else{
			shtps_fwctl_s3700_set_sleepmode_off(fc_p);
		}
		#if defined( SHTPS_HOST_LPWG_MODE_ENABLE )
			if(shtps_check_host_lpwg_enable() != 0){
				if(SHTPS_LPWG_DOZE_ENABLE == 0){
					shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_ACTIVE);
				}else{
					shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_DOZE);
				}
			}else{
				shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_LPWG);
			}
		#else
			shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_LPWG);
		#endif /* SHTPS_HOST_LPWG_MODE_ENABLE */

		#if defined( SHTPS_ACTIVE_SLEEP_WAIT_ALWAYS_ENABLE )
			if(SHTPS_SLEEP_OUT_WAIT_MS > 0){
				mdelay(SHTPS_SLEEP_OUT_WAIT_MS);
			}
		#endif /* SHTPS_ACTIVE_SLEEP_WAIT_ALWAYS_ENABLE */
	}

	return 0;	// no error check
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_fingermax(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	if(fc_p->map_p->fn12.ctrl.maxFingerNum > SHTPS_FINGER_MAX){
		return SHTPS_FINGER_MAX;
	}
	return fc_p->map_p->fn12.ctrl.maxFingerNum;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_fingerinfo(struct shtps_fwctl_info *fc_p, u8 *buf_p, int read_cnt, u8 *irqsts_p, u8 *extsts_p, u8 **finger_pp)
{
	u32 read_finger_info_size;
	int fingerMax = shtps_fwctl_s3700_get_fingermax(fc_p);

	SHTPS_LOG_FWCTL_FUNC_CALL();
	if(fingerMax < SHTPS_FINGER_MAX){
		read_finger_info_size = (fingerMax * 8);
	}else{
		read_finger_info_size = (SHTPS_FINGER_MAX * 8);
	}

	M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn01.dataBase, buf_p, 2);

	if(SHTPS_TOUCH_PERFORMANCE_UP_MODE == 1){
		u8 object_attention = 0;
		int i;

		read_finger_info_size = 0;
		M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.data.num[15].addr, &object_attention, 1);
		for(i = 0; i < SHTPS_FINGER_MAX; i++){
			if( ((object_attention >> i) & 0x01) != 0 ){
				read_finger_info_size = ((i + 1) * 8);
			}
		}
	}
#if defined ( SHTPS_TOUCH_EMURATOR_ENABLE )
	if(shtps_touch_emu_is_running() != 0){
		shtps_touch_emu_set_finger_info(buf_p, sizeof(buf_p));
	} else {
		M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.data.num[1].addr, &buf_p[2], read_finger_info_size);

		if(shtps_touch_emu_is_recording() != 0){
			shtps_touch_emu_rec_finger_info(&buf_p[2]);
		}
	}
#else /* SHTPS_TOUCH_EMURATOR_ENABLE */
	M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.data.num[1].addr, &buf_p[2], read_finger_info_size);
#endif /* #if defined ( SHTPS_TOUCH_EMURATOR_ENABLE ) */

	*irqsts_p = buf_p[1];
	*finger_pp = &buf_p[2];
	*extsts_p = 0;

	return 0;	// no error check
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_one_fingerinfo(struct shtps_fwctl_info *fc_p, int id, u8 *buf_p, u8 **finger_pp)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	*finger_pp = buf_p;
	return M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.data.num[1].addr, buf_p, 8);
}

/* -------------------------------------------------------------------------- */
static u8* shtps_fwctl_s3700_get_finger_info_buf(struct shtps_fwctl_info *fc_p, int fingerid, int fingerMax, u8 *buf_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return &buf_p[fingerid * 8];
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_finger_state(struct shtps_fwctl_info *fc_p, int fingerid, int fingerMax, u8 *buf_p)
{
	u8 *d_p;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	d_p = shtps_fwctl_s3700_get_finger_info_buf(fc_p, fingerid, fingerMax, buf_p);
	return F12_DATA_FINGERSTATE( d_p );
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_finger_pos_x(struct shtps_fwctl_info *fc_p, u8 *buf_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return F12_DATA_XPOS(buf_p);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_finger_pos_y(struct shtps_fwctl_info *fc_p, u8 *buf_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return F12_DATA_YPOS(buf_p);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_finger_wx(struct shtps_fwctl_info *fc_p, u8 *buf_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return F12_DATA_WX(buf_p);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_finger_wy(struct shtps_fwctl_info *fc_p, u8 *buf_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return F12_DATA_WY(buf_p);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_finger_z(struct shtps_fwctl_info *fc_p, u8 *buf_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return F12_DATA_Z(buf_p);
}

/* -------------------------------------------------------------------------- */
static void shtps_fwctl_s3700_get_gesture(struct shtps_fwctl_info *fc_p, int fingerMax, u8 *buf_p, u8 *gs1_p, u8 *gs2_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	*gs1_p = 0;
	*gs2_p = 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_gesturetype(struct shtps_fwctl_info *fc_p, u8 *status_p)
{
	int rc = 0;

	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn12.data.num[4].addr, status_p, 1);

	SHTPS_LOG_DBG_PRINT("%s() get gesture <0x%02X><%s>\n", __func__, *status_p,
							(*status_p == SHTPS_GESTURE_TYPE_NONE) ? "None" :
							(*status_p == SHTPS_GESTURE_TYPE_ONE_FINGER_SINGLE_TAP) ? "One-Finger single tap" :
							(*status_p == SHTPS_GESTURE_TYPE_ONE_FINGER_TAP_AND_HOLD) ? "One-Finger tap-and-hold" :
							(*status_p == SHTPS_GESTURE_TYPE_ONE_FINGER_DOUBLE_TAP) ? "One-Finger double tap" :
							(*status_p == SHTPS_GESTURE_TYPE_ONE_FINGER_EARLY_TAP) ? "One-Finger early tap" :
							(*status_p == SHTPS_GESTURE_TYPE_ONE_FINGER_FLICK) ? "One-Finger flick" :
							(*status_p == SHTPS_GESTURE_TYPE_ONE_FINGER_PRESS) ? "One-Finger press" :
							(*status_p == SHTPS_GESTURE_TYPE_ONE_FINGER_SWIPE) ? "One-Finger swipe" :
							(*status_p == SHTPS_GESTURE_TYPE_ONE_FINGER_CIRCLE) ? "Circle" :
							(*status_p == SHTPS_GESTURE_TYPE_ONE_FINGER_TRIANGLE) ? "Triangle" :
							(*status_p == SHTPS_GESTURE_TYPE_ONE_FINGER_VEE) ? "Vee" :
							(*status_p == SHTPS_GESTURE_TYPE_TRIPLE_TAP) ? "Triple tap" :
							(*status_p == SHTPS_GESTURE_TYPE_CLICK) ? "Click" :
							(*status_p == SHTPS_GESTURE_TYPE_PINCH) ? "Pinch" :
							(*status_p == SHTPS_GESTURE_TYPE_ROTATE) ? "Rotate" : "Unkown");

	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_fwdate(struct shtps_fwctl_info *fc_p, u8 *year_p, u8 *month_p)
{
	int rc=0;
	u8 buf[2] = { 0x00, 0x00 };

	SHTPS_LOG_FWCTL_FUNC_CALL();
	*year_p  = 0;
	*month_p = 0;
	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn01.queryBase + 4, buf, 2);
	if(rc == 0){
		if((buf[0] != 0x00)&&(buf[1] != 0x00)){
			*year_p  = buf[0] & 0x1F;
			*month_p = ((buf[0] >> 5) & 0x07) | ((buf[1] << 3) & 0x08 );
		}else{
			rc = (-1);
		}
	}
	return rc;
}

/* -------------------------------------------------------------------------- */
static char* shtps_fwctl_s3700_strstr(u8 *data_p, int data_length, u8 *search_p, int search_length)
{
	int i, j;

	for(i = 0; i < data_length; i++){
		if(data_p[i] == search_p[0]){
			for(j = 0; j < search_length; j++){
				if(data_p[i+j] != search_p[j]){
					break;
				}
			}
			if(j == search_length){
				// search success
				return &data_p[i];
			}
		}
	}
	// search fail
	return NULL;
}

static int shtps_fwctl_s3700_make_serial_number_data(u8 *data_p, int data_length, u8 *buf_p)
{
	struct read_info_s {
		char	*search_str;
		int		read_length;
	};
	struct read_info_s read_info[] = {
		{"Year:",	2},
		{"Mth:",	2},
		{"Day:",	2},
		{NULL,		1},
		{"SN:",		9},
	};

	int i;
	char *str_p;
	char tmp_str[30];

	buf_p[0] = '\0';

	for(i = 0; i < (sizeof(read_info) / sizeof(struct read_info_s)); i++){
		if(sizeof(tmp_str) < read_info[i].read_length + 1){
			SHTPS_LOG_ERR_PRINT("%s(): read buffer over error\n", __func__);
			return -1;
		}

		if(read_info[i].search_str != NULL){
			str_p = shtps_fwctl_s3700_strstr(data_p, data_length, read_info[i].search_str, strlen(read_info[i].search_str));
			if(str_p == NULL){
				SHTPS_LOG_ERR_PRINT("%s(): serial number tag \"%s\" not found.\n", __func__, read_info[i].search_str);
				return -1;
			}
			// skip search str and 1byte blank
			str_p += strlen(read_info[i].search_str) + 1;
			strncpy(tmp_str, str_p, read_info[i].read_length);
			tmp_str[read_info[i].read_length] = '\0';
		}
		else{
			// fill with '0'
			memset(tmp_str, '0', read_info[i].read_length);
			tmp_str[read_info[i].read_length] = '\0';
		}

		strcat(buf_p, tmp_str);
	}
	return 0;
}

static int shtps_fwctl_s3700_get_serial_number(struct shtps_fwctl_info *fc_p, u8 *buf_p)
{
	int rc = 0;
	u8 buf[2];
	int retrycnt;
	int blockSize;
	u8 serialBlockSize;
	u8 serial_all[SHTPS_SERIAL_NUMBER_ALL_SIZE+1] = {0};
	u8 serial_data[SHTPS_SERIAL_NUMBER_ALL_SIZE+1] = {0};
	int i;
	
	blockSize = F34_QUERY_BLOCKSIZE(fc_p->map_p->fn34.query.data);
	serialBlockSize = (SHTPS_SERIAL_NUMBER_ALL_SIZE / blockSize) + 1;

	if(fc_p->map_p->fn34.enable != 1){
		rc = -1;
		goto err_exit;
	}

	buf[0] = PARTITION_ID_GUESTSERIALIZATION;
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 1, buf[0]);
	if(rc) {
		rc = -2;
		goto err_exit;
	}
	
	memset(buf, 0, sizeof(buf));
	rc = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 2, buf, 2);
	if(rc) {
		rc = -3;
		goto err_exit;
	}
	
	buf[0] = serialBlockSize;
	buf[1] = 0x00;
	rc = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 3, buf, 2);
	if(rc) {
		rc = -4;
		goto err_exit;
	}
	
	buf[0] = FLASH_COMMAND_READ;
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 4, buf[0]);
	if(rc) {
		rc = -5;
		goto err_exit;
	}
	
	// wait for flash ready
	retrycnt = 500;
	while(retrycnt){
		rc = M_READ_FUNC(fc_p, fc_p->map_p->fn34.dataBase, buf, 1);
		if(buf[0] == 0x40){
			break;
		}
		udelay(SHTPS_READ_SERIAL_WAIT_US);
		retrycnt--;
	}
	if(retrycnt == 0){
		rc = -6;
		goto err_exit;
	}
	
	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn01.dataBase + 1, buf, 1);
	if(rc){
		rc = -7;
		goto err_exit;
	}
	
	rc = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 5, serial_all, SHTPS_SERIAL_NUMBER_ALL_SIZE);
	if(rc){
		rc = -8;
		goto err_exit;
	}
	
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 4, 0x00);
	if(rc){
		rc = -9;
		goto err_exit;
	}
	
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn34.dataBase + 1, 0x00);
	if(rc){
		rc = -10;
		goto err_exit;
	}
	
	rc = shtps_fwctl_s3700_make_serial_number_data(serial_all, SHTPS_SERIAL_NUMBER_ALL_SIZE, serial_data);
	if(rc){
		rc = -11;
		goto err_exit;
	}

	memcpy(buf_p, serial_data, SHTPS_READ_SERIAL_NUMBER_SIZE);
	SHTPS_LOG_DBG_PRINT("serial number = %s\n", serial_data);

	return 0;

err_exit:
	#if defined(SHTPS_READ_SERIAL_NUMBER_CHECK_F01_ENABLE)
	{
		u8 addr;
		u8 sprintfbuf[10];

		rc = M_READ_FUNC(fc_p, 0xe3, &addr, 1);
		if(rc == 0){
			rc = M_READ_FUNC(fc_p, addr + 4, serial_data, SHTPS_READ_SERIAL_NUMBER_FROM_F01_SIZE);
			if(rc == 0){
				for(i = 0; i < SHTPS_READ_SERIAL_NUMBER_FROM_F01_SIZE; i++){
					sprintf(sprintfbuf, "%x%x", ((serial_data[i] >> 4) & 0x0f), (serial_data[i] & 0x0f));
					buf_p[i*2  ] = sprintfbuf[0];
					buf_p[i*2+1] = sprintfbuf[1];
				}
			}
		}

		return rc;
	}
	#endif /* SHTPS_READ_SERIAL_NUMBER_CHECK_F01_ENABLE */
	SHTPS_LOG_ERR_PRINT("%s(): error ret=%d\n", __func__, rc);
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_fwver(struct shtps_fwctl_info *fc_p, u16 *ver_p)
{
	int rc=0;
	u8 buf[4] = { 0x00, 0x00, 0x00, 0x00};

	SHTPS_LOG_FWCTL_FUNC_CALL();
	*ver_p = 0;
	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn34.ctrlBase , buf, 4);
	if(rc == 0){
		if((buf[2] != 0x00) || (buf[3] != 0x00)){
			*ver_p = ((buf[2] << 0x08) & 0xff00) | buf[3];
		}else{
			rc = (-1);
		}
	}
	return rc;

}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_tm_mode(struct shtps_fwctl_info *fc_p)
{
	int tm_mode;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	if(fc_p->map_p->fn05.enable != 0){
		tm_mode = SHTPS_FWTESTMODE_V02;
	}else{
		tm_mode = SHTPS_FWTESTMODE_V03;
	}
	return tm_mode;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_tm_rxsize(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return SHTPS_TM_TXNUM_MAX;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_tm_txsize(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return SHTPS_TM_RXNUM_MAX;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_tm_frameline(struct shtps_fwctl_info *fc_p, u8 tm_mode, u8 *tm_data_p)
{
	int i, j;
	u8  tmp[SHTPS_TM_TXNUM_MAX * 2];
	int index = 0;
	int receive_num = shtps_fwctl_s3700_get_tm_rxsize(fc_p);
	int trans_num   = shtps_fwctl_s3700_get_tm_txsize(fc_p);

	SHTPS_LOG_FWCTL_FUNC_CALL();

	tmp[0] = 0x00;
	tmp[1] = 0x00;
	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn54.dataBase + 1, tmp, 2);

	memset(tmp, 0, sizeof(tmp));

	for(i = 0;i < trans_num;i++){
		M_READ_FUNC(fc_p, fc_p->map_p->fn54.dataBase + 3, tmp, receive_num * 2);
		index += (receive_num * 2);

		#if defined( SHTPS_SENSOR_POINT_SYMMETRY_ENABLE )
			for(j = 0;j < receive_num;j++){
				tm_data_p[(((receive_num - 1) - j) * trans_num * 2) + (((trans_num - 1) - i) * 2)]     = tmp[j * 2];
				tm_data_p[(((receive_num - 1) - j) * trans_num * 2) + (((trans_num - 1) - i) * 2) + 1] = tmp[(j * 2) + 1];
			}
		#else
			for(j = 0;j < receive_num;j++){
				tm_data_p[(j * trans_num * 2) + (i * 2)]     = tmp[j * 2];
				tm_data_p[(j * trans_num * 2) + (i * 2) + 1] = tmp[(j * 2) + 1];
			}
		#endif /* SHTPS_SENSOR_POINT_SYMMETRY_ENABLE */
	}

	#if defined( SHTPS_LOG_DEBUG_ENABLE )
	{
		for(i = 0;i < receive_num;i++){
			for(j = 0, sensor_log_outstr[0] = '\0';j < trans_num;j++){
				sprintf(sensor_log_tmp, "%6d", 
					(signed short)(tm_data_p[(i * trans_num * 2) + (j * 2) + 1] << 0x08 | 
					               tm_data_p[(i * trans_num * 2) + (j * 2)]));
				if(j < (trans_num - 1)){
					strcat(sensor_log_tmp, ", ");
				}
				strcat(sensor_log_outstr, sensor_log_tmp);
			}
			SHTPS_LOG_SENSOR_DATA_PRINT("[%02d]%s\n", i, sensor_log_outstr);
		}
	}
	#endif /* SHTPS_LOG_DEBUG_ENABLE */

	return 0;	// no error check
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_tm_baseline(struct shtps_fwctl_info *fc_p, u8 tm_mode, u8 *tm_data_p)
{
	int i, j;
	u8  tmp[SHTPS_TM_TXNUM_MAX * 2];
	int index = 0;
	int receive_num = shtps_fwctl_s3700_get_tm_rxsize(fc_p);
	int trans_num   = shtps_fwctl_s3700_get_tm_txsize(fc_p);

	SHTPS_LOG_FWCTL_FUNC_CALL();

	tmp[0] = 0x00;
	tmp[1] = 0x00;
	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn54.dataBase + 1, tmp, 2);

	for(i = 0;i < trans_num;i++){
		M_READ_FUNC(fc_p, fc_p->map_p->fn54.dataBase + 3, tmp, receive_num * 2);
		index += (receive_num * 2);
			
		#if defined( SHTPS_SENSOR_POINT_SYMMETRY_ENABLE )
			for(j = 0;j < receive_num;j++){
				tm_data_p[(((receive_num - 1) - j) * trans_num * 2) + (((trans_num - 1) - i) * 2)]     = tmp[j * 2];
				tm_data_p[(((receive_num - 1) - j) * trans_num * 2) + (((trans_num - 1) - i) * 2) + 1] = tmp[(j * 2) + 1];
			}
		#else
			for(j = 0;j < receive_num;j++){
				tm_data_p[(j * trans_num * 2) + (i * 2)]     = tmp[j * 2];
				tm_data_p[(j * trans_num * 2) + (i * 2) + 1] = tmp[(j * 2) + 1];
			}
		#endif /* SHTPS_SENSOR_POINT_SYMMETRY_ENABLE */
	}

	#if defined( SHTPS_LOG_DEBUG_ENABLE )
	{
		for(i = 0;i < receive_num;i++){
			for(j = 0, sensor_log_outstr[0] = '\0';j < trans_num;j++){
				sprintf(sensor_log_tmp, "%05d", 
					(signed short)(tm_data_p[(i * trans_num * 2) + (j * 2) + 1] << 0x08 | 
					               tm_data_p[(i * trans_num * 2) + (j * 2)]));
				if(j < (trans_num - 1)){
					strcat(sensor_log_tmp, ", ");
				}
				strcat(sensor_log_outstr, sensor_log_tmp);
			}
			SHTPS_LOG_SENSOR_DATA_PRINT("[%02d]%s\n", i, sensor_log_outstr);
		}
	}
	#endif /* SHTPS_LOG_DEBUG_ENABLE */
	
	return 0;	// no error check
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_tm_baseline_raw(struct shtps_fwctl_info *fc_p, u8 tm_mode, u8 *tm_data_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return shtps_fwctl_s3700_get_tm_baseline(fc_p, tm_mode, tm_data_p);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_tm_hybrid_adc(struct shtps_fwctl_info *fc_p, u8 tm_mode, u8 *tm_data_p)
{
	int i;
	u8  tmp[SHTPS_TM_TXNUM_MAX * 4];
	int index = 0;
	int receive_num = shtps_fwctl_s3700_get_tm_rxsize(fc_p);
	int trans_num   = shtps_fwctl_s3700_get_tm_txsize(fc_p);

	SHTPS_LOG_FWCTL_FUNC_CALL();

	tmp[0] = 0x00;
	tmp[1] = 0x00;
	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn54.dataBase + 1, tmp, 2);

	M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn54.dataBase + 3, tmp, receive_num * 4);
	index += (receive_num * 4);
		
	for(i = 0;i < receive_num;i++){
		#if defined( SHTPS_SENSOR_POINT_SYMMETRY_ENABLE )
			tm_data_p[(((receive_num - 1) - i) * 4)]     = tmp[i * 4];
			tm_data_p[(((receive_num - 1) - i) * 4) + 1] = tmp[(i * 4) + 1];
			tm_data_p[(((receive_num - 1) - i) * 4) + 2] = tmp[(i * 4) + 2];
			tm_data_p[(((receive_num - 1) - i) * 4) + 3] = tmp[(i * 4) + 3];
		#else
			tm_data_p[(i * 4)]     = tmp[i * 4];
			tm_data_p[(i * 4) + 1] = tmp[(i * 4) + 1];
			tm_data_p[(i * 4) + 2] = tmp[(i * 4) + 2];
			tm_data_p[(i * 4) + 3] = tmp[(i * 4) + 3];
		#endif /* SHTPS_SENSOR_POINT_SYMMETRY_ENABLE */
	}

	M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn54.dataBase + 3, tmp, trans_num * 4);
	index += (trans_num * 4);
		
	for(i = 0;i < trans_num;i++){
		#if defined( SHTPS_SENSOR_POINT_SYMMETRY_ENABLE )
			tm_data_p[(receive_num * 4) + (((trans_num - 1) - i) * 4)]     = tmp[i * 4];
			tm_data_p[(receive_num * 4) + (((trans_num - 1) - i) * 4) + 1] = tmp[(i * 4) + 1];
			tm_data_p[(receive_num * 4) + (((trans_num - 1) - i) * 4) + 2] = tmp[(i * 4) + 2];
			tm_data_p[(receive_num * 4) + (((trans_num - 1) - i) * 4) + 3] = tmp[(i * 4) + 3];
		#else
			tm_data_p[(receive_num * 4) + (i * 4)]     = tmp[i * 4];
			tm_data_p[(receive_num * 4) + (i * 4) + 1] = tmp[(i * 4) + 1];
			tm_data_p[(receive_num * 4) + (i * 4) + 2] = tmp[(i * 4) + 2];
			tm_data_p[(receive_num * 4) + (i * 4) + 3] = tmp[(i * 4) + 3];
		#endif /* SHTPS_SENSOR_POINT_SYMMETRY_ENABLE */
	}

	#if defined( SHTPS_LOG_DEBUG_ENABLE )
	{
		sprintf(sensor_log_outstr, "           ");
		for(i = 0;i < SHTPS_TM_RXNUM_MAX;i++){
			sprintf(sensor_log_tmp, "  [%02d]", i);
			if(i < (trans_num - 1)){
				strcat(sensor_log_tmp, " ");
			}
			strcat(sensor_log_outstr, sensor_log_tmp);
		}
		SHTPS_LOG_SENSOR_DATA_PRINT("%s\n", sensor_log_outstr);

		sprintf(sensor_log_outstr, "           ");
		for(i = 0;i < trans_num;i++){
			sprintf(sensor_log_tmp, "%6d", 
				(signed int)((tm_data_p[(receive_num * 4) + (i * 4) + 3] << 0x18) | 
				             (tm_data_p[(receive_num * 4) + (i * 4) + 2] << 0x10) | 
				             (tm_data_p[(receive_num * 4) + (i * 4) + 1] << 0x08) | 
				             (tm_data_p[(receive_num * 4) + (i * 4)])) );
			if(i < (trans_num - 1)){
				strcat(sensor_log_tmp, ",");
			}
			strcat(sensor_log_outstr, sensor_log_tmp);
		}
		SHTPS_LOG_SENSOR_DATA_PRINT("%s\n", sensor_log_outstr);

		for(i = 0;i < receive_num;i++){
			sprintf(sensor_log_outstr, "[%02d]", i);
			sprintf(sensor_log_tmp, "%6d", 
				(signed int)((tm_data_p[(i * 4) + 3] << 0x18) | 
				             (tm_data_p[(i * 4) + 2] << 0x10) | 
				             (tm_data_p[(i * 4) + 1] << 0x08) | 
				             (tm_data_p[(i * 4)])) );
			strcat(sensor_log_outstr, sensor_log_tmp);
			SHTPS_LOG_SENSOR_DATA_PRINT("%s\n", sensor_log_outstr);
		}
	}
	#endif /* SHTPS_LOG_DEBUG_ENABLE */
	
	return 0;	// no error check
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_tm_adc_range(struct shtps_fwctl_info *fc_p, u8 tm_mode, u8 *tm_data_p)
{
	int i, j;
	u8  tmp[SHTPS_TM_TXNUM_MAX * 2];
	int index = 0;
	int receive_num = shtps_fwctl_s3700_get_tm_rxsize(fc_p);
	int trans_num   = shtps_fwctl_s3700_get_tm_txsize(fc_p);

	SHTPS_LOG_FWCTL_FUNC_CALL();

	tmp[0] = 0x00;
	tmp[1] = 0x00;
	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn54.dataBase + 1, tmp, 2);

	memset(tmp, 0, sizeof(tmp));

	for(i = 0;i < trans_num;i++){
		M_READ_FUNC(fc_p, fc_p->map_p->fn54.dataBase + 3, tmp, receive_num * 2);
		index += (receive_num * 2);
			
		#if defined( SHTPS_SENSOR_POINT_SYMMETRY_ENABLE )
			for(j = 0;j < receive_num;j++){
				tm_data_p[(((receive_num - 1) - j) * trans_num * 2) + (((trans_num - 1) - i) * 2)]     = tmp[j * 2];
				tm_data_p[(((receive_num - 1) - j) * trans_num * 2) + (((trans_num - 1) - i) * 2) + 1] = tmp[(j * 2) + 1];
			}
		#else
			for(j = 0;j < receive_num;j++){
				tm_data_p[(j * trans_num * 2) + (i * 2)]     = tmp[j * 2];
				tm_data_p[(j * trans_num * 2) + (i * 2) + 1] = tmp[(j * 2) + 1];
			}
		#endif /* SHTPS_SENSOR_POINT_SYMMETRY_ENABLE */
	}

	#if defined( SHTPS_LOG_DEBUG_ENABLE )
	{
		for(i = 0;i < receive_num;i++){
			for(j = 0, sensor_log_outstr[0] = '\0';j < trans_num;j++){
				sprintf(sensor_log_tmp, "%5d", 
					(signed short)(tm_data_p[(i * trans_num * 2) + (j * 2) + 1] << 0x08 | 
					               tm_data_p[(i * trans_num * 2) + (j * 2)]));
				if(j < (trans_num - 1)){
					strcat(sensor_log_tmp, ", ");
				}
				strcat(sensor_log_outstr, sensor_log_tmp);
			}
			SHTPS_LOG_SENSOR_DATA_PRINT("[%02d]%s\n", i, sensor_log_outstr);
		}
	}
	#endif /* SHTPS_LOG_DEBUG_ENABLE */
	
	return 0;	// no error check
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_tm_moisture(struct shtps_fwctl_info *fc_p, u8 tm_mode, u8 *tm_data_p)
{
	int i, j;
	u8  tmp[SHTPS_TM_TXNUM_MAX * 2];
	int index = 0;
	int receive_num = shtps_fwctl_s3700_get_tm_rxsize(fc_p);
	int trans_num   = shtps_fwctl_s3700_get_tm_txsize(fc_p);

	SHTPS_LOG_FWCTL_FUNC_CALL();

	tmp[0] = 0x00;
	tmp[1] = 0x00;
	M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn54.dataBase + 1, tmp, 2);

	memset(tmp, 0, sizeof(tmp));

	for(i = 0;i < trans_num;i++){
		M_READ_FUNC(fc_p, fc_p->map_p->fn54.dataBase + 3, tmp, receive_num * 2);
		index += (receive_num * 2);
			
		#if defined( SHTPS_SENSOR_POINT_SYMMETRY_ENABLE )
			for(j = 0;j < receive_num;j++){
				tm_data_p[(((receive_num - 1) - j) * trans_num * 2) + (((trans_num - 1) - i) * 2)]     = tmp[j * 2];
				tm_data_p[(((receive_num - 1) - j) * trans_num * 2) + (((trans_num - 1) - i) * 2) + 1] = tmp[(j * 2) + 1];
			}
		#else
			for(j = 0;j < receive_num;j++){
				tm_data_p[(j * trans_num * 2) + (i * 2)]     = tmp[j * 2];
				tm_data_p[(j * trans_num * 2) + (i * 2) + 1] = tmp[(j * 2) + 1];
			}
		#endif /* SHTPS_SENSOR_POINT_SYMMETRY_ENABLE */
	}

	#if defined( SHTPS_LOG_DEBUG_ENABLE )
	{
		for(i = 0;i < receive_num;i++){
			for(j = 0, sensor_log_outstr[0] = '\0';j < trans_num;j++){
				sprintf(sensor_log_tmp, "%5d", 
					(signed short)(tm_data_p[(i * trans_num * 2) + (j * 2) + 1] << 0x08 | 
					               tm_data_p[(i * trans_num * 2) + (j * 2)]));
				if(j < (trans_num - 1)){
					strcat(sensor_log_tmp, ", ");
				}
				strcat(sensor_log_outstr, sensor_log_tmp);
			}
			SHTPS_LOG_SENSOR_DATA_PRINT("[%02d]%s\n", i, sensor_log_outstr);
		}
	}
	#endif /* SHTPS_LOG_DEBUG_ENABLE */
	
	return 0;	// no error check
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_tm_moisture_no_mask(struct shtps_fwctl_info *fc_p, u8 tm_mode, u8 *tm_data_p)
{
	return shtps_fwctl_s3700_get_tm_moisture(fc_p, tm_mode, tm_data_p);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_cmd_tm_frameline(struct shtps_fwctl_info *fc_p, u8 tm_mode)
{
	int rc;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.dataBase, 0x02);
	if(rc == 0)	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.commandBase, 0x01);
	if(rc != 0){
		SHTPS_LOG_DBG_PRINT("%s(): err %d\n", __func__, rc);
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_cmd_tm_baseline(struct shtps_fwctl_info *fc_p, u8 tm_mode)
{
	int rc;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.dataBase, 0x03);
	if(rc == 0)	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.commandBase, 0x01);
	if(rc != 0){
		SHTPS_LOG_ERR_PRINT("%s(): err %d\n", __func__, rc);
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_cmd_tm_baseline_raw(struct shtps_fwctl_info *fc_p, u8 tm_mode)
{
	int rc;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.commandBase, 0x01);
	if(rc != 0){
		SHTPS_LOG_ERR_PRINT("%s(): err %d\n", __func__, rc);
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_cmd_tm_hybrid_adc(struct shtps_fwctl_info *fc_p, u8 tm_mode)
{
	int rc;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.dataBase, 0x3B);
	if(rc == 0)	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.commandBase, 0x01);
	if(rc != 0){
		SHTPS_LOG_ERR_PRINT("%s(): err %d\n", __func__, rc);
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_cmd_tm_adc_range(struct shtps_fwctl_info *fc_p, u8 tm_mode)
{
	int rc;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.dataBase, 0x17);
	if(rc == 0)	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.commandBase, 0x01);
	if(rc != 0){
		SHTPS_LOG_ERR_PRINT("%s(): err %d\n", __func__, rc);
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_cmd_tm_moisture(struct shtps_fwctl_info *fc_p, u8 tm_mode)
{
	int rc;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.dataBase, 0x4C);
	if(rc == 0)	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.commandBase, 0x01);
	if(rc != 0){
		SHTPS_LOG_ERR_PRINT("%s(): err %d\n", __func__, rc);
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_cmd_tm_moisture_no_mask(struct shtps_fwctl_info *fc_p, u8 tm_mode)
{
	int rc;
	int i;
	char mask_reg_data[0x10];
	char buf[3];

	SHTPS_LOG_FWCTL_FUNC_CALL();

	if(fc_p->init_scan_param_flg == 0){
		fc_p->init_scan_param_flg = 1;

		for(i = 0; i < sizeof(mask_reg_data); i++){
			mask_reg_data[i] = 0xFF;
		}

		M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[34].addr, mask_reg_data, sizeof(mask_reg_data));

		M_READ_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[33].addr, buf, sizeof(buf));
		buf[2] = 0x00;
		M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[33].addr, buf, sizeof(buf));
	}

	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.dataBase, 0x4C);
	if(rc == 0)	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.commandBase, 0x01);
	if(rc != 0){
		SHTPS_LOG_ERR_PRINT("%s(): err %d\n", __func__, rc);
	}
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_initparam(struct shtps_fwctl_info *fc_p)
{
	int rc = 0;

	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.dataBase, 0x00);
	if(rc != 0)	goto err_exit;

	if(fc_p->map_p->fn11.enable){
		u8 data;

		rc = M_READ_FUNC(fc_p, fc_p->map_p->fn11.ctrlBase, &data, 1);
		if(rc != 0)	goto err_exit;

		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn11.ctrlBase, (data & 0xF8) | 0x00);
		if(rc != 0)	goto err_exit;
	}

	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase + 1, SHTPS_IRQ_ALL);

err_exit:
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_initparam_activemode(struct shtps_fwctl_info *fc_p)
{
	int rc = 0;

	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, 0x84);
	if(rc == 0){
		shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_ACTIVE);
	}
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_initparam_dozemode(struct shtps_fwctl_info *fc_p)
{
	int rc = 0;

	SHTPS_LOG_FWCTL_FUNC_CALL();
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, 0x80);
	if(rc == 0){
		shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_DOZE);
	}
	return rc;
}
/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_initparam_lpwgmode(struct shtps_fwctl_info *fc_p)
{
	int rc = 0;

	u8 doze_param[3] = {SHTPS_LPWG_DOZE_INTERVAL_DEF,
						SHTPS_LPWG_DOZE_WAKEUP_THRESHOLD_DEF,
						SHTPS_LPWG_DOZE_HOLDOFF_DEF};

	rc = shtps_fwctl_s3700_set_doze_param(fc_p, doze_param, sizeof(doze_param));

	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_start_testmode(struct shtps_fwctl_info *fc_p, u8 tm_mode)
{
	int rc = 0;

	SHTPS_LOG_FWCTL_FUNC_CALL();
	if(tm_mode == SHTPS_FWTESTMODE_V01){
		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, 0x04);
		if(rc != 0)	goto err_exit;
		shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_ACTIVE);

		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase + 1, 0x00);
		if(rc != 0)	goto err_exit;

		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.dataBase, 0x42);
		if(rc != 0)	goto err_exit;

		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.dataBase, 0xe1);
		if(rc != 0)	goto err_exit;

		rc = M_WRITE_FUNC(fc_p, 0xff, 0x80);
		if(rc != 0)	goto err_exit;
		rc = M_WRITE_FUNC(fc_p, 0x00, 0x01);
		if(rc != 0)	goto err_exit;

	}else{
		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase, 0x04);
		if(rc != 0)	goto err_exit;
		shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_ACTIVE);

		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase + 1, SHTPS_IRQ_ANALOG);
		if(rc != 0)	goto err_exit;
		rc = M_WRITE_FUNC(fc_p, 0xff, 0x01);
		if(rc != 0)	goto err_exit;
	}
	shtps_fwctl_s3700_set_dev_state(fc_p, SHTPS_DEV_STATE_TESTMODE);

	fc_p->init_scan_param_flg = 0;

err_exit:
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_stop_testmode(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return M_WRITE_FUNC(fc_p, 0xff, 0x00);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_baseline_offset_disable(struct shtps_fwctl_info *fc_p)
{
	u8  buf;
	int rc=0;

	SHTPS_LOG_FWCTL_FUNC_CALL();

	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL57_OFFSET, &buf, 1);
	if(rc != 0)	goto err_exit;
	#if defined(SHTPS_BASELINE_OFFSET_DISABLE_WAIT_ENABLE)
		if(SHTPS_BASELINE_OFFSET_CBC_READ_AFTER_WAIT > 0){
			msleep(SHTPS_BASELINE_OFFSET_CBC_READ_AFTER_WAIT);
		}
	#endif /* SHTPS_BASELINE_OFFSET_DISABLE_WAIT_ENABLE */
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL57_OFFSET, buf & ~0x10);
	if(rc != 0)	goto err_exit;

	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL88_OFFSET, &buf, 1);
	if(rc != 0)	goto err_exit;
	#if defined(SHTPS_BASELINE_OFFSET_DISABLE_WAIT_ENABLE)
		if(SHTPS_BASELINE_OFFSET_CBC_READ_AFTER_WAIT > 0){
			msleep(SHTPS_BASELINE_OFFSET_CBC_READ_AFTER_WAIT);
		}
	#endif /* SHTPS_BASELINE_OFFSET_DISABLE_WAIT_ENABLE */
		
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL88_OFFSET, buf & ~0x20);
	if(rc != 0)	goto err_exit;
	
	
	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL149_OFFSET, 0x00);
	if(rc != 0)	goto err_exit;
	
	
	rc = shtps_fwctl_s3700_command_force_update(fc_p);
	if(rc != 0)	goto err_exit;

	rc = shtps_fwctl_s3700_f54_command_completion_wait(fc_p, SHTPS_F54_COMMAND_WAIT_POLL_COUNT, SHTPS_F54_COMMAND_WAIT_POLL_INTERVAL);
	if(rc){
		SHTPS_LOG_ERR_PRINT("F54_CTRL88 update timed out\n");
		return rc;
	}

	#if defined(SHTPS_BASELINE_OFFSET_DISABLE_WAIT_ENABLE)
		if(SHTPS_BASELINE_OFFSET_CBC_SET_AFTER_WAIT > 0){
			msleep(SHTPS_BASELINE_OFFSET_CBC_SET_AFTER_WAIT);
		}
	#endif /* SHTPS_BASELINE_OFFSET_DISABLE_WAIT_ENABLE */

	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase,       0x04);
	if(rc != 0)	goto err_exit;

	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase + 1,   SHTPS_IRQ_ANALOG);
	if(rc != 0)	goto err_exit;

	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.dataBase,       0x14);
	if(rc != 0)	goto err_exit;

	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL41_OFFSET, 0x05);
	if(rc != 0)	goto err_exit;

	rc = shtps_fwctl_s3700_command_force_update(fc_p);
	if(rc != 0)	goto err_exit;

	rc = shtps_fwctl_s3700_f54_command_completion_wait(fc_p, SHTPS_F54_COMMAND_WAIT_POLL_COUNT, SHTPS_F54_COMMAND_WAIT_POLL_INTERVAL);
	if(rc){
		SHTPS_LOG_ERR_PRINT("F54_CTRL41 update timed out\n");
		return rc;
	}

	rc = shtps_fwctl_s3700_command_force_cal(fc_p);
	if(rc != 0)	goto err_exit;
	rc = shtps_fwctl_s3700_f54_command_completion_wait(fc_p, SHTPS_F54_COMMAND_WAIT_POLL_COUNT, SHTPS_F54_COMMAND_WAIT_POLL_INTERVAL);
	if(rc){
		SHTPS_LOG_ERR_PRINT("F54_CTRL41 update timed out\n");
		return rc;
	}

err_exit:
	return rc;
}
/* -------------------------------------------------------------------------- */
static void shtps_fwctl_s3700_set_dev_state(struct shtps_fwctl_info *fc_p, u8 state)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	if(fc_p->dev_state != state){
		SHTPS_LOG_ANALYSIS("[dev_state] set (%s -> %s)\n",
								(fc_p->dev_state == SHTPS_DEV_STATE_SLEEP) ? "sleep" :
								(fc_p->dev_state == SHTPS_DEV_STATE_DOZE) ? "doze" :
								(fc_p->dev_state == SHTPS_DEV_STATE_ACTIVE) ? "active" :
								(fc_p->dev_state == SHTPS_DEV_STATE_LPWG) ? "lpwg" :
								(fc_p->dev_state == SHTPS_DEV_STATE_LOADER) ? "loader" :
								(fc_p->dev_state == SHTPS_DEV_STATE_TESTMODE) ? "testmode" : "unknown",
								(state == SHTPS_DEV_STATE_SLEEP) ? "sleep" :
								(state == SHTPS_DEV_STATE_DOZE) ? "doze" :
								(state == SHTPS_DEV_STATE_ACTIVE) ? "active" :
								(state == SHTPS_DEV_STATE_LPWG) ? "lpwg" :
								(state == SHTPS_DEV_STATE_LOADER) ? "loader" :
								(state == SHTPS_DEV_STATE_TESTMODE) ? "testmode" : "unknown" );
	}

	fc_p->dev_state = state;
}

/* -------------------------------------------------------------------------- */
static u8 shtps_fwctl_s3700_get_dev_state(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return fc_p->dev_state;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_maxXPosition(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return fc_p->map_p->fn12.ctrl.maxXPosition;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_maxYPosition(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return fc_p->map_p->fn12.ctrl.maxYPosition;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_AnalogCMD(struct shtps_fwctl_info *fc_p, u8 *buf_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return M_READ_FUNC(fc_p, fc_p->map_p->fn54.commandBase, buf_p, 1);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_ObjectAttention(struct shtps_fwctl_info *fc_p, u8 *buf_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return M_READ_FUNC(fc_p, fc_p->map_p->fn12.data.num[15].addr, buf_p, 2);
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_lpwg_def_settings(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();

	M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[20].addr,
								fc_p->map_p->fn12_ctrl20_disable_settings, 
								sizeof(fc_p->map_p->fn12_ctrl20_disable_settings));
	
	memcpy(fc_p->map_p->fn12_ctrl20_enable_settings, 
				fc_p->map_p->fn12_ctrl20_disable_settings, sizeof(fc_p->map_p->fn12_ctrl20_enable_settings));

	fc_p->map_p->fn12_ctrl20_enable_settings[2] |= SHTPS_LPWG_REPORT_WG_ONLY;


	M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[27].addr,
								fc_p->map_p->fn12_ctrl27_disable_settings, 
								sizeof(fc_p->map_p->fn12_ctrl27_disable_settings));

	fc_p->map_p->fn12_ctrl27_disable_settings[0] = 0x00;

	memcpy(fc_p->map_p->fn12_ctrl27_enable_settings, 
				fc_p->map_p->fn12_ctrl27_disable_settings, sizeof(fc_p->map_p->fn12_ctrl27_enable_settings));

	#if defined(SHTPS_LPWG_ALLOWED_SWIPES_ENABLE)
		fc_p->map_p->fn12_ctrl27_enable_settings[SHTPS_LPWG_F12_CTRL27_BUFF_SIZE - 1] = SHTPS_LPWG_ALLOWED_SWIPES;
	#endif /* SHTPS_LPWG_ALLOWED_SWIPES_ENABLE */

	M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[18].addr, 
								fc_p->map_p->fn12_ctrl18_disable_settings, 
								sizeof(fc_p->map_p->fn12_ctrl18_disable_settings));

	memcpy(fc_p->map_p->fn12_ctrl18_enable_settings, 
				fc_p->map_p->fn12_ctrl18_disable_settings, sizeof(fc_p->map_p->fn12_ctrl18_enable_settings));

	#if defined(SHTPS_LPWG_DOUBLE_TAP_ENABLE)
		fc_p->map_p->fn12_ctrl18_enable_settings[8] = SHTPS_LPWG_MAXIMUM_TAP_TIME;
		fc_p->map_p->fn12_ctrl18_enable_settings[9] = SHTPS_LPWG_MAXIMUM_TAP_DISTANCE;
	#endif /* SHTPS_LPWG_DOUBLE_TAP_ENABLE */

	#if defined(SHTPS_LPWG_CHANGE_SWIPE_DISTANCE_ENABLE)
		if(SHTPS_LPWG_F12_CTRL18_BUFF_SIZE > 0){
			fc_p->map_p->fn12_ctrl18_enable_settings[SHTPS_LPWG_F12_CTRL18_BUFF_SIZE - 1] = SHTPS_LPWG_SWIPE_MINIMUM_DISTANCE;
		}
	#endif /* SHTPS_LPWG_CHANGE_SWIPE_DISTANCE_ENABLE */

	if(SHTPS_LPWG_CLIP_SET_TYPE == 1){
		M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[8].addr,
									fc_p->map_p->fn12_ctrl8_disable_settings, 
									sizeof(fc_p->map_p->fn12_ctrl8_disable_settings));

		memcpy(fc_p->map_p->fn12_ctrl8_enable_settings, 
					fc_p->map_p->fn12_ctrl8_disable_settings, sizeof(fc_p->map_p->fn12_ctrl8_enable_settings));
		{
			u16 rx_pitch = 0;
			u16 tx_pitch = 0;
			u8 rx_clip = 0;
			u8 tx_clip = 0;
			int tmp_clip = 0;

			rx_pitch = (((((fc_p->map_p->fn12_ctrl8_enable_settings[5] << 8) | fc_p->map_p->fn12_ctrl8_enable_settings[4]) * 100) * 16) / 65535);
			tx_pitch = (((((fc_p->map_p->fn12_ctrl8_enable_settings[7] << 8) | fc_p->map_p->fn12_ctrl8_enable_settings[6]) * 100) * 16) / 65535);

			if(rx_pitch > 0){
				tmp_clip = (((SHTPS_LPWG_RX_CLIP_AREA_MM * 100) * 128) / rx_pitch);
				if(tmp_clip > 0xFF){
					rx_clip = 0xFF;
					SHTPS_LOG_DBG_PRINT("%s(): rx_clip set req over val <0x%X>\n", __func__, tmp_clip);
				}else{
					rx_clip = (tmp_clip & 0xFF);
				}
			}
			if(tx_pitch > 0){
				tmp_clip = (((SHTPS_LPWG_TX_CLIP_AREA_MM * 100) * 128) / tx_pitch);
				if(tmp_clip > 0xFF){
					tx_clip = 0xFF;
					SHTPS_LOG_DBG_PRINT("%s(): tx_clip set req over val <0x%X>\n", __func__, tmp_clip);
				}else{
					tx_clip = (tmp_clip & 0xFF);
				}
			}

			fc_p->map_p->fn12_ctrl8_enable_settings[8]  = rx_clip;
			fc_p->map_p->fn12_ctrl8_enable_settings[9]  = rx_clip;
			fc_p->map_p->fn12_ctrl8_enable_settings[10] = tx_clip;
			fc_p->map_p->fn12_ctrl8_enable_settings[11] = tx_clip;
		}

		if(SHTPS_LPWG_F12_CTRL08_BUFF_SIZE == 15){
			fc_p->map_p->fn12_ctrl8_enable_settings[14] = SHTPS_LPWG_REPORT_BEYOND_ACTIVE_AREA;
		}
	}
	else if(SHTPS_LPWG_CLIP_SET_TYPE == 2){
		if( (fc_p->map_p->fn12.ctrl.num[18].sub_size >= 20) && (SHTPS_LPWG_F12_CTRL18_BUFF_SIZE >= 18) ){
			fc_p->map_p->fn12_ctrl18_enable_settings[0] =  (SHTPS_LPWG_CLIP_AREA_X_THRESHOLD_DOUBLE_TAP & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[1] = ((SHTPS_LPWG_CLIP_AREA_X_THRESHOLD_DOUBLE_TAP >> 8) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[2] =  (SHTPS_LPWG_CLIP_AREA_Y_THRESHOLD_DOUBLE_TAP & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[3] = ((SHTPS_LPWG_CLIP_AREA_Y_THRESHOLD_DOUBLE_TAP >> 8) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[4] =  ((CONFIG_SHTPS_SY3000_LCD_SIZE_X - 1 - SHTPS_LPWG_CLIP_AREA_X_THRESHOLD_DOUBLE_TAP) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[5] = (((CONFIG_SHTPS_SY3000_LCD_SIZE_X - 1 - SHTPS_LPWG_CLIP_AREA_X_THRESHOLD_DOUBLE_TAP) >> 8) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[6] =  ((CONFIG_SHTPS_SY3000_LCD_SIZE_Y - 1 - SHTPS_LPWG_CLIP_AREA_Y_THRESHOLD_DOUBLE_TAP) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[7] = (((CONFIG_SHTPS_SY3000_LCD_SIZE_Y - 1 - SHTPS_LPWG_CLIP_AREA_Y_THRESHOLD_DOUBLE_TAP) >> 8) & 0xFF);

			fc_p->map_p->fn12_ctrl18_enable_settings[10] =  (SHTPS_LPWG_CLIP_AREA_X_THRESHOLD_SWEEP_ON & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[11] = ((SHTPS_LPWG_CLIP_AREA_X_THRESHOLD_SWEEP_ON >> 8) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[12] =  (SHTPS_LPWG_CLIP_AREA_Y_THRESHOLD_SWEEP_ON & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[13] = ((SHTPS_LPWG_CLIP_AREA_Y_THRESHOLD_SWEEP_ON >> 8) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[14] =  ((CONFIG_SHTPS_SY3000_LCD_SIZE_X - 1 - SHTPS_LPWG_CLIP_AREA_X_THRESHOLD_SWEEP_ON) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[15] = (((CONFIG_SHTPS_SY3000_LCD_SIZE_X - 1 - SHTPS_LPWG_CLIP_AREA_X_THRESHOLD_SWEEP_ON) >> 8) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[16] =  ((CONFIG_SHTPS_SY3000_LCD_SIZE_Y - 1 - SHTPS_LPWG_CLIP_AREA_Y_THRESHOLD_SWEEP_ON) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[17] = (((CONFIG_SHTPS_SY3000_LCD_SIZE_Y - 1 - SHTPS_LPWG_CLIP_AREA_Y_THRESHOLD_SWEEP_ON) >> 8) & 0xFF);
		}
		else if( (fc_p->map_p->fn12.ctrl.num[18].sub_size >= 10) && (SHTPS_LPWG_F12_CTRL18_BUFF_SIZE >= 8) ){
			fc_p->map_p->fn12_ctrl18_enable_settings[0] =  (SHTPS_LPWG_CLIP_AREA_X_THRESHOLD_SWEEP_ON & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[1] = ((SHTPS_LPWG_CLIP_AREA_X_THRESHOLD_SWEEP_ON >> 8) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[2] =  (SHTPS_LPWG_CLIP_AREA_Y_THRESHOLD_SWEEP_ON & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[3] = ((SHTPS_LPWG_CLIP_AREA_Y_THRESHOLD_SWEEP_ON >> 8) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[4] =  ((CONFIG_SHTPS_SY3000_LCD_SIZE_X - 1 - SHTPS_LPWG_CLIP_AREA_X_THRESHOLD_SWEEP_ON) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[5] = (((CONFIG_SHTPS_SY3000_LCD_SIZE_X - 1 - SHTPS_LPWG_CLIP_AREA_X_THRESHOLD_SWEEP_ON) >> 8) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[6] =  ((CONFIG_SHTPS_SY3000_LCD_SIZE_Y - 1 - SHTPS_LPWG_CLIP_AREA_Y_THRESHOLD_SWEEP_ON) & 0xFF);
			fc_p->map_p->fn12_ctrl18_enable_settings[7] = (((CONFIG_SHTPS_SY3000_LCD_SIZE_Y - 1 - SHTPS_LPWG_CLIP_AREA_Y_THRESHOLD_SWEEP_ON) >> 8) & 0xFF);
		}
	}

	M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[15].addr,
								fc_p->map_p->fn12_ctrl15_disable_settings,
								sizeof(fc_p->map_p->fn12_ctrl15_disable_settings));
	memcpy(fc_p->map_p->fn12_ctrl15_enable_settings,
				fc_p->map_p->fn12_ctrl15_disable_settings, sizeof(fc_p->map_p->fn12_ctrl15_enable_settings));
	fc_p->map_p->fn12_ctrl15_enable_settings[5] = SHTPS_LPWG_PALM_AREA_VALUE;

	M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[22].addr,
								fc_p->map_p->fn12_ctrl22_disable_settings,
								sizeof(fc_p->map_p->fn12_ctrl22_disable_settings));
	memcpy(fc_p->map_p->fn12_ctrl22_enable_settings,
				fc_p->map_p->fn12_ctrl22_disable_settings, sizeof(fc_p->map_p->fn12_ctrl22_enable_settings));
	fc_p->map_p->fn12_ctrl22_enable_settings[0] = SHTPS_LPWG_PALM_FILTER_MODE_VALUE;

	return 0;	// no error check
}


/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_command_force_update(struct shtps_fwctl_info *fc_p)
{
	int rc = 0;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	if(fc_p->map_p->fn54.enable != 0){
		SHTPS_LOG_DBG_PRINT("[command] f54 force_update execute\n");
		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.commandBase, 0x04);
	}
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_command_force_cal(struct shtps_fwctl_info *fc_p)
{
	int rc = 0;
	SHTPS_LOG_FWCTL_FUNC_CALL();
	if(fc_p->map_p->fn54.enable != 0){
		SHTPS_LOG_DBG_PRINT("[command] f54 force_cal execute\n");
		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn54.commandBase, 0x02);
	}
	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_f54_command_completion_wait(struct shtps_fwctl_info *fc_p, int max, int interval)
{
	u8  buf   = 0xFF;
	int count = 0;
	int ret   = 0;

	SHTPS_LOG_FWCTL_FUNC_CALL();
	if( shtps_fwctl_s3700_get_dev_state(fc_p) == SHTPS_DEV_STATE_SLEEP ){
		SHTPS_LOG_DBG_PRINT("%s(): not wait by sleep state\n", __func__);
		return 0;
	}

	do{
		msleep(interval);
		M_READ_FUNC(fc_p, fc_p->map_p->fn54.commandBase, &buf, 1);
		SHTPS_LOG_DBG_PRINT("%s(): F54_COMMAND00 = 0x%02x (cnt=%d)\n", __func__, buf, count);
	}while(++count < max && buf != 0);

	if(buf != 0){
		SHTPS_LOG_ERR_PRINT("%s(): wait count over\n", __func__);
		ret = -1;
	}
	return ret;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_cover_set_report_num_max(struct shtps_fwctl_info *fc_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_cover_set_finger_amplitude_threshold(struct shtps_fwctl_info *fc_p, u8 param)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_cover_mode_on(struct shtps_fwctl_info *fc_p)
{
	int rc = 0;

	SHTPS_LOG_FWCTL_FUNC_CALL();

	#if defined(SHTPS_COVER_ENABLE)
	{
		u8 reg_param_cover_window[8];

		reg_param_cover_window[0] = (SHTPS_REG_PRM_COVER_WINDOW_LEFT & 0xFF);
		reg_param_cover_window[1] = ((SHTPS_REG_PRM_COVER_WINDOW_LEFT >> 8) & 0xFF);
		reg_param_cover_window[2] = (SHTPS_REG_PRM_COVER_WINDOW_RIGHT & 0xFF);
		reg_param_cover_window[3] = ((SHTPS_REG_PRM_COVER_WINDOW_RIGHT >> 8) & 0xFF);
		reg_param_cover_window[4] = (SHTPS_REG_PRM_COVER_WINDOW_TOP & 0xFF);
		reg_param_cover_window[5] = ((SHTPS_REG_PRM_COVER_WINDOW_TOP >> 8) & 0xFF);
		reg_param_cover_window[6] = (SHTPS_REG_PRM_COVER_WINDOW_BOTTOM & 0xFF);
		reg_param_cover_window[7] = ((SHTPS_REG_PRM_COVER_WINDOW_BOTTOM >> 8) & 0xFF);

		rc = M_WRITE_PACKET_FUNC(fc_p,
								 fc_p->map_p->fn12.ctrl.num[25].addr,
								 reg_param_cover_window, sizeof(reg_param_cover_window));
		if(rc == 0){
			u8 buf[3];
			rc = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[23].addr, buf, sizeof(buf));
			if(rc >= 0){
				buf[2] |= 0x20;
				rc = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[23].addr, buf, sizeof(buf));
			}
		}
		if(rc == 0)	rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[26].addr, 0x02);
		if(rc == 0)	rc = shtps_fwctl_s3700_command_force_update(fc_p);
		if(rc == 0)	rc = shtps_fwctl_s3700_f54_command_completion_wait(fc_p, SHTPS_F54_COMMAND_WAIT_POLL_COUNT, SHTPS_F54_COMMAND_WAIT_POLL_INTERVAL);
	}
	#endif /* SHTPS_COVER_ENABLE */

	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_cover_mode_off(struct shtps_fwctl_info *fc_p)
{
	int rc = 0;

	SHTPS_LOG_FWCTL_FUNC_CALL();

	#if defined(SHTPS_COVER_ENABLE)
		{
			u8 buf[3];
			rc = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[23].addr, buf, sizeof(buf));
			if(rc >= 0){
				buf[2] &= (~0x20);
				rc = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[23].addr, buf, sizeof(buf));
			}
		}
		rc = M_WRITE_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[26].addr, 0x00);
		if(rc == 0)	rc = shtps_fwctl_s3700_command_force_update(fc_p);
		if(rc == 0)	rc = shtps_fwctl_s3700_f54_command_completion_wait(fc_p, SHTPS_F54_COMMAND_WAIT_POLL_COUNT, SHTPS_F54_COMMAND_WAIT_POLL_INTERVAL);
	#endif /* SHTPS_COVER_ENABLE */

	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_set_custom_report_rate(struct shtps_fwctl_info *fc_p, u8 rate)
{
	int ret = 0;

	SHTPS_LOG_FUNC_CALL_INPARAM(rate);

	#if defined( SHTPS_CTRL_FW_REPORT_RATE )
		if(fc_p->map_p->fn51.enable != 0){
			ret = M_WRITE_FUNC(fc_p,
								SHTPS_F51_CUSTOM_REPORTRATE_ADR,
								rate);
			if(ret == 0){
				ret = rate;
			}
		}else{
			ret = -1;
		}
	#endif /* SHTPS_CTRL_FW_REPORT_RATE */

	return ret;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_set_lpwg_sweep_on(struct shtps_fwctl_info *fc_p, u8 enable)
{

	if(enable == 0){
		fc_p->map_p->fn12_ctrl27_enable_settings[0] &= (~0x02);
	}else{
		fc_p->map_p->fn12_ctrl27_enable_settings[0] |= 0x02;
	}

	M_WRITE_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[27].addr, fc_p->map_p->fn12_ctrl27_enable_settings[0]);

	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_set_lpwg_double_tap(struct shtps_fwctl_info *fc_p, u8 enable)
{
	if(enable == 0){
		fc_p->map_p->fn12_ctrl27_enable_settings[0] &= (~0x01);
	}else{
		fc_p->map_p->fn12_ctrl27_enable_settings[0] |= 0x01;
	}

	M_WRITE_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[27].addr, fc_p->map_p->fn12_ctrl27_enable_settings[0]);

	return 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_glove_enable(struct shtps_fwctl_info *fc_p)
{
	int ret = 0;

	SHTPS_LOG_FWCTL_FUNC_CALL();

#if defined(SHTPS_GLOVE_DETECT_ENABLE)
	if(fc_p->map_p->fn12.ctrl.support_object_glove != 0){
		#if defined(SHTPS_DEF_GLOVE_CHANGE_LANDLIFTFILTER_ENABLE)
			if(SHTPS_PRM_GLOVE_CHANGE_LLF_ENABLE == 1){
				u8 buf[8];
				ret = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[11].addr, buf, sizeof(buf));
				if(ret >= 0){
					buf[7] = SHTPS_PRM_GLOVE_CHANGE_LLF_LAND_LOCK_DISTANCE;
					M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[11].addr, buf, sizeof(buf));
				}
			}
		#endif /* SHTPS_DEF_GLOVE_CHANGE_LANDLIFTFILTER_ENABLE */
		#if defined(SHTPS_DEF_GLOVE_CHANGE_THERMAL_UPDATE_INTERVAL_ENABLE)
			if(SHTPS_PRM_GLOVE_CHANGE_TUI_ENABLE == 1){
				u8 buf[8];
				ret = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL133_OFFSET, buf, sizeof(buf));
				if(ret == 0){
					buf[4] = SHTPS_PRM_GLOVE_CHANGE_TUI_TX & 0xFF;
					buf[5] = (SHTPS_PRM_GLOVE_CHANGE_TUI_TX >> 8) & 0xFF;
					buf[6] = SHTPS_PRM_GLOVE_CHANGE_TUI_RX & 0xFF;
					buf[7] = (SHTPS_PRM_GLOVE_CHANGE_TUI_RX >> 8) & 0xFF;
					ret = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL133_OFFSET, buf, sizeof(buf));
					if(ret == 0)	ret = shtps_fwctl_s3700_command_force_update(fc_p);
					if(ret == 0)	ret = shtps_fwctl_s3700_f54_command_completion_wait(fc_p, SHTPS_F54_COMMAND_WAIT_POLL_COUNT, SHTPS_F54_COMMAND_WAIT_POLL_INTERVAL);
				}
			}
		#endif /* SHTPS_DEF_GLOVE_CHANGE_THERMAL_UPDATE_INTERVAL_ENABLE */
		#if defined(SHTPS_DEF_GLOVE_CHANGE_FREQ_SCAN_THRESHOLD_ENABLE)
			if(SHTPS_PRM_GLOVE_CHANGE_FST_ENABLE == 1){
				u8 buf[6];
				ret = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL89_OFFSET, buf, sizeof(buf));
				if(ret == 0){
					buf[4] = SHTPS_PRM_GLOVE_CHANGE_FST & 0xFF;
					buf[5] = (SHTPS_PRM_GLOVE_CHANGE_FST >> 8) & 0xFF;
					ret = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL89_OFFSET, buf, sizeof(buf));
					if(ret == 0)	ret = shtps_fwctl_s3700_command_force_update(fc_p);
					if(ret == 0)	ret = shtps_fwctl_s3700_f54_command_completion_wait(fc_p, SHTPS_F54_COMMAND_WAIT_POLL_COUNT, SHTPS_F54_COMMAND_WAIT_POLL_INTERVAL);
				}
			}
		#endif /* SHTPS_DEF_GLOVE_CHANGE_FREQ_SCAN_THRESHOLD_ENABLE */
		#if defined(SHTPS_DEF_GLOVE_DISABLE_MOISTURE_ENABLE)
			if(SHTPS_PRM_GLOVE_DISABLE_MOISTURE_ENABLE == 1){
				u8 buf[1];
				ret = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[33].addr, buf, sizeof(buf));
				if(ret == 0){
					buf[0] &= (~0x01);
					ret = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[33].addr, buf, sizeof(buf));
				}
			}
		#endif /* SHTPS_DEF_GLOVE_DISABLE_MOISTURE_ENABLE */
		fc_p->map_p->reg_F12_CTRL23_object_report_enable |= 0x20;
		ret = M_WRITE_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[23].addr, fc_p->map_p->reg_F12_CTRL23_object_report_enable);
	}else{
		SHTPS_LOG_DBG_PRINT("%s(): not supported\n", __func__);
	}
#endif /* SHTPS_GLOVE_DETECT_ENABLE */

	return ret;
}

static int shtps_fwctl_s3700_glove_disable(struct shtps_fwctl_info *fc_p)
{
	int ret = 0;

	SHTPS_LOG_FWCTL_FUNC_CALL();

#if defined(SHTPS_GLOVE_DETECT_ENABLE)
	if(fc_p->map_p->fn12.ctrl.support_object_glove != 0){
		#if defined(SHTPS_DEF_GLOVE_CHANGE_LANDLIFTFILTER_ENABLE)
			if(SHTPS_PRM_GLOVE_CHANGE_LLF_ENABLE == 1){
				u8 buf[8];
				ret = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[11].addr, buf, sizeof(buf));
				if(ret >= 0){
					buf[7] = fc_p->map_p->reg_F12_CTRL11_val[7];
					M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[11].addr, buf, sizeof(buf));
				}
			}
		#endif /* SHTPS_DEF_GLOVE_CHANGE_LANDLIFTFILTER_ENABLE */
		#if defined(SHTPS_DEF_GLOVE_CHANGE_THERMAL_UPDATE_INTERVAL_ENABLE)
			if(SHTPS_PRM_GLOVE_CHANGE_TUI_ENABLE == 1){
				u8 buf[8];
				ret = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL133_OFFSET, buf, sizeof(buf));
				if(ret == 0){
					buf[4] = fc_p->map_p->reg_fn54_ctrl133_TX_thermal_update_interval_def[0];
					buf[5] = fc_p->map_p->reg_fn54_ctrl133_TX_thermal_update_interval_def[1];
					buf[6] = fc_p->map_p->reg_fn54_ctrl133_RX_thermal_update_interval_def[0];
					buf[7] = fc_p->map_p->reg_fn54_ctrl133_RX_thermal_update_interval_def[1];
					ret = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL133_OFFSET, buf, sizeof(buf));
					if(ret == 0)	ret = shtps_fwctl_s3700_command_force_update(fc_p);
					if(ret == 0)	ret = shtps_fwctl_s3700_f54_command_completion_wait(fc_p, SHTPS_F54_COMMAND_WAIT_POLL_COUNT, SHTPS_F54_COMMAND_WAIT_POLL_INTERVAL);
				}
			}
		#endif /* SHTPS_DEF_GLOVE_CHANGE_THERMAL_UPDATE_INTERVAL_ENABLE */
		#if defined(SHTPS_DEF_GLOVE_CHANGE_FREQ_SCAN_THRESHOLD_ENABLE)
			if(SHTPS_PRM_GLOVE_CHANGE_FST_ENABLE == 1){
				u8 buf[6];
				ret = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL89_OFFSET, buf, sizeof(buf));
				if(ret == 0){
					buf[4] = fc_p->map_p->reg_fn54_ctrl89_freq_scan_threshold_def[0];
					buf[5] = fc_p->map_p->reg_fn54_ctrl89_freq_scan_threshold_def[1];
					ret = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn54.ctrlBase + SHTPS_REG_F54_ANALOG_CTRL89_OFFSET, buf, sizeof(buf));
					if(ret == 0)	ret = shtps_fwctl_s3700_command_force_update(fc_p);
					if(ret == 0)	ret = shtps_fwctl_s3700_f54_command_completion_wait(fc_p, SHTPS_F54_COMMAND_WAIT_POLL_COUNT, SHTPS_F54_COMMAND_WAIT_POLL_INTERVAL);
				}
			}
		#endif /* SHTPS_DEF_GLOVE_CHANGE_FREQ_SCAN_THRESHOLD_ENABLE */
		#if defined(SHTPS_DEF_GLOVE_DISABLE_MOISTURE_ENABLE)
			if(SHTPS_PRM_GLOVE_DISABLE_MOISTURE_ENABLE == 1){
				u8 buf[1];
				ret = M_READ_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[33].addr, buf, sizeof(buf));
				if(ret == 0){
					buf[0] = (buf[0] & (~0x01)) | (fc_p->map_p->reg_F12_CTRL33_def & 0x01);
					ret = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[33].addr, buf, sizeof(buf));
				}
			}
		#endif /* SHTPS_DEF_GLOVE_DISABLE_MOISTURE_ENABLE */
		fc_p->map_p->reg_F12_CTRL23_object_report_enable &= (~0x20);
		ret = M_WRITE_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[23].addr, fc_p->map_p->reg_F12_CTRL23_object_report_enable);
	}else{
		SHTPS_LOG_DBG_PRINT("%s(): not supported\n", __func__);
	}
#endif /* SHTPS_GLOVE_DETECT_ENABLE */

	return ret;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_productid(struct shtps_fwctl_info *fc_p, u8 *buf_p)
{
	char productID[11];
	int rc = 0;

	SHTPS_LOG_FWCTL_FUNC_CALL();

	rc = M_READ_FUNC(fc_p, fc_p->map_p->fn01.queryBase + 11, productID, sizeof(productID));
	productID[10] = '\0';
	if(rc == 0){
		strcpy(buf_p, &productID[0]);
	}

	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_update_max_position(struct shtps_fwctl_info *fc_p, int x, int y)
{
	int rc = 0;
	u8 maxPosition[4];

	fc_p->map_p->fn12.ctrl.maxXPosition = x;
	fc_p->map_p->fn12.ctrl.maxYPosition = y;
	maxPosition[0] = (x & 0xff);
	maxPosition[1] = ((x >> 8) & 0xff);
	maxPosition[2] = (y & 0xff);
	maxPosition[3] = ((y >> 8) & 0xff);
	rc = M_WRITE_PACKET_FUNC(fc_p, fc_p->map_p->fn12.ctrl.num[8].addr, maxPosition, 4);

	return rc;
}

/* -------------------------------------------------------------------------- */
static int shtps_fwctl_s3700_get_interrupt_enable(struct shtps_fwctl_info *fc_p, u8 *buf_p)
{
	SHTPS_LOG_FWCTL_FUNC_CALL();
	return M_READ_FUNC(fc_p, fc_p->map_p->fn01.ctrlBase + 1, buf_p, 1);
}

/* -------------------------------------------------------------------------- */

struct shtps_fwctl_functbl		shtps_fwctl_s3700_function_table = {
		.ic_init_f									= shtps_fwctl_s3700_ic_init,
		.loader_init_writeconfig_f					= shtps_fwctl_s3700_loader_init_writeconfig,
		.loader_cmd_get_partition_table_f			= shtps_fwctl_s3700_loader_cmd_get_partition_table,
		.loader_get_partition_table_f				= shtps_fwctl_s3700_loader_get_partition_table,
		.loader_get_config_blocknum_f				= shtps_fwctl_s3700_loader_get_config_blocknum,
		.loader_get_firm_blocknum_f					= shtps_fwctl_s3700_loader_get_firm_blocknum,
		.loader_get_blocksize_f						= shtps_fwctl_s3700_loader_get_blocksize,
		.loader_get_result_writeconfig_f			= shtps_fwctl_s3700_loader_get_result_writeconfig,
		.loader_get_result_writeimage_f				= shtps_fwctl_s3700_loader_get_result_writeimage,
		.loader_get_result_erase_f					= shtps_fwctl_s3700_loader_get_result_erase,
		.loader_write_config_f						= shtps_fwctl_s3700_loader_write_config,
		.loader_write_image_f						= shtps_fwctl_s3700_loader_write_image,
		.loader_cmd_f								= shtps_fwctl_s3700_loader_cmd,
		.loader_cmd_erase_f							= shtps_fwctl_s3700_loader_cmd_erase,
		.loader_cmd_erase_config_f					= shtps_fwctl_s3700_loader_cmd_erase_config,
		.loader_cmd_writeimage_f					= shtps_fwctl_s3700_loader_cmd_writeimage,
		.loader_cmd_writeconfig_f					= shtps_fwctl_s3700_loader_cmd_writeconfig,
		.loader_cmd_enterbl_f						= shtps_fwctl_s3700_loader_cmd_enterbl,
		.loader_exit_f								= shtps_fwctl_s3700_loader_exit,
		.check_crc_error_f							= shtps_fwctl_s3700_check_crc_error,
		.get_device_status_f						= shtps_fwctl_s3700_get_device_status,
		.soft_reset_f								= shtps_fwctl_s3700_soft_reset,
		.irqclear_get_irqfactor_f					= shtps_fwctl_s3700_irqclear_get_irqfactor,
		.rezero_f									= shtps_fwctl_s3700_rezero,
		.get_pageselect_f							= shtps_fwctl_s3700_get_pageselect,
		.map_construct_f							= shtps_fwctl_s3700_map_construct,
		.is_sleeping_f								= shtps_fwctl_s3700_is_sleeping,
		.set_doze_f									= shtps_fwctl_s3700_set_doze,
		.set_doze_param_f							= shtps_fwctl_s3700_set_doze_param,
		.set_active_f								= shtps_fwctl_s3700_set_active,
		.set_sleepmode_on_f							= shtps_fwctl_s3700_set_sleepmode_on,
		.set_sleepmode_off_f						= shtps_fwctl_s3700_set_sleepmode_off,
		.set_lpwg_mode_on_f							= shtps_fwctl_s3700_set_lpwg_mode_on,
		.set_lpwg_mode_off_f						= shtps_fwctl_s3700_set_lpwg_mode_off,
		.set_lpwg_mode_cal_f						= shtps_fwctl_s3700_set_lpwg_mode_cal,
		.get_fingermax_f							= shtps_fwctl_s3700_get_fingermax,
		.get_fingerinfo_f							= shtps_fwctl_s3700_get_fingerinfo,
		.get_one_fingerinfo_f						= shtps_fwctl_s3700_get_one_fingerinfo,
		.get_finger_info_buf_f						= shtps_fwctl_s3700_get_finger_info_buf,
		.get_finger_state_f							= shtps_fwctl_s3700_get_finger_state,
		.get_finger_pos_x_f							= shtps_fwctl_s3700_get_finger_pos_x,
		.get_finger_pos_y_f							= shtps_fwctl_s3700_get_finger_pos_y,
		.get_finger_wx_f							= shtps_fwctl_s3700_get_finger_wx,
		.get_finger_wy_f							= shtps_fwctl_s3700_get_finger_wy,
		.get_finger_z_f								= shtps_fwctl_s3700_get_finger_z,
		.get_gesture_f								= shtps_fwctl_s3700_get_gesture,
		.get_gesturetype_f							= shtps_fwctl_s3700_get_gesturetype,
		.get_fwdate_f								= shtps_fwctl_s3700_get_fwdate,
		.get_serial_number_f						= shtps_fwctl_s3700_get_serial_number,
		.get_fwver_f								= shtps_fwctl_s3700_get_fwver,
		.get_tm_mode_f								= shtps_fwctl_s3700_get_tm_mode,
		.get_tm_rxsize_f							= shtps_fwctl_s3700_get_tm_rxsize,
		.get_tm_txsize_f							= shtps_fwctl_s3700_get_tm_txsize,
		.get_tm_frameline_f							= shtps_fwctl_s3700_get_tm_frameline,
		.get_tm_baseline_f							= shtps_fwctl_s3700_get_tm_baseline,
		.get_tm_baseline_raw_f						= shtps_fwctl_s3700_get_tm_baseline_raw,
		.get_tm_hybrid_adc_f						= shtps_fwctl_s3700_get_tm_hybrid_adc,
		.get_tm_adc_range_f							= shtps_fwctl_s3700_get_tm_adc_range,
		.get_tm_moisture_f							= shtps_fwctl_s3700_get_tm_moisture,
		.get_tm_moisture_no_mask_f					= shtps_fwctl_s3700_get_tm_moisture_no_mask,
		.cmd_tm_frameline_f							= shtps_fwctl_s3700_cmd_tm_frameline,
		.cmd_tm_baseline_f							= shtps_fwctl_s3700_cmd_tm_baseline,
		.cmd_tm_baseline_raw_f						= shtps_fwctl_s3700_cmd_tm_baseline_raw,
		.cmd_tm_hybrid_adc_f						= shtps_fwctl_s3700_cmd_tm_hybrid_adc,
		.cmd_tm_adc_range_f							= shtps_fwctl_s3700_cmd_tm_adc_range,
		.cmd_tm_moisture_f							= shtps_fwctl_s3700_cmd_tm_moisture,
		.cmd_tm_moisture_no_mask_f					= shtps_fwctl_s3700_cmd_tm_moisture_no_mask,
		.initparam_f								= shtps_fwctl_s3700_initparam,
		.initparam_activemode_f						= shtps_fwctl_s3700_initparam_activemode,
		.initparam_dozemode_f						= shtps_fwctl_s3700_initparam_dozemode,
		.initparam_lpwgmode_f						= shtps_fwctl_s3700_initparam_lpwgmode,
		.start_testmode_f							= shtps_fwctl_s3700_start_testmode,
		.stop_testmode_f							= shtps_fwctl_s3700_stop_testmode,
		.baseline_offset_disable_f					= shtps_fwctl_s3700_baseline_offset_disable,
		.set_dev_state_f							= shtps_fwctl_s3700_set_dev_state,
		.get_dev_state_f							= shtps_fwctl_s3700_get_dev_state,
		.get_maxXPosition_f							= shtps_fwctl_s3700_get_maxXPosition,
		.get_maxYPosition_f							= shtps_fwctl_s3700_get_maxYPosition,
		.get_AnalogCMD_f							= shtps_fwctl_s3700_get_AnalogCMD,
		.get_ObjectAttention_f						= shtps_fwctl_s3700_get_ObjectAttention,
		.cover_set_report_num_max_f					= shtps_fwctl_s3700_cover_set_report_num_max,
		.cover_set_finger_amplitude_threshold_f		= shtps_fwctl_s3700_cover_set_finger_amplitude_threshold,
		.cover_mode_on_f							= shtps_fwctl_s3700_cover_mode_on,
		.cover_mode_off_f							= shtps_fwctl_s3700_cover_mode_off,
		.set_custom_report_rate_f					= shtps_fwctl_s3700_set_custom_report_rate,
		.set_lpwg_sweep_on_f						= shtps_fwctl_s3700_set_lpwg_sweep_on,
		.set_lpwg_double_tap_f						= shtps_fwctl_s3700_set_lpwg_double_tap,
		.glove_enable_f								= shtps_fwctl_s3700_glove_enable,
		.glove_disable_f							= shtps_fwctl_s3700_glove_disable,
		.get_productid_f							= shtps_fwctl_s3700_get_productid,
		.update_max_position_f						= shtps_fwctl_s3700_update_max_position,
		.get_interrupt_enable_f						= shtps_fwctl_s3700_get_interrupt_enable,
};
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
