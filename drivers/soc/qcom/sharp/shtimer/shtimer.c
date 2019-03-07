/* drivers/sharp/shtimer/shtimer.c
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

#include <asm/uaccess.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/time.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/pm_wakeup.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/rtc.h>
#include <linux/module.h>
#include <linux/alarmtimer.h>
#include <linux/compat.h>

#define SHTIMER_MAGIC 't'
#define SHTIMER_CMD_SET _IOW(SHTIMER_MAGIC, 0, struct timespec)
#define SHTIMER_CMD_CANCEL _IO(SHTIMER_MAGIC, 1)
#define SHTIMER_CMD_WAIT _IO(SHTIMER_MAGIC, 2)
#define SHTIMER_CMD_WAKELOCK _IO(SHTIMER_MAGIC, 3)
#define SHTIMER_CMD_WAKEUNLOCK _IO(SHTIMER_MAGIC, 4)
#ifdef CONFIG_COMPAT
#define SHTIMER_CMD_SET_COMPAT _IOW(SHTIMER_MAGIC, 0, struct compat_timespec)
#endif

#define SHTIMER_NONE_SIG   0x00000000
#define SHTIMER_CANCEL_SIG 0x00000001
#define SHTIMER_EXPIRE_SIG 0x00000002

#define SLEEP_NEGATE_TIME 50000000

static LIST_HEAD(shtimer_head);

static const char shtimer_wake_lock_name[] = "shtimer";
static unsigned long shtimer_descriptor = 0;
static struct wakeup_source shtimer_wake_lock_id;
static DEFINE_SPINLOCK(shtimer_slock);

typedef struct
{
	struct list_head link;
	struct alarm alarm;
	unsigned long descriptor;
	unsigned long pending;
	bool waiting;
	bool is_wake_lock;
	wait_queue_head_t wait_queue;
	ktime_t tv;
	ktime_t base_tv;
} shtimer_list;

/* before call this, we use spin_lock */
static void shtimer_wake_lock(shtimer_list *list)
{
	list->is_wake_lock = true;
	__pm_stay_awake(&shtimer_wake_lock_id);
}

static void shtimer_wake_unlock(shtimer_list *list)
{
	shtimer_list *tmp_list = NULL;
	struct list_head *item;

	list->is_wake_lock = false;

	list_for_each(item, &shtimer_head){
		tmp_list = list_entry(item, shtimer_list, link);

		if(tmp_list->is_wake_lock) return;
	}

	__pm_relax(&shtimer_wake_lock_id);
}

static long shtimer_do_ioctl(struct file *file, unsigned int cmd, struct timespec *tv)
{
	int ret = 0;
	unsigned long flags, descriptor;
	shtimer_list *list = NULL;
	struct list_head *item;
	ktime_t ts;
	ktime_t te;

	if(file->private_data == NULL) {
		printk("%s file->private_data is null\n",__func__);
		return 0;
	}

	descriptor = *(unsigned long*)file->private_data;

	spin_lock_irqsave(&shtimer_slock, flags);
	list_for_each(item, &shtimer_head){
		list = list_entry(item, shtimer_list, link);

		if(list->descriptor == descriptor) break;
	}
	spin_unlock_irqrestore(&shtimer_slock, flags);

	if(item == &shtimer_head) return 0;

	switch(cmd){
	case SHTIMER_CMD_CANCEL:
		alarm_try_to_cancel(&list->alarm);

		spin_lock_irqsave(&shtimer_slock, flags);
		if(!list->waiting){
			spin_unlock_irqrestore(&shtimer_slock, flags);
			break;
		}
		shtimer_wake_lock(list);
		list->pending |= SHTIMER_CANCEL_SIG;
		list->waiting = false;
		wake_up(&list->wait_queue);
		spin_unlock_irqrestore(&shtimer_slock, flags);
		break;

#ifdef CONFIG_COMPAT
	case SHTIMER_CMD_SET_COMPAT:
#endif
	case SHTIMER_CMD_SET:
		alarm_try_to_cancel(&list->alarm);

		spin_lock_irqsave(&shtimer_slock, flags);
		ts = ktime_get_boottime();
		wake_up(&list->wait_queue);
		list->pending = SHTIMER_NONE_SIG;
		list->waiting = true;
		list->tv = timespec_to_ktime(*tv);
		list->base_tv = ktime_get();
		te = ktime_add(ts, list->tv);
		spin_unlock_irqrestore(&shtimer_slock, flags);

		alarm_start(&list->alarm, te);

	case SHTIMER_CMD_WAIT:
		spin_lock_irqsave(&shtimer_slock, flags);
		if(cmd == SHTIMER_CMD_SET && SLEEP_NEGATE_TIME < list->tv.tv64){
			/* aARM wake up cost is over 50ms */
			shtimer_wake_unlock(list);
		}
		spin_unlock_irqrestore(&shtimer_slock, flags);
		ret = wait_event_interruptible(list->wait_queue, list->pending);
		if(ret){
			printk("%s: EINTR occurred\n", __func__);
			ret = EINTR;
			goto freeze_request;
		}

		spin_lock_irqsave(&shtimer_slock, flags);
		shtimer_wake_lock(list);
		if((list->pending & SHTIMER_EXPIRE_SIG) == 0 ||
		   (list->pending & SHTIMER_CANCEL_SIG) != 0 ){
			ret = -EINVAL;
		}
		spin_unlock_irqrestore(&shtimer_slock, flags);
		break;

	case SHTIMER_CMD_WAKELOCK:
		spin_lock_irqsave(&shtimer_slock, flags);
		shtimer_wake_lock(list);
		spin_unlock_irqrestore(&shtimer_slock, flags);
		break;

	case SHTIMER_CMD_WAKEUNLOCK:
		spin_lock_irqsave(&shtimer_slock, flags);
		shtimer_wake_unlock(list);
		spin_unlock_irqrestore(&shtimer_slock, flags);
		break;

	default:
		ret = 0;
		break;
	}

freeze_request:

	return ret;
}

