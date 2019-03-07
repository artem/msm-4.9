/* drivers/sharp/shsys/sh_systime.c
 *
 * Copyright (C) 2012 Sharp Corporation
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

#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <asm/uaccess.h>

#include <linux/io.h>
#include <asm/io.h>

#include <misc/sh_systime.h>

#ifdef CONFIG_SHSYS_SHUTDOWN_TIME_CUST
#define SHUTDOWN_COMPLETE_TIME_MAGIC 0x1f2e3d4c
#define MSM_SHARED_IMEM_BASE	0xfe80f000
#define SHUTDOWN_COMPLETE_TIME_ADDR (MSM_SHARED_IMEM_BASE + 0x800)
#endif /*  CONFIG_SHSYS_SHUTDOWN_TIME_CUST  */

static int timestamp_point = 0;
unsigned int TIMETICK_CLK_OFFSET = 0;
unsigned int TIMETICK_CLK_FREQ = 0;

static int sh_systime_set_timestamp(const char *val, struct kernel_param *kp)
{
    int ret;
    sharp_smem_common_type *p_sh_smem_common_type = NULL;
    unsigned long long systime = 0;
    int i;

    ret = param_set_int(val, kp);
    if (ret)
        return ret;

    ret = sh_systime_read_current(&systime);
    if (ret)
        return ret;

    p_sh_smem_common_type = sh_smem_get_common_address();
    if (p_sh_smem_common_type != NULL) {
        if ((timestamp_point == SHSYS_TIMEMSTAMP_SHUTDOWN_START) || (timestamp_point == SHSYS_TIMEMSTAMP_HOTBOOT_START)
            || ((SHSYS_TIMEMSTAMP_FREE <= timestamp_point) && (timestamp_point < SHSYS_TIMEMSTAMP_MAX_NUM))) {
            p_sh_smem_common_type->shsys_timestamp[timestamp_point] = systime;
        }
        else if (timestamp_point == SHSYS_TIMEMSTAMP_KEYGUARD_START) {
            if (p_sh_smem_common_type->shsys_timestamp[timestamp_point] == 0) {
                p_sh_smem_common_type->shsys_timestamp[timestamp_point] = systime;
            }
        }
        else {
            printk("=== Current shsys_timestamp list (MAX_NUM %d) ===\n", SHSYS_TIMEMSTAMP_MAX_NUM);
            for (i = 0; i < SHSYS_TIMEMSTAMP_MAX_NUM; i++) {
                printk("shsys_timestamp[%d] = %llu[us]\n", i, p_sh_smem_common_type->shsys_timestamp[i]);
            }
        }
    }

    return 0;
}

module_param_call(timestamp_point, sh_systime_set_timestamp, param_get_int, &timestamp_point, 0664);

void sh_systime_log_shutdown_complete_time(void)
{
    int ret;
    sharp_smem_common_type *p_sh_smem_common_type = NULL;
    unsigned long long systime = 0;

#ifdef CONFIG_SHSYS_SHUTDOWN_TIME_CUST
    void __iomem *shutdown_complete_time_addr = NULL;
#endif /*  CONFIG_SHSYS_SHUTDOWN_TIME_CUST  */

    p_sh_smem_common_type = sh_smem_get_common_address();
    if (p_sh_smem_common_type != NULL) {
        if (p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_SHUTDOWN_START] == 0) {
            return;
        }

        ret = sh_systime_read_current(&systime);
        if (ret != 0) {
            return;
        }

        if (systime >= p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_SHUTDOWN_START]) {
            systime = systime - p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_SHUTDOWN_START];
        }
        else {
            systime = systime + (0xFFFFFFFFFFFFFFFF - p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_SHUTDOWN_START]);
        }

        p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_SHUTDOWN_START] = 0;

        printk("shutdown complete time %llu[us]\n", systime);

#ifdef CONFIG_SHSYS_SHUTDOWN_TIME_CUST
        shutdown_complete_time_addr = ioremap_nocache(SHUTDOWN_COMPLETE_TIME_ADDR, 8);
        if (shutdown_complete_time_addr != NULL) {
            __raw_writel(SHUTDOWN_COMPLETE_TIME_MAGIC, shutdown_complete_time_addr);
            __raw_writel(systime, shutdown_complete_time_addr + sizeof(unsigned int));
            iounmap(shutdown_complete_time_addr);
        }
#endif /*  CONFIG_SHSYS_SHUTDOWN_TIME_CUST  */
    }
}
EXPORT_SYMBOL(sh_systime_log_shutdown_complete_time);

static int sh_systime_open(struct inode *inode, struct file *filp)
{
    return 0;
}

int sh_systime_read_current(unsigned long long *systime)
{
    void __iomem *regadr = NULL;
    unsigned int curr_timetick, last_timetick;

    regadr = ioremap_nocache(TIMETICK_CLK_OFFSET, 4);
    if (regadr != NULL) {
        curr_timetick = ioread32(regadr);
    }
    else {
        return -EFAULT;
    }

    /* Keep grabbing the time until a stable count is given */
    do {
        last_timetick = curr_timetick;
        curr_timetick = ioread32(regadr);
    } while (curr_timetick != last_timetick);

    iounmap(regadr);

    /* The following calculation is more exact than  "CALCULATE_TIMESTAMP"
     *   systime[ns] = curr_timetick[clk] * (1[ns] / TIMETICK_CLK_FREQ[Hz])
     */
    *systime = 1 * 1000 * 1000 * 1000 / TIMETICK_CLK_FREQ;
    *systime *= curr_timetick; /* Calculate separately(afraid of overflow) */
    do_div(*systime, 1000);

    return 0;
}
EXPORT_SYMBOL(sh_systime_read_current);

