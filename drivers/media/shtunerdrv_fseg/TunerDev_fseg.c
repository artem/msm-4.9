/**************************************************************************************************/
/** 
	@file		TunerDev_fseg.c
	@brief		Tuner Device Control
*/
/**************************************************************************************************/

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/err.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <asm/uaccess.h> /* for access_ok() */
#include <linux/regulator/consumer.h> /* for regulator_xx() */
#include <linux/delay.h>
#include <linux/qpnp/pin.h>
#include <linux/platform_device.h> /* for platform_driver_xx() */
#include <asm/io.h>

#ifdef CONFIG_SHARP_SMEM_CUST
#include "soc/qcom/sh_smem.h"
#endif

#ifdef CONFIG_SHARP_SHTERM
#include "misc/shterm_k.h"
#endif

#include "gpio_def_fseg.h"

static int gpio_init(void);
static int gpio_get(unsigned int id, int *val);
static int gpio_set(unsigned int id, int val);
static int tuner_vreg_enable(void);
static int tuner_vreg_disable(void);
static int tuner_clk_enable(void);
static int tuner_clk_disable(void);
static int hw_revision_get(unsigned int *val);

static int tuner_probe(struct platform_device *dev);
static int tuner_remove(struct platform_device *dev);

//static struct clk *gp_clk;

stGPIO_DEF use_gpiono[] = {
	/* GPIO No (ID)			, Direction		, Init Value	, Initialized	, diag-label	*/
#if VALID_GPIO_DTVEN
	{GPIO_DTVEN_PORTID		, DirctionOut	, 0				, 0				, "qcom,dtv-en-gpio" },
#endif
#if VALID_GPIO_DTVRST
	{GPIO_DTVRST_PORTID 	, DirctionOut	, 0				, 0				, "qcom,dtv-rst-gpio" },
#endif
#if VALID_GPIO_DTVLNAEN
	{GPIO_DTVLNAEN_PORTID	, DirctionOut	, 0				, 0				, "define if you need" },
#endif
#if VALID_GPIO_DTVANTSW
	{GPIO_DTVANTSW_PORTID	, DirctionOut	, 0				, 0				, "define if you need" },
#endif
#if VALID_GPIO_DTVMANTSL
	{GPIO_DTVMANTSL_PORTID	, DirctionOut	, 0				, 0				, "qcom,mant-sl-gpio" },
#endif
#if VALID_GPIO_DTVUANTSL
	{GPIO_DTVUANTSL_PORTID	, DirctionOut	, 0				, 0				, "qcom,uant-sl-gpio" },
#endif
#if VALID_GPIO_DTVCANTSL
	{GPIO_DTVCANTSL_PORTID	, DirctionOut	, 0				, 0				, "define if you need" },
#endif
#if VALID_GPIO_DTV_STDBY
	{GPIO_DTV_STDBY_PORTID	, DirctionOut	, 1				, 1				, "qcom,ofdm-rst-gpio" },
#endif
};

struct device_node *np;

#if 1
static struct regulator *reg_28v;
static const char *reg_id_28v = "pm8998_l18";
static const int min_uV_28v = 2856000;
static const int max_uV_28v = 2856000;
#endif

/**************************************************************************************************/
/**
	@brief	tuner_open
	@param	struct inode	*inode		[I]
	@param	struct file		*file		[I]
	@retval	0	Success
	@retval	-1	Failed
*/
/**************************************************************************************************/
static int tuner_open(struct inode *inode, struct file *file)
{
	int ret;

	ret  = gpio_init();

	if (ret == 1) {
		/* Failed */
		printk("%s:%d !!!tuner_open gpio_init() error \n", __FILE__, __LINE__);
		return (-1);
	}
	return (0);
}

/**************************************************************************************************/
/**
	@brief	tuner_release
	@param	struct inode	*inode		[I]
	@param	struct file		*file		[I]
	@retval	0	Success
	@retval	-1	Failed
*/
/**************************************************************************************************/
static int tuner_release(struct inode *inode, struct file *file)
{
	return (0);
}

