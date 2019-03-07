/* drivers/sharp/sh_wlan_factory.c
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

#include <linux/module.h>
#include <soc/qcom/sh_smem.h>

#ifdef CONFIG_SHARP_BOOT
#include <soc/qcom/sharp/sh_boot_manager.h>
/* HW Revision */
#define SH_WLAN_HW_VERSION_ES_0			0
#define SH_WLAN_HW_VERSION_ES_1			1
#define SH_WLAN_HW_VERSION_RESERVE1		2
#define SH_WLAN_HW_VERSION_PP_1			3
#define SH_WLAN_HW_VERSION_PP_1_5		4
#define SH_WLAN_HW_VERSION_PP_2			5
#define SH_WLAN_HW_VERSION_RESERVE2		6
#define SH_WLAN_HW_VERSION_MP			7
#define SH_WLAN_HW_VERSION_UNKNOWN		0xFF

/* NV Kind */
#define SH_WLAN_NV_VER1					1
#define SH_WLAN_NV_VER2					2

/* HW Rev  */
static int hw_rev;							/* 1 or 2 */
module_param(hw_rev, int, S_IRUGO);
MODULE_PARM_DESC(hw_rev, "Hardware Revision");
#endif /* CONFIG_SHARP_BOOT */

/* NV Level  */
static char *nv_tx_level = NULL;			/* NV3, NV16 Common variable */
static char nv_tx_level_buf[10];
module_param(nv_tx_level, charp, S_IRUGO);
MODULE_PARM_DESC(nv_tx_level, "NV TX level (NV switching info)");

#if defined(CONFIG_ARCH_DIO)
/* NV3 define table */
const char NV3_suffix_table[] = "HML";

#elif defined(CONFIG_ARCH_JOHNNY)
/* NV3 define table */
const char NV3_suffix_table[] = "HML";

#elif defined(CONFIG_ARCH_PUCCI)
/* NV16 define table */
const char *NV16_suffix_table[] = {
			/* File Name          Value */
			"2L5L_2L5L",		/*   0  */
			"2L5L_2L5H",		/*   1  */
			"2L5L_2H5L",		/*   2  */
			"2L5L_2H5H",		/*   3  */
			"2L5H_2L5L",		/*   4  */
			"2L5H_2L5H",		/*   5  */
			"2L5H_2H5L",		/*   6  */
			"2L5H_2H5H",		/*   7  */
			"2H5L_2L5L",		/*   8  */
			"2H5L_2L5H",		/*   9  */
			"2H5L_2H5L",		/*  10  */
			"2H5L_2H5H",		/*  11  */
			"2H5H_2L5L",		/*  12  */
			"2H5H_2L5H",		/*  13  */
			"2H5H_2H5L",		/*  14  */
			"2H5H_2H5H"			/*  15  */
		};
#else
/* NV define Default */
const char NV3_suffix_table[] = "HML";
#endif


/* wlanmac define */
static char *wlanmac_from_smem = NULL;
static char wlanmac_from_smem_buf[18];
module_param(wlanmac_from_smem, charp, S_IRUGO);
MODULE_PARM_DESC(wlanmac_from_smem, "store the wlan mac");


#ifdef CONFIG_SHARP_BOOT
/*
 * hw_rev_search()
 * rev			: Rev Version
 * return		: NV Kind
 */
int hw_rev_search(unsigned short rev )
{
	int ret = SH_WLAN_NV_VER1;

	switch(rev){
	case SH_WLAN_HW_VERSION_ES_0 		:
	case SH_WLAN_HW_VERSION_ES_1 		:
	case SH_WLAN_HW_VERSION_PP_1		:
	case SH_WLAN_HW_VERSION_PP_1_5		:
		ret = SH_WLAN_NV_VER1;
		break;

	case SH_WLAN_HW_VERSION_PP_2		:
	case SH_WLAN_HW_VERSION_MP			:
		ret = SH_WLAN_NV_VER2;
		break;

	case SH_WLAN_HW_VERSION_RESERVE1	:
	case SH_WLAN_HW_VERSION_RESERVE2	:
	case SH_WLAN_HW_VERSION_UNKNOWN		:
	default :
		/* SH_WLAN_NV_VER1 * */
		break;
	}
	return ret;
}

/*
 * wcnss_get_nvlevel()
 * return		: level
 */