static int sh_systime_read_timestamp(unsigned long long *timestamp)
{
    sharp_smem_common_type *p_sh_smem_common_type = NULL;
    unsigned long long systime = 0;
    int ret;

    p_sh_smem_common_type = sh_smem_get_common_address();
    if (p_sh_smem_common_type != NULL) {
        if (p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_BOOT_END] == 0) {
            ret = sh_systime_read_current(&systime);
            if (ret == 0) {
                p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_BOOT_END] = systime;
            }
        }
        if (p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_HOTBOOT_START] != 0) {
            ret = sh_systime_read_current(&systime);
            if (ret == 0) {
                if (systime >= p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_HOTBOOT_START]) {
                    systime = systime - p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_HOTBOOT_START];
                }
                else {
                    systime = systime + (0xFFFFFFFFFFFFFFFF - p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_HOTBOOT_START]);
                }
                p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_HOTBOOT_COMP] = systime;
                p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_HOTBOOT_START] = 0;
            }
        }
        memcpy(timestamp, p_sh_smem_common_type->shsys_timestamp, sizeof(p_sh_smem_common_type->shsys_timestamp));
    }
    else {
        return -EFAULT;
    }

    return 0;
}

static ssize_t sh_systime_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
    unsigned long long systime;
    unsigned long long timestamp[32];
    int ret;

    if (count == sizeof(systime)) {
        ret = sh_systime_read_current(&systime);
        if (ret != 0) {
            return ret;
        }
        if (copy_to_user(buf, (void *)&systime, count)) {
            return -EFAULT;
        }
    }
    else if (count == sizeof(timestamp)) {
        ret = sh_systime_read_timestamp(timestamp);
        if (ret != 0) {
            return ret;
        }
        if (copy_to_user(buf, (void *)timestamp, count)) {
            return -EFAULT;
        }
    }
    else {
        return -EFAULT;
    }

    return count;
}

static int sh_systime_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static void sh_systime_get_platform_info( void )
{
    struct device_node *np;
    u32 freq = 0;
    const u32 *addr = NULL;
    int ret = 0;

    np = of_find_compatible_node(NULL, NULL, "qcom,mpm2-sleep-counter");
    if (!np) {
        printk("shsystimed: can't find mpm2-sleep-counter\n");
        return;
    }

    ret = of_property_read_u32(np, "clock-frequency", &freq);
    if (!ret) {
        TIMETICK_CLK_FREQ = freq;
        if (TIMETICK_CLK_FREQ != TIMETICK_CLK_FREQ_ASSUMED) {
            printk("shsystimed: clock-frequency does not match. (actual value is %d) Please modify sh_systime.h\n", 
              TIMETICK_CLK_FREQ);
        }
    }
    else {
        printk("shsystimed: can't find clock-frequency, set %d as default (ret=%d)\n", 
          TIMETICK_CLK_FREQ_ASSUMED, ret);
        TIMETICK_CLK_FREQ = TIMETICK_CLK_FREQ_ASSUMED;
    }

    addr = of_get_address(np, 0, NULL, NULL);
    if (addr) {
        TIMETICK_CLK_OFFSET = be32_to_cpu(*addr);
    }
    else {
        printk("shsystimed: failed to get TIMETICK_CLK_OFFSET\n");
    }
}

static struct file_operations sh_systime_fops = {
    .owner          = THIS_MODULE,
    .open           = sh_systime_open,
    .read           = sh_systime_read,
    .release        = sh_systime_release,
};

static struct miscdevice sh_systime_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "sh_systime",
    .fops = &sh_systime_fops,
};

static int __init sh_systime_init( void )
{
    sharp_smem_common_type *p_sh_smem_common_type = NULL;
    unsigned long long systime = 0;
    unsigned long long t = 0;
    int ret;

    sh_systime_get_platform_info();
    if (!TIMETICK_CLK_OFFSET) {
        printk("sh_systime_init: TIMETICK_CLK_OFFSET is invalid. aborting...\n");
        return 0;
    }

    p_sh_smem_common_type = sh_smem_get_common_address();
    if (p_sh_smem_common_type != NULL) {
        ret = sh_systime_read_current(&systime);
        t = cpu_clock(smp_processor_id());
        if ((ret == 0) && (t != 0)) {
            do_div(t, 1000);
            p_sh_smem_common_type->shsys_timestamp[SHSYS_TIMEMSTAMP_KERNEL_START] = systime - t;
        }
        else {
            printk("sh_systime_init: sh_systime_read_current() or cpu_clock() failed\n");
        }
    }
    else {
        printk("sh_systime_init: sh_smem_get_common_address() failed\n");
    }

    ret = misc_register(&sh_systime_dev);
    if (ret != 0) {
        printk("sh_systime_init: fail to misc_register ret %d\n", ret);
    }

    return ret;
}

module_init(sh_systime_init);

MODULE_DESCRIPTION("sh_systime");
MODULE_LICENSE("GPL v2");

