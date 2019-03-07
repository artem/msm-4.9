/* drivers/sharp/shsys/sh_smem.c
 *
 * Copyright (C) 2015 Sharp Corporation
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
 
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>

#include <soc/qcom/smem.h>
#include <soc/qcom/sh_smem.h>

static int sh_smem_get_softupdate_flg_to_user(char *buffer, const struct kernel_param *kp);

static sharp_smem_common_type *p_sharp_smem_common_type = NULL;
static unsigned char softupdate_flg = 0;
static struct kernel_param_ops param_ops_softupdate_flg = {
	.get = sh_smem_get_softupdate_flg_to_user,
};

/*=============================================================================

FUNCTION sh_smem_get_common_address

=============================================================================*/
sharp_smem_common_type *sh_smem_get_common_address( void )
{
    if (p_sharp_smem_common_type == NULL) {
        p_sharp_smem_common_type = smem_alloc(SMEM_ID_VENDOR0, SH_SMEM_COMMON_SIZE,0,0);
    }
    
    return p_sharp_smem_common_type;
}
EXPORT_SYMBOL(sh_smem_get_common_address);

/*=============================================================================

FUNCTION sh_smem_get_softupdate_flg

DESCRIPTION

DEPENDENCIES
  None

RETURN VALUE

SIDE EFFECTS
  None

NOTE
  None

=============================================================================*/
unsigned char sh_smem_get_softupdate_flg( void )
{
    if (p_sharp_smem_common_type == NULL) {
        p_sharp_smem_common_type = sh_smem_get_common_address();
    }
    
    return p_sharp_smem_common_type->shusb_softupdate_mode_flag;
}
EXPORT_SYMBOL(sh_smem_get_softupdate_flg);

/*=============================================================================

FUNCTION sh_smem_get_softupdate_flg_to_user

DESCRIPTION

DEPENDENCIES
  None

RETURN VALUE

SIDE EFFECTS
  None

NOTE
  None

=============================================================================*/
static int sh_smem_get_softupdate_flg_to_user(char *buffer, const struct kernel_param *kp)
{
    int ret = 0;

    softupdate_flg = sh_smem_get_softupdate_flg();
    ret = param_get_int(buffer, kp);

    return ret;
}
module_param_cb(softupdate_flg, &param_ops_softupdate_flg, &softupdate_flg, 0644);

/*=============================================================================

FUNCTION sh_smem_get_fota_boot_mode

DESCRIPTION

DEPENDENCIES
  None

RETURN VALUE

SIDE EFFECTS
  None

NOTE
  None

=============================================================================*/
unsigned long sh_smem_get_fota_boot_mode( void )
{
    if (p_sharp_smem_common_type == NULL) {
        p_sharp_smem_common_type = sh_smem_get_common_address();
    }
    
    return p_sharp_smem_common_type->fota_boot_mode;
}
EXPORT_SYMBOL(sh_smem_get_fota_boot_mode);

/*=============================================================================

FUNCTION sh_smem_get_pvs_flg

DESCRIPTION

DEPENDENCIES
  None

RETURN VALUE

SIDE EFFECTS
  None

NOTE
  None

=============================================================================*/
unsigned char sh_smem_get_pvs_flg( void )
{
    if (p_sharp_smem_common_type == NULL) {
        p_sharp_smem_common_type = sh_smem_get_common_address();
    }
    
    return p_sharp_smem_common_type->sh_pvs_flg;
}
EXPORT_SYMBOL(sh_smem_get_pvs_flg);