static void wcnss_get_nvlevel(void)
{
	typedef struct {
		char version[4];
			signed char nv_switch_2_4G;
			signed char nv_switch_5G;
			char padding;
		char nv_data_count;
	} TS_SHDIAG_WIFI_INFO;

	sharp_smem_common_type *SMemCommAdrP;
	TS_SHDIAG_WIFI_INFO *WifiInfo;
	int val=0;

	SMemCommAdrP = sh_smem_get_common_address();

	if (SMemCommAdrP) {
		WifiInfo = (TS_SHDIAG_WIFI_INFO *)(SMemCommAdrP->shdarea_WlanNVSwitch);
		val = (int)WifiInfo->nv_switch_2_4G;
	} else {
		pr_err("%s: Cannot read NV level from smem.\n", __FUNCTION__);
	}

	pr_info("%s: read NV level:%d\n", __FUNCTION__, val);

#if defined(CONFIG_ARCH_DIO)
	/* NV3 define */
	if ( val < -1  || 1 < val ) {
		pr_err("%s: NV level is invalid value(%d).\n", __FUNCTION__, val);
		val = 0;
	}
	nv_tx_level_buf[0] = NV3_suffix_table[val + 1];

#elif defined(CONFIG_ARCH_JOHNNY)
	/* NV3 define */
	if ( val < -1  || 1 < val ) {
		pr_err("%s: NV level is invalid value(%d).\n", __FUNCTION__, val);
		val = 0;
	}
	nv_tx_level_buf[0] = NV3_suffix_table[val + 1];

#elif defined(CONFIG_ARCH_PUCCI)
	/* NV16 define */
	if ( val  < 0 ||  15 < val ) {
		pr_err("%s: NV level C2 invalid value(%d).\n", __FUNCTION__, val);
		val = 0;
	}
	memcpy(nv_tx_level_buf, NV16_suffix_table[val], sizeof(nv_tx_level_buf));

#else
	/* NV define Default */
	pr_err("%s: NV level is define Default(%d).\n", __FUNCTION__, val);
	nv_tx_level_buf[0] = NV3_suffix_table[0];
#endif /* CONFIG_ARCH_DIO */

}
#endif /* CONFIG_SHARP_BOOT */

/*
 * sh_wlan_fac_init()
 */
static int __init sh_wlan_fac_init(void){
	sharp_smem_common_type *SMemCommAdrP;

	printk(KERN_INFO "%s: enter\n", __FUNCTION__);
	memset(wlanmac_from_smem_buf, 0, 18);

	/* Mac Address Read */
	wlanmac_from_smem = wlanmac_from_smem_buf;
	SMemCommAdrP = sh_smem_get_common_address();
	if(SMemCommAdrP != NULL){
		snprintf(wlanmac_from_smem_buf, 13, "%02x%02x%02x%02x%02x%02x",
			(unsigned char)SMemCommAdrP->shdarea_WlanMacAddress[0],
			(unsigned char)SMemCommAdrP->shdarea_WlanMacAddress[1],
			(unsigned char)SMemCommAdrP->shdarea_WlanMacAddress[2],
			(unsigned char)SMemCommAdrP->shdarea_WlanMacAddress[3],
			(unsigned char)SMemCommAdrP->shdarea_WlanMacAddress[4],
			(unsigned char)SMemCommAdrP->shdarea_WlanMacAddress[5]);
		printk(KERN_INFO "%s: MAC address is %s\n", __FUNCTION__, wlanmac_from_smem_buf);
	}else{
		snprintf(wlanmac_from_smem_buf, 13, "000000000000");
		printk(KERN_ERR "%s: SMemCommAdrP is NULL, MAC address is set to all zero\n", __FUNCTION__);
	}

#ifdef CONFIG_SHARP_BOOT
	/* NV Buf Initialize  */
	memset(nv_tx_level_buf, 0, sizeof(nv_tx_level_buf));
	nv_tx_level = nv_tx_level_buf;

	/* Rev Search */
	hw_rev = hw_rev_search(sh_boot_get_hw_revision());
	pr_debug("%s: hw_rev = %d\n", __func__, hw_rev);

	/* Calibration Search */
	wcnss_get_nvlevel();

	/* Set Strings Search */
	pr_debug("%s: level = %s\n", __func__, nv_tx_level_buf);

#else

  #if defined(CONFIG_ARCH_DIO)
	/* NV3 define */
	memset(nv_tx_level_buf, 'L', 1);
  #elif defined(CONFIG_ARCH_JOHNNY)
	/* NV3 define */
	memset(nv_tx_level_buf, 'L', 1);
  #elif defined(CONFIG_ARCH_PUCCI)
	/* NV16 define */
	memcpy(nv_tx_level_buf, NV16_suffix_table[0], sizeof(nv_tx_level_buf));
  #else
	/* NV define default */
	memset(nv_tx_level_buf, 'L', 1);
  #endif
	nv_tx_level = nv_tx_level_buf;
	pr_debug("%s: SHARP_BOOT Undefined. level = L or 2L5L_2L5L fixed\n", __func__);

#endif
	printk(KERN_INFO "%s: leave\n", __FUNCTION__);
	return 0;
}

static void __exit sh_wlan_fac_exit(void){
}

module_init(sh_wlan_fac_init);
module_exit(sh_wlan_fac_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_DESCRIPTION("SHARP WLAN FACTORY MODULE");
