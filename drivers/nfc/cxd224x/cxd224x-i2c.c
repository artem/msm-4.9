/*
 *  cxd224x-i2c.c - cxd224x NFC i2c driver
 *
 * Copyright (C) 2013-2016 Sony Corporation.
 * Copyright (C) 2012 Broadcom Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/version.h>
#include "nfc_wakelock.h"
#include <linux/compat.h>
#include <linux/of_gpio.h>
#include <linux/sched.h>

#include "nfc.h"
#include "cxd224x.h"

#define CXD224X_WAKE_LOCK_TIMEOUT	10		/* wake lock timeout for HOSTINT (sec) */
#define CXD224X_WAKE_LOCK_NAME	"cxd224x-i2c"		/* wake lock for HOSTINT */
#define CXD224X_WAKE_LOCK_TIMEOUT_LP	3		/* wake lock timeout for low-power-mode (sec) */
#define CXD224X_WAKE_LOCK_NAME_LP "cxd224x-i2c-lp"	/* wake lock for low-power-mode */

/* do not change below */
#define MAX_BUFFER_SIZE		780

/* Read data */
#define PACKET_HEADER_SIZE_NCI	(3)
#define PACKET_HEADER_SIZE_HCI	(3)
#define PACKET_TYPE_NCI		(16)
#define PACKET_TYPE_HCIEV	(4)
#define MAX_PACKET_SIZE		(PACKET_HEADER_SIZE_NCI + 255)

/* RESET */
#define RESET_ASSERT_MS         (1)

#ifdef CONFIG_NFC_CXD224X_TST1	// for TIMPANI
/* SERVICE MODE */
#define SVMODE_TST1_TIME        (100) // 100ms
#endif	// CONFIG_NFC_CXD224X_TST1

struct cxd224x_dev {
	wait_queue_head_t read_wq;
	struct mutex read_mutex;
	struct i2c_client *client;
	struct miscdevice cxd224x_device;
	unsigned int en_gpio;
	unsigned int irq_gpio;
	unsigned int wake_gpio;
	unsigned int rst_gpio;
#ifdef CONFIG_NFC_CXD224X_TST1	// for TIMPANI
	unsigned int tst1_gpio;
#endif	// CONFIG_NFC_CXD224X_TST1
	bool irq_enabled;
	struct mutex lock;
	spinlock_t irq_enabled_lock;
	unsigned int users;
	unsigned int count_irq;
	struct wake_lock wakelock;	/* wake lock for HOSTINT */
	struct wake_lock wakelock_lp;	/* wake lock for low-power-mode */
	/* Driver message queue */
	struct workqueue_struct	*wqueue;
	struct work_struct qmsg;
	struct pinctrl		*pinctrl;
	struct pinctrl_state	*pin_default_int;
	struct pinctrl_state	*pin_default_wake;
#ifndef CONFIG_NFC_CXD224X_RST_USE_PMIC
	struct pinctrl_state	*pin_default_rst;
#endif
};

#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
static struct cxd224x_dev *cxd224x_dev_data = NULL;
void cxd224x_dev_reset(void)
{
	NFC_DRV_DBG_LOG("START");
	if(cxd224x_dev_data == NULL){
		NFC_DRV_DBG_LOG("RET cxd224x_dev_data = null");
		return;
	}
	if(!gpio_is_valid(cxd224x_dev_data->rst_gpio)){
		NFC_DRV_DBG_LOG("RET rst_gpio = %d",cxd224x_dev_data->rst_gpio);
		return;
	}
	gpio_set_value(cxd224x_dev_data->rst_gpio, CXDNFC_RST_ACTIVE);
	cxd224x_dev_data->count_irq=0;
	msleep(RESET_ASSERT_MS);
	gpio_set_value(cxd224x_dev_data->rst_gpio, ~CXDNFC_RST_ACTIVE & 0x1);

	NFC_DRV_DBG_LOG("END");
}
EXPORT_SYMBOL(cxd224x_dev_reset);
#endif

