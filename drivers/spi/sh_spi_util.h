/* drivers/spi/sh_spi_util.h
 *
 * Copyright (C) 2016 Sharp Corporation
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

#ifndef _SH_SPI_UTIL_H
#define _SH_SPI_UTIL_H


#if defined(CONFIG_SHARP_SPI_EXPAND_DEBUG_FUNCTION)

extern int sh_debug_spi;

enum {
	SH_LOG_NONE    = 0,
	SH_LOG_PM      = 1U << 0, /* 1*/
	SH_LOG_COM     = 1U << 1, /* 2*/
	SH_LOG_GPIO    = 1U << 2, /* 4*/
	SH_LOG_IRQ     = 1U << 3, /* 8*/
	SH_LOG_DEBUG   = 1U << 4, /*16*/
	SH_LOG_TRACE_H = 1U << 5, /*32*/
	SH_LOG_TRACE_M = 1U << 6, /*64*/
	SH_LOG_TRACE_L = 1U << 7, /*128*/
};

/* Always output error log */
#define SH_SPILOG_ERR(dev, fmt, ...) do {\
	dev_err(dev, "[BSP]:%s() " fmt, __func__, ##__VA_ARGS__);\
} while (0)

/* output by setting value */
#define SH_SPILOG_DEBUG(level, dev, fmt, ...) do {\
	if (sh_debug_spi & level) {\
		dev_info(dev, "[BSP]:%s() " fmt, __func__, ##__VA_ARGS__);\
	}\
} while (0)

#else	/* CONFIG_SHARP_SPI_EXPAND_DEBUG_FUNCTION */
#define SH_SPILOG_ERR(dev, fmt, ...)
#define SH_SPILOG_DEBUG(level, dev, fmt, ...)
#endif	/* CONFIG_SHARP_SPI_EXPAND_DEBUG_FUNCTION */

#endif /* _SH_SPI_UTIL_H */
