/* drivers/input/touchscreen/shtps/sy3000/shtps_spi.c
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
#include <linux/spi/spi.h>
#include <linux/slab.h>

/* -------------------------------------------------------------------------- */
#include <linux/input/shtps_dev.h>

#include "shtps_param_extern.h"
#include "shtps_log.h"
#include "shtps_spi.h"

#if defined( SHTPS_DEF_FWCTL_PROTOCOL_SPI )

/* -------------------------------------------------------------------------- */
#if defined( SHTPS_LOG_ERROR_ENABLE )
	#define SHTPS_LOG_SPI_FUNC_CALL()	// SHTPS_LOG_FUNC_CALL()
#else
	#define SHTPS_LOG_SPI_FUNC_CALL()
#endif
/* -------------------------------------------------------------------------- */
struct shtps_rmi_spi;

static int shtps_spi_error_detect_flg = 0;

static int shtps_spi_write_fw_data(void *spi, u16 addr, u8 *buf, u16 blockSize);
static void shtps_spi_write_async_spicomplete(void *arg);
static int shtps_spi_write_async(void *spi, u16 addr, u8 data);
static int shtps_spi_write_block(void *spi, u16 addr, u8 *data, u32 size);
static int shtps_spi_write(void *spi, u16 addr, u8 data);

static int shtps_spi_read_block(void *spi, u16 addr, u8 *readbuf, u32 size, u8 *buf);
static int shtps_spi_read(void *spi, u16 addr, u8 *buf, u32 size);
static void shtps_spi_one_cs_spicomplete(void *arg);

static int shtps_spi_write_packet(void *spi, u16 addr, u8 *data, u32 size);
static int shtps_spi_read_packet (void *spi, u16 addr, u8 *buf,  u32 size);
static int shtps_spi_active(void *spi);
static int shtps_spi_standby(void *spi);

#if defined( SHTPS_DEVICE_ACCESS_LOG_ENABLE )
#define SPI_IO_WRITE	0
#define SPI_IO_READ		1

static int shtps_spi_transfer_log_enable = 0;

void shtps_set_spi_transfer_log_enable(int para)
{
	shtps_spi_transfer_log_enable = para;
}

int shtps_get_spi_transfer_log_enable(void)
{
	return shtps_spi_transfer_log_enable;
}

static void shtps_spi_transfer_log(int io, u16 addr, u8 *buf, int size)
{
	if(shtps_spi_transfer_log_enable == 1){
		u8 *strbuf = NULL;
		u8 datastrbuf[10];
		int allocSize = 0;
		int i;

		allocSize = 100 + (3 * size);
		strbuf = (u8 *)kzalloc(allocSize, GFP_KERNEL);
		if(strbuf == NULL){
			SHTPS_LOG_DBG_PRINT("shtps_spi_transfer_log() alloc error. size = %d\n", allocSize);
			return;
		}

		sprintf(strbuf, "[SPI] %s %04X :", (io == SPI_IO_WRITE ? "W" : "R"), addr);
		for(i = 0; i < size; i++){
			sprintf(datastrbuf, " %02X", buf[i]);
			strcat(strbuf, datastrbuf);
		}

		DBG_PRINTK("%s\n", strbuf);

		if(strbuf != NULL){
			kfree(strbuf);
		}
	}
}
#endif /* #if defined( SHTPS_DEVICE_ACCESS_LOG_ENABLE ); */

int shtps_get_spi_error_detect_flg(void)
{
	return shtps_spi_error_detect_flg;
}

void shtps_clr_spi_error_detect_flg(void)
{
	shtps_spi_error_detect_flg = 0;
}