#ifdef DEBUG_NFC_DRV
void dumpData(const char *data, int len, const char *log)
{
	int i;
	const char *p = data;
	char wk_buf[5];
	char out_buf[80];

	if(debug_flg){
		NFC_DRV_DBG_LOG("dump start(len:%d)",len);
		memset(out_buf, 0x00, sizeof(out_buf));
		strcat(out_buf,log);
		for (i = 1; i <= len; i++) {
			memset(wk_buf, 0x00, sizeof(wk_buf));
			sprintf(wk_buf,"%02X ",*p++);
			strcat(out_buf,wk_buf);
			if ((i % 16 == 0) && (i != len)) {
				printk(KERN_INFO "%s\n", out_buf);
				memset(out_buf, 0x00, sizeof(out_buf));
				strcat(out_buf,log);
			}
		}
		printk(KERN_INFO "%s\n", out_buf);
		NFC_DRV_DBG_LOG( "dump end");
	}

	return;
}
#endif

#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
static void cxd224x_workqueue(struct work_struct *work)
{
	struct cxd224x_dev *cxd224x_dev = container_of(work, struct cxd224x_dev, qmsg);
	unsigned long flags;

	if(!gpio_is_valid(cxd224x_dev->rst_gpio)){
		NFC_DRV_DBG_LOG("RET rst_gpio = %d",cxd224x_dev->rst_gpio);
		return;
	}

	DBGLOG_DETAIL("%s, xrst assert\n", __func__);
	spin_lock_irqsave(&cxd224x_dev->irq_enabled_lock, flags);
	gpio_set_value(cxd224x_dev->rst_gpio, CXDNFC_RST_ACTIVE);
	cxd224x_dev->count_irq=0; /* clear irq */
	spin_unlock_irqrestore(&cxd224x_dev->irq_enabled_lock, flags);
	msleep(RESET_ASSERT_MS);
	DBGLOG_DETAIL("%s, xrst deassert\n", __func__);
	gpio_set_value(cxd224x_dev->rst_gpio, ~CXDNFC_RST_ACTIVE & 0x1);
}

static int __init init_wqueue(struct cxd224x_dev *cxd224x_dev)
{
	INIT_WORK(&cxd224x_dev->qmsg, cxd224x_workqueue);
	cxd224x_dev->wqueue = create_workqueue("cxd224x-i2c_wrokq");
	if (cxd224x_dev->wqueue == NULL)
		return -EBUSY;
	return 0;
}
#endif /* CONFIG_NFC_CXD224X_RST */

static void cxd224x_init_stat(struct cxd224x_dev *cxd224x_dev)
{
	cxd224x_dev->count_irq = 0;
}

static void cxd224x_disable_irq(struct cxd224x_dev *cxd224x_dev)
{
	unsigned long flags;
	spin_lock_irqsave(&cxd224x_dev->irq_enabled_lock, flags);
	if (cxd224x_dev->irq_enabled) {
		disable_irq_nosync(cxd224x_dev->client->irq);
		cxd224x_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&cxd224x_dev->irq_enabled_lock, flags);
}

static void cxd224x_enable_irq(struct cxd224x_dev *cxd224x_dev)
{
	unsigned long flags;
	spin_lock_irqsave(&cxd224x_dev->irq_enabled_lock, flags);
	if (!cxd224x_dev->irq_enabled) {
		cxd224x_dev->irq_enabled = true;
		enable_irq(cxd224x_dev->client->irq);
	}
	spin_unlock_irqrestore(&cxd224x_dev->irq_enabled_lock, flags);
}

static irqreturn_t cxd224x_dev_irq_handler(int irq, void *dev_id)
{
	struct cxd224x_dev *cxd224x_dev = dev_id;
	unsigned long flags;
	spin_lock_irqsave(&cxd224x_dev->irq_enabled_lock, flags);
	cxd224x_dev->count_irq++;
	spin_unlock_irqrestore(&cxd224x_dev->irq_enabled_lock, flags);
	wake_up(&cxd224x_dev->read_wq);
	NFC_DRV_DBG_LOG("irq_handler() cnt_irq:%d",cxd224x_dev->count_irq);

	return IRQ_HANDLED;
}

static unsigned int cxd224x_dev_poll(struct file *filp, poll_table *wait)
{
	struct cxd224x_dev *cxd224x_dev = filp->private_data;
	unsigned int mask = 0;
	unsigned long flags;
	poll_wait(filp, &cxd224x_dev->read_wq, wait);

	spin_lock_irqsave(&cxd224x_dev->irq_enabled_lock, flags);
	if (cxd224x_dev->count_irq > 0)
	{
		cxd224x_dev->count_irq--;
		mask |= POLLIN | POLLRDNORM;
	}
	spin_unlock_irqrestore(&cxd224x_dev->irq_enabled_lock, flags);
	if(mask)
		wake_lock_timeout(&cxd224x_dev->wakelock, CXD224X_WAKE_LOCK_TIMEOUT*HZ);

	NFC_DRV_DBG_LOG("poll() cnt_irq:%d ret:((0x%x)",cxd224x_dev->count_irq, mask);
	return mask;
}

