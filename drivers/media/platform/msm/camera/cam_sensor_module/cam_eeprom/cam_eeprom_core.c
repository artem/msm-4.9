/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/crc32.h>
#include <media/cam_sensor.h>

#include "cam_eeprom_core.h"
#include "cam_eeprom_soc.h"
#include "cam_debug_util.h"

/* SHLOCAL_CAMERA_IMAGE_QUALITY-> */
#include "shading_correct.h"
/* SHLOCAL_CAMERA_IMAGE_QUALITY<- */

/**
 * cam_eeprom_read_memory() - read map data into buffer
 * @e_ctrl:     eeprom control struct
 * @block:      block to be read
 *
 * This function iterates through blocks stored in block->map, reads each
 * region and concatenate them into the pre-allocated block->mapdata
 */
static int cam_eeprom_read_memory(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_eeprom_memory_block_t *block)
{
	int                                rc = 0;
	int                                j;
	struct cam_sensor_i2c_reg_setting  i2c_reg_settings;
	struct cam_sensor_i2c_reg_array    i2c_reg_array;
	struct cam_eeprom_memory_map_t    *emap = block->map;
	struct cam_eeprom_soc_private     *eb_info;
	uint8_t                           *memptr = block->mapdata;

	if (!e_ctrl) {
		CAM_ERR(CAM_EEPROM, "e_ctrl is NULL");
		return -EINVAL;
	}

	eb_info = (struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;

	for (j = 0; j < block->num_map; j++) {
		CAM_DBG(CAM_EEPROM, "slave-addr = 0x%X", emap[j].saddr);
		if (emap[j].saddr) {
			eb_info->i2c_info.slave_addr = emap[j].saddr;
			rc = cam_eeprom_update_i2c_info(e_ctrl,
				&eb_info->i2c_info);
			if (rc) {
				CAM_ERR(CAM_EEPROM,
					"failed: to update i2c info rc %d",
					rc);
				return rc;
			}
		}

		if (emap[j].page.valid_size) {
			i2c_reg_settings.addr_type = emap[j].page.addr_type;
			i2c_reg_settings.data_type = emap[j].page.data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = emap[j].page.addr;
			i2c_reg_array.reg_data = emap[j].page.data;
/* SHLOCAL_CAMERA_DRIVERS-> */
#if 0
			i2c_reg_array.delay = emap[j].page.delay;
#else
			i2c_reg_settings.delay = emap[j].page.delay;
#endif
/* SHLOCAL_CAMERA_DRIVERS<- */
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);
			if (rc) {
				CAM_ERR(CAM_EEPROM, "page write failed rc %d",
					rc);
				return rc;
			}
		}

		if (emap[j].pageen.valid_size) {
			i2c_reg_settings.addr_type = emap[j].pageen.addr_type;
			i2c_reg_settings.data_type = emap[j].pageen.data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = emap[j].pageen.addr;
			i2c_reg_array.reg_data = emap[j].pageen.data;
/* SHLOCAL_CAMERA_DRIVERS-> */
#if 0
			i2c_reg_array.delay = emap[j].pageen.delay;
#else
			i2c_reg_settings.delay = emap[j].pageen.delay;
#endif
/* SHLOCAL_CAMERA_DRIVERS<- */
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);
			if (rc) {
				CAM_ERR(CAM_EEPROM, "page enable failed rc %d",
					rc);
				return rc;
			}
		}

		if (emap[j].poll.valid_size) {
			rc = camera_io_dev_poll(&e_ctrl->io_master_info,
				emap[j].poll.addr, emap[j].poll.data,
				0, emap[j].poll.addr_type,
				emap[j].poll.data_type,
				emap[j].poll.delay);
			if (rc) {
				CAM_ERR(CAM_EEPROM, "poll failed rc %d",
					rc);
				return rc;
			}
		}

		if (emap[j].mem.valid_size) {
			rc = camera_io_dev_read_seq(&e_ctrl->io_master_info,
				emap[j].mem.addr, memptr,
				emap[j].mem.addr_type,
				emap[j].mem.data_type,
				emap[j].mem.valid_size);
			if (rc) {
				CAM_ERR(CAM_EEPROM, "read failed rc %d",
					rc);
				return rc;
			}
			memptr += emap[j].mem.valid_size;
		}

		if (emap[j].pageen.valid_size) {
			i2c_reg_settings.addr_type = emap[j].pageen.addr_type;
			i2c_reg_settings.data_type = emap[j].pageen.data_type;
			i2c_reg_settings.size = 1;
			i2c_reg_array.reg_addr = emap[j].pageen.addr;
			i2c_reg_array.reg_data = 0;
/* SHLOCAL_CAMERA_DRIVERS-> */
#if 0
			i2c_reg_array.delay = emap[j].pageen.delay;
#else
			i2c_reg_settings.delay = emap[j].pageen.delay;
#endif
/* SHLOCAL_CAMERA_DRIVERS<- */
			i2c_reg_settings.reg_setting = &i2c_reg_array;
			rc = camera_io_dev_write(&e_ctrl->io_master_info,
				&i2c_reg_settings);
			if (rc) {
				CAM_ERR(CAM_EEPROM,
					"page disable failed rc %d",
					rc);
				return rc;
			}
		}
	}
	return rc;
}

/**
 * cam_eeprom_power_up - Power up eeprom hardware
 * @e_ctrl:     ctrl structure
 * @power_info: power up/down info for eeprom
 *
 * Returns success or failure
 */
static int cam_eeprom_power_up(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_sensor_power_ctrl_t *power_info)
{
	int32_t                 rc = 0;
	struct cam_hw_soc_info *soc_info =
		&e_ctrl->soc_info;

	/* Parse and fill vreg params for power up settings */
	rc = msm_camera_fill_vreg_params(
		&e_ctrl->soc_info,
		power_info->power_setting,
		power_info->power_setting_size);
	if (rc) {
		CAM_ERR(CAM_EEPROM,
			"failed to fill power up vreg params rc:%d", rc);
		return rc;
	}

	/* Parse and fill vreg params for power down settings*/
	rc = msm_camera_fill_vreg_params(
		&e_ctrl->soc_info,
		power_info->power_down_setting,
		power_info->power_down_setting_size);
	if (rc) {
		CAM_ERR(CAM_EEPROM,
			"failed to fill power down vreg params  rc:%d", rc);
		return rc;
	}

	power_info->dev = soc_info->dev;

	rc = cam_sensor_core_power_up(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "failed in eeprom power up rc %d", rc);
		return rc;
	}

	if (e_ctrl->io_master_info.master_type == CCI_MASTER) {
		rc = camera_io_init(&(e_ctrl->io_master_info));
		if (rc) {
			CAM_ERR(CAM_EEPROM, "cci_init failed");
			return -EINVAL;
		}
	}
	return rc;
}

