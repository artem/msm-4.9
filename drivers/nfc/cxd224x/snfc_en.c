/* drivers/sharp/nfc/snfc_en.c
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

/* INCLUDE FILES */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/input.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/major.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/qpnp/pin.h>
#include <asm/uaccess.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#include "nfc.h"
#include "snfc_en.h"

/* MACROS */
#define D_SNFC_EN_DEVS 			(1)
#define D_SNFC_EN_DEV_NAME 		("snfc_en")

/* VARIABLES */
struct snfc_en_platform_data {
	unsigned int nint;
	unsigned int status;
#ifdef DEBUG_NFC_DRV_PIN
	unsigned int clkreq;
#endif	/* DEBUG_NFC_DRV_PIN */
};

struct snfc_en_info {
	struct miscdevice miscdev;
	struct mutex mutex;
	struct device *dev;
	struct snfc_en_platform_data *pdata;
};

static struct device *snfc_en_dev = NULL;

/* FUNCTION */

static int snfc_en_open(struct inode *inode, struct file *file)
{
	struct snfc_en_info *info = container_of(file->private_data, struct snfc_en_info, miscdev);

	int ret = 0;

	NFC_DRV_DBG_LOG("START");

	file->private_data = &info->miscdev;

	NFC_DRV_DBG_LOG("END ret=%d", ret);
	return ret;
}

static int snfc_en_close(struct inode *inode, struct file *file)
{
//	struct snfc_en_info *info = container_of(file->private_data, struct snfc_en_info, miscdev);
	NFC_DRV_DBG_LOG("START");
	NFC_DRV_DBG_LOG("END");
	return 0;
}

static long snfc_en_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct snfc_en_info *info = container_of(file->private_data, struct snfc_en_info, miscdev);
	int ret = 0;

	NFC_DRV_DBG_LOG("START");

	mutex_lock(&info->mutex);

	switch (cmd) {
	case NFC_SNFC_EN_IOCTL_HVDD_H:
		break;
	case NFC_SNFC_EN_IOCTL_HVDD_L:
		break;
	case NFC_SNFC_IOCTL_GET_NINT:
		NFC_DRV_DBG_LOG("get nint");
		ret = gpio_get_value(info->pdata->nint);
		if(copy_to_user((unsigned int *)arg, &ret, sizeof(unsigned int)) != 0) {
			NFC_DRV_ERR_LOG("copy failed to user");
		}
		break;
	case NFC_SNFC_IOCTL_GET_STATUS:
		NFC_DRV_DBG_LOG("get status");
		ret = gpio_get_value(info->pdata->status);
		if(copy_to_user((unsigned int *)arg, &ret, sizeof(unsigned int)) != 0) {
			NFC_DRV_ERR_LOG("copy failed to user");
		}
		break;
#ifdef DEBUG_NFC_DRV_PIN
	case NFC_SNFC_IOCTL_GET_CLKREQ:
		NFC_DRV_DBG_LOG("get clk_req");
		ret = gpio_get_value(info->pdata->clkreq);
		if(copy_to_user((unsigned int *)arg, &ret, sizeof(unsigned int)) != 0) {
			NFC_DRV_ERR_LOG("copy failed to user");
		}
		break;
#endif	/* DEBUG_NFC_DRV_PIN */
	default:
		NFC_DRV_DBG_LOG("Unknow ioctl 0x%x", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	mutex_unlock(&info->mutex);

	NFC_DRV_DBG_LOG("END");

	return ret;
}

static ssize_t snfc_en_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
	NFC_DRV_DBG_LOG("START");
	NFC_DRV_DBG_LOG("END");
	return 0;
}

ssize_t snfc_en_write(struct file *file, const char __user *data, size_t len, loff_t *ppos)
{
	char sw;
	ssize_t ret = len;

	NFC_DRV_DBG_LOG("START");

	/* length check */
	if (len < 1) {
		NFC_DRV_ERR_LOG("length check len=%d", (int)len);
		return -EIO;
	}

	if (copy_from_user(&sw, data, 1)) {
		NFC_DRV_ERR_LOG("copy_from_user");
		return -EFAULT;
	}

	switch(sw){
	case SNFC_OFF_SEQUENCE_NFC:
		NFC_DRV_DBG_LOG("off sequence_nfc");
		break;
	case SNFC_ON_SEQUENCE:
		NFC_DRV_DBG_LOG("on sequence");
#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
#ifndef CONFIG_NFC_CXD224X_PROBE_RST
		cxd224x_dev_reset();
#endif
#endif
		break;
	case SNFC_OFF_SEQUENCE_SIM:
		NFC_DRV_DBG_LOG("off sequence_sim");
		break;
	default:
		NFC_DRV_ERR_LOG("write data = %d", sw);
		ret = -EFAULT;
		break;
	}

	NFC_DRV_DBG_LOG("END");

	return ret;
}