/* -------------------------------------------------------------------------- */
static int shtps_spi_write_fw_data(void *spi, u16 addr, u8 *buf, u16 blockSize)
{
	struct spi_message	message;
	struct spi_transfer	t;
	u8					tx_buf[20];
	int					status = 0;
	int					i;
	int					retry = SHTPS_SPI_RETRY_COUNT;

	SHTPS_LOG_SPI_FUNC_CALL();
	do{
		memset(&t, 0, sizeof(t));

		tx_buf[0] = (addr >> 8) & 0xff;
		tx_buf[1] = addr & 0xff;

		for(i = 0; i < blockSize; i++){
			tx_buf[2+i] = buf[i];
		}

		spi_message_init(&message);

		t.tx_buf 		= tx_buf;
		t.rx_buf 		= NULL;
		t.len    		= 2+blockSize;
		t.speed_hz		= SHTPS_SY3X00_SPI_CLOCK_FW_WRITE_SPEED;
		#if defined(CONFIG_SHARP_SPI_DEASSERT_WAIT)
			t.deassert_wait	= SHTPS_SY3X00_SPI_CLOCK_FW_WRITE_WAIT;
		#else
			t.delay_usecs	= SHTPS_SY3X00_SPI_CLOCK_FW_WRITE_WAIT;
		#endif /* CONFIG_SHARP_SPI_DEASSERT_WAIT */
		spi_message_add_tail(&t, &message);
		#if defined( SHTPS_DEVICE_ACCESS_LOG_ENABLE )
			shtps_spi_transfer_log(SPI_IO_WRITE, addr, buf, blockSize);
		#endif /* #if defined( SHTPS_DEVICE_ACCESS_LOG_ENABLE ); */

		#ifdef SHTPS_LOG_SPIACCESS_SEQ_ENABLE
			_log_msg_sync(LOGMSG_ID__SPI_WRITE, "0x%04X|0x%02X", addr, data);
		#endif /* SHTPS_LOG_SPIACCESS_SEQ_ENABLE */
			status = spi_sync((struct spi_device *)spi, &message);
		#ifdef SHTPS_LOG_SPIACCESS_SEQ_ENABLE
			_log_msg_sync(LOGMSG_ID__SPI_WRITE_COMP, "%d", status);
		#endif /* SHTPS_LOG_SPIACCESS_SEQ_ENABLE */

		if(status){
			msleep(SHTPS_SPI_RETRY_WAIT);
		}
	}while(status != 0 && retry-- > 0);

	if(status){
		SHTPS_LOG_ERR_PRINT("spi write error\n");
	}
	#if defined( SHTPS_LOG_DEBUG_ENABLE )
	else if(retry < (SHTPS_SPI_RETRY_COUNT)){
		SHTPS_LOG_DBG_PRINT("spi retry num = %d\n", SHTPS_SPI_RETRY_COUNT - retry);
	}
	#endif /* SHTPS_LOG_DEBUG_ENABLE */
	return status;
}

/* -----------------------------------------------------------------------------------
 */
static void shtps_spi_write_async_spicomplete(void *arg)
{
	complete(arg);
	udelay(SHTPS_SY3X00_SPI_TANSACTION_WRITE_WAIT);
}
static int shtps_spi_write_async(void *spi, u16 addr, u8 data)
{
	DECLARE_COMPLETION_ONSTACK(done);
	struct spi_message	message;
	struct spi_transfer	t;
	u8					tx_buf[8];
	int					status;

	SHTPS_LOG_SPI_FUNC_CALL();
	memset(&t, 0, sizeof(t));

	spi_message_init(&message);
	spi_message_add_tail(&t, &message);
	t.tx_buf 		= tx_buf;
	t.rx_buf 		= NULL;
	t.len    		= 3;
	t.speed_hz		= SHTPS_SY3X00_SPI_CLOCK_WRITE_SPEED;
	#if defined(CONFIG_SHARP_SPI_DEASSERT_WAIT)
		t.deassert_wait	= SHTPS_SY3X00_SPI_CLOCK_WRITE_WAIT;
	#else
		t.delay_usecs	= SHTPS_SY3X00_SPI_CLOCK_WRITE_WAIT;
	#endif /* CONFIG_SHARP_SPI_DEASSERT_WAIT */

	tx_buf[0] = (addr >> 8) & 0xff;
	tx_buf[1] = addr & 0xff;
	tx_buf[2] = data;

	#if defined( SHTPS_DEVICE_ACCESS_LOG_ENABLE )
		shtps_spi_transfer_log(SPI_IO_WRITE, addr, &data, 1);
	#endif /* #if defined( SHTPS_DEVICE_ACCESS_LOG_ENABLE ); */
	#ifdef SHTPS_LOG_SPIACCESS_SEQ_ENABLE
		_log_msg_sync(LOGMSG_ID__SPI_WRITE, "0x%04X|0x%02X", addr, data);
	#endif /* SHTPS_LOG_SPIACCESS_SEQ_ENABLE */

	message.complete = shtps_spi_write_async_spicomplete;
	message.context  = &done;
	status = spi_async((struct spi_device *)spi, &message);
	if(status == 0){
		wait_for_completion(&done);
		status = message.status;
	}
	message.context = NULL;

	#ifdef SHTPS_LOG_SPIACCESS_SEQ_ENABLE
		_log_msg_sync(LOGMSG_ID__SPI_WRITE_COMP, "%d", status);
	#endif /* SHTPS_LOG_SPIACCESS_SEQ_ENABLE */

	return status;
}