/**************************************************************************************************/
/**
	@brief	tuner_release
	@param	struct file		*file	[I]
	@param	unsigned int	cmd		[I]
	@param	unsigned long	arg		[I]
	@retval	0		Suncess
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static long tuner_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;
	ioctl_cmd io_cmd_work;
	ioctl_cmd *io_cmd = &io_cmd_work;

	if (copy_from_user(io_cmd, (ioctl_cmd*)arg, sizeof(ioctl_cmd))) {
		printk("%s:%d  !!!tuner_ioctl copy_from_user error %08x\n", __FILE__, __LINE__, (unsigned int)arg);
		return -EFAULT;
	}

	switch ( cmd ) {
	case IOC_GPIO_VAL_SET:
		ret = gpio_set(io_cmd->id, io_cmd->val);
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl set error[cmd %x val %d][ret:%d]\n", __FILE__, __LINE__, io_cmd->id, io_cmd->val, ret);
			return -EINVAL;
		}
		break;
	case IOC_GPIO_VAL_GET:
		ret = gpio_get(io_cmd->id, &(io_cmd->val));
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl get error[cmd %x val %d][ret:%d]\n", __FILE__, __LINE__, io_cmd->id, io_cmd->val, ret);
			return -EINVAL;
		}
		if (copy_to_user((ioctl_cmd*)arg, io_cmd, sizeof(ioctl_cmd))) {
			printk("%s:%d  !!!tuner_ioctl copy_to_user error %08x\n", __FILE__, __LINE__, (unsigned int)arg);
			return -EFAULT;
		}
		break;
	case IOC_VREG_ENABLE:
		ret = tuner_vreg_enable();
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl set error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		break;
	case IOC_VREG_DISABLE:
		ret = tuner_vreg_disable();
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl set error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		break;
	case IOC_CLK_ENABLE:
		ret = tuner_clk_enable();
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl set error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		break;
	case IOC_CLK_DISABLE:
		ret = tuner_clk_disable();
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl set error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		break;
	case IOC_HW_REV_GET:
		ret = hw_revision_get(&(io_cmd->val));
		if (ret != 0) {
			printk("%s:%d  !!!tuner_ioctl get error[ret:%d]\n", __FILE__, __LINE__, ret);
			return -EINVAL;
		}
		if (copy_to_user((ioctl_cmd*)arg, io_cmd, sizeof(ioctl_cmd))) {
			printk("%s:%d  !!!tuner_ioctl copy_to_user error %08x\n", __FILE__, __LINE__, (unsigned int)arg);
			return -EFAULT;
		}
		break;
	default:
		/* NOTE:  returning a fault code here could cause trouble
		 * in buggy userspace code.  Some old kernel bugs returned
		 * zero in this case, and userspace code might accidentally
		 * have depended on that bug.
		 */
		return -ENOTTY;
	}
	return 0;
}

#ifdef CONFIG_OF						// Open firmware must be defined for dts usage
static struct of_device_id tuner_table[] = {
	{ . compatible = "tunctrl" ,},			// Compatible node must match dts
	{ },
};
#else
#define tuner_table NULL
#endif

static struct file_operations tuner_fops = {
	.owner				= THIS_MODULE,
	.compat_ioctl		= tuner_ioctl,
	.unlocked_ioctl		= tuner_ioctl,
	.open				= tuner_open,
	.release			= tuner_release,
};

static struct miscdevice tuner_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tunctrl_fseg",
	.fops = &tuner_fops,
};