static ssize_t cxd224x_dev_read(struct file *filp, char __user *buf,
				  size_t count, loff_t *offset)
{
	struct cxd224x_dev *cxd224x_dev = filp->private_data;
	unsigned char tmp[MAX_BUFFER_SIZE];
	int total, len, ret;

	total = 0;
	len = 0;

	if (count > MAX_BUFFER_SIZE)
		count = MAX_BUFFER_SIZE;

	mutex_lock(&cxd224x_dev->read_mutex);

	ret = i2c_master_recv(cxd224x_dev->client, tmp, PACKET_HEADER_SIZE_NCI);
	if (ret != PACKET_HEADER_SIZE_NCI) {
		DBGLOG_ERR("failed to read header %d\n", ret);
		mutex_unlock(&cxd224x_dev->read_mutex);
		return -EIO;
	}
	if (ret == PACKET_HEADER_SIZE_NCI && (tmp[0] != 0xff)) {
		total = ret;

		len = tmp[PACKET_HEADER_SIZE_NCI-1];

		/** make sure full packet fits in the buffer
		**/
		if (len > 0 && (len + total) <= count) {
			/** read the remainder of the packet.
			**/
			ret = i2c_master_recv(cxd224x_dev->client, tmp+total, len);
			if (ret != len) {
				DBGLOG_ERR("failed to read payload %d\n", ret);
				mutex_unlock(&cxd224x_dev->read_mutex);
				return -EIO;
			}
			if (ret == len)
				total += len;
		}
	}

	mutex_unlock(&cxd224x_dev->read_mutex);

	if (total > count || copy_to_user(buf, tmp, total)) {
		DBGLOG_ERR("failed to copy to user space, total = %d\n", total);
		total = -EFAULT;
	}

#ifdef DEBUG_NFC_DRV
	dumpData(tmp, (int)total, "[NFC][i2c_read  ] ");
#endif
	return total;
}

static ssize_t cxd224x_dev_write(struct file *filp, const char __user *buf,
				   size_t count, loff_t *offset)
{
	struct cxd224x_dev *cxd224x_dev = filp->private_data;
	char tmp[MAX_BUFFER_SIZE];
	int ret;

	if (count > MAX_BUFFER_SIZE) {
		DBGLOG_ERR("out of memory\n");
		return -ENOMEM;
	}

	if (copy_from_user(tmp, buf, count)) {
		DBGLOG_ERR("failed to copy from user space\n");
		return -EFAULT;
	}

#ifdef DEBUG_NFC_DRV
	dumpData(tmp, (int)count, "[NFC][i2c_write ] ");
#endif
	mutex_lock(&cxd224x_dev->read_mutex);
	/* Write data */

	ret = i2c_master_send(cxd224x_dev->client, tmp, count);
	if (ret != count) {
		DBGLOG_ERR("failed to write %d\n", ret);
		ret = -EIO;
	}
	mutex_unlock(&cxd224x_dev->read_mutex);

	return ret;
}

static int cxd224x_dev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	int call_enable = 0;
	struct cxd224x_dev *cxd224x_dev = container_of(filp->private_data,
							   struct cxd224x_dev,
							   cxd224x_device);
	NFC_DRV_DBG_LOG("OPEN");

	filp->private_data = cxd224x_dev;
	mutex_lock(&cxd224x_dev->lock);
	if (!cxd224x_dev->users)
	{
		cxd224x_init_stat(cxd224x_dev);
		call_enable = 1;
	}
	cxd224x_dev->users++;
	mutex_unlock(&cxd224x_dev->lock);
	if (call_enable)
	{
		cxd224x_enable_irq(cxd224x_dev);
		enable_irq_wake(cxd224x_dev->client->irq);
	}

	DBGLOG_DETAIL("open %d,%d users=%d\n", imajor(inode), iminor(inode), cxd224x_dev->users);

	return ret;
}