/**
 * cam_eeprom_power_down - Power down eeprom hardware
 * @e_ctrl:    ctrl structure
 *
 * Returns success or failure
 */
static int cam_eeprom_power_down(struct cam_eeprom_ctrl_t *e_ctrl)
{
	struct cam_sensor_power_ctrl_t *power_info;
	struct cam_hw_soc_info         *soc_info;
	struct cam_eeprom_soc_private  *soc_private;
	int                             rc = 0;

	if (!e_ctrl) {
		CAM_ERR(CAM_EEPROM, "failed: e_ctrl %pK", e_ctrl);
		return -EINVAL;
	}

	soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;
	soc_info = &e_ctrl->soc_info;

	if (!power_info) {
		CAM_ERR(CAM_EEPROM, "failed: power_info %pK", power_info);
		return -EINVAL;
	}
	rc = msm_camera_power_down(power_info, soc_info);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "power down the core is failed:%d", rc);
		return rc;
	}

	if (e_ctrl->io_master_info.master_type == CCI_MASTER)
		camera_io_release(&(e_ctrl->io_master_info));

	return rc;
}

/**
 * cam_eeprom_match_id - match eeprom id
 * @e_ctrl:     ctrl structure
 *
 * Returns success or failure
 */
static int cam_eeprom_match_id(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int                      rc;
	struct camera_io_master *client = &e_ctrl->io_master_info;
	uint8_t                  id[2];

	rc = cam_spi_query_id(client, 0, CAMERA_SENSOR_I2C_TYPE_WORD,
		&id[0], 2);
	if (rc)
		return rc;
	CAM_DBG(CAM_EEPROM, "read 0x%x 0x%x, check 0x%x 0x%x",
		id[0], id[1], client->spi_client->mfr_id0,
		client->spi_client->device_id0);
	if (id[0] != client->spi_client->mfr_id0
		|| id[1] != client->spi_client->device_id0)
		return -ENODEV;
	return 0;
}

/**
 * cam_eeprom_parse_read_memory_map - Parse memory map
 * @of_node:    device node
 * @e_ctrl:     ctrl structure
 *
 * Returns success or failure
 */
int32_t cam_eeprom_parse_read_memory_map(struct device_node *of_node,
	struct cam_eeprom_ctrl_t *e_ctrl)
{
	int32_t                         rc = 0;
	struct cam_eeprom_soc_private  *soc_private;
	struct cam_sensor_power_ctrl_t *power_info;

	if (!e_ctrl) {
		CAM_ERR(CAM_EEPROM, "failed: e_ctrl is NULL");
		return -EINVAL;
	}

	soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	rc = cam_eeprom_parse_dt_memory_map(of_node, &e_ctrl->cal_data);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "failed: eeprom dt parse rc %d", rc);
		return rc;
	}
	rc = cam_eeprom_power_up(e_ctrl, power_info);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "failed: eeprom power up rc %d", rc);
		goto data_mem_free;
	}

	e_ctrl->cam_eeprom_state = CAM_EEPROM_CONFIG;
	if (e_ctrl->eeprom_device_type == MSM_CAMERA_SPI_DEVICE) {
		rc = cam_eeprom_match_id(e_ctrl);
		if (rc) {
			CAM_DBG(CAM_EEPROM, "eeprom not matching %d", rc);
			goto power_down;
		}
	}
	rc = cam_eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
	if (rc) {
		CAM_ERR(CAM_EEPROM, "read_eeprom_memory failed");
		goto power_down;
	}

	rc = cam_eeprom_power_down(e_ctrl);
	if (rc)
		CAM_ERR(CAM_EEPROM, "failed: eeprom power down rc %d", rc);

	e_ctrl->cam_eeprom_state = CAM_EEPROM_ACQUIRE;
	return rc;
power_down:
	cam_eeprom_power_down(e_ctrl);
data_mem_free:
	vfree(e_ctrl->cal_data.mapdata);
	vfree(e_ctrl->cal_data.map);
	e_ctrl->cal_data.num_data = 0;
	e_ctrl->cal_data.num_map = 0;
	e_ctrl->cam_eeprom_state = CAM_EEPROM_ACQUIRE;
	return rc;
}

