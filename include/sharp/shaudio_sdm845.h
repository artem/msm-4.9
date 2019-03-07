/* include/sharp/shaudio_msm8998.h  (Shaudio msm8998 Driver)
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

#ifndef SHAUDIO_SDM845_H
#define SHAUDIO_SDM845_H

int register_tfa98xx_snd_notifier(struct notifier_block *nb);
int unregister_tfa98xx_snd_notifier(struct notifier_block *nb);
int snd_tfa98xx_proxy_enable_cal_bit(int func);
int snd_tfa98xx_proxy_check_mtpex(int *val);
int snd_tfa98xx_proxy_check_impedance(int *val);

int register_tfa98xx_sdm845_notifier(struct notifier_block *nb);
int unregister_tfa98xx_sdm845_notifier(struct notifier_block *nb);

int register_wcd934x_snd_notifier(struct notifier_block *nb);
int unregister_wcd934x_snd_notifier(struct notifier_block *nb);
int snd_wcd934x_proxy_msm_headset_hp_state(void);
int snd_wcd934x_proxy_msm_headset_bu_state(void);
int snd_wcd934x_proxy_diag_codec_set_bias_mode(int mode);

#endif /* SHAUDIO_MSM8998_H */