static int tuner_probe(struct platform_device *dev)
{
	int ret, i ;
	int loop = sizeof(use_gpiono)/sizeof(stGPIO_DEF);
	int portno;

	ret = misc_register(&tuner_dev);
	if (ret) {
		printk("%s.%s.%d !!! fail to misc_register (MISC_DYNAMIC_MINOR)\n", __FILE__, __func__, __LINE__);
		return ret;
	}

	np = of_find_node_by_name( NULL, "tunctrl" );
	if ( np ) {
		for ( i=0; i<loop; i++ ) {
			portno = of_get_named_gpio( np, use_gpiono[i].label ,0 );
			ret = gpio_request( portno, NULL );

			if ( ret < 0 ) {
				printk(KERN_DEBUG "%s gpio_request() error : %d\n", use_gpiono[i].label, ret);
			}
		}
	}
	else {
		printk(KERN_DEBUG "of_find_node_by_name() error \n");
#if 1
		misc_deregister(&tuner_dev);
#endif
		return -1;
	}

#if 1
	reg_28v = regulator_get(tuner_dev.this_device, reg_id_28v);
	if (IS_ERR(reg_28v)) {
		printk("Unable to get %s regulator\n", reg_id_28v);
		misc_deregister(&tuner_dev);
		return -1;
	}

	ret = regulator_set_voltage(reg_28v, min_uV_28v, max_uV_28v);
	if (ret != 0) {
		printk("Unable to set %s regulator voltage[ret:%d]\n", reg_id_28v, ret);
		regulator_put(reg_28v);
		misc_deregister(&tuner_dev);
		return ret;
	}
#endif

	return 0;
}

static int tuner_remove(struct platform_device *dev)
{
	int i ;
	int loop = sizeof(use_gpiono)/sizeof(stGPIO_DEF);
	int portno;

#if 1
	regulator_put(reg_28v);
#endif

	np = of_find_node_by_name( NULL, "tunctrl" );
	if ( np ) {
		for ( i=0; i<loop; i++ ) {
			portno = of_get_named_gpio( np, use_gpiono[i].label ,0 );
			gpio_free( portno );
		}
	}
	else {
		printk(KERN_DEBUG "of_find_node_by_name() error \n");
	}

	misc_deregister(&tuner_dev);

	return 0;
}

static struct platform_driver tuner_driver = {
	.probe = tuner_probe,
	.remove = tuner_remove,
	.driver = {
		.of_match_table = tuner_table,
		.name = "tunctrl",
		.owner = THIS_MODULE,
	},
};

/**************************************************************************************************/
/**
	@brief	tuner_init
	@param	none
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int __init tuner_init(void)
{
	int ret;

	/* platform_driver_register */
	ret = platform_driver_register(&tuner_driver);
	if (ret) {
		printk("%s:%d platform_driver_register() failed \n", __FILE__, __LINE__);
	}
	else {
//		printk("%s:%d platform_driver_register() OK \n", __FILE__, __LINE__);
	}

	return ret;
}

/**************************************************************************************************/
/**
	@brief	tuner_cleanup
	@param	none
	@retval	none
*/
/**************************************************************************************************/
static void __exit tuner_cleanup(void)
{
	platform_driver_unregister(&tuner_driver);
}

/**************************************************************************************************/
/**
	@brief	gpio_init
	@param	none
	@retval	0	Success
	@retval	1	Failed
*/
/**************************************************************************************************/
static int gpio_init(void)
{
	int loop = sizeof(use_gpiono)/sizeof(stGPIO_DEF);
	int errcnt = 0;
	stGPIO_DEF *p = &use_gpiono[0];
	int i;
	int portno;

	for (i=0; i<loop; i++, p++) {
		if (p->direction == DirctionIn) {
			/* GPIO Input */
			portno = of_get_named_gpio( np, use_gpiono[i].label ,0 );
		//	printk(KERN_DEBUG "of_get_named_gpio() -- gpio_direction_input [%d][%s] \n", portno, use_gpiono[i].label);
			if ( gpio_direction_input( portno ) < 0 ) {
				errcnt ++;
				printk( "%s:%d gpio_direction_input error %s \n", __FILE__,__LINE__, use_gpiono[i].label );
				continue;
			}
			p->init_done = 1;
			continue;
		}
		if (p->direction == DirctionOut) {
			/* GPIO Output */
			portno = of_get_named_gpio( np, use_gpiono[i].label ,0 );
		//	printk(KERN_DEBUG "of_get_named_gpio() -- direction_output [%d][%s] \n", portno, use_gpiono[i].label);
			if ( gpio_direction_output( portno, use_gpiono[i].out_val ) < 0 ) {
				errcnt ++;
				printk( "%s:%d gpio_direction_output error %s \n", __FILE__,__LINE__, use_gpiono[i].label );
				continue;
			}
			p->init_done = 1;
			continue;
		}
	}

	if (errcnt != 0) {
		printk("%s: gpio_init error count %d\n", __FILE__, errcnt);
		return 1;
	}
	return 0;
}