/**
 * cam_eeprom_get_dev_handle - get device handle
 * @e_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_get_dev_handle(struct cam_eeprom_ctrl_t *e_ctrl,
	void *arg)
{
	struct cam_sensor_acquire_dev    eeprom_acq_dev;
	struct cam_create_dev_hdl        bridge_params;
	struct cam_control              *cmd = (struct cam_control *)arg;

	if (e_ctrl->bridge_intf.device_hdl != -1) {
		CAM_ERR(CAM_EEPROM, "Device is already acquired");
		return -EFAULT;
	}
	if (copy_from_user(&eeprom_acq_dev, (void __user *) cmd->handle,
		sizeof(eeprom_acq_dev))) {
		CAM_ERR(CAM_EEPROM,
			"EEPROM:ACQUIRE_DEV: copy from user failed");
		return -EFAULT;
	}

	bridge_params.session_hdl = eeprom_acq_dev.session_handle;
	bridge_params.ops = &e_ctrl->bridge_intf.ops;
	bridge_params.v4l2_sub_dev_flag = 0;
	bridge_params.media_entity_flag = 0;
	bridge_params.priv = e_ctrl;

	eeprom_acq_dev.device_handle =
		cam_create_device_hdl(&bridge_params);
	e_ctrl->bridge_intf.device_hdl = eeprom_acq_dev.device_handle;
	e_ctrl->bridge_intf.session_hdl = eeprom_acq_dev.session_handle;

	CAM_DBG(CAM_EEPROM, "Device Handle: %d", eeprom_acq_dev.device_handle);
	if (copy_to_user((void __user *) cmd->handle, &eeprom_acq_dev,
		sizeof(struct cam_sensor_acquire_dev))) {
		CAM_ERR(CAM_EEPROM, "EEPROM:ACQUIRE_DEV: copy to user failed");
		return -EFAULT;
	}
	return 0;
}

/**
 * cam_eeprom_update_slaveInfo - Update slave info
 * @e_ctrl:     ctrl structure
 * @cmd_buf:    command buffer
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_update_slaveInfo(struct cam_eeprom_ctrl_t *e_ctrl,
	void *cmd_buf)
{
	int32_t                         rc = 0;
	struct cam_eeprom_soc_private  *soc_private;
	struct cam_cmd_i2c_info        *cmd_i2c_info = NULL;

	soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	cmd_i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
	soc_private->i2c_info.slave_addr = cmd_i2c_info->slave_addr;
	soc_private->i2c_info.i2c_freq_mode = cmd_i2c_info->i2c_freq_mode;

	rc = cam_eeprom_update_i2c_info(e_ctrl,
		&soc_private->i2c_info);
	CAM_DBG(CAM_EEPROM, "Slave addr: 0x%x Freq Mode: %d",
		soc_private->i2c_info.slave_addr,
		soc_private->i2c_info.i2c_freq_mode);

	return rc;
}

/**
 * cam_eeprom_parse_memory_map - Parse memory map info
 * @data:             memory block data
 * @cmd_buf:          command buffer
 * @cmd_length:       command buffer length
 * @num_map:          memory map size
 * @cmd_length_bytes: command length processed in this function
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_parse_memory_map(
	struct cam_eeprom_memory_block_t *data,
	void *cmd_buf, int cmd_length, uint16_t *cmd_length_bytes,
	int *num_map)
{
	int32_t                            rc = 0;
	int32_t                            cnt = 0;
	int32_t                            processed_size = 0;
	uint8_t                            generic_op_code;
	struct cam_eeprom_memory_map_t    *map = data->map;
	struct common_header              *cmm_hdr =
		(struct common_header *)cmd_buf;
	uint16_t                           cmd_length_in_bytes = 0;
	struct cam_cmd_i2c_random_wr      *i2c_random_wr = NULL;
	struct cam_cmd_i2c_continuous_rd  *i2c_cont_rd = NULL;
	struct cam_cmd_conditional_wait   *i2c_poll = NULL;
	struct cam_cmd_unconditional_wait *i2c_uncond_wait = NULL;

	generic_op_code = cmm_hdr->third_byte;
	switch (cmm_hdr->cmd_type) {
	case CAMERA_SENSOR_CMD_TYPE_I2C_RNDM_WR:
		i2c_random_wr = (struct cam_cmd_i2c_random_wr *)cmd_buf;
		cmd_length_in_bytes   = sizeof(struct cam_cmd_i2c_random_wr) +
			((i2c_random_wr->header.count - 1) *
			sizeof(struct i2c_random_wr_payload));

		for (cnt = 0; cnt < (i2c_random_wr->header.count);
			cnt++) {
			map[*num_map + cnt].page.addr =
				i2c_random_wr->random_wr_payload[cnt].reg_addr;
			map[*num_map + cnt].page.addr_type =
				i2c_random_wr->header.addr_type;
			map[*num_map + cnt].page.data =
				i2c_random_wr->random_wr_payload[cnt].reg_data;
			map[*num_map + cnt].page.data_type =
				i2c_random_wr->header.data_type;
			map[*num_map + cnt].page.valid_size = 1;
		}

		*num_map += (i2c_random_wr->header.count - 1);
		cmd_buf += cmd_length_in_bytes / sizeof(int32_t);
		processed_size +=
			cmd_length_in_bytes;
		break;
	case CAMERA_SENSOR_CMD_TYPE_I2C_CONT_RD:
		i2c_cont_rd = (struct cam_cmd_i2c_continuous_rd *)cmd_buf;
		cmd_length_in_bytes = sizeof(struct cam_cmd_i2c_continuous_rd);

		map[*num_map].mem.addr = i2c_cont_rd->reg_addr;
		map[*num_map].mem.addr_type = i2c_cont_rd->header.addr_type;
		map[*num_map].mem.data_type = i2c_cont_rd->header.data_type;
		map[*num_map].mem.valid_size =
			i2c_cont_rd->header.count;
		cmd_buf += cmd_length_in_bytes / sizeof(int32_t);
		processed_size +=
			cmd_length_in_bytes;
		data->num_data += map[*num_map].mem.valid_size;
		break;
	case CAMERA_SENSOR_CMD_TYPE_WAIT:
		if (generic_op_code ==
			CAMERA_SENSOR_WAIT_OP_HW_UCND ||
			generic_op_code ==
			CAMERA_SENSOR_WAIT_OP_SW_UCND) {
			i2c_uncond_wait =
				(struct cam_cmd_unconditional_wait *)cmd_buf;
			cmd_length_in_bytes =
				sizeof(struct cam_cmd_unconditional_wait);

			if (*num_map < 1) {
				CAM_ERR(CAM_EEPROM,
					"invalid map number, num_map=%d",
					*num_map);
				return -EINVAL;
			}

			/*
			 * Though delay is added all of them, but delay will
			 * be applicable to only one of them as only one of
			 * them will have valid_size set to >= 1.
			 */
			map[*num_map - 1].mem.delay = i2c_uncond_wait->delay;
			map[*num_map - 1].page.delay = i2c_uncond_wait->delay;
			map[*num_map - 1].pageen.delay = i2c_uncond_wait->delay;
		} else if (generic_op_code ==
			CAMERA_SENSOR_WAIT_OP_COND) {
			i2c_poll = (struct cam_cmd_conditional_wait *)cmd_buf;
			cmd_length_in_bytes =
				sizeof(struct cam_cmd_conditional_wait);

			map[*num_map].poll.addr = i2c_poll->reg_addr;
			map[*num_map].poll.addr_type = i2c_poll->addr_type;
			map[*num_map].poll.data = i2c_poll->reg_data;
			map[*num_map].poll.data_type = i2c_poll->data_type;
			map[*num_map].poll.delay = i2c_poll->timeout;
			map[*num_map].poll.valid_size = 1;
		}
		cmd_buf += cmd_length_in_bytes / sizeof(int32_t);
		processed_size +=
			cmd_length_in_bytes;
		break;
	default:
		break;
	}

	*cmd_length_bytes = processed_size;
	return rc;
}

