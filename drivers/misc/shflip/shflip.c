/*
 * shflip.c - Cover flip detection by Hall IC
 *
 * Copyright (C) 2017 SHARP CORPORATION
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
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/extcon.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/notifier.h>
#include <misc/shflip.h>
#ifdef CONFIG_SHARP_SHTERM
#include <misc/shterm_k.h>
#endif /* CONFIG_SHARP_SHTERM */

#define SHFLIP_LOG_TAG "SHFLIP"

int shflip_err_log  = 1;
int shflip_dbg_log  = 0;

#if defined (CONFIG_ANDROID_ENGINEERING)
module_param(shflip_err_log,  int, 0600);
module_param(shflip_dbg_log,  int, 0600);
#endif /* CONFIG_ANDROID_ENGINEERING */

#define SHFLIP_DEBUG_LOG(fmt, args...)\
		if(shflip_dbg_log == 1) { \
			printk(KERN_INFO "[%s][%s(%d)] " fmt"\n", SHFLIP_LOG_TAG, __func__, __LINE__, ## args);\
		}

#define SHFLIP_ERR_LOG(fmt, args...)\
		if(shflip_err_log == 1) { \
			printk(KERN_ERR "[%s][%s(%d)] " fmt"\n", SHFLIP_LOG_TAG, __func__, __LINE__, ## args);\
		}

enum {
	SHFLIP_CLOSE	= 0,	/* GPIO L:Detect */
	SHFLIP_OPEN,
};

struct flip_data {
	struct input_dev *input;
	unsigned int irq;
	struct extcon_dev edev_flip;
	int irq_gpio;
};

struct flip_data *msm_flip_data;

static BLOCKING_NOTIFIER_HEAD(shflip_notifier_list);

int register_shflip_notifier(struct notifier_block *nb)
{
	int ret, extcon_state;

	ret = blocking_notifier_chain_register(&shflip_notifier_list, nb);
	extcon_state = extcon_get_state(&msm_flip_data->edev_flip, EXTCON_FLIP);
	blocking_notifier_call_chain(&shflip_notifier_list, extcon_state, NULL);

	return ret;
}
EXPORT_SYMBOL(register_shflip_notifier);

int unregister_shflip_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&shflip_notifier_list, nb);
}
EXPORT_SYMBOL(unregister_shflip_notifier);

int msm_flip_get_state(void)
{
	return gpio_get_value(msm_flip_data->irq_gpio);
}

static ssize_t state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int extcon_state;

	extcon_state = extcon_get_state(&msm_flip_data->edev_flip, EXTCON_FLIP);
	SHFLIP_DEBUG_LOG("extcon_state = %d\n", extcon_state);
	switch (extcon_state) {
	case 0:
		return sprintf(buf, "OPEN\n");
	case 1:
		return sprintf(buf, "CLOSE\n");
	}
	return -EINVAL;
}

static DEVICE_ATTR_RO(state);

static struct attribute *shflip_attrs[] = {
	&dev_attr_state.attr,
	NULL,
};

ATTRIBUTE_GROUPS(shflip);

static struct of_device_id shflip_switch_of_match[] = {
	{ .compatible = "sharp,flip", },
	{ },
};
MODULE_DEVICE_TABLE(of, shflip_switch_of_match);