static int tuner_clk_enable(void)
{
#if 0
	unsigned 	gpio_cfg;
	int			ret;

	gpio_cfg = GPIO_CFG( CLOCK_CONTROL_PORTNO, 5, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_6MA );	/* gp_clk_2b */

	ret = gpio_tlmm_config( gpio_cfg, GPIO_CFG_ENABLE );
	if ( ret < 0 ) {
		printk("%s: gpio_cfg error %d\n", __FILE__, ret);
		return 1;
	}

	gp_clk = clk_get( NULL, "gp2_clk" );
	clk_set_rate( gp_clk, 27000000 );
	clk_prepare( gp_clk );
	clk_enable( gp_clk );

//	writel(0xA00, MSM_CLK_CTL_BASE + 0x2D24 + 32*( 2 ));		/* gp_clk_2b */

#endif
	return 0;
}

static int tuner_clk_disable(void)
{
#if 0
	clk_disable( gp_clk );
	clk_unprepare( gp_clk );

//	writel(0x0000, MSM_CLK_CTL_BASE + 0x2D24 + 32*( 2 ));		/* gp_clk_2b */

#endif
	return 0;
}
	

/**************************************************************************************************/
/**
	@brief	gpio_set
	@param	unsigned int	id			[I]	GPIO No
	@param	int				value		[I]	GPIO Value
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int gpio_set(unsigned int id, int value)
{
	int loop = sizeof(use_gpiono)/sizeof(stGPIO_DEF);
	stGPIO_DEF *p = use_gpiono;
	int flag = 0;
	int i;
	int portno ;

	for(i=0; i<loop; i++, p++){
		if (p->id == id){
			flag = 1;

			portno = of_get_named_gpio( np, use_gpiono[i].label ,0 );
			break;
		}
	}
	if (flag == 0) {
		printk("%s: !!! gpio_set() error No.%d value %d \n", __FILE__, id, value);
		return EINVAL;
	}
	gpio_set_value_cansleep(portno, value);
	return 0;
}

/**************************************************************************************************/
/**
	@brief	gpio_get
	@param	unsigned int	id			[I]	GPIO No
	@param	int				*val		[I]	GPIO Value
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int gpio_get(unsigned int id, int *val)
{
	int loop = sizeof(use_gpiono)/sizeof(stGPIO_DEF);
	stGPIO_DEF *p = &use_gpiono[0];
	int flag = 0;
	int i;
	int portno ;

	*val = 0;

	for(i=0; i<loop; i++, p++){
		if (p->id == id){
			flag = 1;

			portno = of_get_named_gpio( np, use_gpiono[i].label ,0 );
		//	printk(KERN_DEBUG "of_get_named_gpio() [%d][%s] \n", portno, use_gpiono[i].label);
			break;
		}
	}
	if (flag == 0) {
		printk("%s: !!! gpio_get() No.%d error \n", __FILE__, id);
		return EINVAL;
	}
	*val = gpio_get_value_cansleep(portno);

	return 0;
}

/**************************************************************************************************/
/**
	@brief	tuner_vreg_enable
	@param	none
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int tuner_vreg_enable(void)
{
	int	ret;
#if 0
	struct regulator *reg;
	struct device *dev = tuner_dev.this_device;
	const char *id = "pm8994_l27";
	int min_uV = 1200000, max_uV = 1200000;
	unsigned int hw_rev;
#endif

#if 0
	/* for LNA (2.8V -- L18) */
	struct regulator *reg_28v;
	struct device *dev_28v = tuner_dev.this_device;
	const char *id_28v = "pm8998_l18";
	int min_28v_uV = 2800000, max_28v_uV = 2800000;

	reg_28v = regulator_get(dev_28v, id_28v);
	if (IS_ERR(reg_28v)) {
		printk("Unable to get %s regulator\n", id_28v);
		return -1;
	}

    regulator_set_voltage(reg_28v, min_28v_uV, max_28v_uV);

	if (!regulator_is_enabled(reg_28v)) {
		ret = regulator_enable(reg_28v);
	}

	regulator_put(reg_28v);
	/* --- */
