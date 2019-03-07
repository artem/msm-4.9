/* drivers/sharp/nfc/nfc.h (NFC Common Header)
 *
 * Copyright (C) 2015 SHARP CORPORATION
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

#ifndef NFC_H
#define NFC_H

#ifdef CONFIG_ANDROID_ENGINEERING
/* DEBUG_LOG */
#if 1
#define DEBUG_NFC_DRV
#endif
/* SWITCH_DEBUG_PIN */
#if 0
#define DEBUG_NFC_DRV_PIN
#endif
#endif	// CONFIG_ANDROID_ENGINEERING

#ifdef DEBUG_NFC_DRV
#include <linux/moduleparam.h>

static int debug_flg = 0;
module_param(debug_flg, int, 0600);

#define NFC_DRV_DBG_LOG(fmt, args...){								\
	if(debug_flg){													\
		printk(KERN_INFO "[NFC][%s]" fmt "\n", __func__, ## args);	\
	}																\
}
#define	DBGLOG_TRACE(fmt, args...){									\
	if(debug_flg){													\
		printk(KERN_INFO  "[NFC]" fmt, ## args);					\
	}																\
}
#define	DBGLOG_NCI(fmt, args...){									\
	if(debug_flg){													\
		printk(           "[NFC]" fmt, ## args);					\
	}																\
}
#define	DBGLOG_DETAIL(fmt, args...){								\
	if(debug_flg){													\
		printk(KERN_DEBUG "[NFC]" fmt, ## args);					\
	}																\
}

#else	// DEBUG_NFC_DRV
#define NFC_DRV_DBG_LOG(fmt, args...)
#define	DBGLOG_TRACE(fmt, args...)
#define	DBGLOG_NCI(fmt, args...)
#define	DBGLOG_DETAIL(fmt, args...)
#endif	// DEBUG_NFC_DRV

#define NFC_DRV_ERR_LOG(fmt, args...) printk(KERN_ERR  "[NFC][%s]ERR " fmt "\n", __func__, ## args)
#define DBGLOG_ERR(fmt, args...) printk(KERN_ERR  "[NFC][%s]ERR " fmt "", __func__, ## args)

#define CONFIG_NFC_CXD224X_RST
#if 0	// for TIMPANI
#define CONFIG_NFC_CXD224X_TST1
#endif	// for TIMPANI

#if defined(CONFIG_NFC_CXD224X_RST) || defined(CONFIG_NFC_CXD224X_RST_MODULE)
void cxd224x_dev_reset(void);
#endif

#endif /* NFC_H */