/**
 * cam_eeprom_init_pkt_parser - Parse eeprom packet
 * @e_ctrl:       ctrl structure
 * @csl_packet:	  csl packet received
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_init_pkt_parser(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_packet *csl_packet)
{
	int32_t                         rc = 0;
	int                             i = 0;
	struct cam_cmd_buf_desc        *cmd_desc = NULL;
	uint32_t                       *offset = NULL;
	uint32_t                       *cmd_buf = NULL;
	uint64_t                        generic_pkt_addr;
	size_t                          pkt_len = 0;
	uint32_t                        total_cmd_buf_in_bytes = 0;
	uint32_t                        processed_cmd_buf_in_bytes = 0;
	struct common_header           *cmm_hdr = NULL;
	uint16_t                        cmd_length_in_bytes = 0;
	struct cam_cmd_i2c_info        *i2c_info = NULL;
	int                             num_map = -1;
	struct cam_eeprom_memory_map_t *map = NULL;
	struct cam_eeprom_soc_private  *soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info = &soc_private->power_info;

	e_ctrl->cal_data.map = vzalloc((MSM_EEPROM_MEMORY_MAP_MAX_SIZE *
		MSM_EEPROM_MAX_MEM_MAP_CNT) *
		(sizeof(struct cam_eeprom_memory_map_t)));
	if (!e_ctrl->cal_data.map) {
		rc = -ENOMEM;
		CAM_ERR(CAM_EEPROM, "failed");
		return rc;
	}
	map = e_ctrl->cal_data.map;

	offset = (uint32_t *)&csl_packet->payload;
	offset += (csl_packet->cmd_buf_offset / sizeof(uint32_t));
	cmd_desc = (struct cam_cmd_buf_desc *)(offset);

	/* Loop through multiple command buffers */
	for (i = 0; i < csl_packet->num_cmd_buf; i++) {
		total_cmd_buf_in_bytes = cmd_desc[i].length;
		processed_cmd_buf_in_bytes = 0;
		if (!total_cmd_buf_in_bytes)
			continue;
		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			(uint64_t *)&generic_pkt_addr, &pkt_len);
		if (rc) {
			CAM_ERR(CAM_EEPROM, "Failed to get cpu buf");
			return rc;
		}
		cmd_buf = (uint32_t *)generic_pkt_addr;
		if (!cmd_buf) {
			CAM_ERR(CAM_EEPROM, "invalid cmd buf");
			return -EINVAL;
		}
		cmd_buf += cmd_desc[i].offset / sizeof(uint32_t);
		/* Loop through multiple cmd formats in one cmd buffer */
		while (processed_cmd_buf_in_bytes < total_cmd_buf_in_bytes) {
			cmm_hdr = (struct common_header *)cmd_buf;
			switch (cmm_hdr->cmd_type) {
			case CAMERA_SENSOR_CMD_TYPE_I2C_INFO:
				i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;
				/* Configure the following map slave address */
				map[num_map + 1].saddr = i2c_info->slave_addr;
				rc = cam_eeprom_update_slaveInfo(e_ctrl,
					cmd_buf);
				cmd_length_in_bytes =
					sizeof(struct cam_cmd_i2c_info);
				processed_cmd_buf_in_bytes +=
					cmd_length_in_bytes;
				cmd_buf += cmd_length_in_bytes/
					sizeof(uint32_t);
				break;
			case CAMERA_SENSOR_CMD_TYPE_PWR_UP:
			case CAMERA_SENSOR_CMD_TYPE_PWR_DOWN:
				cmd_length_in_bytes = total_cmd_buf_in_bytes;
				rc = cam_sensor_update_power_settings(cmd_buf,
					cmd_length_in_bytes, power_info);
				processed_cmd_buf_in_bytes +=
					cmd_length_in_bytes;
				cmd_buf += cmd_length_in_bytes/
					sizeof(uint32_t);
				if (rc) {
					CAM_ERR(CAM_EEPROM, "Failed");
					return rc;
				}
				break;
			case CAMERA_SENSOR_CMD_TYPE_I2C_RNDM_WR:
			case CAMERA_SENSOR_CMD_TYPE_I2C_CONT_RD:
			case CAMERA_SENSOR_CMD_TYPE_WAIT:
				num_map++;
				rc = cam_eeprom_parse_memory_map(
					&e_ctrl->cal_data, cmd_buf,
					total_cmd_buf_in_bytes,
					&cmd_length_in_bytes, &num_map);
				processed_cmd_buf_in_bytes +=
					cmd_length_in_bytes;
				cmd_buf += cmd_length_in_bytes/sizeof(uint32_t);
				break;
			default:
				break;
			}
		}
		e_ctrl->cal_data.num_map = num_map + 1;
	}
	return rc;
}

/* SHLOCAL_CAMERA_IMAGE_QUALITY-> *//* lsc for imx318/imx351r/imx351t/ov8856 */
#define MIN(x, y) (((x) > (y)) ? (y):(x))
#define MAX(x, y) (((x) > (y)) ? (x):(y))
#define IMX351_OTP_LSC_W    (9)
#define IMX351_OTP_LSC_H    (7)
#define IMX351_OTP_LSC_SIZE (IMX351_OTP_LSC_W * IMX351_OTP_LSC_H)
#define OV8856_OTP_LSC_W    (9)
#define OV8856_OTP_LSC_H    (7)
#define OV8856_OTP_LSC_SIZE (OV8856_OTP_LSC_W * OV8856_OTP_LSC_H)
#define OV8856_OTP_LSC_BUF_SIZE 96
#define MESH_ROLLOFF_W      (17)
#define MESH_ROLLOFF_H      (13)
#define MESH_ROLLOFF_SIZE   (MESH_ROLLOFF_W * MESH_ROLLOFF_H)

#define IMX318_OTP_LSC_OFFSET   (256)
#define IMX351_OTP_LSC_OFFSET   (64)
#define IMX351_OTP_EXLSC_OFFSET (576) /* >= 576 */
#define OV8856_OTP_LSC_OFFSET   (64)
#define OV8856_OTP_EXLSC_OFFSET (576) /* >= 512 */


static void cam_eeprom_convert_grid(int32_t *src, int32_t *dst, int32_t src_w, int32_t src_h, int32_t dst_w, int32_t dst_h)
{
	int32_t src_x, src_y, dst_x, dst_y;
	int32_t ref_x_Q10, ref_y_Q10, dref_x_Q10, dref_y_Q10;

	for(dst_y = 0; dst_y < dst_h; dst_y++) {
		for(dst_x = 0; dst_x < dst_w; dst_x++) {
			ref_x_Q10 = 1024 * dst_x * (src_w - 1) / (dst_w - 1);
			ref_y_Q10 = 1024 * dst_y * (src_h - 1) / (dst_h - 1);
			src_x = (int32_t)(ref_x_Q10 / 1024);
			src_y = (int32_t)(ref_y_Q10 / 1024);
			dref_x_Q10 = ref_x_Q10 - src_x * 1024;
			dref_y_Q10 = ref_y_Q10 - src_y * 1024;

			dst[dst_w * dst_y + dst_x] = (1024 - dref_x_Q10) * (1024 - dref_y_Q10) * src[src_w * src_y + src_x]
				+ (1024 - dref_x_Q10) * dref_y_Q10 * src[src_w * MIN(src_y + 1, src_h - 1) + src_x]
				+ dref_x_Q10 * (1024 - dref_y_Q10) * src[src_w * src_y + MIN(src_x + 1, src_w - 1)]
				+ dref_x_Q10 * dref_y_Q10 * src[src_w * MIN(src_y + 1, src_h - 1) + MIN(src_x + 1, src_w - 1)];
			dst[dst_w * dst_y + dst_x] /= (1024 * 1024);
		}
	}

	return;
}

static void cam_eeprom_write_exlsc(int32_t *tmp_lsc_buf, uint8_t *otp_exlsc_buf)
{
	int32_t i;
	uint16_t val;
	for(i = 0; i < MESH_ROLLOFF_SIZE; i++) {
		val = (uint16_t)tmp_lsc_buf[i];
		otp_exlsc_buf[i * 2] = val & 0x00FF;
		otp_exlsc_buf[i * 2 + 1] = (val & 0xFF00) >> 8;
	}
}