static int cxd224x_dev_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	int call_disable = 0;
	struct cxd224x_dev *cxd224x_dev = filp->private_data;

	NFC_DRV_DBG_LOG("CLOSE");

	mutex_lock(&cxd224x_dev->lock);
	cxd224x_dev->users--;
	if (!cxd224x_dev->users)
	{
		call_disable = 1;
	}
	mutex_unlock(&cxd224x_dev->lock);
	if (call_disable)
	{
		disable_irq_wake(cxd224x_dev->client->irq);
		cxd224x_disable_irq(cxd224x_dev);
	}

	DBGLOG_DETAIL("release %d,%d users=%d\n", imajor(inode), iminor(inode), cxd224x_dev->users);

	return ret;
}

static long cxd224x_dev_unlocked_ioctl(struct file *filp,
					 unsigned int cmd, unsigned long arg)
{
	struct cxd224x_dev *cxd224x_dev = filp->private_data;
#ifdef DEBUG_NFC_DRV_PIN
	int ret = 0;
#endif	/* DEBUG_NFC_DRV_PIN */
#ifdef CONFIG_NFC_CXD224X_TST1	// for TIMPANI
	unsigned long flags;
#endif	// CONFIG_NFC_CXD224X_TST1
	switch (cmd) {
	case CXDNFC_RST_CTL:
#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
		NFC_DRV_DBG_LOG("IOCTL(CXDNFC_RST_CTL)");
		return (queue_work(cxd224x_dev->wqueue, &cxd224x_dev->qmsg) ? 0 : 1);
#endif
		break;
	case CXDNFC_POWER_CTL:
#if defined(CONFIG_NFC_CXD224X_VEN) || defined(CONFIG_NFC_CXD224X_VEN_MODULE)
		if (arg == 0) {
			gpio_set_value(cxd224x_dev->en_gpio, 1);
		} else if (arg == 1) {
			gpio_set_value(cxd224x_dev->en_gpio, 0);
		} else {
			/* do nothing */
		}
#else
                return 1; /* not support */
#endif
		break;
	case CXDNFC_WAKE_CTL:
		if (arg == 0) {
			NFC_DRV_DBG_LOG("IOCTL(CXDNFC_WAKE_CTL PON H)");
			wake_lock_timeout(&cxd224x_dev->wakelock_lp, CXD224X_WAKE_LOCK_TIMEOUT_LP*HZ);
			/* PON HIGH (normal power mode)*/
			gpio_set_value(cxd224x_dev->wake_gpio, 1);
			DBGLOG_DETAIL("[%s(),line(%d)] PON HIGH wake value(%d)\n",__func__, __LINE__, gpio_get_value(cxd224x_dev->wake_gpio) );
		} else if (arg == 1) {
			NFC_DRV_DBG_LOG("IOCTL(CXDNFC_WAKE_CTL PON L)");
			/* PON LOW (low power mode) */
			gpio_set_value(cxd224x_dev->wake_gpio, 0);
			DBGLOG_DETAIL("[%s(),line(%d)] PON LOW wake value(%d)\n",__func__, __LINE__, gpio_get_value(cxd224x_dev->wake_gpio) );
			wake_unlock(&cxd224x_dev->wakelock);
			wake_unlock(&cxd224x_dev->wakelock_lp);
		} else {
			/* do nothing */
		}
		break;
#ifdef DEBUG_NFC_DRV_PIN
	case CXDNFC_GET_PON_CTL:
		NFC_DRV_DBG_LOG("get pon");
		ret = gpio_get_value(cxd224x_dev->wake_gpio);
		if(copy_to_user((unsigned int *)arg, &ret, sizeof(unsigned int)) != 0) {
			NFC_DRV_ERR_LOG("copy failed to user");
		}
		break;
#endif	/* DEBUG_NFC_DRV_PIN */
#ifdef CONFIG_NFC_CXD224X_TST1	// for TIMPANI
	case CXDNFC_TST1_CTL:
		NFC_DRV_DBG_LOG("service mode");
		if(!gpio_is_valid(cxd224x_dev->rst_gpio)){
			NFC_DRV_DBG_LOG("RET rst_gpio = %d",cxd224x_dev->rst_gpio);
			return -EPERM;
		}
		if(!gpio_is_valid(cxd224x_dev->tst1_gpio)){
			NFC_DRV_DBG_LOG("RET tst1_gpio = %d",cxd224x_dev->tst1_gpio);
			return -EPERM;
		}
		// TST1 - H
		gpio_set_value(cxd224x_dev->tst1_gpio, 1);
		msleep(SVMODE_TST1_TIME);

		// XRST - H -> L
		spin_lock_irqsave(&cxd224x_dev->irq_enabled_lock, flags);
		gpio_set_value(cxd224x_dev->rst_gpio, CXDNFC_RST_ACTIVE);
		cxd224x_dev->count_irq=0;
		spin_unlock_irqrestore(&cxd224x_dev->irq_enabled_lock, flags);
		msleep(RESET_ASSERT_MS);
		gpio_set_value(cxd224x_dev->rst_gpio, ~CXDNFC_RST_ACTIVE & 0x1);

		// TST1 - L
		msleep(SVMODE_TST1_TIME);
		gpio_set_value(cxd224x_dev->tst1_gpio, 0);
		break;
#endif	// CONFIG_NFC_CXD224X_TST1
	default:
		DBGLOG_ERR("%s, unknown cmd (%x, %lx)\n", __func__, cmd, arg);
		return 0;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long cxd224x_dev_compat_unlocked_ioctl(struct file *filep,
					 unsigned int cmd, unsigned long arg)
{
	long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = cxd224x_dev_unlocked_ioctl(filep, cmd, arg);

	return ret;
}
#endif	/* CONFIG_COMPAT */

static const struct file_operations cxd224x_dev_fops = {
	.owner = THIS_MODULE,
	.llseek = no_llseek,
	.poll = cxd224x_dev_poll,
	.read = cxd224x_dev_read,
	.write = cxd224x_dev_write,
	.open = cxd224x_dev_open,
	.release = cxd224x_dev_release,
	.unlocked_ioctl = cxd224x_dev_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cxd224x_dev_compat_unlocked_ioctl
#endif  /* CONFIG_COMPAT */
};

static struct cxd224x_platform_data *cxd224x_get_dts_config(struct device *dev)
{
	int ret;
	struct device_node *node = dev->of_node;
	struct cxd224x_platform_data *pdata;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		DBGLOG_ERR("unable to allocate memory for platform data\n");
		ret = -ENOMEM;
		goto err;
	}
	pdata->irq_gpio = of_get_named_gpio(node, "cxd224x,irq_gpio", 0);
	if (!gpio_is_valid(pdata->irq_gpio)) {
		ret = -EINVAL;
		DBGLOG_ERR("failed to of_get_named_gpio(cxd224x,irq_gpio(%d))\n", pdata->irq_gpio);
		goto err;
	}
	pdata->wake_gpio = of_get_named_gpio(node, "cxd224x,wake_gpio", 0);
	if (!gpio_is_valid(pdata->wake_gpio)) {
		ret = -EINVAL;
		DBGLOG_ERR("failed to of_get_named_gpio(cxd224x,wake_gpio(%d))\n",pdata->wake_gpio);
		goto err;
	}
#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
	pdata->rst_gpio = of_get_named_gpio(node, "cxd224x,rst_gpio", 0);
	if (!gpio_is_valid(pdata->rst_gpio)) {
		DBGLOG_ERR("failed to of_get_named_gpio(cxd224x,rst_gpio(%d))\n",pdata->rst_gpio);
	}
#endif
#ifdef CONFIG_NFC_CXD224X_TST1	// for TIMPANI
	pdata->tst1_gpio = of_get_named_gpio(node, "cxd224x,tst1_gpio", 0);
	if (!gpio_is_valid(pdata->tst1_gpio)) {
		DBGLOG_ERR("failed to of_get_named_gpio(cxd224x,tst1_gpio(%d))\n",pdata->tst1_gpio);
	}
#endif	// CONFIG_NFC_CXD224X_TST1
	pdata->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pdata->pinctrl)) {
		ret = -EINVAL;
		DBGLOG_ERR("Failed to get pinctrl\n");
		goto err;
	}

	pdata->pin_default_int = pinctrl_lookup_state(pdata->pinctrl, "cxd224x_default_int");
	if (IS_ERR_OR_NULL(pdata->pin_default_int)) {
		ret = -EINVAL;
		DBGLOG_ERR("Failed to look up default state\n");
		goto err;
	}

	pdata->pin_default_wake = pinctrl_lookup_state(pdata->pinctrl, "cxd224x_default_wake");
	if (IS_ERR_OR_NULL(pdata->pin_default_wake)) {
		ret = -EINVAL;
		DBGLOG_ERR("Failed to look up default state\n");
		goto err;
	}