static const struct file_operations snfc_en_fops = {
	.owner			= THIS_MODULE,
	.open			= snfc_en_open,
	.read			= snfc_en_read,
	.write			= snfc_en_write,
	.release		= snfc_en_close,
	.unlocked_ioctl	= snfc_en_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= snfc_en_ioctl
#endif //CONFIG_COMPAT
};

static int snfc_en_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct snfc_en_info *info = NULL;
	struct snfc_en_platform_data *pdata = NULL;
	struct device_node *np = NULL;
	int ret = 0;

	NFC_DRV_DBG_LOG("START");

	if(dev) {
		NFC_DRV_DBG_LOG("alloc for platform data");
		pdata = kzalloc(sizeof(struct snfc_en_platform_data), GFP_KERNEL);
		if (!pdata) {
			NFC_DRV_ERR_LOG("No platform data");
			ret = -ENOMEM;
			goto err_pdata;
		}
	} else {
		NFC_DRV_ERR_LOG("failed alloc platform data");
		ret = -ENOMEM;
		goto err_pdata;
	}

	np = dev->of_node;
	pdata->nint     = of_get_named_gpio(np, "qcom,nfc-nint"  , 0);
	pdata->status   = of_get_named_gpio(np, "qcom,nfc-status", 0);
#ifdef DEBUG_NFC_DRV_PIN
	pdata->clkreq   = of_get_named_gpio(np, "qcom,nfc-clkreq", 0);
#endif	/* DEBUG_NFC_DRV_PIN */

	info = kzalloc(sizeof(struct snfc_en_info), GFP_KERNEL);
	if (!info) {
		NFC_DRV_ERR_LOG("failed to allocate memory for snfc_en_info");
		ret = -ENOMEM;
		kfree(pdata);
		goto err_info_alloc;
	}

	info->dev = dev;
	info->pdata = pdata;

	mutex_init(&info->mutex);
	dev_set_drvdata(dev, info);

	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	info->miscdev.name = D_SNFC_EN_DEV_NAME;
	info->miscdev.fops = &snfc_en_fops;
	info->miscdev.parent = dev;
	ret = misc_register(&info->miscdev);
	if (ret < 0) {
		NFC_DRV_ERR_LOG("failed to register Device");
		goto err_dev_reg;
	}

	snfc_en_dev = dev;

	NFC_DRV_DBG_LOG("END");

	return 0;

err_dev_reg:
err_info_alloc:
	kfree(info);
err_pdata:
	return ret;
}

static int snfc_en_remove(struct platform_device *pdev)
{
	struct snfc_en_info *info = dev_get_drvdata(&pdev->dev);

	misc_deregister(&info->miscdev);

	kfree(info);

	return 0;
}


/* entry point */
#ifdef CONFIG_PM
static int snfc_en_suspend(struct device *dev)
{
	NFC_DRV_DBG_LOG("START");
	NFC_DRV_DBG_LOG("END");
	return 0;
}

static int snfc_en_resume(struct device *dev)
{
	NFC_DRV_DBG_LOG("START");
	NFC_DRV_DBG_LOG("END");
	return 0;
}

static SIMPLE_DEV_PM_OPS(snfc_en_pm_ops, snfc_en_suspend, snfc_en_resume);
#endif

static struct platform_device_id snfc_en_id_table[] = {
	{ D_SNFC_EN_DEV_NAME, 0 },
	{ }
};

static struct of_device_id snfc_en_match_table[] = {
	{ .compatible = D_SNFC_EN_DEV_NAME, },
	{},
};

MODULE_DEVICE_TABLE(platform, snfc_en_id_table);
static struct platform_driver snfc_en_driver = {
	.probe = snfc_en_probe,
	.id_table = snfc_en_id_table,
	.remove = snfc_en_remove,
	.driver = {
		.name = D_SNFC_EN_DEV_NAME,
#ifdef CONFIG_PM
		.pm = &snfc_en_pm_ops,
#endif //CONFIG_PM
		.of_match_table = snfc_en_match_table,
	},
};

module_platform_driver(snfc_en_driver);

MODULE_LICENSE("GPL v2");
