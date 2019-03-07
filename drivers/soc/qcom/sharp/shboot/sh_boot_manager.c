/* drivers/soc/qcom/sharp/shboot/sh_boot_manager.c
 *
 * Copyright (C) 2017 Sharp Corporation
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

/*===========================================================================
INCLUDE
===========================================================================*/
#include <linux/module.h>
#ifdef CONFIG_SHARP_SMEM_CUST
#include <soc/qcom/sh_smem.h>
#endif /* CONFIG_SHARP_SMEM_CUST */
#include <soc/qcom/sharp/sh_boot_manager.h>

/*===========================================================================
DEFINE
===========================================================================*/

/*===========================================================================
PROTOTYPES
===========================================================================*/
static int sh_boot_get_bootmode_to_user(char *buffer, const struct kernel_param *kp);
static int sh_boot_set_bootmode_from_user(const char *buffer, const struct kernel_param *kp);
static int sh_boot_get_handset_to_user(char *buffer, const struct kernel_param *kp);

/*===========================================================================
GLOBAL VARIABLES
===========================================================================*/
static unsigned long boot_mode = 0;
static unsigned char handset = 0;
static struct kernel_param_ops param_ops_bootmode = {
	.get = sh_boot_get_bootmode_to_user,
	.set = sh_boot_set_bootmode_from_user,
};
static struct kernel_param_ops param_ops_handset = {
	.get = sh_boot_get_handset_to_user,
};

/*=============================================================================
FUNCTION
=============================================================================*/
unsigned short sh_boot_get_hw_revision(void)
{
#ifdef CONFIG_SHARP_SMEM_CUST
	sharp_smem_common_type *p_sharp_smem_common_type;

	p_sharp_smem_common_type = sh_smem_get_common_address();
	if( p_sharp_smem_common_type != 0 )
	{
		return p_sharp_smem_common_type->sh_hw_revision;
	}else{
		return 0xFF;
	}
#else /* CONFIG_SHARP_SMEM_CUST */
	return 0xFF;
#endif /* CONFIG_SHARP_SMEM_CUST */
}
EXPORT_SYMBOL(sh_boot_get_hw_revision);

unsigned long sh_boot_get_bootmode(void)
{
#ifdef CONFIG_SHARP_SMEM_CUST
	sharp_smem_common_type *p_sharp_smem_common_type;

	p_sharp_smem_common_type  = sh_smem_get_common_address();
	if( p_sharp_smem_common_type != 0 )
	{
		return p_sharp_smem_common_type->sh_boot_mode;
	}else{
		return 0;
	}
#else /* CONFIG_SHARP_SMEM_CUST */
	return 0;
#endif /* CONFIG_SHARP_SMEM_CUST */
}
EXPORT_SYMBOL(sh_boot_get_bootmode);

unsigned char sh_boot_get_handset(void)
{
#ifdef CONFIG_SHARP_SMEM_CUST
    sharp_smem_common_type *p_sharp_smem_common_type;

    p_sharp_smem_common_type  = sh_smem_get_common_address();
	if( p_sharp_smem_common_type != 0 )
	{
	return p_sharp_smem_common_type->sh_hw_handset;
	}else{
		return 1;
	}
#else /* CONFIG_SHARP_SMEM_CUST */
	return 0;
#endif /* CONFIG_SHARP_SMEM_CUST */
}
EXPORT_SYMBOL(sh_boot_get_handset);

static void sh_boot_set_bootmode(unsigned short mode)
{
#ifdef CONFIG_SHARP_SMEM_CUST
	sharp_smem_common_type *p_sharp_smem_common_type;

	p_sharp_smem_common_type = sh_smem_get_common_address();
	if (p_sharp_smem_common_type != 0)
		p_sharp_smem_common_type->sh_boot_mode = mode;
#endif /* CONFIG_SHARP_SMEM_CUST */
}

MODULE_DESCRIPTION("SH Boot Manager");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.0");

/*=============================================================================

FUNCTION sh_boot_get_bootmode_to_user

DESCRIPTION

DEPENDENCIES
  None

RETURN VALUE

SIDE EFFECTS
  None

NOTE
  None

=============================================================================*/
static int sh_boot_get_bootmode_to_user(char *buffer, const struct kernel_param *kp)
{
	int ret = 0;

	boot_mode = sh_boot_get_bootmode();
	ret = param_get_int(buffer, kp);

	return ret;
}

/*=============================================================================

FUNCTION sh_boot_set_bootmode_from_user

DESCRIPTION

DEPENDENCIES
  None

RETURN VALUE

SIDE EFFECTS
  None

NOTE
  None

=============================================================================*/
static int sh_boot_set_bootmode_from_user(const char *buffer, const struct kernel_param *kp)
{
	int ret = -EINVAL;
	unsigned long mode;

	if (kstrtoul(buffer, 0, &mode) == 0) {
		switch (mode) {
		case SH_BOOT_O_C:
		case SH_BOOT_NORMAL:
			boot_mode = mode;
			sh_boot_set_bootmode(boot_mode);
			ret = 0;
			break;
		default:
			break;
		}
	}

	return ret;
}
module_param_cb(boot_mode, &param_ops_bootmode, &boot_mode, 0644);

/*=============================================================================

FUNCTION sh_boot_get_handset_to_user

DESCRIPTION

DEPENDENCIES
  None

RETURN VALUE

SIDE EFFECTS
  None

NOTE
  None

=============================================================================*/
static int sh_boot_get_handset_to_user(char *buffer, const struct kernel_param *kp)
{
	int ret = 0;

	handset = sh_boot_get_handset();
	ret = param_get_int(buffer, kp);

	return ret;
}
module_param_cb(handset, &param_ops_handset, &handset, 0644);