static int shtps_spi_write_block(void *spi, u16 addr, u8 *data, u32 size)
{
	struct spi_message	message;
	struct spi_transfer	t;
	u8					*tx_buf;
	int					status = 0;
	int					i;
	int					allocSize = 0;

	SHTPS_LOG_SPI_FUNC_CALL();

	allocSize = sizeof(u8) * (2 + size);
	tx_buf = (u8 *)kzalloc(allocSize, GFP_KERNEL);
	if(tx_buf == NULL){
		SHTPS_LOG_DBG_PRINT("shtps_spi_write_block() alloc error. size = %d\n", allocSize);
		return -ENOMEM;
	}
	memset(tx_buf, 0, allocSize);

	memset(&t, 0, sizeof(t));

	tx_buf[0] = (addr >> 8) & 0xff;
	tx_buf[1] = addr & 0xff;

	for(i = 0; i < size; i++){
		tx_buf[2+i] = data[i];
	}

	spi_message_init(&message);

	t.tx_buf 		= tx_buf;
	t.rx_buf 		= NULL;
	t.len    		= 2+size;
	t.speed_hz		= SHTPS_SY3X00_SPI_CLOCK_WRITE_SPEED;
	#if defined(CONFIG_SHARP_SPI_DEASSERT_WAIT)
		t.deassert_wait	= SHTPS_SY3X00_SPI_CLOCK_WRITE_WAIT;
	#else
		t.delay_usecs	= SHTPS_SY3X00_SPI_CLOCK_WRITE_WAIT;
	#endif /* CONFIG_SHARP_SPI_DEASSERT_WAIT */
	spi_message_add_tail(&t, &message);
	#if defined( SHTPS_DEVICE_ACCESS_LOG_ENABLE )
		shtps_spi_transfer_log(SPI_IO_WRITE, addr, data, size);
	#endif /* #if defined( SHTPS_DEVICE_ACCESS_LOG_ENABLE ); */

	#ifdef SHTPS_LOG_SPIACCESS_SEQ_ENABLE
		_log_msg_sync(LOGMSG_ID__SPI_WRITE, "0x%04X|0x%02X", addr, data);
	#endif /* SHTPS_LOG_SPIACCESS_SEQ_ENABLE */
		status = spi_sync((struct spi_device *)spi, &message);
	#ifdef SHTPS_LOG_SPIACCESS_SEQ_ENABLE
		_log_msg_sync(LOGMSG_ID__SPI_WRITE_COMP, "%d", status);
	#endif /* SHTPS_LOG_SPIACCESS_SEQ_ENABLE */

	if(status){
		SHTPS_LOG_ERR_PRINT("spi write error\n");
	}

	if(allocSize > 0){
		kfree(tx_buf);
	}

	return status;
}

static int shtps_spi_write(void *spi, u16 addr, u8 data)
{
	int retry = SHTPS_SPI_RETRY_COUNT;
	int err;

	SHTPS_LOG_SPI_FUNC_CALL();
	do{
		err = shtps_spi_write_async(spi, addr, data);
		if(err){
			msleep(SHTPS_SPI_RETRY_WAIT);
		}
	}while(err != 0 && retry-- > 0);

	if(err){
		shtps_spi_error_detect_flg = 1;
		SHTPS_LOG_ERR_PRINT("spi write error\n");
	}
	#if defined( SHTPS_LOG_DEBUG_ENABLE )
	else if(retry < (SHTPS_SPI_RETRY_COUNT)){
		SHTPS_LOG_DBG_PRINT("spi retry num = %d\n", SHTPS_SPI_RETRY_COUNT - retry);
	}
	#endif /* SHTPS_LOG_DEBUG_ENABLE */

	return err;
}