static void cam_eeprom_read_imx351_lsc(uint8_t *otp_buf, int32_t *tmp_lsc_buf)
{
	int32_t i;
	for(i = 0; i < IMX351_OTP_LSC_SIZE; i++) {
		tmp_lsc_buf[i] = (uint32_t)((otp_buf[i * 2 + 1] << 8) + otp_buf[i * 2]);
	}
}

static void cam_eeprom_extract_imx351_lsc(uint8_t *otp_buf)
{
	int32_t *tmp_lsc_buf = (int32_t*)kzalloc(IMX351_OTP_LSC_SIZE * sizeof(int32_t), GFP_KERNEL);
	int32_t *tmp_exlsc_buf = (int32_t*)kzalloc(MESH_ROLLOFF_SIZE * sizeof(int32_t), GFP_KERNEL);
	uint8_t *otp_lsc_buf = otp_buf + IMX351_OTP_LSC_OFFSET;
	uint8_t *otp_exlsc_buf = otp_buf + IMX351_OTP_EXLSC_OFFSET;

	if(tmp_lsc_buf != NULL && tmp_exlsc_buf != NULL) {
		cam_eeprom_read_imx351_lsc(otp_lsc_buf, tmp_lsc_buf);
		cam_eeprom_convert_grid(tmp_lsc_buf, tmp_exlsc_buf, IMX351_OTP_LSC_W, IMX351_OTP_LSC_H, MESH_ROLLOFF_W, MESH_ROLLOFF_H);
		cam_eeprom_write_exlsc(tmp_exlsc_buf, otp_exlsc_buf);

		otp_lsc_buf += IMX351_OTP_LSC_SIZE * 2;
		otp_exlsc_buf += MESH_ROLLOFF_SIZE * 2;
		cam_eeprom_read_imx351_lsc(otp_lsc_buf, tmp_lsc_buf);
		cam_eeprom_convert_grid(tmp_lsc_buf, tmp_exlsc_buf, IMX351_OTP_LSC_W, IMX351_OTP_LSC_H, MESH_ROLLOFF_W, MESH_ROLLOFF_H);
		cam_eeprom_write_exlsc(tmp_exlsc_buf, otp_exlsc_buf);

		otp_lsc_buf += IMX351_OTP_LSC_SIZE * 2;
		otp_exlsc_buf += MESH_ROLLOFF_SIZE * 2;
		cam_eeprom_read_imx351_lsc(otp_lsc_buf, tmp_lsc_buf);
		cam_eeprom_convert_grid(tmp_lsc_buf, tmp_exlsc_buf, IMX351_OTP_LSC_W, IMX351_OTP_LSC_H, MESH_ROLLOFF_W, MESH_ROLLOFF_H);
		cam_eeprom_write_exlsc(tmp_exlsc_buf, otp_exlsc_buf);

		otp_lsc_buf += IMX351_OTP_LSC_SIZE * 2;
		otp_exlsc_buf += MESH_ROLLOFF_SIZE * 2;
		cam_eeprom_read_imx351_lsc(otp_lsc_buf, tmp_lsc_buf);
		cam_eeprom_convert_grid(tmp_lsc_buf, tmp_exlsc_buf, IMX351_OTP_LSC_W, IMX351_OTP_LSC_H, MESH_ROLLOFF_W, MESH_ROLLOFF_H);
		cam_eeprom_write_exlsc(tmp_exlsc_buf, otp_exlsc_buf);
	}
	
	if(tmp_lsc_buf != NULL) {
		kfree(tmp_lsc_buf);
	}
	if(tmp_exlsc_buf != NULL) {
		kfree(tmp_exlsc_buf);
	}
}

static void cam_eeprom_read_ov8856_lsc(uint8_t *otp_buf, int32_t *tmp_lsc_buf)
{
	int32_t i;
	for(i = 0; i < OV8856_OTP_LSC_SIZE; i+=2) {
		tmp_lsc_buf[i] = (uint32_t)(((otp_buf[i / 2 * 3] & 0x03) << 8) + otp_buf[i / 2 * 3 + 1]);
		if(i + 1 < OV8856_OTP_LSC_SIZE) {
			tmp_lsc_buf[i + 1] = (uint32_t)(((otp_buf[i / 2 * 3] & 0x30) << 4) + otp_buf[i / 2 * 3 + 2]);
		}
	}
}

static void cam_eeprom_extract_ov8856_lsc(uint8_t *otp_buf)
{
	int32_t *tmp_lsc_buf = (int32_t*)kzalloc(OV8856_OTP_LSC_SIZE * sizeof(int32_t), GFP_KERNEL);
	int32_t *tmp_exlsc_buf = (int32_t*)kzalloc(MESH_ROLLOFF_SIZE * sizeof(int32_t), GFP_KERNEL);
	uint8_t *otp_lsc_buf = otp_buf + OV8856_OTP_LSC_OFFSET;
	uint8_t *otp_exlsc_buf = otp_buf + OV8856_OTP_EXLSC_OFFSET;

	if(tmp_lsc_buf != NULL && tmp_exlsc_buf != NULL) {
		cam_eeprom_read_ov8856_lsc(otp_lsc_buf, tmp_lsc_buf);
		cam_eeprom_convert_grid(tmp_lsc_buf, tmp_exlsc_buf, OV8856_OTP_LSC_W, OV8856_OTP_LSC_H, MESH_ROLLOFF_W, MESH_ROLLOFF_H);
		cam_eeprom_write_exlsc(tmp_exlsc_buf, otp_exlsc_buf);

		otp_lsc_buf += OV8856_OTP_LSC_BUF_SIZE;
		otp_exlsc_buf += MESH_ROLLOFF_SIZE * 2;
		cam_eeprom_read_ov8856_lsc(otp_lsc_buf, tmp_lsc_buf);
		cam_eeprom_convert_grid(tmp_lsc_buf, tmp_exlsc_buf, OV8856_OTP_LSC_W, OV8856_OTP_LSC_H, MESH_ROLLOFF_W, MESH_ROLLOFF_H);
		cam_eeprom_write_exlsc(tmp_exlsc_buf, otp_exlsc_buf);

		otp_lsc_buf += OV8856_OTP_LSC_BUF_SIZE;
		otp_exlsc_buf += MESH_ROLLOFF_SIZE * 2;
		cam_eeprom_read_ov8856_lsc(otp_lsc_buf, tmp_lsc_buf);
		cam_eeprom_convert_grid(tmp_lsc_buf, tmp_exlsc_buf, OV8856_OTP_LSC_W, OV8856_OTP_LSC_H, MESH_ROLLOFF_W, MESH_ROLLOFF_H);
		cam_eeprom_write_exlsc(tmp_exlsc_buf, otp_exlsc_buf);

		otp_lsc_buf += OV8856_OTP_LSC_BUF_SIZE;
		otp_exlsc_buf += MESH_ROLLOFF_SIZE * 2;
		cam_eeprom_read_ov8856_lsc(otp_lsc_buf, tmp_lsc_buf);
		cam_eeprom_convert_grid(tmp_lsc_buf, tmp_exlsc_buf, OV8856_OTP_LSC_W, OV8856_OTP_LSC_H, MESH_ROLLOFF_W, MESH_ROLLOFF_H);
		cam_eeprom_write_exlsc(tmp_exlsc_buf, otp_exlsc_buf);
	}
	
	if(tmp_lsc_buf != NULL) {
		kfree(tmp_lsc_buf);
	}
	if(tmp_exlsc_buf != NULL) {
		kfree(tmp_exlsc_buf);
	}
}

