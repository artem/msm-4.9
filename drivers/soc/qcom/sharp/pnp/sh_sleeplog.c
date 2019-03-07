/* drivers/soc/qcom/sharp/pnp/sh_sleeplog.c
 *
 * Copyright (C) 2013 SHARP CORPORATION
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
#ifdef CONFIG_SHARP_PNP_SLEEP_SLEEPLOG
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/rculist.h>
#include <linux/pm_wakeup.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include <soc/qcom/pm.h>
#include <soc/qcom/sharp/sh_sleeplog.h>

static spinlock_t lock;

/**
 * Definitions
 */
#define MAX_ALARM_COUNT 100
#define MAX_WAKEUP_SOURCES_COUNT 100
#define IRQ_TABLE_SIZE 1021
#define MAX_SCREEN_STATE_ARRAY 100
#define MAX_UID_STATS 100

#ifdef CONFIG_ARM64
#define UNITSIZE_ALARM 10
#else
#define UNITSIZE_ALARM 6
#endif /* CONFIG_ARM64 */

#define UNITSIZE_WAKELOCK 24
#define UNITSIZE_IRQ 4
#define UNITSIZE_PC 8
#ifdef CONFIG_ARM64
#define UNITSIZE_SCREEN_STATE 18
#else
#define UNITSIZE_SCREEN_STATE 10
#endif /* CONFIG_ARM64 */
#define UNITSIZE_TCP 42

#define COUNT_BUFFER 2

#define BUFFER_SIZE ( \
     COUNT_BUFFER + UNITSIZE_ALARM        * MAX_ALARM_COUNT \
   + COUNT_BUFFER + UNITSIZE_WAKELOCK     * MAX_WAKEUP_SOURCES_COUNT \
   + COUNT_BUFFER + UNITSIZE_IRQ          * IRQ_TABLE_SIZE \
   + COUNT_BUFFER + UNITSIZE_PC           * 2 \
   + COUNT_BUFFER + UNITSIZE_SCREEN_STATE * MAX_SCREEN_STATE_ARRAY \
   + COUNT_BUFFER + UNITSIZE_TCP          * MAX_UID_STATS )

#define BAD_PROCESS_STRING "UNKNOWN"
#define MAX_WAKEUP_SOURCES_NAME 16
#define PMIC_WAKEUP_GIC_IRQ 222

#define SLEEPLOG_MAGIC 'S'
#define SLEEPLOG_IOC_CREATE _IOR(SLEEPLOG_MAGIC, 0, int)
#define SLEEPLOG_IOC_DELBUF _IOR(SLEEPLOG_MAGIC, 1, int)

/**
 * Structures
 */
#ifdef CONFIG_ARM64
struct alarm_count{
	int64_t function;
	int count;
};
#else
struct alarm_count{
	int function;
	int count;
};
#endif
struct alarm_count_chain{
	struct list_head link;
	struct alarm_count count;
};
struct screen_state{
	struct timespec time;
	int state;
};

/**
 * Local buffers
 */
struct alarm_count_chain allocated_alarm_count[MAX_ALARM_COUNT];
static int sh_irq_count[IRQ_TABLE_SIZE];
struct screen_state screen_state_array[MAX_SCREEN_STATE_ARRAY];

LIST_HEAD(alarm_list);
static bool sh_pmic_wakeup_flag;
static int screen_state_write_point = 0;
static int uid_stats_write_count = 0;

static char *dump_buffer = NULL;
static int buffer_size = 0;
static int dumped_size = 0;

static int64_t last_suspend_time = 0;
static int64_t last_idle_time = 0;

/**
 * Functions
 */
void sh_write_buffer_word(char *buffer, int value)
{
	unsigned short temp = value > 0x0000FFFF ? 0xFFFF : (unsigned short)value;
	memcpy(buffer, &temp, 2);
}

static char *sh_write_buffer_alarm_list(char *buffer){
	char *head;
	short size;
	struct list_head *pos;

	head = buffer;
	buffer += sizeof(short);

	list_for_each(pos, &alarm_list){
		struct alarm_count_chain *c = 
			list_entry(pos, struct alarm_count_chain, link);
		if(c->count.count <= 0) continue;
#ifdef CONFIG_ARM64
		memcpy(buffer, &c->count.function, sizeof(int64_t));
		buffer += sizeof(int64_t);
#else
		memcpy(buffer, &c->count.function, sizeof(int));
		buffer += sizeof(int);
#endif
		sh_write_buffer_word(buffer, c->count.count);
		buffer += sizeof(short);
	}

	size = buffer - head - sizeof(short);
	memcpy(head, &size, sizeof(short));

	return buffer;
}