static void shtps_spi_one_cs_spicomplete(void *arg)
{
	complete(arg);
	udelay(SHTPS_SY3X00_SPI_TANSACTION_READ_WAIT);
}
static int shtps_spi_read_block(void *spi, u16 addr, u8 *readbuf, u32 size, u8 *buf)
{
	DECLARE_COMPLETION_ONSTACK(done);
	struct spi_message	message;
	struct spi_transfer	t;
	u8 *txbuf_p;
	u8 *rxbuf_p;
	int status;
	int i;

	SHTPS_LOG_SPI_FUNC_CALL();
	memset(&t, 0, sizeof(t));
	memset(buf, 0, (size+2*2));

	txbuf_p = buf;
	rxbuf_p = buf + sizeof(u8)*(size + 2);

	txbuf_p[0] = ((addr >> 8) & 0xff) | 0x80;
	txbuf_p[1] = addr & 0xff;

	spi_message_init(&message);
	spi_message_add_tail(&t, &message);

	t.tx_buf		= txbuf_p;
	t.rx_buf  		= rxbuf_p;
	t.len			= size + 2;
	t.bits_per_word	= 8;
	t.speed_hz		= SHTPS_SY3X00_SPI_CLOCK_READ_SPEED;
	#if defined(CONFIG_SHARP_SPI_DEASSERT_WAIT)
		t.deassert_wait	= SHTPS_SY3X00_SPI_CLOCK_READ_WAIT;
	#else
		t.delay_usecs	= SHTPS_SY3X00_SPI_CLOCK_READ_WAIT;
	#endif /* CONFIG_SHARP_SPI_DEASSERT_WAIT */

	message.complete = shtps_spi_one_cs_spicomplete;
	message.context  = &done;
	status = spi_async((struct spi_device *)spi, &message);
	if(status == 0){
		wait_for_completion(&done);
		status = message.status;
	}
	message.context = NULL;

	rxbuf_p += 2;
	for(i = 0;i < size;i++){
		*readbuf++ = *rxbuf_p++;
	}
	#if defined( SHTPS_DEVICE_ACCESS_LOG_ENABLE )
		shtps_spi_transfer_log(SPI_IO_READ, addr, readbuf-size, size);
	#endif /* #if defined( SHTPS_DEVICE_ACCESS_LOG_ENABLE ); */

	return status;
}
static int shtps_spi_read(void *spi, u16 addr, u8 *buf, u32 size)
{
	int status = 0;
	int	i;
	u32	s;
	u8 transbuf[(SHTPS_SY3X00_SPIBLOCK_BUFSIZE+2)*2];
	int retry = SHTPS_SPI_RETRY_COUNT;

	SHTPS_LOG_SPI_FUNC_CALL();
	do{
		s = size;
		for(i=0;i<size;i+=SHTPS_SY3X00_SPIBLOCK_BUFSIZE){
			status = shtps_spi_read_block(spi,
				addr+i,
				buf+i,
				(s>SHTPS_SY3X00_SPIBLOCK_BUFSIZE)?(SHTPS_SY3X00_SPIBLOCK_BUFSIZE):(s),
				transbuf);
			//
			if(status){
				msleep(SHTPS_SPI_RETRY_WAIT);
				break;
			}
			s -= SHTPS_SY3X00_SPIBLOCK_BUFSIZE;
		}
	}while(status != 0 && retry-- > 0);

	if(status){
		shtps_spi_error_detect_flg = 1;
		SHTPS_LOG_ERR_PRINT("spi read error\n");
	}
	#if defined( SHTPS_LOG_DEBUG_ENABLE )
	else if(retry < (SHTPS_SPI_RETRY_COUNT)){
		SHTPS_LOG_DBG_PRINT("spi retry num = %d\n", SHTPS_SPI_RETRY_COUNT - retry);
	}
	#endif /* SHTPS_LOG_DEBUG_ENABLE */
	
	return status;
}

