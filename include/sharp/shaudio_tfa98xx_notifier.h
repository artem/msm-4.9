/* include/sharp/shaudio_tfa98xx_notifier.h  (Shaudio tfa98xx Driver)
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
#ifndef SHAUDIO_TFA98XX_NOTIFIER_H
#define SHAUDIO_TFA98XX_NOTIFIER_H

enum tfa98xx_notifier_cmd {

  TFA98XX_NOTIFIER_DETECTED = 1,
  TFA98XX_NOTIFIER_LATER_PROBE,
  TFA98XX_NOTIFIER_ENABLE_CAL_BIT,
  TFA98XX_NOTIFIER_CHECK_MTPEX,
  TFA98XX_NOTIFIER_CHECK_IMPEDANCE,

};

#endif /* SHAUDIO_TFA98XX_NOTIFIER_H */
