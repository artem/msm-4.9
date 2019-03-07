/* include/sharp/shrlog.h
 *
 * Copyright (C) 2010 Sharp Corporation
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

#ifndef _SHRLOG_H_
#define _SHRLOG_H_

#include <linux/sched.h>

#define SHRLOG_FIXED_MAGIC_NUM (0x88990011)
#define SHRLOG_FIXED_LINUX_MAGIC_NUM (0x88992211)
#define SHRLOG_FIXED_MAGIC_NUM_A (0x951C0A275C38FC31ULL)
#define SHRLOG_FIXED_MAGIC_NUM_B (0x539A6BA63E2D4CCAULL)

#if defined(CONFIG_ARM64)
typedef unsigned int shrlog_uint32;
typedef unsigned long shrlog_uint64;
#elif defined(CONFIG_ARM)
typedef unsigned long shrlog_uint32;
typedef unsigned long long shrlog_uint64;
#else
#error "types in linux not specified."
#endif

struct shrlog_ram_fixed_T {
	shrlog_uint64 shrlog_ram_fixed_addr;
	shrlog_uint64 magic_num1;
	shrlog_uint64 handle_dload1;
	shrlog_uint64 linux_phys_offset;
	shrlog_uint64 imem_restart_reason;
	shrlog_uint64 linux_kimage_voffset;
	shrlog_uint64 linux_kalsr_offset;
	shrlog_uint64 linux_ttbr;
	shrlog_uint64 handle_dload2;
	shrlog_uint64 smagic1;
	shrlog_uint64 pad[28];
	shrlog_uint64 smagic2;
	shrlog_uint64 magic_num2;
};

typedef unsigned long shrlog_kernel_t;

typedef struct
{
	/* Be sure to add virtual addresses, which must be checked first, by using shrlog_kernel_t */
	shrlog_kernel_t xtime_sec_addr;
	shrlog_kernel_t symbol_put_addr;
	shrlog_kernel_t per_cpu_offset_addr;

#if defined(CONFIG_SHARP_SHLOG_TASKLIST)
	shrlog_kernel_t init_task_addr;
#ifdef CONFIG_ARM
	shrlog_kernel_t __start_unwind_idx_addr;
	shrlog_kernel_t __origin_unwind_idx_addr;
	shrlog_kernel_t __stop_unwind_idx_addr;
#endif
#endif /* CONFIG_SHARP_SHLOG_TASKLIST */

#if defined(CONFIG_SHARP_SHLOG_LATESTSTACK)
	shrlog_kernel_t runqueues_addr;
#endif /* CONFIG_SHARP_SHLOG_LATESTSTACK */

	shrlog_kernel_t mcount_addr;

} shrlog_fixed_linux_addresses_t;

typedef struct {
	shrlog_kernel_t info_size;
	union {
		shrlog_fixed_linux_addresses_t st;
		shrlog_kernel_t values[sizeof(shrlog_fixed_linux_addresses_t)/sizeof(shrlog_kernel_t)];
	} adr;
	shrlog_uint32 size_of_task_struct;
	shrlog_uint32 size_of_thread_struct;
	shrlog_uint32 size_of_mm_struct;
	shrlog_uint32 stack_offset;
	shrlog_uint32 tasks_offset;
	shrlog_uint32 pid_offset;
	shrlog_uint32 thread_group_offset;
	shrlog_uint32 comm_offset;
	shrlog_uint32 mm_offset;
	shrlog_uint32 pgd_offset;
	shrlog_uint32 size_of_module;
	shrlog_uint32 module_list_offset;
	shrlog_uint32 module_name_offset;
	shrlog_uint32 module_name_size;
	shrlog_uint32 module_version_offset;
	shrlog_uint32 module_startup_offset;
	shrlog_uint32 module_init_offset;
	shrlog_uint32 module_core_offset;
	shrlog_uint32 module_init_size_offset;
	shrlog_uint32 module_core_size_offset;
#ifdef CONFIG_ARM64
	shrlog_uint32 size_of_cpu_context;
	shrlog_uint32 cpu_context_fp_offset;
	shrlog_uint32 cpu_context_sp_offset;
	shrlog_uint32 cpu_context_pc_offset;
#endif /* CONFIG_ARM64 */
#ifdef CONFIG_ARM
	shrlog_uint32 size_of_thread_info;
	shrlog_uint32 cpu_context_in_thread_info;
	shrlog_uint32 size_of_cpu_context_save;
	shrlog_uint32 cpu_context_save_fp_offset;
	shrlog_uint32 cpu_context_save_sp_offset;
	shrlog_uint32 cpu_context_save_pc_offset;
#endif /* CONFIG_ARM */

#if defined(CONFIG_SHARP_SHLOG_RAMOOPS)
	shrlog_kernel_t oops_addr;
	shrlog_uint32 oops_size;
	shrlog_uint32 oops_console_size;
	shrlog_uint32 oops_record_size;
	shrlog_uint32 oops_ftrace_size;
	shrlog_uint32 oops_pmsg_size;
#endif /* SHLOG_SHLOG_RAMOOPS */

#if defined(CONFIG_SHARP_SHLOG_TASKLIST)
#endif /* CONFIG_SHARP_SHLOG_TASKLIST */

#if defined(CONFIG_SHARP_SHLOG_LATESTSTACK)
	shrlog_uint32 rq_curr_offset;
	shrlog_uint32 rq_idle_offset;
	shrlog_uint32 rq_stop_offset;
	shrlog_uint32 rq_nr_running_offset;
#endif /* CONFIG_SHARP_SHLOG_LATESTSTACK */

	shrlog_kernel_t module_addr;

	shrlog_kernel_t xtime_sec_value;
	shrlog_kernel_t _text_value;
	shrlog_kernel_t _stext_value;
	shrlog_kernel_t _etext_value;

	shrlog_kernel_t info_magic;
} shrlog_fixed_apps_info;

#if defined(CONFIG_SHARP_SHLOG_SHOW_ABOOTLOG)
#ifndef RLOG_ABOOT_LOG_BUF_SIZE
#define RLOG_ABOOT_LOG_BUF_SIZE  (4096)
#endif
typedef struct {
	shrlog_uint32 magic_top;
	shrlog_uint32 bufsize;
	shrlog_uint32 cur_idx;
	shrlog_uint32 total_size;
	char data[RLOG_ABOOT_LOG_BUF_SIZE];
	shrlog_uint32 magic_end;
} shrlog_aboot_log_t;
#endif /* CONFIG_SHARP_SHLOG_SHOW_ABOOTLOG */

#endif /* _SHRLOG_H_ */