static int shtps_spi_write_packet(void *spi, u16 addr, u8 *data, u32 size)
{
	int status = 0;
	int retry = SHTPS_SPI_RETRY_COUNT;

	SHTPS_LOG_SPI_FUNC_CALL();
	do{
		status = shtps_spi_write_block(spi,
			addr,
			data,
			size);
		//
		if(status){
			msleep(SHTPS_SPI_RETRY_WAIT);
		}
	}while(status != 0 && retry-- > 0);

	if(status){
		shtps_spi_error_detect_flg = 1;
		SHTPS_LOG_ERR_PRINT("spi write error\n");
	}
	#if defined( SHTPS_LOG_DEBUG_ENABLE )
	else if(retry < (SHTPS_SPI_RETRY_COUNT)){
		SHTPS_LOG_DBG_PRINT("spi retry num = %d\n", SHTPS_SPI_RETRY_COUNT - retry);
	}
	#endif /* SHTPS_LOG_DEBUG_ENABLE */

	return status;
}

static int shtps_spi_read_packet(void *spi, u16 addr, u8 *buf, u32 size)
{
	int status = 0;
	int	i;
	u32	s;
	u8 transbuf[(SHTPS_SY3X00_SPIBLOCK_BUFSIZE+2)*2];
	int retry = SHTPS_SPI_RETRY_COUNT;
	u16 addr_org = addr;

	SHTPS_LOG_SPI_FUNC_CALL();
	do{
		s = size;
		for(i=0;i<size;i+=SHTPS_SY3X00_SPIBLOCK_BUFSIZE){
			status = shtps_spi_read_block(spi,
				addr,
				buf+i,
				(s>SHTPS_SY3X00_SPIBLOCK_BUFSIZE)?(SHTPS_SY3X00_SPIBLOCK_BUFSIZE):(s),
				transbuf);
			//
			if(status){
				msleep(SHTPS_SPI_RETRY_WAIT);
				addr = addr_org;
				break;
			}
			s -= SHTPS_SY3X00_SPIBLOCK_BUFSIZE;
			addr = 0x7fff;	/* specifying no address after first read */
		}
	}while(status != 0 && retry-- > 0);

	if(status){
		shtps_spi_error_detect_flg = 1;
		SHTPS_LOG_ERR_PRINT("spi read error\n");
	}
	#if defined( SHTPS_LOG_DEBUG_ENABLE )
	else if(retry < (SHTPS_SPI_RETRY_COUNT)){
		SHTPS_LOG_DBG_PRINT("spi retry num = %d\n", SHTPS_SPI_RETRY_COUNT - retry);
	}
	#endif /* SHTPS_LOG_DEBUG_ENABLE */

	return status;
}


/* -----------------------------------------------------------------------------------
*/
static int shtps_spi_active(void *spi)
{
	#if defined( SHTPS_SPICLOCK_CONTROL_ENABLE )
		extern	void sh_spi_tps_active(struct spi_device *spi);

		sh_spi_tps_active((struct spi_device *)spi);
		SHTPS_LOG_DBG_PRINT("spi active\n");
	#endif /* SHTPS_SPICLOCK_CONTROL_ENABLE */

	return 0;
}

/* -----------------------------------------------------------------------------------
*/
static int shtps_spi_standby(void *spi)
{
	#if defined( SHTPS_SPICLOCK_CONTROL_ENABLE )
		extern	void sh_spi_tps_standby(struct spi_device *spi);

		sh_spi_tps_standby((struct spi_device *)spi);
		SHTPS_LOG_DBG_PRINT("spi standby\n");
	#endif /* SHTPS_SPICLOCK_CONTROL_ENABLE */

	return 0;
}

/* -----------------------------------------------------------------------------------
*/
static const struct shtps_ctrl_functbl TpsCtrl_SpiFuncTbl = {
	.firmware_write_f = shtps_spi_write_fw_data,
	.block_write_f    = shtps_spi_write_block,
	.block_read_f     = shtps_spi_read_block,
	.write_f          = shtps_spi_write,
	.read_f           = shtps_spi_read,
	.packet_write_f   = shtps_spi_write_packet,
	.packet_read_f    = shtps_spi_read_packet,
	.active_f         = shtps_spi_active,
	.standby_f        = shtps_spi_standby,
};