#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
#ifndef CONFIG_NFC_CXD224X_RST_USE_PMIC
	pdata->pin_default_rst = pinctrl_lookup_state(pdata->pinctrl, "cxd224x_default_rst");
	if (IS_ERR_OR_NULL(pdata->pin_default_rst)) {
		ret = -EINVAL;
		DBGLOG_ERR("Failed to look up default state\n");
		goto err;
	}
#endif
#endif
	DBGLOG_DETAIL("[%s(),line(%d)] pdata->irq_gpio(%d)\n",__func__, __LINE__, pdata->irq_gpio);
	DBGLOG_DETAIL("[%s(),line(%d)] pdata->wake_gpio(%d)\n",__func__, __LINE__, pdata->wake_gpio);
	DBGLOG_DETAIL("[%s(),line(%d)] pdata->rst_gpio(%d)\n",__func__, __LINE__, pdata->rst_gpio);

	return pdata;
err:
	if (pdata)
		devm_kfree(dev, pdata);

	return ERR_PTR(ret);
}

static int cxd224x_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	int ret;
	struct cxd224x_platform_data *platform_data;
	struct cxd224x_dev *cxd224x_dev;
	int irq_gpio_ok  = 0;
#if defined(CONFIG_NFC_CXD224X_VEN) || defined(CONFIG_NFC_CXD224X_VEN_MODULE)
	int en_gpio_ok   = 0;