static void cam_eeprom_swap_data(uint8_t *buf1, uint8_t *buf2, uint32_t swap_size)
{
	uint32_t i = 0;
	uint8_t tmp;
	for(i = 0; i < swap_size; i++) {
		tmp = *(buf1 + i);
		*(buf1 + i) = *(buf2 + i);
		*(buf2 + i) = tmp;
	}
}

static void cam_eeprom_rotate_exlsc(uint8_t *otp_buf, uint32_t exlsc_offset)
{
	uint32_t i = 0;
	uint8_t *otp_exlsc_buf = otp_buf + exlsc_offset;

	for(i = 0; i < MESH_ROLLOFF_SIZE / 2; i++) {
		cam_eeprom_swap_data(otp_exlsc_buf + i * 2, otp_exlsc_buf + (MESH_ROLLOFF_SIZE - 1 - i) * 2, 2);
	}

	otp_exlsc_buf += MESH_ROLLOFF_SIZE * 2;
	for(i = 0; i < MESH_ROLLOFF_SIZE / 2; i++) {
		cam_eeprom_swap_data(otp_exlsc_buf + i * 2, otp_exlsc_buf + (MESH_ROLLOFF_SIZE - 1 - i) * 2, 2);
	}

	otp_exlsc_buf += MESH_ROLLOFF_SIZE * 2;
	for(i = 0; i < MESH_ROLLOFF_SIZE / 2; i++) {
		cam_eeprom_swap_data(otp_exlsc_buf + i * 2, otp_exlsc_buf + (MESH_ROLLOFF_SIZE - 1 - i) * 2, 2);
	}

	otp_exlsc_buf += MESH_ROLLOFF_SIZE * 2;
	for(i = 0; i < MESH_ROLLOFF_SIZE / 2; i++) {
		cam_eeprom_swap_data(otp_exlsc_buf + i * 2, otp_exlsc_buf + (MESH_ROLLOFF_SIZE - 1 - i) * 2, 2);
	}
}

/* SHLOCAL_CAMERA_IMAGE_QUALITY<- */
/* SHLOCAL_CAMERA_IMAGE_QUALITY-> *//* af for imx318 */
#define IMX318_OTP_AF_OFFSET (0x90)
#define START_MARGIN (1000)
#define END_MARGIN (0)

#define FOC_10   (12306)
#define FOC_50   (2392)
#define PCOC_UM  (582)
#define DIR_UM   (5900)
#define MAX_LSB  (1023)

static void cam_eeprom_extract_imx318_af(uint8_t *otp_buf)
{
	uint32_t dac_10cm = 0, dac_50cm = 0;
	uint32_t start_pos = 0, end_pos = 0;
	uint32_t peak_pos_inf_up, peak_pos_inf_side;
	uint32_t dac_start_current;
	uint32_t fullstroke = 0;

	dac_10cm =((otp_buf[IMX318_OTP_AF_OFFSET] & 0x30)<< 4) | otp_buf[IMX318_OTP_AF_OFFSET + 1];
	dac_50cm =((otp_buf[IMX318_OTP_AF_OFFSET] & 0x0C)<< 6) | otp_buf[IMX318_OTP_AF_OFFSET + 2];
	dac_start_current = ((otp_buf[IMX318_OTP_AF_OFFSET] & 0x03)<< 8)| + otp_buf[IMX318_OTP_AF_OFFSET + 3];
	
	fullstroke = ((otp_buf[IMX318_OTP_AF_OFFSET + 4] & 0xf0)>>4) * 1000 + (otp_buf[IMX318_OTP_AF_OFFSET + 4] & 0x0f) * 100
                 +((otp_buf[IMX318_OTP_AF_OFFSET + 5] & 0xf0)>>4) *10 +(otp_buf[IMX318_OTP_AF_OFFSET + 5] & 0x0f);
                 
    dac_10cm = dac_10cm * 100;
    dac_50cm = dac_50cm * 100;
    dac_start_current = dac_start_current * 100;
    fullstroke = fullstroke * 10;
	
	CAM_DBG(CAM_EEPROM, "OTP: dac_10cm*100: %d, dac_50cm*100: %d,  dac_start_current*100 = %d, fullstroke*100 = %d", 
        dac_10cm, dac_50cm, dac_start_current, fullstroke);

	peak_pos_inf_up = (dac_10cm - FOC_10 * (dac_10cm - dac_50cm) /(FOC_10 - FOC_50));

	peak_pos_inf_side = (peak_pos_inf_up - (DIR_UM * (dac_10cm - dac_50cm) /(FOC_10 - FOC_50)));

	start_pos = peak_pos_inf_side - (DIR_UM * (dac_10cm - dac_50cm) /(FOC_10 - FOC_50)) - START_MARGIN;
	start_pos = start_pos / 100;

	end_pos = dac_start_current + (fullstroke * (dac_10cm - dac_50cm) /(FOC_10 - FOC_50)) + END_MARGIN;
	end_pos = end_pos / 100;

	if(start_pos < 0){
		start_pos = 0;
	}

	if(end_pos > MAX_LSB){
		end_pos = MAX_LSB;
	}

	CAM_DBG(CAM_EEPROM, "macro(end_pos) = %d(0x%04x), Inf(start_pos) =  %d(0x%04x)", end_pos, end_pos, start_pos, start_pos);
	/* macro update */
	otp_buf[IMX318_OTP_AF_OFFSET] = end_pos & 0x00FF;
	otp_buf[IMX318_OTP_AF_OFFSET + 1] = (end_pos & 0xFF00) >> 8;
	/* inf update */
	otp_buf[IMX318_OTP_AF_OFFSET + 2] = start_pos & 0x00FF;
	otp_buf[IMX318_OTP_AF_OFFSET + 2 + 1] = (start_pos & 0xFF00) >> 8;

}
/* SHLOCAL_CAMERA_IMAGE_QUALITY<- */