/* -----------------------------------------------------------------------------------
*/
static int shtps_spi_probe(struct spi_device *spi_p)
{
	extern	int shtps_rmi_core_probe( struct device *ctrl_dev_p,
									struct shtps_ctrl_functbl *func_p,
									void *ctrl_p,
									char *modalias,
									int gpio_irq);
	int result;

	_log_msg_sync( LOGMSG_ID__PROBE, "");
	SHTPS_LOG_SPI_FUNC_CALL();

	/* ---------------------------------------------------------------- */
	result = spi_setup(spi_p);
	if(result < 0){
		SHTPS_LOG_DBG_PRINT("spi_setup fail\n");
		goto fail_err;
	}

	/* ---------------------------------------------------------------- */
	result = shtps_rmi_core_probe(
							&spi_p->dev,
							(struct shtps_ctrl_functbl*)&TpsCtrl_SpiFuncTbl,
							(void *)spi_p,
							spi_p->modalias,
							spi_p->irq );
	if(result){
		goto fail_err;
	}

	/* ---------------------------------------------------------------- */
	/*	no available
		spi_set_drvdata(spi_p, ts_p); -> dev_set_drvdata(&spi->dev, ts_p); 
	*/

	_log_msg_sync( LOGMSG_ID__PROBE_DONE, "");
	return 0;
	/* ---------------------------------------------------------------- */


fail_err:
	_log_msg_sync( LOGMSG_ID__PROBE_FAIL, "");
	return result;
}

/* -----------------------------------------------------------------------------------
*/
static int shtps_spi_remove(struct spi_device *spi_p)
{
	extern int shtps_rmi_core_remove(struct shtps_rmi_spi *, struct device *);
	struct shtps_rmi_spi *ts_p = spi_get_drvdata(spi_p);
	int ret = shtps_rmi_core_remove(ts_p, &spi_p->dev);

	_log_msg_sync( LOGMSG_ID__REMOVE_DONE, "");
	SHTPS_LOG_DBG_PRINT("shtps_spi_remove() done\n");
	return ret;
}

/* -----------------------------------------------------------------------------------
*/
static int shtps_spi_suspend(struct device *dev)
{
	extern int shtps_rmi_core_suspend(struct shtps_rmi_spi *);
	struct shtps_rmi_spi *ts_p = (struct shtps_rmi_spi*)dev_get_drvdata(dev);
	int ret = shtps_rmi_core_suspend(ts_p);

	_log_msg_sync( LOGMSG_ID__SUSPEND, "");
	return ret;
}
/* -----------------------------------------------------------------------------------
*/
static int shtps_spi_resume(struct device *dev)
{
	extern int shtps_rmi_core_resume(struct shtps_rmi_spi *);
	struct shtps_rmi_spi *ts_p = (struct shtps_rmi_spi*)dev_get_drvdata(dev);
	int ret = shtps_rmi_core_resume(ts_p);

	_log_msg_sync( LOGMSG_ID__RESUME, "");
	return ret;
}

/* -----------------------------------------------------------------------------------
*/
#ifdef CONFIG_OF // Open firmware must be defined for dts useage
static struct of_device_id shtps_rmi_table[] = {
	{ . compatible = "sharp,shtps_rmi" ,}, // Compatible node must match dts
	{ },
};
#endif

static const struct dev_pm_ops shtps_spi_pm_ops = {
	.suspend	= shtps_spi_suspend,
	.resume		= shtps_spi_resume,
};

static struct spi_driver shtps_spi_driver = {
	.probe		= shtps_spi_probe,
	.remove		= shtps_spi_remove,
	.driver		= {
		#ifdef CONFIG_OF // Open firmware must be defined for dts useage
			.of_match_table = shtps_rmi_table,
		#endif
		.name = "shtps_rmi",	// SH_TOUCH_DEVNAME,
		.owner = THIS_MODULE,
		.pm = &shtps_spi_pm_ops,
	},
};

/* -----------------------------------------------------------------------------------
*/
static int __init shtps_spi_init(void)
{
	SHTPS_LOG_DBG_PRINT("shtps_spi_init() start\n");
	_log_msg_sync( LOGMSG_ID__INIT, "");

	return spi_register_driver(&shtps_spi_driver);
}
module_init(shtps_spi_init);

/* -----------------------------------------------------------------------------------
*/
static void __exit shtps_spi_exit(void)
{
	spi_unregister_driver(&shtps_spi_driver);

	SHTPS_LOG_DBG_PRINT("shtps_spi_exit() done\n");
	_log_msg_sync( LOGMSG_ID__EXIT, "");
}
module_exit(shtps_spi_exit);

/* -----------------------------------------------------------------------------------
*/
#endif /* defined( SHTPS_DEF_FWCTL_PROTOCOL_SPI ) */