#endif
#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
#ifndef CONFIG_NFC_CXD224X_RST_USE_PMIC
	int rst_gpio_ok = 0;
#endif
#endif
	int wake_gpio_ok = 0;

	DBGLOG_DETAIL("[%s(),line(%d)] client->name(%s) client->addr(0x%x)\n",
		__func__, __LINE__, client->name, (int)client->addr);

	platform_data = client->dev.platform_data;

	if (client->dev.of_node) {
		platform_data = cxd224x_get_dts_config(&client->dev);
		if (IS_ERR(platform_data)) {
			ret =  PTR_ERR(platform_data);
			DBGLOG_ERR("failed to get_dts_config %d\n", ret);
			goto err_exit;
		}
		client->dev.platform_data = platform_data;
	}

	DBGLOG_DETAIL("%s, probing cxd224x driver flags = %x\n", __func__, client->flags);
	if (platform_data == NULL) {
		DBGLOG_ERR("nfc probe fail\n");
		return -ENODEV;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		DBGLOG_ERR("need I2C_FUNC_I2C\n");
		return -ENODEV;
	}

	ret = pinctrl_select_state(platform_data->pinctrl, platform_data->pin_default_int);
	if (ret) {
		DBGLOG_ERR("Can't select pinctrl state ret(%d)\n", ret);
		return -ENODEV;
	}

	ret = gpio_request_one(platform_data->irq_gpio, GPIOF_IN, "nfc_int");
	if (ret) {
		DBGLOG_ERR("failed gpio_reques(nfc_int) ret(%d)\n", ret);
		return -ENODEV;
	}
	irq_gpio_ok=1;

#if defined(CONFIG_NFC_CXD224X_VEN) || defined(CONFIG_NFC_CXD224X_VEN_MODULE)
	ret = gpio_request_one(platform_data->en_gpio, GPIOF_OUT_INIT_LOW, "nfc_cen");
	if (ret){
		DBGLOG_ERR("failed gpio_reques(nfc_cen) ret(%d)\n", ret);
		goto err_exit;
	}
	en_gpio_ok=1;
	gpio_set_value(platform_data->en_gpio, 0);
#endif

#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
	if(gpio_is_valid(platform_data->rst_gpio)){
#ifndef CONFIG_NFC_CXD224X_RST_USE_PMIC
		ret = pinctrl_select_state(platform_data->pinctrl, platform_data->pin_default_rst);
		if (ret) {
			DBGLOG_ERR("Can't select pinctrl state ret(%d)\n", ret);
			return -ENODEV;
		}
		ret = gpio_request_one(platform_data->rst_gpio, GPIOF_OUT_INIT_LOW, "nfc_rst");
		if (ret){
			DBGLOG_ERR("failed gpio_reques(nfc_rst) ret(%d)\n", ret);
			goto err_exit;
		}
		rst_gpio_ok=1;
#endif
#ifdef CONFIG_NFC_CXD224X_PROBE_RST
		DBGLOG_DETAIL("%s, xrst assert\n", __func__);
		gpio_set_value(platform_data->rst_gpio, CXDNFC_RST_ACTIVE);
		msleep(RESET_ASSERT_MS);
		DBGLOG_DETAIL("%s, xrst deassert\n", __func__);
		gpio_set_value(platform_data->rst_gpio, ~CXDNFC_RST_ACTIVE & 0x1);
#endif
	}
