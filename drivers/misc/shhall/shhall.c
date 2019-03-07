/*
 * shhall.c - Cover detection by Hall IC
 *
 * Copyright (C) 2018 SHARP CORPORATION
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

#define SHHALL_LOG_TAG "SHFLIP"

int shhall_err_log  = 1;
int shhall_dbg_log  = 0;

#if defined (CONFIG_ANDROID_ENGINEERING)
module_param(shhall_err_log,  int, 0600);
module_param(shhall_dbg_log,  int, 0600);
#endif /* CONFIG_ANDROID_ENGINEERING */

#define SHHALL_DEBUG_LOG(fmt, args...)\
		if(shhall_dbg_log == 1) { \
			printk(KERN_INFO "[%s][%s(%d)] " fmt"\n", SHHALL_LOG_TAG, __func__, __LINE__, ## args);\
		}

#define SHHALL_ERR_LOG(fmt, args...)\
		if(shhall_err_log == 1) { \
			printk(KERN_ERR "[%s][%s(%d)] " fmt"\n", SHHALL_LOG_TAG, __func__, __LINE__, ## args);\
		}

enum {
	SHHALL_CLOSE	= 0,	/* GPIO L:Detect */
	SHHALL_OPEN,
};

struct hall_data {
	struct input_dev *input;
	unsigned int irq;
	int irq_gpio;
	unsigned int state;
};

struct hall_data *msm_hall_data;

static BLOCKING_NOTIFIER_HEAD(shflip_notifier_list);

int register_shflip_notifier(struct notifier_block *nb)
{
	int ret;

	SHHALL_DEBUG_LOG("msm_hall_data->state = %d\n", msm_hall_data->state);
	ret = blocking_notifier_chain_register(&shflip_notifier_list, nb);
	blocking_notifier_call_chain(&shflip_notifier_list, msm_hall_data->state, NULL);

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
	return gpio_get_value(msm_hall_data->irq_gpio);
}

static ssize_t state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	SHHALL_DEBUG_LOG("state = %d\n", msm_hall_data->state);

	switch (msm_hall_data->state) {
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
	struct hall_data *fd = dev_id;
	struct input_dev *input = fd->input;
	int value;

	/* Get the value of the GPIO pin */
	value = gpio_get_value(fd->irq_gpio);
	SHHALL_DEBUG_LOG("gpio = %d\n", value);

	/* Decision of opening and closing state */
	if(value == SHHALL_OPEN) {
		SHHALL_DEBUG_LOG("flip: open\n");
		input_report_switch(input, SW_LID, 0);
		fd->state = 0;
		blocking_notifier_call_chain(&shflip_notifier_list, 0, NULL);
#ifdef CONFIG_SHARP_SHTERM
		if (shterm_flip_status_set(SHTERM_FLIP_STATE_OPEN) != SHTERM_SUCCESS) {
			SHHALL_ERR_LOG("shterm_flip_status_set(open): FAILURE");
		}
#endif /* CONFIG_SHARP_SHTERM */
	}
	else if(value == SHHALL_CLOSE) {
		SHHALL_DEBUG_LOG("flip: close\n");
		input_report_switch(input, SW_LID, 1);
		fd->state = 1;
		blocking_notifier_call_chain(&shflip_notifier_list, 1, NULL);
#ifdef CONFIG_SHARP_SHTERM
		if (shterm_flip_status_set(SHTERM_FLIP_STATE_CLOSE) != SHTERM_SUCCESS) {
			SHHALL_ERR_LOG("shterm_flip_status_set(close): FAILURE");
		}
#endif /* CONFIG_SHARP_SHTERM */
	}

	input_sync(input);

	return IRQ_HANDLED;
}

static int shflip_probe(struct platform_device *pdev)
{
	struct hall_data *fd;
	struct input_dev *input;
	int error = 0;
	int wakeup = 1;
	unsigned long irqflags;
	int value;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	/* memory allocate */
	fd = kzalloc(sizeof(struct hall_data), GFP_KERNEL);
	if (!fd) {
		SHHALL_ERR_LOG("failed to allocate state");
		error = -ENOMEM;
		goto fail0;
	}

	platform_set_drvdata(pdev, fd);

	/* Get the value of the GPIO pin */
	fd->irq_gpio = of_get_named_gpio(np, "qcom,shflip-det-gpio", 0);
	msm_hall_data = fd;

	value = gpio_get_value(fd->irq_gpio);
	SHHALL_DEBUG_LOG("Startup gpio[%d] = %d", fd->irq_gpio, value);
	fd->state = (value == SHHALL_OPEN) ? 0 : 1;

	/* driver's data entry */
	input = input_allocate_device();
	if (!input) {
		SHHALL_ERR_LOG("failed to input allocate device state");
		error = -ENOMEM;
		goto fail2;
	}
	input_set_drvdata(input, fd);

	fd->input = input;
	input->name	= "shflip";
	input->id.vendor	= 0x0001;
	input->id.product	= 1;
	input->id.version	= 1;

	if(value == SHHALL_CLOSE) {
		__change_bit(SW_LID, input->sw);
	}

	fd->irq = gpio_to_irq(fd->irq_gpio);
	irqflags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

	error = request_threaded_irq(fd->irq, shflip_irq_isr, shflip_irq_thread, irqflags, pdev->name, fd);
	if (error) {
		SHHALL_ERR_LOG("Unable to request_irq error = %d", error);
		goto fail2;
	}

	input_set_capability(input, EV_SW, SW_LID);

	error = input_register_device(input);
	if (error) {
		SHHALL_ERR_LOG("input_register_device error=%d", error);
		goto fail3;
	}

	device_init_wakeup(&pdev->dev, wakeup);

	return error;
fail3:
	free_irq(fd->irq, pdev);
	input_free_device(input);
fail2:
	kfree(fd);
	msm_hall_data = NULL;
fail0:
	return error;

}

static int shflip_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	struct hall_data *fd = platform_get_drvdata(pdev);
	SHHALL_DEBUG_LOG("suspend");
	disable_irq_nosync(fd->irq);
	enable_irq_wake(fd->irq);
	return 0;
}

static int shflip_resume(struct platform_device *pdev)
{
	struct hall_data *fd = platform_get_drvdata(pdev);
	SHHALL_DEBUG_LOG("resume");
	enable_irq(fd->irq);
	disable_irq_wake(fd->irq);
	return 0;
}

static int shflip_remove(struct platform_device *pdev)
{
	struct hall_data *fd = platform_get_drvdata(pdev);
	struct input_dev *input = fd->input;

	free_irq(fd->irq, pdev);
	input_free_device(input);
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
