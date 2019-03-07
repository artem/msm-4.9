/* include/sharp/shaudio_tfa98xx.h  (Shaudio tfa98xx Driver)
 *
 * Copyright (C) 2014 SHARP CORPORATION
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

#ifndef SHAUDIO_TFA98XX_H
#define SHAUDIO_TFA98XX_H

int register_tfa98xx_shub_api_notifier(struct notifier_block *nb);
int unregister_tfa98xx_shub_api_notifier(struct notifier_block *nb);

#endif /* SHAUDIO_TFA98XX_H */
