/* drivers/input/touchscreen/shtps/sy3000/shtps_rmi_devctl.h
 *
 * Copyright (c) 2017, Sharp. All rights reserved.
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

#ifndef __SHTPS_RMI_DEVCTL_H__
#define __SHTPS_RMI_DEVCTL_H__

#include <linux/pinctrl/pinconf-generic.h>

/* -----------------------------------------------------------------------------------
 */
#define SHTPS_VREG_ID_VDDH				"shtps_rmi,vddh"
#define SHTPS_VREG_ID_VBUS				"shtps_rmi,vbus"

/* -----------------------------------------------------------------------------------
 */
int shtps_device_setup(struct device* dev, int rst);
void shtps_device_teardown(struct device* dev, int rst);
void shtps_device_reset(int rst);
void shtps_device_sleep(struct device* dev);
void shtps_device_wakeup(struct device* dev);

#if defined(SHTPS_POWER_OFF_IN_SLEEP_ENABLE)
struct shtps_power_on_off_info{
	struct device *dev;
	int rst_pin;
	int tp_i2c_sda;
	int tp_i2c_scl;
	int tp_int;
	struct pinctrl				*pinctrl;
	struct pinctrl_state		*panel_active;
	struct pinctrl_state		*panel_standby;
	struct pinctrl_state		*int_active;
	struct pinctrl_state		*int_standby;
};
void shtps_device_power_on_off_init(struct shtps_power_on_off_info *shtps_power_on_off_info);
void shtps_device_power_off(void);
void shtps_device_power_on(void);
#endif /* SHTPS_POWER_OFF_IN_SLEEP_ENABLE */

#if defined(SHTPS_DYNAMIC_RESET_ESD_ENABLE)
struct shtps_esd_regulator_info{
	struct device *dev;
	int rst_pin;
	int tp_spi_mosi;
	int tp_spi_miso;
	int tp_spi_cs_n;
	int tp_spi_clk;
	int tp_int;
	struct pinctrl				*pinctrl;
	struct pinctrl_state		*esd_spi;
	struct pinctrl_state		*esd_gpio;
	struct pinctrl_state		*int_active;
	struct pinctrl_state		*int_standby;
};

void shtps_esd_regulator_reset(void);
void shtps_esd_regulator_get(void);
void shtps_esd_regulator_put(void);
void shtps_esd_regulator_init(struct shtps_esd_regulator_info *esd_regulator_info);
void shtps_esd_regulator_remove(void);
#endif /* SHTPS_DYNAMIC_RESET_ESD_ENABLE */

#endif /* __SHTPS_RMI_DEVCTL_H__ */