static long shtimer_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct timespec ts = {0,0};

	if (SHTIMER_CMD_SET == cmd) {
		if (copy_from_user(&ts, (void __user *)arg, sizeof(struct timespec)))
			return 0;
	}
	return shtimer_do_ioctl(file, cmd, &ts);
}

#ifdef CONFIG_COMPAT
static long shtimer_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct timespec ts = {0,0};

	if (SHTIMER_CMD_SET_COMPAT == cmd) {
		if (compat_get_timespec(&ts, (void __user *)arg))
			return 0;
	}
	return shtimer_do_ioctl(file, cmd, &ts);
}
#endif

static enum alarmtimer_restart shtimer_timer_expired(struct alarm *alarm, ktime_t now)
{
	unsigned long flags;
	shtimer_list *list;
	struct list_head *item;

	spin_lock_irqsave(&shtimer_slock, flags);
	list_for_each(item, &shtimer_head){
		list = list_entry(item, shtimer_list, link);
		if(&list->alarm == alarm) break;
	}

	if(item == &shtimer_head){
		printk("%s: list is null",__func__);
		spin_unlock_irqrestore(&shtimer_slock, flags);
		return ALARMTIMER_NORESTART;
	}

	shtimer_wake_lock(list);
	list->pending |= SHTIMER_EXPIRE_SIG;
	list->waiting = false;
	wake_up(&list->wait_queue);
	spin_unlock_irqrestore(&shtimer_slock, flags);
	return ALARMTIMER_NORESTART;
}

static int shtimer_open(struct inode *inode, struct file *file)
{
	unsigned long *descriptor;
	shtimer_list *list;
	unsigned long flags;

	descriptor = (unsigned long*)kmalloc(sizeof(long), GFP_KERNEL);
	if(descriptor == NULL) {
		printk("%s: descriptor is null",__func__);
		return -ENOMEM;
	}

	list = (shtimer_list*)kmalloc(sizeof(shtimer_list), GFP_KERNEL);
	if(list == NULL){
		printk("%s: list is null",__func__);
		kfree(descriptor);
		return -ENOMEM;
	}

	memset(list, 0x00, sizeof(shtimer_list));
	alarm_init(&list->alarm, ALARM_BOOTTIME, shtimer_timer_expired);
	list->alarm.function = shtimer_timer_expired;
	list->descriptor = shtimer_descriptor;
	list->waiting = false;
	list->is_wake_lock = false;
	init_waitqueue_head(&list->wait_queue);

	*descriptor = shtimer_descriptor;
	shtimer_descriptor++;
	file->private_data = (void*)descriptor;

	spin_lock_irqsave(&shtimer_slock, flags);
	list_add(&list->link, &shtimer_head);
	spin_unlock_irqrestore(&shtimer_slock, flags);

	return 0;
}

static int shtimer_release(struct inode *inode, struct file *file)
{
	unsigned long descriptor;
	shtimer_list *list;
	struct list_head *item;
	unsigned long flags;

	if(file->private_data == NULL) return 0;
	descriptor = *(unsigned long*)file->private_data;

	spin_lock_irqsave(&shtimer_slock, flags);
	list_for_each(item, &shtimer_head){
		list = list_entry(item, shtimer_list, link);
		if(!list) {
			spin_unlock_irqrestore(&shtimer_slock, flags);
			return -1;
		}

		if(list->descriptor == descriptor) break;
	}

	if(item == &shtimer_head) {
		spin_unlock_irqrestore(&shtimer_slock, flags);
		return 0;
	}

	alarm_try_to_cancel(&list->alarm);
	shtimer_wake_unlock(list);
	list_del(&list->link);
	spin_unlock_irqrestore(&shtimer_slock, flags);
	if(list->descriptor) kfree(file->private_data);
	if(list) kfree(list);
	file->private_data = NULL;

	return 0;
}

static struct file_operations shtimer_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = shtimer_ioctl,
	.open = shtimer_open,
	.release = shtimer_release,
#ifdef CONFIG_COMPAT
	.compat_ioctl = shtimer_compat_ioctl,
#endif
};

static struct miscdevice shtimer_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "shtimer",
	.fops = &shtimer_fops,
};

static int __init shtimer_init(void)
{
	int ret;

	ret = misc_register(&shtimer_misc);
	if(ret)
		return ret;

	wakeup_source_init(&shtimer_wake_lock_id, shtimer_wake_lock_name);

	return 0;
}

static void  __exit shtimer_exit(void)
{
	wakeup_source_trash(&shtimer_wake_lock_id);

	misc_deregister(&shtimer_misc);
}

module_init(shtimer_init);
module_exit(shtimer_exit);
MODULE_LICENSE("GPL");
