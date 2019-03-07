/* drivers/sharp/shterm/shterm_dev.c
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

#include <linux/string.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>
#include <linux/alarmtimer.h>
#include <linux/ioctl.h>
#include <linux/power_supply.h>
#ifdef SHTERM_BS_TIMER
#include "sharp/shdisp_kerl.h"
#endif
#include "misc/shterm_k.h"

#define SHTERM_NOR_TIMER_SEC (60*60)
#define SHTERM_CHG_TIMER_SEC (10*60)
#define SHTERM_TMP_TIMER_SEC (30*60)

#define SHTERM_CPU_TMP_THRESHOLD 50

#define SHTERM_MAGIC 't'
#define SHTERM_CMD_START_TIMER _IO(SHTERM_MAGIC, 256)
#define SHTERM_CMD_GET_TIMER _IO(SHTERM_MAGIC, 255)
#define SHTERM_CMD_GET_EVENT _IOR(SHTERM_MAGIC, 254, shbattlog_disp_type)

static struct alarm shterm_dev_alarms;
static struct work_struct shterm_dev_work;
#ifdef SHTERM_BS_TIMER
static int timer_type = -1;
#endif
static struct wakeup_source shterm_wake_lock;

extern const shbattlog_disp_type event_str[];

static void shterm_work_periodic_event( struct work_struct *work_p )
{
    shbattlog_info_t info = {0};
    struct timespec ts;
    static struct power_supply *psy;
    union power_supply_propval ps_ret = {0,};

#ifdef SHTERM_BS_TIMER
    if( timer_type != -1 ){
        shdisp_api_check_blackscreen_timeout( timer_type );
    }
#endif
    getnstimeofday(&ts);

    /* chg? */
    if (psy == NULL) {
        psy = power_supply_get_by_name("battery");
    }
    if (psy != NULL) {
        if (psy->desc->get_property(psy, POWER_SUPPLY_PROP_STATUS, &ps_ret)) {
            pr_err("failed to get prop battery\n");
            ps_ret.intval = POWER_SUPPLY_STATUS_DISCHARGING;
        }
    }
    else {
         pr_err("failed to get by name battery\n");
         ps_ret.intval = POWER_SUPPLY_STATUS_DISCHARGING;
    }

    if( POWER_SUPPLY_STATUS_CHARGING == ps_ret.intval ){
        info.event_num = SHBATTLOG_EVENT_BATT_REPORT_CHG;
        ts.tv_sec += SHTERM_CHG_TIMER_SEC;
#ifdef SHTERM_BS_TIMER
        timer_type = SHDISP_BLACKSCREEN_TYPE_T10;
#endif
    }
    else if( info.cpu_temp >= SHTERM_CPU_TMP_THRESHOLD ){
        info.event_num = SHBATTLOG_EVENT_BATT_REPORT_NORM;
        ts.tv_sec += SHTERM_TMP_TIMER_SEC;
#ifdef SHTERM_BS_TIMER
        timer_type = SHDISP_BLACKSCREEN_TYPE_T30;
#endif
    }
    else {
        info.event_num = SHBATTLOG_EVENT_BATT_REPORT_NORM;
        ts.tv_sec += SHTERM_NOR_TIMER_SEC;
#ifdef SHTERM_BS_TIMER
        timer_type = SHDISP_BLACKSCREEN_TYPE_T30;
#endif
    }

    /* send event */
    shterm_k_set_event( &info );
    /* set alarm */
    alarm_start( &shterm_dev_alarms, timespec_to_ktime(ts) );
}

static enum alarmtimer_restart shterm_timer_func( struct alarm *alarm, ktime_t now )
{
    if( !(shterm_wake_lock.active) ){
        __pm_wakeup_event( &shterm_wake_lock, 5 * HZ );
    }
    schedule_work( &shterm_dev_work );

    return ALARMTIMER_NORESTART;
}

static int shterm_dev_open( struct inode *inode, struct file *filp )
{
    return 0;
}

static long shterm_dev_ioctl( struct file *file, unsigned int cmd, unsigned long arg )
{
    int ret = 0;
    struct timespec ts;

    // for SHTERM_CMD_GET_EVENT
    shbattlog_disp_type userparam;
    void __user *argp = (void __user *)arg;

    switch( cmd ){
    case SHTERM_CMD_START_TIMER:
        getnstimeofday(&ts);
        ts.tv_sec += 60;
        alarm_start( &shterm_dev_alarms, timespec_to_ktime(ts) );
        break;

    case SHTERM_CMD_GET_TIMER:
        if( shterm_wake_lock.active ){
            __pm_relax( &shterm_wake_lock );
        }
        break;

    case SHTERM_CMD_GET_EVENT:
        if (copy_from_user(&userparam, argp, sizeof(shbattlog_disp_type))) {
            printk("%s: copy_from_user failed\n", __FUNCTION__);
            return -EFAULT;
        }

        if (copy_to_user(argp, &event_str[userparam.id], sizeof(shbattlog_disp_type))) {
            printk("%s: copy_to_user failed\n", __FUNCTION__);
            return -EFAULT;
        }
        break;

    default:
        ret = -1;
        break;
    }

    return ret;
}

static int shterm_dev_close( struct inode *inode, struct file *filp )
{
    return 0;
}

static struct file_operations shterm_dev_fops = {
    .owner          = THIS_MODULE,
    .open           = shterm_dev_open,
    .unlocked_ioctl = shterm_dev_ioctl,
    .release        = shterm_dev_close,
};

static struct miscdevice shterm_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "shterm_dev",
    .fops = &shterm_dev_fops,
};

static int __init shterm_init( void )
{
    int ret;

    ret = misc_register( &shterm_dev );
    if( ret ){
        printk( "misc_register failure shterm_init\n" );
        return ret;
    }

	wakeup_source_init(&shterm_wake_lock, "shterm");

    INIT_WORK( &shterm_dev_work, shterm_work_periodic_event );

    /* init & set alarm */
    alarm_init( &shterm_dev_alarms, ALARM_REALTIME, shterm_timer_func );

    return ret;
}

module_init(shterm_init);