char *sh_write_buffer_wakeup_sources_internal(char *buffer, struct list_head *wakeup_sources)
{
	struct wakeup_source *ws;
	ktime_t active_time;
	unsigned long flags;
	ktime_t total_time, delta;
	int copy_size;
	int i = 0;
	short size;
	char *head;

	head = buffer;
	buffer += sizeof(short);

	rcu_read_lock();
	list_for_each_entry_rcu(ws, wakeup_sources, entry){
		spin_lock_irqsave(&ws->lock, flags);
		total_time = ws->total_time;
		if (ws->active) {
			ktime_t now = ktime_get();
			active_time = ktime_sub(now, ws->last_time);
			total_time = ktime_add(total_time, active_time);
		}

		delta = ktime_sub(total_time, ws->last_log_time);
		ws->last_log_time = total_time;
		spin_unlock_irqrestore(&ws->lock, flags);

		if(ktime_to_ms(delta) < 3000) continue;

		copy_size = strlen(ws->name) < MAX_WAKEUP_SOURCES_NAME ? 
			strlen(ws->name) : MAX_WAKEUP_SOURCES_NAME;
		memcpy(buffer, ws->name, copy_size);
		buffer += MAX_WAKEUP_SOURCES_NAME;
		memcpy(buffer, &delta, sizeof(ktime_t));
		buffer += sizeof(ktime_t);

		i++;
		if(i == MAX_WAKEUP_SOURCES_COUNT)
			break;
	}
	rcu_read_unlock();

	size = buffer - head - sizeof(short);
	memcpy(head, &size, sizeof(short));

	return buffer;
}

static char *sh_write_buffer_irq_counter(char *buffer)
{
	int i = 0;
	short size;
	char *head;

	head = buffer;
	buffer += sizeof(short);

	for(i=0; i<IRQ_TABLE_SIZE; i++){
		if(sh_irq_count[i] <= 0) continue;
		sh_write_buffer_word(buffer, (short)i);
		buffer += sizeof(short);
		sh_write_buffer_word(buffer, sh_irq_count[i]);
		buffer += sizeof(short);
	}

	size = buffer - head - sizeof(short);
	memcpy(head, &size, sizeof(short));

	return buffer;
}

static char *sh_write_buffer_pm_stats(char *buffer)
{
	short n = 2 * sizeof(int64_t);
	int64_t suspend, idle;
	int64_t suspend_result = sh_get_pm_stats_suspend();
	int64_t idle_result = sh_get_pm_stats_idle();

	suspend = suspend_result - last_suspend_time;
	last_suspend_time = suspend_result;
	idle = idle_result - last_idle_time;
	last_idle_time = idle_result;

	memcpy(buffer, &n, sizeof(short));
	buffer += sizeof(short);
	memcpy(buffer, &suspend, sizeof(int64_t));
	buffer += sizeof(int64_t);
	memcpy(buffer, &idle, sizeof(int64_t));
	buffer += sizeof(int64_t);

	return buffer;
}

static char *sh_write_buffer_screen_state(char *buffer)
{
	int i = 0;
	short size;
	char *head;

	head = buffer;
	buffer += sizeof(short);

	for(i=0; i<MAX_SCREEN_STATE_ARRAY; i++){
		if(screen_state_array[i].time.tv_sec == 0 && screen_state_array[i].time.tv_nsec == 0) break;
		memcpy(buffer, &screen_state_array[i].time, sizeof(struct timespec));
		buffer += sizeof(struct timespec);
		sh_write_buffer_word(buffer, screen_state_array[i].state);
		buffer += sizeof(short);
	}

	size = buffer - head - sizeof(short);
	memcpy(head, &size, sizeof(short));

	return buffer;
}