static irqreturn_t shflip_irq_isr(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

irqreturn_t shflip_irq_thread(int irq, void *dev_id)
{
	struct flip_data *fd = dev_id;
	struct input_dev *input = fd->input;
	int value;

	/* Get the value of the GPIO pin */
	value = gpio_get_value(fd->irq_gpio);
	SHFLIP_DEBUG_LOG("gpio = %d\n", value);

	/* Decision of opening and closing state */
	if(value == SHFLIP_OPEN) {
		SHFLIP_DEBUG_LOG("flip: open\n");
		input_report_switch(input, SW_LID, 0);
		extcon_set_state_sync(&fd->edev_flip, EXTCON_FLIP, 0);
		blocking_notifier_call_chain(&shflip_notifier_list, 0, NULL);
#ifdef CONFIG_SHARP_SHTERM
		if (shterm_flip_status_set(SHTERM_FLIP_STATE_OPEN) != SHTERM_SUCCESS) {
			SHFLIP_ERR_LOG("shterm_flip_status_set(open): FAILURE");
		}
#endif /* CONFIG_SHARP_SHTERM */
	}
	else if(value == SHFLIP_CLOSE) {
		SHFLIP_DEBUG_LOG("flip: close\n");
		input_report_switch(input, SW_LID, 1);
		extcon_set_state_sync(&fd->edev_flip, EXTCON_FLIP, 1);
		blocking_notifier_call_chain(&shflip_notifier_list, 1, NULL);
#ifdef CONFIG_SHARP_SHTERM
		if (shterm_flip_status_set(SHTERM_FLIP_STATE_CLOSE) != SHTERM_SUCCESS) {
			SHFLIP_ERR_LOG("shterm_flip_status_set(close): FAILURE");
		}
#endif /* CONFIG_SHARP_SHTERM */
	}

	input_sync(input);

	return IRQ_HANDLED;
}

static const unsigned int shflip_supported_cable[] = {
	EXTCON_FLIP,
	EXTCON_NONE,
};

static int shflip_probe(struct platform_device *pdev)
{
	struct flip_data *fd;
	struct input_dev *input;
	int error = 0;
	int wakeup = 1;
	unsigned long irqflags;
	int value;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	/* memory allocate */
	fd = kzalloc(sizeof(struct flip_data), GFP_KERNEL);
	if (!fd) {
		SHFLIP_ERR_LOG("failed to allocate state");
		error = -ENOMEM;
		goto fail0;
	}

	platform_set_drvdata(pdev, fd);

	/* flip data setting */
	memset(&fd->edev_flip, 0x0, sizeof(fd->edev_flip));
	fd->edev_flip.name	= "flip";

	fd->edev_flip.supported_cable = shflip_supported_cable;
	fd->edev_flip.dev.parent = &pdev->dev;
	error = extcon_dev_register(&fd->edev_flip);
	if (error) {
		SHFLIP_ERR_LOG("failed extcon_dev_register: error(%d)", error);
		goto fail1;
	}

	/* Get the value of the GPIO pin */
	fd->irq_gpio = of_get_named_gpio(np, "qcom,shflip-det-gpio", 0);
	msm_flip_data = fd;

	value = gpio_get_value(fd->irq_gpio);
	SHFLIP_DEBUG_LOG("Startup gpio[%d] = %d", fd->irq_gpio, value);
	extcon_set_state_sync(&fd->edev_flip, EXTCON_FLIP, (value == SHFLIP_OPEN) ? 0 : 1);

	/* driver's data entry */
	input = input_allocate_device();
	if (!input) {
		SHFLIP_ERR_LOG("failed to input allocate device state");
		error = -ENOMEM;
		goto fail2;
	}
	input_set_drvdata(input, fd);

	fd->input = input;
	input->name	= "shflip";
	input->id.vendor	= 0x0001;
	input->id.product	= 1;
	input->id.version	= 1;

	if(value == SHFLIP_CLOSE) {
		__change_bit(SW_LID, input->sw);
	}

	fd->irq = gpio_to_irq(fd->irq_gpio);
	irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

	error = request_threaded_irq(fd->irq, shflip_irq_isr, shflip_irq_thread, irqflags, pdev->name, fd);
	if (error) {
		SHFLIP_ERR_LOG("Unable to request_irq error = %d", error);
		goto fail2;
	}

	input_set_capability(input, EV_SW, SW_LID);

	error = input_register_device(input);
	if (error) {
		SHFLIP_ERR_LOG("input_register_device error=%d", error);
		goto fail3;
	}

	device_init_wakeup(&pdev->dev, wakeup);

	return error;
fail3:
	free_irq(fd->irq, pdev);
	input_free_device(input);
fail2:
	extcon_dev_unregister(&fd->edev_flip);
fail1:
	kfree(fd);
	msm_flip_data = NULL;
fail0:
	return error;

}

static int shflip_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct flip_data *fd = platform_get_drvdata(pdev);
	SHFLIP_DEBUG_LOG("flip suspend");
	disable_irq_nosync(fd->irq);
	enable_irq_wake(fd->irq);
	return 0;
}

static int shflip_resume(struct platform_device *pdev)
{
	struct flip_data *fd = platform_get_drvdata(pdev);
	SHFLIP_DEBUG_LOG("flip resume");
	enable_irq(fd->irq);
	disable_irq_wake(fd->irq);
	return 0;
}

static int shflip_remove(struct platform_device *pdev)
{
	struct flip_data *fd = platform_get_drvdata(pdev);
	struct input_dev *input = fd->input;

	free_irq(fd->irq, pdev);
	input_free_device(input);
	extcon_dev_unregister(&fd->edev_flip);
	kfree(fd);

	return 0;
}

static struct platform_driver shflip_driver = {
	.probe		= shflip_probe,
	.remove		= shflip_remove,
	.suspend    = shflip_suspend,
	.resume     = shflip_resume,
	.driver		= {
		.name	= "shflip",
		.owner	= THIS_MODULE,
		.of_match_table = shflip_switch_of_match,
		.groups = shflip_groups,
	},
};

static int __init shflip_init(void)
{
	return platform_driver_register(&shflip_driver);
}

static void __exit shflip_exit(void)
{
	platform_driver_unregister(&shflip_driver);
}

module_exit(shflip_exit);
module_init(shflip_init);

MODULE_DESCRIPTION("SHARP FLIP DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("2.0");
