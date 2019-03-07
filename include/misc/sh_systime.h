/* include/sharp/sh_systime.h
 *
 * Copyright (C) 2013 Sharp Corporation
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

#ifndef _SH_SYSTIME_H_
#define _SH_SYSTIME_H_

#include <soc/qcom/sh_smem.h>


/*
 * TIMETICK_CLK_OFFSET is register address of "mpm2-sleep-counter".
 * This address is defined by dtsi and it differs from each platform.
 * Here is example of Anasui(msm8953.dtsi). -> NOTE: address is "0x4a3000"
 * ------
 * qcom,mpm2-sleep-counter@4a3000 {
 * 	compatible = "qcom,mpm2-sleep-counter";
 * 	reg = <0x4a3000 0x1000>;
 * 	clock-frequency = <32768>;
 * };
 * ------
 * And also TIMETICK_CLK_FREQ is clock-frequency.
 * Below this section, TIMETICK_CLK_SPEED_INT and CALCULATE_TIMESTAMP are
 * copied from boot_target.h
 * They assume that frequency is 32768Hz, but it may be changed on feature platform.
 * So we record log if frequency is different from now.
 */
extern unsigned int TIMETICK_CLK_OFFSET;
extern unsigned int TIMETICK_CLK_FREQ;
#define TIMETICK_CLK_FREQ_ASSUMED 32768


/* 
 * ref. boot_images/core/boot/secboot3/hw/msm8992/boot_target.h
 *
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 
 */

/*=========================================================================== 
  Clock frequency is 32.768 KHz
  The number of microseconds per cycle is:
  1 / (32.768 KHz) = 30.52ms
  The int part is 30
===========================================================================*/
#define TIMETICK_CLK_SPEED_INT       30

/*=========================================================================== 
  Clock frequency is 32.768 KHz
  The number of microseconds per cycle is:
  1 / (32.768 KHz) = 30.52ms
  we round 0.52 to 1/2.
  Timestamp is calculated as : count*int + count/2
  Floating point arithmetic should not be used to avoid error and speed penalty
===========================================================================*/
#define CALCULATE_TIMESTAMP(COUNT) \
  ((COUNT)*TIMETICK_CLK_SPEED_INT + (COUNT)/2)

/* 
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 
 */


/* 
 * ref. LINUX/android/bootable/bootloader/lk/platform/msm_shared/smem.h
 *
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 
 */
#ifndef __PLATFORM_MSM_SHARED_SMEM_H
struct smem_proc_comm {
	unsigned command;
	unsigned status;
	unsigned data1;
	unsigned data2;
};

struct smem_heap_info {
	unsigned initialized;
	unsigned free_offset;
	unsigned heap_remaining;
	unsigned reserved;
};

struct smem_alloc_info {
	unsigned allocated;
	unsigned offset;
	unsigned size;
	unsigned reserved;
};

typedef enum {
	SMEM_SPINLOCK_ARRAY = 7,

	SMEM_AARM_PARTITION_TABLE = 9,

	SMEM_SLEEP_POWER_COLLAPSE_DISABLED = 89,

	SMEM_APPS_BOOT_MODE = 106,

	SMEM_SHARP_ID_LOCATION = 134,

	SMEM_BOARD_INFO_LOCATION = 137,

	SMEM_USABLE_RAM_PARTITION_TABLE = 402,

	SMEM_POWER_ON_STATUS_INFO = 403,

	SMEM_RLOCK_AREA = 404,

	SMEM_BOOT_INFO_FOR_APPS = 418,

	SMEM_FIRST_VALID_TYPE = SMEM_SPINLOCK_ARRAY,
	SMEM_LAST_VALID_TYPE = SMEM_BOOT_INFO_FOR_APPS,

	SMEM_MAX_SIZE = SMEM_BOOT_INFO_FOR_APPS + 1,
} smem_mem_type_t;

struct smem {
	struct smem_proc_comm proc_comm[4];
	unsigned version_info[32];
	struct smem_heap_info heap_info;
	struct smem_alloc_info alloc_info[SMEM_MAX_SIZE];
};
#endif				/* __PLATFORM_MSM_SHARED_SMEM_H */
/* 
 * <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< 
 */


typedef enum
{
  SHSYS_TIMEMSTAMP_SBL1_START = 0,
  SHSYS_TIMEMSTAMP_SBL1_END,
  SHSYS_TIMEMSTAMP_APPSBL_START,
  SHSYS_TIMEMSTAMP_APPSBL_END,
  SHSYS_TIMEMSTAMP_DECOMPRESS_START,
  SHSYS_TIMEMSTAMP_DECOMPRESS_END,
  SHSYS_TIMEMSTAMP_KERNEL_START,
  SHSYS_TIMEMSTAMP_BOOT_END,
  SHSYS_TIMEMSTAMP_SHUTDOWN_START = 12,
  SHSYS_TIMEMSTAMP_HOTBOOT_START,
  SHSYS_TIMEMSTAMP_HOTBOOT_COMP,
  SHSYS_TIMEMSTAMP_KEYGUARD_START,
  SHSYS_TIMEMSTAMP_FREE,
  SHSYS_TIMEMSTAMP_MAX_NUM = 32
}SHSYS_TIMEMSTAMP_POINT;


#define SH_SMEM_ALLOCINFO \
     (struct smem_alloc_info *)(&(((struct smem *)((void *)(MSM_SHARED_BASE)))->alloc_info[SMEM_SHARP_ID_LOCATION]))

#define SH_SMEM_OFFSET \
     (sharp_smem_common_type *)((void *)(MSM_SHARED_BASE + *(volatile unsigned *)(&(SH_SMEM_ALLOCINFO)->offset)))

#define SET_SHSYS_TIMESTAMP(point) \
    if (*(volatile unsigned *)(&(SH_SMEM_ALLOCINFO)->allocated)) { \
        (SH_SMEM_OFFSET)->shsys_timestamp[point] = CALCULATE_TIMESTAMP(*(volatile unsigned *)TIMETICK_CLK_OFFSET); \
    }


int sh_systime_read_current(unsigned long long *systime);
void sh_systime_log_shutdown_complete_time(void);

#endif /* _SH_SYSTIME_H_ */
