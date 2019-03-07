/* include/sharp/snfc_en.h
 *
 * Copyright (C) 2016 SHARP CORPORATION
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
#ifndef _LINUX_SNFC_EN_H
#define _LINUX_SNFC_EN_H

#define SNFC_OFF_SEQUENCE_NFC 0
#define SNFC_ON_SEQUENCE      1
#define SNFC_OFF_SEQUENCE_SIM 2

#define NFC_SNFC_EN_IOC_MAGIC 'd'
#define NFC_SNFC_EN_IOCTL_HVDD_H        _IO  ( NFC_SNFC_EN_IOC_MAGIC, 0x01)
#define NFC_SNFC_EN_IOCTL_HVDD_L        _IO  ( NFC_SNFC_EN_IOC_MAGIC, 0x02)
#define NFC_SNFC_IOCTL_GET_NINT         _IOR ( NFC_SNFC_EN_IOC_MAGIC, 0x03, unsigned int)
#define NFC_SNFC_IOCTL_GET_STATUS       _IOR ( NFC_SNFC_EN_IOC_MAGIC, 0x04, unsigned int)
#define NFC_SNFC_IOCTL_GET_CLKREQ       _IOR ( NFC_SNFC_EN_IOC_MAGIC, 0x05, unsigned int)
#endif /* _LINUX_SNFC_EN_H */

