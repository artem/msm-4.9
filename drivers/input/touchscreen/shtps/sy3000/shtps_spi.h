/* drivers/input/touchscreen/shtps/sy3000/shtps_spi.h
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
#ifndef __SHTPS_SPI_H__
#define __SHTPS_SPI_H__

typedef int (*tps_write_fw_data_t)(void *, u16, u8 *, u16);
typedef int (*tps_write_block_t)(void *, u16, u8 *, u32);
typedef int (*tps_read_block_t)( void *, u16, u8 *, u32, u8 *);
typedef int (*tps_write_t)(void *, u16, u8);
typedef int (*tps_read_t)( void *, u16, u8 *, u32);
typedef int (*tps_write_packet_t)(void *, u16, u8 *, u32);
typedef int (*tps_read_packet_t)( void *, u16, u8 *, u32);
typedef int (*tps_active_t)( void *);
typedef int (*tps_standby_t)(void *);

struct shtps_ctrl_functbl{
	tps_write_fw_data_t		firmware_write_f;
	tps_write_block_t		block_write_f;
	tps_read_block_t		block_read_f;
	tps_write_t				write_f;
	tps_read_t				read_f;
	tps_write_packet_t		packet_write_f;
	tps_read_packet_t		packet_read_f;
	tps_active_t			active_f;
	tps_standby_t			standby_f;
};

#define M_FIRMWARE_WRITE_FUNC(A, B, C, D)	(A)->devctrl_func_p->firmware_write_f((A)->tps_ctrl_p, B, C, D)
#define M_WRITE_BLOCK_FUNC(A, B, C, D)		(A)->devctrl_func_p->block_write_f((A)->tps_ctrl_p, B, C, D)
#define M_READ_BLOCK_FUNC(A, B, C, D, E)	(A)->devctrl_func_p->block_read_f((A)->tps_ctrl_p, B, C, D, E)
#define M_WRITE_FUNC(A, B, C)				(A)->devctrl_func_p->write_f((A)->tps_ctrl_p, B, C)
#define M_READ_FUNC(A, B, C, D)				(A)->devctrl_func_p->read_f((A)->tps_ctrl_p, B, C, D)
#define M_WRITE_PACKET_FUNC(A, B, C, D)		(A)->devctrl_func_p->packet_write_f((A)->tps_ctrl_p, B, C, D)
#define M_READ_PACKET_FUNC(A, B, C, D)		(A)->devctrl_func_p->packet_read_f((A)->tps_ctrl_p, B, C, D)
#define M_ACTIVE_FUNC(A)					(A)->devctrl_func_p->active_f((A)->tps_ctrl_p)
#define M_STANDBY_FUNC(A)					(A)->devctrl_func_p->standby_f((A)->tps_ctrl_p)

void shtps_set_spi_transfer_log_enable(int para);
int shtps_get_spi_transfer_log_enable(void);

int shtps_get_spi_error_detect_flg(void);
void shtps_clr_spi_error_detect_flg(void);

#endif /* __SHTPS_SPI_H__ */