/**
 * cam_eeprom_get_cal_data - parse the userspace IO config and
 *                                        copy read data to share with userspace
 * @e_ctrl:     ctrl structure
 * @csl_packet: csl packet received
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_get_cal_data(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_packet *csl_packet)
{
	struct cam_buf_io_cfg *io_cfg;
	uint32_t              i = 0;
	int                   rc = 0;
	uint64_t              buf_addr;
	size_t                buf_size;
	uint8_t               *read_buffer;

	io_cfg = (struct cam_buf_io_cfg *) ((uint8_t *)
		&csl_packet->payload +
		csl_packet->io_configs_offset);

	CAM_DBG(CAM_EEPROM, "number of IO configs: %d:",
		csl_packet->num_io_configs);

	for (i = 0; i < csl_packet->num_io_configs; i++) {
		CAM_DBG(CAM_EEPROM, "Direction: %d:", io_cfg->direction);
		if (io_cfg->direction == CAM_BUF_OUTPUT) {
			rc = cam_mem_get_cpu_buf(io_cfg->mem_handle[0],
				(uint64_t *)&buf_addr, &buf_size);
			CAM_DBG(CAM_EEPROM, "buf_addr : %pK, buf_size : %zu\n",
				(void *)buf_addr, buf_size);

			read_buffer = (uint8_t *)buf_addr;
			if (!read_buffer) {
				CAM_ERR(CAM_EEPROM,
					"invalid buffer to copy data");
				return -EINVAL;
			}
			read_buffer += io_cfg->offsets[0];

			if (buf_size < e_ctrl->cal_data.num_data) {
				CAM_ERR(CAM_EEPROM,
					"failed to copy, Invalid size");
				return -EINVAL;
			}

			CAM_DBG(CAM_EEPROM, "copy the data, len:%d",
				e_ctrl->cal_data.num_data);
			memcpy(read_buffer, e_ctrl->cal_data.mapdata,
					e_ctrl->cal_data.num_data);

/* SHLOCAL_CAMERA_IMAGE_QUALITY-> *//* lsc for imx318/imx351r/imx351t/ov8856 */
			/*
			  use buf_size to identify camera module. buf_size derives from registerData of \chi-cdk\vendor\eeprom\xxx_eeprom.xml.
			  imx318     : buf_size = 2160
			  imx318d : buf_size = 2144
			  imx351r : buf_size = 2344
			  imx351t : buf_size = 2352
			  ov8856  : buf_size = 2368
			*/
			
			/* imx351r/imx351t */
			if(buf_size == 2344 || buf_size == 2352) {
				/* extend eeprom area for imx351 lsc */
				CAM_DBG(CAM_EEPROM, "extend eeprom area for imx351 lsc. buf_size=%zu num_data=%d", buf_size, e_ctrl->cal_data.num_data);
				cam_eeprom_extract_imx351_lsc(read_buffer);
			}

			/* ov8856 */
			if(buf_size == 2368) {
				/* extend eeprom area for ov8856 lsc */
				CAM_DBG(CAM_EEPROM, "extend eeprom area for ov8856 lsc. buf_size=%zu num_data=%d", buf_size, e_ctrl->cal_data.num_data);
				cam_eeprom_extract_ov8856_lsc(read_buffer);
			}
			
			/* imx318 */
			if(buf_size == 2160) {
				cam_eeprom_shading_correction_imx318(read_buffer, IMX318_OTP_LSC_OFFSET);
			}
			
			/* imx318/imx318d */
			if(buf_size == 2144 || buf_size == 2160) {
				cam_eeprom_rotate_exlsc(read_buffer, IMX318_OTP_LSC_OFFSET);
				cam_eeprom_extract_imx318_af(read_buffer);
			}
			
			/* imx351r */
			if(buf_size == 2344) {
				cam_eeprom_rotate_exlsc(read_buffer, IMX351_OTP_EXLSC_OFFSET);
			}
/* SHLOCAL_CAMERA_IMAGE_QUALITY<- */

		} else {
			CAM_ERR(CAM_EEPROM, "Invalid direction");
			rc = -EINVAL;
		}
	}
	return rc;
}

