/* include/sharp/sh_sleeplog.h
 * Copyright (c) 2013-2014, Sharp Corporation. All rights reserved.
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
#ifdef CONFIG_SHARP_PNP_SLEEP_SLEEPLOG
#ifndef _SH_SLEEPLOG_H
#define _SH_SLEEPLOG_H

#include <linux/alarmtimer.h>
#include <linux/time.h>
#include <linux/suspend.h>
#include <linux/sched.h>
#include <asm/current.h>

#define UID_STATS_MAX_PROCESS_NAME 32
#ifdef CONFIG_ARM64
void sh_count_mark_alarm(enum alarmtimer_type alarm_type, int64_t function);
#else
void sh_count_mark_alarm(enum alarmtimer_type alarm_type, int function);
#endif /* CONFIG_ARM64 */
void sh_count_gic_counter(int irq);
void sh_count_irq_counter(int irq);
void sh_count_irq_if_pmic_wakeup(int irq);
void sh_set_screen_state(struct timespec ts, suspend_state_t state);

char *sh_write_buffer_wakeup_sources(char *buffer);
char *sh_write_buffer_wakeup_sources_internal(
	char *buffer, struct list_head *wakeup_sources);
char *sh_write_buffer_uid_stat(char *buffer);
char *sh_write_buffer_uid_stat_internal(char *buffer,
	uid_t uid, char *process_name, unsigned int tcp_rcv, unsigned int tcp_snd);
char *sh_write_buffer_uid_stat_finish(char *buffer, char *head);
void sh_get_process_name(struct task_struct *task, char *result_name);

#endif  /* _SH_SLEEPLOG_H */
#endif /* CONFIG_SHARP_PNP_SLEEP_SLEEPLOG */