#endif

	ret = pinctrl_select_state(platform_data->pinctrl, platform_data->pin_default_wake);
	if (ret) {
		DBGLOG_ERR("probing cxd224x driver flagsCan't select pinctrl state(%d)\n", ret);
		return ret;
	}
	ret = gpio_request_one(platform_data->wake_gpio, GPIOF_OUT_INIT_LOW, "nfc_wake");
	if (ret){
		DBGLOG_ERR("failed gpio_reques(nfc_wake) ret(%d)\n", ret);
		goto err_exit;
	}
	wake_gpio_ok=1;
	gpio_set_value(platform_data->wake_gpio, 0);

	cxd224x_dev = kzalloc(sizeof(*cxd224x_dev), GFP_KERNEL);
	if (cxd224x_dev == NULL) {
		DBGLOG_ERR("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	cxd224x_dev->irq_gpio = platform_data->irq_gpio;
#if defined(CONFIG_NFC_CXD224X_VEN) || defined(CONFIG_NFC_CXD224X_VEN_MODULE)
	cxd224x_dev->en_gpio = platform_data->en_gpio;
#endif
	cxd224x_dev->wake_gpio = platform_data->wake_gpio;
	cxd224x_dev->rst_gpio = platform_data->rst_gpio;
#ifdef CONFIG_NFC_CXD224X_TST1	// for TIMPANI
	cxd224x_dev->tst1_gpio = platform_data->tst1_gpio;
#endif	// CONFIG_NFC_CXD224X_TST1
	cxd224x_dev->client = client;
	cxd224x_dev->pinctrl = platform_data->pinctrl;
	cxd224x_dev->pin_default_int = platform_data->pin_default_int;
	cxd224x_dev->pin_default_wake = platform_data->pin_default_wake;
#ifndef CONFIG_NFC_CXD224X_RST_USE_PMIC
	cxd224x_dev->pin_default_rst = platform_data->pin_default_rst;
#endif
	wake_lock_init(&cxd224x_dev->wakelock, WAKE_LOCK_SUSPEND, CXD224X_WAKE_LOCK_NAME);
	wake_lock_init(&cxd224x_dev->wakelock_lp, WAKE_LOCK_SUSPEND, CXD224X_WAKE_LOCK_NAME_LP);
	cxd224x_dev->users =0;

	/* init mutex and queues */
	init_waitqueue_head(&cxd224x_dev->read_wq);
	mutex_init(&cxd224x_dev->read_mutex);
	mutex_init(&cxd224x_dev->lock);
	spin_lock_init(&cxd224x_dev->irq_enabled_lock);

#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
	if (init_wqueue(cxd224x_dev) != 0) {
		DBGLOG_ERR("init workqueue failed\n");
		goto err_exit;
	}
#endif

	cxd224x_dev->cxd224x_device.minor = MISC_DYNAMIC_MINOR;
	cxd224x_dev->cxd224x_device.name = "cxd224x-i2c";
	cxd224x_dev->cxd224x_device.fops = &cxd224x_dev_fops;

	ret = misc_register(&cxd224x_dev->cxd224x_device);
	if (ret) {
		DBGLOG_ERR("misc_register failed\n");
		goto err_misc_register;
	}

	/* request irq.  the irq is set whenever the chip has data available
	 * for reading.  it is cleared when all data has been read.
	 */
	DBGLOG_DETAIL("requesting IRQ %d\n", client->irq);
	cxd224x_dev->irq_enabled = true;
	ret = request_irq(client->irq, cxd224x_dev_irq_handler,
			  IRQF_TRIGGER_FALLING, client->name, cxd224x_dev);
	if (ret) {
		DBGLOG_ERR("request_irq failed\n");
		goto err_request_irq_failed;
	}
	cxd224x_disable_irq(cxd224x_dev);
	DBGLOG_DETAIL("[%s(),line(%d)]  hostint value(%d)\n",__func__, __LINE__, gpio_get_value(platform_data->irq_gpio) );
	DBGLOG_DETAIL("[%s(),line(%d)]  wake value(%d)\n",__func__, __LINE__, gpio_get_value(platform_data->wake_gpio) );
	DBGLOG_DETAIL("[%s(),line(%d)]  rst value(%d)\n",__func__, __LINE__, gpio_get_value(platform_data->rst_gpio) );
	i2c_set_clientdata(client, cxd224x_dev);
	DBGLOG_DETAIL("%s, probing cxd224x driver exited successfully\n", __func__);
#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
	cxd224x_dev_data = cxd224x_dev;
#endif

	return 0;

err_request_irq_failed:
	misc_deregister(&cxd224x_dev->cxd224x_device);
err_misc_register:
	mutex_destroy(&cxd224x_dev->read_mutex);
	kfree(cxd224x_dev);
err_exit:
	if(irq_gpio_ok)
		gpio_free(platform_data->irq_gpio);
#if defined(CONFIG_NFC_CXD224X_VEN) || defined(CONFIG_NFC_CXD224X_VEN_MODULE)
	if(en_gpio_ok)
		gpio_free(platform_data->en_gpio);
#endif
#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
#ifndef CONFIG_NFC_CXD224X_RST_USE_PMIC
	if(gpio_is_valid(cxd224x_dev->rst_gpio)){
		if(rst_gpio_ok)
			gpio_free(platform_data->rst_gpio);
	}
#endif
#endif
	if(wake_gpio_ok)
		gpio_free(platform_data->wake_gpio);

	return ret;
}

static int cxd224x_remove(struct i2c_client *client)
{
	struct cxd224x_dev *cxd224x_dev;
	NFC_DRV_DBG_LOG("START");

	cxd224x_dev = i2c_get_clientdata(client);
	wake_lock_destroy(&cxd224x_dev->wakelock);
	wake_lock_destroy(&cxd224x_dev->wakelock_lp);
	free_irq(client->irq, cxd224x_dev);
	misc_deregister(&cxd224x_dev->cxd224x_device);
	mutex_destroy(&cxd224x_dev->read_mutex);
	gpio_free(cxd224x_dev->irq_gpio);
#if defined(CONFIG_NFC_CXD224X_VEN) || defined(CONFIG_NFC_CXD224X_VEN_MODULE)
	gpio_free(cxd224x_dev->en_gpio);
#endif
	gpio_free(cxd224x_dev->wake_gpio);
#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
	cxd224x_dev_data = NULL;
#endif

	kfree(cxd224x_dev);

	NFC_DRV_DBG_LOG("END");

	return 0;
}

#ifdef CONFIG_PM_SLEEP
#if 0
static int cxd224x_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cxd224x_platform_data *platform_data = pdev->dev.platform_data;

	if (device_may_wakeup(&pdev->dev)) {
		int irq = gpio_to_irq(platform_data->irq_gpio);
		enable_irq_wake(irq);
	}
	return 0;
}