/**
 * cam_eeprom_pkt_parse - Parse csl packet
 * @e_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
static int32_t cam_eeprom_pkt_parse(struct cam_eeprom_ctrl_t *e_ctrl, void *arg)
{
	int32_t                         rc = 0;
	struct cam_control             *ioctl_ctrl = NULL;
	struct cam_config_dev_cmd       dev_config;
	uint64_t                        generic_pkt_addr;
	size_t                          pkt_len;
	struct cam_packet              *csl_packet = NULL;
	struct cam_eeprom_soc_private  *soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info = &soc_private->power_info;

	ioctl_ctrl = (struct cam_control *)arg;

	if (copy_from_user(&dev_config, (void __user *) ioctl_ctrl->handle,
		sizeof(dev_config)))
		return -EFAULT;
	rc = cam_mem_get_cpu_buf(dev_config.packet_handle,
		(uint64_t *)&generic_pkt_addr, &pkt_len);
	if (rc) {
		CAM_ERR(CAM_EEPROM,
			"error in converting command Handle Error: %d", rc);
		return rc;
	}

	if (dev_config.offset > pkt_len) {
		CAM_ERR(CAM_EEPROM,
			"Offset is out of bound: off: %lld, %zu",
			dev_config.offset, pkt_len);
		return -EINVAL;
	}

	csl_packet = (struct cam_packet *)
		(generic_pkt_addr + dev_config.offset);
	switch (csl_packet->header.op_code & 0xFFFFFF) {
	case CAM_EEPROM_PACKET_OPCODE_INIT:
		if (e_ctrl->userspace_probe == false) {
			rc = cam_eeprom_parse_read_memory_map(
					e_ctrl->soc_info.dev->of_node, e_ctrl);
			if (rc < 0) {
				CAM_ERR(CAM_EEPROM, "Failed: rc : %d", rc);
				return rc;
			}
			rc = cam_eeprom_get_cal_data(e_ctrl, csl_packet);
			vfree(e_ctrl->cal_data.mapdata);
			vfree(e_ctrl->cal_data.map);
			e_ctrl->cal_data.num_data = 0;
			e_ctrl->cal_data.num_map = 0;
			CAM_DBG(CAM_EEPROM,
				"Returning the data using kernel probe");
			break;
		}
		rc = cam_eeprom_init_pkt_parser(e_ctrl, csl_packet);
		if (rc) {
			CAM_ERR(CAM_EEPROM,
				"Failed in parsing the pkt");
			return rc;
		}

		e_ctrl->cal_data.mapdata =
			vzalloc(e_ctrl->cal_data.num_data);
		if (!e_ctrl->cal_data.mapdata) {
			rc = -ENOMEM;
			CAM_ERR(CAM_EEPROM, "failed");
			goto error;
		}

		rc = cam_eeprom_power_up(e_ctrl,
			&soc_private->power_info);
		if (rc) {
			CAM_ERR(CAM_EEPROM, "failed rc %d", rc);
			goto memdata_free;
		}

		e_ctrl->cam_eeprom_state = CAM_EEPROM_CONFIG;
		rc = cam_eeprom_read_memory(e_ctrl, &e_ctrl->cal_data);
		if (rc) {
			CAM_ERR(CAM_EEPROM,
				"read_eeprom_memory failed");
			goto power_down;
		}

		rc = cam_eeprom_get_cal_data(e_ctrl, csl_packet);
		rc = cam_eeprom_power_down(e_ctrl);
		e_ctrl->cam_eeprom_state = CAM_EEPROM_ACQUIRE;
		vfree(e_ctrl->cal_data.mapdata);
		vfree(e_ctrl->cal_data.map);
		kfree(power_info->power_setting);
		kfree(power_info->power_down_setting);
		power_info->power_setting = NULL;
		power_info->power_down_setting = NULL;
		power_info->power_setting_size = 0;
		power_info->power_down_setting_size = 0;
		e_ctrl->cal_data.num_data = 0;
		e_ctrl->cal_data.num_map = 0;
		break;
	default:
		break;
	}
	return rc;
power_down:
	cam_eeprom_power_down(e_ctrl);
memdata_free:
	vfree(e_ctrl->cal_data.mapdata);
error:
	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	power_info->power_setting = NULL;
	power_info->power_down_setting = NULL;
	vfree(e_ctrl->cal_data.map);
	e_ctrl->cal_data.num_data = 0;
	e_ctrl->cal_data.num_map = 0;
	e_ctrl->cam_eeprom_state = CAM_EEPROM_INIT;
	return rc;
}

void cam_eeprom_shutdown(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int rc;
	struct cam_eeprom_soc_private  *soc_private =
		(struct cam_eeprom_soc_private *)e_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info = &soc_private->power_info;

	if (e_ctrl->cam_eeprom_state == CAM_EEPROM_INIT)
		return;

	if (e_ctrl->cam_eeprom_state == CAM_EEPROM_CONFIG) {
		rc = cam_eeprom_power_down(e_ctrl);
		if (rc < 0)
			CAM_ERR(CAM_EEPROM, "EEPROM Power down failed");
		e_ctrl->cam_eeprom_state = CAM_EEPROM_ACQUIRE;
	}

	if (e_ctrl->cam_eeprom_state == CAM_EEPROM_ACQUIRE) {
		rc = cam_destroy_device_hdl(e_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_EEPROM, "destroying the device hdl");

		e_ctrl->bridge_intf.device_hdl = -1;
		e_ctrl->bridge_intf.link_hdl = -1;
		e_ctrl->bridge_intf.session_hdl = -1;

		kfree(power_info->power_setting);
		kfree(power_info->power_down_setting);
		power_info->power_setting = NULL;
		power_info->power_down_setting = NULL;
		power_info->power_setting_size = 0;
		power_info->power_down_setting_size = 0;
	}

	e_ctrl->cam_eeprom_state = CAM_EEPROM_INIT;
}

/**
 * cam_eeprom_driver_cmd - Handle eeprom cmds
 * @e_ctrl:     ctrl structure
 * @arg:        Camera control command argument
 *
 * Returns success or failure
 */
int32_t cam_eeprom_driver_cmd(struct cam_eeprom_ctrl_t *e_ctrl, void *arg)
{
	int                            rc = 0;
	struct cam_eeprom_query_cap_t  eeprom_cap = {0};
	struct cam_control            *cmd = (struct cam_control *)arg;

	if (!e_ctrl || !cmd) {
		CAM_ERR(CAM_EEPROM, "Invalid Arguments");
		return -EINVAL;
	}

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_EEPROM, "Invalid handle type: %d",
			cmd->handle_type);
		return -EINVAL;
	}

	mutex_lock(&(e_ctrl->eeprom_mutex));
	switch (cmd->op_code) {
	case CAM_QUERY_CAP:
		eeprom_cap.slot_info = e_ctrl->soc_info.index;
		if (e_ctrl->userspace_probe == false)
			eeprom_cap.eeprom_kernel_probe = true;
		else
			eeprom_cap.eeprom_kernel_probe = false;

		if (copy_to_user((void __user *) cmd->handle,
			&eeprom_cap,
			sizeof(struct cam_eeprom_query_cap_t))) {
			CAM_ERR(CAM_EEPROM, "Failed Copy to User");
			return -EFAULT;
			goto release_mutex;
		}
		CAM_DBG(CAM_EEPROM, "eeprom_cap: ID: %d", eeprom_cap.slot_info);
		break;
	case CAM_ACQUIRE_DEV:
		rc = cam_eeprom_get_dev_handle(e_ctrl, arg);
		if (rc) {
			CAM_ERR(CAM_EEPROM, "Failed to acquire dev");
			goto release_mutex;
		}
		e_ctrl->cam_eeprom_state = CAM_EEPROM_ACQUIRE;
		break;
	case CAM_RELEASE_DEV:
		if (e_ctrl->cam_eeprom_state != CAM_EEPROM_ACQUIRE) {
			rc = -EINVAL;
			CAM_WARN(CAM_EEPROM,
			"Not in right state to release : %d",
			e_ctrl->cam_eeprom_state);
			goto release_mutex;
		}

		if (e_ctrl->bridge_intf.device_hdl == -1) {
			CAM_ERR(CAM_EEPROM,
				"Invalid Handles: link hdl: %d device hdl: %d",
				e_ctrl->bridge_intf.device_hdl,
				e_ctrl->bridge_intf.link_hdl);
			rc = -EINVAL;
			goto release_mutex;
		}
		rc = cam_destroy_device_hdl(e_ctrl->bridge_intf.device_hdl);
		if (rc < 0)
			CAM_ERR(CAM_EEPROM,
				"failed in destroying the device hdl");
		e_ctrl->bridge_intf.device_hdl = -1;
		e_ctrl->bridge_intf.link_hdl = -1;
		e_ctrl->bridge_intf.session_hdl = -1;
		e_ctrl->cam_eeprom_state = CAM_EEPROM_INIT;
		break;
	case CAM_CONFIG_DEV:
		rc = cam_eeprom_pkt_parse(e_ctrl, arg);
		if (rc) {
			CAM_ERR(CAM_EEPROM, "Failed in eeprom pkt Parsing");
			goto release_mutex;
		}
		break;
	default:
		CAM_DBG(CAM_EEPROM, "invalid opcode");
		break;
	}

release_mutex:
	mutex_unlock(&(e_ctrl->eeprom_mutex));

	return rc;
}

