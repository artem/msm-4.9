/* drivers/misc/uid_stat.c
 *
 * Copyright (C) 2008 - 2009 Google, Inc.
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

#include <linux/atomic.h>

#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stat.h>
#include <linux/uid_stat.h>
#include <net/activity_stats.h>

static DEFINE_SPINLOCK(uid_lock);
static LIST_HEAD(uid_list);
static struct proc_dir_entry *parent;

#ifdef CONFIG_SHARP_PNP_SLEEP_SLEEPLOG
#include <soc/qcom/sharp/sh_sleeplog.h>
#endif /* CONFIG_SHARP_PNP_SLEEP_SLEEPLOG */

struct uid_stat {
	struct list_head link;
	uid_t uid;
	atomic_t tcp_rcv;
	atomic_t tcp_snd;
#ifdef CONFIG_SHARP_PNP_SLEEP_SLEEPLOG
	unsigned int last_tcp_rcv;
	unsigned int last_tcp_snd;
	char process_name[UID_STATS_MAX_PROCESS_NAME];
#endif /* CONFIG_SHARP_PNP_SLEEP_SLEEPLOG */
};

static struct uid_stat *find_uid_stat(uid_t uid) {
	struct uid_stat *entry;

	list_for_each_entry(entry, &uid_list, link) {
		if (entry->uid == uid) {
			return entry;
		}
	}
	return NULL;
}

static int uid_stat_atomic_int_show(struct seq_file *m, void *v)
{
	unsigned int bytes;
	atomic_t *counter = m->private;

	bytes = (unsigned int) (atomic_read(counter) + INT_MIN);
	seq_printf(m, "%u\n", bytes);
	return seq_has_overflowed(m) ? -ENOSPC : 0;
}

static int uid_stat_read_atomic_int_open(struct inode *inode, struct file *file)
{
	return single_open(file, uid_stat_atomic_int_show, PDE_DATA(inode));
}

static const struct file_operations uid_stat_read_atomic_int_fops = {
	.open		= uid_stat_read_atomic_int_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

/* Create a new entry for tracking the specified uid. */
static struct uid_stat *create_stat(uid_t uid) {
	struct uid_stat *new_uid;
	/* Create the uid stat struct and append it to the list. */
	new_uid = kmalloc(sizeof(struct uid_stat), GFP_ATOMIC);
	if (!new_uid)
		return NULL;

	new_uid->uid = uid;
	/* Counters start at INT_MIN, so we can track 4GB of network traffic. */
	atomic_set(&new_uid->tcp_rcv, INT_MIN);
	atomic_set(&new_uid->tcp_snd, INT_MIN);

	list_add_tail(&new_uid->link, &uid_list);
	return new_uid;
}

static void create_stat_proc(struct uid_stat *new_uid)
{
	char uid_s[32];
	struct proc_dir_entry *entry;
	sprintf(uid_s, "%d", new_uid->uid);
	entry = proc_mkdir(uid_s, parent);

	/* Keep reference to uid_stat so we know what uid to read stats from. */
	proc_create_data("tcp_snd", S_IRUGO, entry,
			 &uid_stat_read_atomic_int_fops, &new_uid->tcp_snd);

	proc_create_data("tcp_rcv", S_IRUGO, entry,
			 &uid_stat_read_atomic_int_fops, &new_uid->tcp_rcv);
}

static struct uid_stat *find_or_create_uid_stat(uid_t uid)
{
	struct uid_stat *entry;
	unsigned long flags;
	spin_lock_irqsave(&uid_lock, flags);
	entry = find_uid_stat(uid);
	if (entry) {
		spin_unlock_irqrestore(&uid_lock, flags);
		return entry;
	}
	entry = create_stat(uid);
	spin_unlock_irqrestore(&uid_lock, flags);
#ifdef CONFIG_SHARP_PNP_SLEEP_SLEEPLOG
	if (entry){
		create_stat_proc(entry);
		sh_get_process_name(current, entry->process_name);
	}
#else /* CONFIG_SHARP_PNP_SLEEP_SLEEPLOG */
	if (entry)
		create_stat_proc(entry);
#endif /* CONFIG_SHARP_PNP_SLEEP_SLEEPLOG */
	return entry;
}

int uid_stat_tcp_snd(uid_t uid, int size) {
	struct uid_stat *entry;
	activity_stats_update();
	entry = find_or_create_uid_stat(uid);
	if (!entry)
		return -1;
	atomic_add(size, &entry->tcp_snd);
	return 0;
}

int uid_stat_tcp_rcv(uid_t uid, int size) {
	struct uid_stat *entry;
	activity_stats_update();
	entry = find_or_create_uid_stat(uid);
	if (!entry)
		return -1;
	atomic_add(size, &entry->tcp_rcv);
	return 0;
}

#ifdef CONFIG_SHARP_PNP_SLEEP_SLEEPLOG
char *sh_write_buffer_uid_stat(char *buffer) {
	unsigned long flags;
	struct uid_stat *entry;
	char *head, *ret;
	unsigned int rcv, snd, diff_rcv, diff_snd;

	head = buffer;
	buffer += sizeof(short);

	spin_lock_irqsave(&uid_lock, flags);
	list_for_each_entry(entry, &uid_list, link) {
		rcv = (unsigned int) (atomic_read(&entry->tcp_rcv) + INT_MIN);
		if(rcv > entry->last_tcp_rcv) {
			diff_rcv = rcv - entry->last_tcp_rcv;
		} else {
			diff_rcv = 0;
		}
		entry->last_tcp_rcv = rcv;

		snd = (unsigned int) (atomic_read(&entry->tcp_snd) + INT_MIN);
		if(snd > entry->last_tcp_snd) {
			diff_snd = snd - entry->last_tcp_snd;
		} else {
			diff_snd = 0;
		}
		entry->last_tcp_snd = snd;

		if(diff_snd > 0 || diff_rcv > 0) {
			ret = sh_write_buffer_uid_stat_internal(buffer, entry->uid, entry->process_name, diff_rcv, diff_snd);
			if(!ret) break;
			buffer = ret;
		}
	}
	spin_unlock_irqrestore(&uid_lock, flags);

	return sh_write_buffer_uid_stat_finish(buffer, head);
}
#endif /* CONFIG_SHARP_PNP_SLEEP_SLEEPLOG */

static int __init uid_stat_init(void)
{
	parent = proc_mkdir("uid_stat", NULL);
	if (!parent) {
		pr_err("uid_stat: failed to create proc entry\n");
		return -1;
	}
	return 0;
}

__initcall(uid_stat_init);