static int cxd224x_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cxd224x_platform_data *platform_data = pdev->dev.platform_data;

	if (device_may_wakeup(&pdev->dev)) {
		int irq = gpio_to_irq(platform_data->irq_gpio);
		disable_irq_wake(irq);
	}
	return 0;
}

static const struct dev_pm_ops cxd224x_pm_ops = {
	.suspend	= cxd224x_suspend,
	.resume		= cxd224x_resume,
};
#endif
#endif

static const struct i2c_device_id cxd224x_id[] = {
	{"cxd224x-i2c", 0},
	{}
};

static struct of_device_id cxd224x_match_table[] = {
	{ .compatible = "cxd224x-i2c", },
	{ },
};

static struct i2c_driver cxd224x_driver = {
	.id_table = cxd224x_id,
	.probe = cxd224x_probe,
	.remove = cxd224x_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "cxd224x-i2c",
#ifdef CONFIG_PM
#if 0
		.pm	= &cxd224x_pm_ops,
#endif
#endif
		.of_match_table = cxd224x_match_table,
	},
};

/*
 * module load/unload record keeping
 */

static int __init cxd224x_dev_init(void)
{
	return i2c_add_driver(&cxd224x_driver);
}
module_init(cxd224x_dev_init);

static void __exit cxd224x_dev_exit(void)
{
	i2c_del_driver(&cxd224x_driver);
}
module_exit(cxd224x_dev_exit);

MODULE_AUTHOR("Sony");
MODULE_DESCRIPTION("NFC cxd224x driver");
MODULE_LICENSE("GPL");
