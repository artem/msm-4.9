/* include/sharp/shaudio_wcd934x_notifier.h  (Shaudio wcd934x Driver)
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
#ifndef SHAUDIO_WCD934X_NOTIFIER_H
#define SHAUDIO_WCD934X_NOTIFIER_H

enum wcd934x_notifier_cmd {

  WCD934X_NOTIFIER_MSM_HEADSET_HP_STATE = 1,
  WCD934X_NOTIFIER_MSM_HEADSET_BU_STATE,
  WCD934X_NOTIFIER_DIAG_CODEC_SET_BIAS_MODE,

};

#endif /* SHAUDIO_WCD934X_NOTIFIER_H */