char *sh_write_buffer_uid_stat_internal(char *buffer, 
	uid_t uid, char *process_name, unsigned int tcp_rcv, unsigned int tcp_snd)
{
	if(uid_stats_write_count > MAX_UID_STATS)
		return 0;

	sh_write_buffer_word(buffer, uid);
	buffer += sizeof(uid_t);
	memcpy(buffer, process_name, UID_STATS_MAX_PROCESS_NAME);
	buffer += UID_STATS_MAX_PROCESS_NAME;
	memcpy(buffer, &tcp_rcv, sizeof(unsigned int));
	buffer += sizeof(unsigned int);
	memcpy(buffer, &tcp_snd, sizeof(unsigned int));
	buffer += sizeof(unsigned int);

	uid_stats_write_count++;

	return buffer;
}

char *sh_write_buffer_uid_stat_finish(char *buffer, char *head)
{
	short size;

	size = buffer - head - sizeof(short);
	memcpy(head, &size, sizeof(short));

	return buffer;
}

static void init_data(void){
	struct list_head *pos;
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	list_for_each(pos, &alarm_list){
		struct alarm_count_chain *c;
		c = list_entry(pos, struct alarm_count_chain, link);
		c->count.count = 0;
	}
	memset(sh_irq_count, 0, sizeof(int) * IRQ_TABLE_SIZE);
	memset(screen_state_array, 0, sizeof(screen_state_array));
	screen_state_write_point = 0;
	spin_unlock_irqrestore(&lock, flags);
}
#ifdef CONFIG_ARM64
void sh_count_mark_alarm(enum alarmtimer_type alarm_type, int64_t function)
#else
void sh_count_mark_alarm(enum alarmtimer_type alarm_type, int function)
#endif /* CONFIG_ARM64 */

{
	struct list_head *pos;
	int i = 0;
	unsigned long flags;

	if(alarm_type != ALARM_REALTIME &&
		alarm_type != ALARM_BOOTTIME)
		return;
	
	list_for_each(pos, &alarm_list) {
		struct alarm_count_chain *c;
		c = list_entry(pos, struct alarm_count_chain, link);
		if(c->count.function == function){
			spin_lock_irqsave(&lock, flags);
			c->count.count++;
			spin_unlock_irqrestore(&lock, flags);
			return;
		}
		i++;
	}
	
	if(i < MAX_ALARM_COUNT){
		spin_lock_irqsave(&lock, flags);
		allocated_alarm_count[i].count.function = function;
		allocated_alarm_count[i].count.count    = 1;
		list_add_tail(&allocated_alarm_count[i].link, pos);
		spin_unlock_irqrestore(&lock, flags);
	}
}

static void sh_up_pmic_wakeup_flag(void)
{
	sh_pmic_wakeup_flag = true;
}

static bool sh_is_pmic_wakeup_flag(void)
{
	return sh_pmic_wakeup_flag;
}

static void sh_down_pmic_wakeup_flag(void)
{
	sh_pmic_wakeup_flag = false;
}

void sh_count_gic_counter(int irq)
{
	sh_count_irq_counter(irq);
	if(irq == PMIC_WAKEUP_GIC_IRQ)
		sh_up_pmic_wakeup_flag();
}

void sh_count_irq_counter(int irq)
{
	unsigned long flags;

	if(irq >= 0 && irq < IRQ_TABLE_SIZE){
		spin_lock_irqsave(&lock, flags);
		sh_irq_count[irq]++;
		spin_unlock_irqrestore(&lock, flags);
	}
}

void sh_count_irq_if_pmic_wakeup(int irq)
{
	if(sh_is_pmic_wakeup_flag()){
		sh_count_irq_counter(irq);
		sh_down_pmic_wakeup_flag();
	}
}

void sh_set_screen_state(struct timespec ts, suspend_state_t state)
{
	unsigned long flags;

	spin_lock_irqsave(&lock, flags);
	screen_state_array[screen_state_write_point].time = ts;
	screen_state_array[screen_state_write_point].state =
		state > PM_SUSPEND_ON ? 0 : 1;

	screen_state_write_point =
		(screen_state_write_point + 1) % MAX_SCREEN_STATE_ARRAY;
	spin_unlock_irqrestore(&lock, flags);
}