#else
	ret = regulator_enable(reg_28v);
	if (ret != 0) {
		printk("Unable to enable %s regulator[ret:%d]\n", reg_id_28v, ret);
		return ret;
	}
#endif

#if 0
	msleep(1);

	hw_revision_get( &hw_rev );
	if ( hw_rev <= 1 ) {		/* ES0 and ES1 */
		reg= regulator_get(dev, id);
		if (IS_ERR(reg)) {
			printk("Unable to get %s regulator\n", id);
			return -1;
		}

	    regulator_set_voltage(reg, min_uV, max_uV);

		if (!regulator_is_enabled(reg)) {
			ret = regulator_enable(reg);
		}

		regulator_put(reg);
	}
#endif

#ifdef CONFIG_SHARP_SHTERM
	shterm_k_set_info( SHTERM_INFO_DTB, 1 );
#endif

	return 0;
}

/**************************************************************************************************/
/**
	@brief	tuner_vreg_disable
	@param	none
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int tuner_vreg_disable(void)
{
	int	ret;
#if 0
	struct regulator *reg;
	struct device *dev = tuner_dev.this_device;
	const char *id = "pm8994_l27";
	unsigned int hw_rev;
#endif

#if 0
	/* for LNA (2.8V -- L18) */
	struct regulator *reg_28v;
	struct device *dev_28v = tuner_dev.this_device;
	const char *id_28v = "pm8998_l18";
	/* --- */
#endif

#ifdef CONFIG_SHARP_SHTERM
	shterm_k_set_info( SHTERM_INFO_DTB, 0 );
#endif

#if 0
	hw_revision_get( &hw_rev );
	if ( hw_rev <= 1 ) {		/* ES0 and ES1 */
		reg= regulator_get(dev, id);
		if (IS_ERR(reg)) {
			printk("Unable to get %s regulator\n", id);
			return -1;
		}

		if (regulator_is_enabled(reg)) {
			ret = regulator_disable(reg);
		}

		regulator_put(reg);
	}
#endif

#if 0
	/* for LNA (2.8V -- L18) */
	reg_28v = regulator_get(dev_28v, id_28v);
	if (IS_ERR(reg_28v)) {
		printk("Unable to get %s regulator\n", id_28v);
		return -1;
	}

	if (regulator_is_enabled(reg_28v)) {
		ret = regulator_disable(reg_28v);
	}

	regulator_put(reg_28v);
	/* --- */
#else
	ret = regulator_disable(reg_28v);
	if (ret != 0) {
		printk("Unable to disable %s regulator[ret:%d]\n", reg_id_28v, ret);
		return ret;
	}
#endif

	return 0;
}

/**************************************************************************************************/
/**
	@brief	hw_revision_get
	@param	int		   *val		[I]	sh_hw_revision value
	@retval	0		Success
	@retval	!=0		Failed
*/
/**************************************************************************************************/
static int hw_revision_get(unsigned int *val)
{
#ifdef CONFIG_SHARP_SMEM_CUST
	sharp_smem_common_type *sh_smem_common = NULL;

	sh_smem_common = (sharp_smem_common_type *)sh_smem_get_common_address();
	if (sh_smem_common != NULL) {
		*val = (unsigned int)sh_smem_common->sh_hw_revision;
		printk(KERN_DEBUG "sh_hw_revision [%d] \n", *val);
	}
	else{
		return -1;
	}
	return 0;
#else
	return -1;
#endif
}

MODULE_LICENSE("GPL");
module_init(tuner_init);
module_exit(tuner_cleanup);