void sh_get_process_name(struct task_struct *task, char *result_name)
{
	struct mm_struct *mm;
	unsigned int len;

	memset(result_name, 0, UID_STATS_MAX_PROCESS_NAME);
	if(!task){
		strncpy(result_name, BAD_PROCESS_STRING, strlen(BAD_PROCESS_STRING));
		return;
	}

	mm = get_task_mm(task);

	if (!mm || !mm->arg_end){
		strncpy(result_name, BAD_PROCESS_STRING, strlen(BAD_PROCESS_STRING));
	}else{
		len = mm->arg_end - mm->arg_start < UID_STATS_MAX_PROCESS_NAME ? 
			mm->arg_end - mm->arg_start : UID_STATS_MAX_PROCESS_NAME;
		access_process_vm(task, mm->arg_start, result_name, len, 0);
	}
	if(mm){
		mmput(mm);
	}
}

static int create_buffer(void)
{
	char *head, *buffer;
	buffer_size = PAGE_ALIGN(BUFFER_SIZE);

	if(dump_buffer == NULL) {
		if( (dump_buffer = kzalloc(buffer_size, GFP_KERNEL)) == NULL ){
			pr_err("shsleeplog : fail to allocate buffer.\n");
			return 0;
		}
	}

	head = buffer = dump_buffer;

	buffer = sh_write_buffer_alarm_list(buffer);
	buffer = sh_write_buffer_wakeup_sources(buffer);
	buffer = sh_write_buffer_irq_counter(buffer);
	buffer = sh_write_buffer_pm_stats(buffer);
	buffer = sh_write_buffer_screen_state(buffer);
	uid_stats_write_count = 0;
	buffer = sh_write_buffer_uid_stat(buffer);

	dumped_size = buffer - head;

	init_data();

	return buffer_size;
}
static void delete_buffer(void)
{
	if(dump_buffer != NULL) {
		kfree(dump_buffer);
		dump_buffer = NULL;
	}
}

#ifndef VM_RESERVED
#define VM_RESERVED (VM_DONTEXPAND | VM_DONTDUMP)
#endif

static int sh_sleeplog_mmap(struct file *filep, struct vm_area_struct *vma)
{
	int ret = 0;
	unsigned long pfn;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long length = vma->vm_end - vma->vm_start;

	if(dump_buffer == NULL) return -EINVAL;
	if( (offset >= buffer_size) || (length > (buffer_size - offset)) ) {
		ret = -EINVAL;
		goto exit_after_alloc;
	}

	vma->vm_flags |= VM_RESERVED;
	pfn = virt_to_phys(dump_buffer + offset) >> PAGE_SHIFT;
	if( (ret = remap_pfn_range(vma, vma->vm_start, pfn, length, vma->vm_page_prot)) != 0 ){
		pr_err("shsleeplog : failed to remap : %d\n", ret);
		ret = -EAGAIN;
		goto exit_after_alloc;
	}

exit_after_alloc:
//	delete_buffer();

	return ret;
}

static long sh_sleeplog_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *) arg;

	switch (cmd) {
	case SLEEPLOG_IOC_CREATE:
		create_buffer();
		if (!access_ok(VERIFY_WRITE, argp, sizeof(int)))
			return -EFAULT;
		if(copy_to_user(argp, &dumped_size, sizeof(int))) {
			pr_err("%s() copy_to_user error\n", __func__);
			return -EFAULT;
		}
		break;
	case SLEEPLOG_IOC_DELBUF:
		delete_buffer();
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct file_operations sh_sleeplog_fops = {
	.owner = THIS_MODULE,
	.mmap  = sh_sleeplog_mmap,
	.unlocked_ioctl = sh_sleeplog_ioctl,
};

static struct miscdevice sh_sleeplog_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sh_sleeplog",
	.fops = &sh_sleeplog_fops,
};

static int __init sh_sleeplog_init( void )
{
	int ret;
	spin_lock_init(&lock);
	ret = misc_register(&sh_sleeplog_dev);
	return ret;
}
static void __exit sh_sleeplog_exit(void)
{
	misc_deregister(&sh_sleeplog_dev);
}

module_init(sh_sleeplog_init);
module_exit(sh_sleeplog_exit);

MODULE_DESCRIPTION("sh_sleeplog");
#endif /* CONFIG_SHARP_PNP_SLEEP_SLEEPLOG */
MODULE_LICENSE("GPL v2");
