/* drivers/input/touchscreen/shtps/sy3000/shtps_rmi_devctl.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

#include <linux/input/shtps_dev.h>
#include "shtps_param_extern.h"
#include "shtps_rmi_devctl.h"
#include "shtps_log.h"

/* -----------------------------------------------------------------------------------
 */
#define SHTPS_HWRESET_TIME_US					1
#define SHTPS_HWRESET_AFTER_TIME_MS				1
#define SHTPS_HWRESET_WAIT_MS					290

#if defined(SHTPS_POWER_OFF_IN_SLEEP_ENABLE)
static struct shtps_power_on_off_info shtps_devctl_power_on_off_info = {0};
static struct shtps_power_on_off_info *shtps_devctl_power_on_off_info_p = NULL;
#endif /* SHTPS_POWER_OFF_IN_SLEEP_ENABLE */

#if defined(SHTPS_DYNAMIC_RESET_ESD_ENABLE)
static struct regulator *shtps_esd = NULL;
static struct regulator *shtps_esd_vddh = NULL;
static struct shtps_esd_regulator_info shtps_devctl_esd_regulator_info = {0};
static struct shtps_esd_regulator_info *shtps_devctl_esd_regulator_info_p = NULL;
#endif /* SHTPS_DYNAMIC_RESET_ESD_ENABLE */

/* -----------------------------------------------------------------------------------
 */
int shtps_device_setup(struct device* dev, int rst)
{
	int ret = 0;
#if defined(SHTPS_POWER_ON_CONTROL_ENABLE)
	struct regulator *reg_vddh = NULL;
	struct regulator *reg_vbus = NULL;
#endif /* SHTPS_POWER_ON_CONTROL_ENABLE */

	ret = gpio_request(rst, "shtps_rst");
	if (ret) {
		SHTPS_LOG_ERR_PRINT("%s() request gpio failed (rst)\n", __func__);
		return ret;
	}

#if defined(SHTPS_POWER_ON_CONTROL_ENABLE)
	reg_vddh = devm_regulator_get(dev, SHTPS_VREG_ID_VDDH);

	if (IS_ERR(reg_vddh)) {
		SHTPS_LOG_ERR_PRINT("%s() regulator_init vddh Err\n", __func__);
		reg_vddh = NULL;
		return -1;
	}

	ret = regulator_enable(reg_vddh);
	if (ret){
		SHTPS_LOG_ERR_PRINT("%s() regulator_enable vddh Err[%d]\n", __func__, ret);
	}

	regulator_put(reg_vddh);
	reg_vddh = NULL;

	udelay(SHTPS_POWER_VDDH_WAIT_US);

	reg_vbus = devm_regulator_get(dev, SHTPS_VREG_ID_VBUS);

	if (IS_ERR(reg_vbus)) {
		SHTPS_LOG_ERR_PRINT("%s() regulator_init vbus Err\n", __func__);
		reg_vbus = NULL;
		return -1;
	}

	ret = regulator_enable(reg_vbus);
	if (ret){
		SHTPS_LOG_ERR_PRINT("%s() regulator_enable vbus Err[%d]\n", __func__, ret);
	}
#if 0
	ret = regulator_set_mode(reg_vbus, REGULATOR_MODE_NORMAL);
	if (ret != 0) {
		SHTPS_LOG_ERR_PRINT("%s() regulator_set_mode vbus Err[%d]\n", __func__, ret);
	}
#endif
	regulator_put(reg_vbus);
	reg_vbus = NULL;

	udelay(SHTPS_POWER_VBUS_WAIT_US);
#endif /* SHTPS_POWER_ON_CONTROL_ENABLE */

    return 0;
}

void shtps_device_teardown(struct device* dev, int rst)
{
    gpio_free(rst);
}

void shtps_device_reset(int rst)
{
	gpio_set_value(rst, 0);
	mb();
	udelay(SHTPS_HWRESET_TIME_US);

	gpio_set_value(rst, 1);
	mdelay(SHTPS_HWRESET_AFTER_TIME_MS);
}
#if defined(SHTPS_POWER_OFF_IN_SLEEP_ENABLE)
void shtps_device_power_on_off_init(struct shtps_power_on_off_info *shtps_power_on_off_info)
{
	SHTPS_LOG_FUNC_CALL();

	if( shtps_power_on_off_info == NULL ||
		shtps_power_on_off_info->dev == NULL ||
		IS_ERR_OR_NULL(shtps_power_on_off_info->pinctrl) ||
		IS_ERR_OR_NULL(shtps_power_on_off_info->panel_active) ||
		IS_ERR_OR_NULL(shtps_power_on_off_info->panel_standby) ||
		IS_ERR_OR_NULL(shtps_power_on_off_info->int_active) ||
		IS_ERR_OR_NULL(shtps_power_on_off_info->int_standby) )
	{
		SHTPS_LOG_ERR_PRINT("shtps_power_on_off_info param init error\n");
		return;
	}
	memcpy(&shtps_devctl_power_on_off_info, shtps_power_on_off_info, sizeof(shtps_devctl_power_on_off_info));
	shtps_devctl_power_on_off_info_p = &shtps_devctl_power_on_off_info;
}

void shtps_device_power_off(void)
{
	int ret;
	int enabled = 0;
	struct regulator *reg_vddh = NULL;
	struct regulator *reg_vbus = NULL;

	SHTPS_LOG_FUNC_CALL();

	if(shtps_devctl_power_on_off_info_p == NULL){
		return;
	}

	/* =========== reset =========== */
	gpio_set_value(shtps_devctl_power_on_off_info_p->rst_pin, 0);


#if 0
	/* =========== panel =========== */
	gpio_request(shtps_devctl_power_on_off_info_p->tp_i2c_sda, "TP_I2C_SDA");
	gpio_request(shtps_devctl_power_on_off_info_p->tp_i2c_scl, "TP_I2C_SCL");

	SHTPS_LOG_DBG_PRINT("pinctrl_select_state(standby)\n");
	pinctrl_select_state(shtps_devctl_power_on_off_info_p->pinctrl, shtps_devctl_power_on_off_info_p->panel_standby);

	gpio_free(shtps_devctl_power_on_off_info_p->tp_i2c_sda);
	gpio_free(shtps_devctl_power_on_off_info_p->tp_i2c_scl);

	pinctrl_free_state(shtps_devctl_power_on_off_info_p->pinctrl);
#endif

	/* =========== int =========== */
	gpio_request(shtps_devctl_power_on_off_info_p->tp_int, "TP_INT");

	SHTPS_LOG_DBG_PRINT("pinctrl_select_state(tp_int standby)\n");
	pinctrl_select_state(shtps_devctl_power_on_off_info_p->pinctrl, shtps_devctl_power_on_off_info_p->int_standby);

	gpio_free(shtps_devctl_power_on_off_info_p->tp_int);


	/* =========== power vbus =========== */
	reg_vbus = devm_regulator_get(shtps_devctl_power_on_off_info_p->dev, SHTPS_VREG_ID_VBUS);

	if (IS_ERR(reg_vbus)) {
		SHTPS_LOG_ERR_PRINT("devm_regulator_get(reg_vbus) Err\n");
		reg_vbus = NULL;
		return;
	}

	enabled = regulator_is_enabled(reg_vbus);
	if(enabled == 1){
		SHTPS_LOG_DBG_PRINT("regulator_disable(reg_vbus)\n");
		ret = regulator_disable(reg_vbus);
		if (ret){
			SHTPS_LOG_ERR_PRINT("regulator_disable(reg_vbus) Err[%d]\n",ret);
		}
	}

	regulator_put(reg_vbus);
	reg_vbus = NULL;

	/* =========== power vddh =========== */
	reg_vddh = devm_regulator_get(shtps_devctl_power_on_off_info_p->dev, SHTPS_VREG_ID_VDDH);

	if (IS_ERR(reg_vddh)) {
		SHTPS_LOG_ERR_PRINT("devm_regulator_get(reg_vddh) Err\n");
		reg_vddh = NULL;
		return;
	}

	enabled = regulator_is_enabled(reg_vddh);
	if(enabled == 1){
		SHTPS_LOG_DBG_PRINT("regulator_disable(reg_vddh)\n");
		ret = regulator_disable(reg_vddh);
		if (ret){
			SHTPS_LOG_ERR_PRINT("regulator_disable(reg_vddh) Err[%d]\n",ret);
		}
	}

	regulator_put(reg_vddh);
	reg_vddh = NULL;

	msleep(SHTPS_POWER_VDDH_OFF_AFTER_MS);

	return;
}

void shtps_device_power_on(void)
{
	int ret;
	int enabled = 0;
	struct regulator *reg_vddh = NULL;
	struct regulator *reg_vbus = NULL;

	SHTPS_LOG_FUNC_CALL();

	if(shtps_devctl_power_on_off_info_p == NULL){
		return;
	}

	/* =========== power vddh =========== */
	reg_vddh = devm_regulator_get(shtps_devctl_power_on_off_info_p->dev, SHTPS_VREG_ID_VDDH);

	if (IS_ERR(reg_vddh)) {
		SHTPS_LOG_ERR_PRINT("devm_regulator_get(reg_vddh) Err\n");
		reg_vddh = NULL;
		return;
	}

	enabled = regulator_is_enabled(reg_vddh);
	if(enabled == 0){
		SHTPS_LOG_DBG_PRINT("regulator_enable(reg_vddh)\n");
		ret = regulator_enable(reg_vddh);
		if (ret){
			SHTPS_LOG_ERR_PRINT("regulator_enable(reg_vddh) Err[%d]\n",ret);
		}
	}

	regulator_put(reg_vddh);
	reg_vddh = NULL;

	udelay(SHTPS_POWER_VDDH_WAIT_US);

	/* =========== power vbus =========== */
	reg_vbus = devm_regulator_get(shtps_devctl_power_on_off_info_p->dev, SHTPS_VREG_ID_VBUS);

	if (IS_ERR(reg_vbus)) {
		SHTPS_LOG_ERR_PRINT("devm_regulator_get(reg_vbus) Err\n");
		reg_vbus = NULL;
		return;
	}

	enabled = regulator_is_enabled(reg_vbus);
	if(enabled == 0){
		SHTPS_LOG_DBG_PRINT("regulator_enable(reg_vbus)\n");
		ret = regulator_enable(reg_vbus);
		if (ret){
			SHTPS_LOG_ERR_PRINT("regulator_enable(reg_vbus) Err[%d]\n",ret);
		}
	}

#if 0
	ret = regulator_set_mode(reg_vbus, REGULATOR_MODE_NORMAL);
	if (ret != 0) {
		SHTPS_LOG_ERR_PRINT("regulator_set_mode Err[%d]\n",ret);
	}
#endif

	regulator_put(reg_vbus);
	reg_vbus = NULL;

	udelay(SHTPS_POWER_VBUS_WAIT_US);


	/* =========== int =========== */
	gpio_request(shtps_devctl_power_on_off_info_p->tp_int, "TP_INT");

	SHTPS_LOG_DBG_PRINT("pinctrl_select_state(tp_int active)\n");
	pinctrl_select_state(shtps_devctl_power_on_off_info_p->pinctrl, shtps_devctl_power_on_off_info_p->int_active);

	gpio_free(shtps_devctl_power_on_off_info_p->tp_int);


#if 0
	/* =========== panel =========== */
	gpio_request(shtps_devctl_power_on_off_info_p->tp_i2c_sda, "TP_I2C_SDA");
	gpio_request(shtps_devctl_power_on_off_info_p->tp_i2c_scl, "TP_I2C_SCL");

	SHTPS_LOG_DBG_PRINT("pinctrl_select_state(active)\n");
	pinctrl_select_state(shtps_devctl_power_on_off_info_p->pinctrl, shtps_devctl_power_on_off_info_p->panel_active);

	gpio_free(shtps_devctl_power_on_off_info_p->tp_i2c_sda);
	gpio_free(shtps_devctl_power_on_off_info_p->tp_i2c_scl);

	pinctrl_free_state(shtps_devctl_power_on_off_info_p->pinctrl);
#endif

	return;
}
#endif /* SHTPS_POWER_OFF_IN_SLEEP_ENABLE */

void shtps_device_sleep(struct device* dev)
{
	int ret = 0;
	int enabled = 0;
	struct regulator *reg;

	reg = devm_regulator_get(dev, SHTPS_VREG_ID_VDDH);
	if (IS_ERR(reg)) {
		pr_err("Unable to get %s regulator\n", SHTPS_VREG_ID_VDDH);
		return;
	}

	enabled = regulator_is_enabled(reg);
	if(enabled == 0){
		ret = regulator_enable(reg);
		enabled = regulator_is_enabled(reg);
	}

	if (enabled){
		ret = regulator_set_mode(reg, REGULATOR_MODE_IDLE);
		if (ret != 0) {
			pr_err("Unable to set mode. ret[%d]\n", ret);
		}
	}else{
//		WARN_ON(!enabled);
		SHTPS_LOG_ERR_PRINT("%s regulator is not enabled\n", SHTPS_VREG_ID_VDDH);
	}

	if(ret != 0) {
		pr_err("regulator_set_mode fail, ret=%d, mode=%d\n", ret, REGULATOR_MODE_IDLE);
	}

	regulator_put(reg);
}

void shtps_device_wakeup(struct device* dev)
{
	int ret = 0;
	int enabled = 0;
	struct regulator *reg;

	reg = devm_regulator_get(dev, SHTPS_VREG_ID_VDDH);
	if (IS_ERR(reg)) {
		pr_err("Unable to get %s regulator\n", SHTPS_VREG_ID_VDDH);
		return;
	}

	enabled = regulator_is_enabled(reg);
	if(enabled == 0){
		ret = regulator_enable(reg);
		enabled = regulator_is_enabled(reg);
	}

	if (enabled){
		ret = regulator_set_mode(reg, REGULATOR_MODE_NORMAL);
		if (ret != 0) {
			pr_err("Unable to set mode. ret[%d]\n", ret);
		}
	}else{
//		WARN_ON(!enabled);
		SHTPS_LOG_ERR_PRINT("%s regulator is not enabled\n", SHTPS_VREG_ID_VDDH);
	}

	if(ret != 0) {
		pr_err("regulator_set_mode fail, ret=%d, mode=%d\n", ret, REGULATOR_MODE_NORMAL);
	}

	regulator_put(reg);
}

#if defined(SHTPS_DYNAMIC_RESET_ESD_ENABLE)
void shtps_esd_regulator_reset(void)
{
	int ret;
	int i;

	SHTPS_LOG_FUNC_CALL();

	if(shtps_esd == NULL || shtps_esd_vddh == NULL || shtps_devctl_esd_regulator_info_p == NULL){
		SHTPS_LOG_ERR_PRINT("shtps_esd_regulator_reset Err shtps_esd=%p shtps_esd_vddh=%p shtps_devctl_esd_regulator_info_p=%p\n", shtps_esd, shtps_esd_vddh, shtps_devctl_esd_regulator_info_p);
		return;
	}

	msleep(100);

	gpio_set_value(shtps_devctl_esd_regulator_info_p->rst_pin, 0);

	gpio_request(shtps_devctl_esd_regulator_info_p->tp_spi_mosi, "TP_SPI_MOSI");
	gpio_request(shtps_devctl_esd_regulator_info_p->tp_spi_miso, "TP_SPI_MISO");
	gpio_request(shtps_devctl_esd_regulator_info_p->tp_spi_cs_n, "TP_SPI_CS_N");
	gpio_request(shtps_devctl_esd_regulator_info_p->tp_spi_clk, "TP_SPI_CLK");

	SHTPS_LOG_DBG_PRINT("pinctrl_select_state(gpio)\n");

	for(i = 0; i < 5; i++){
		ret = pinctrl_select_state(shtps_devctl_esd_regulator_info_p->pinctrl, shtps_devctl_esd_regulator_info_p->esd_gpio);
		if(ret == 0){
			break;
		}

		msleep(100);
		SHTPS_LOG_ERR_PRINT("pinctrl_select_state(gpio) fail. retry(%d/5)\n", (i + 1));
	}

	gpio_request(shtps_devctl_esd_regulator_info_p->tp_int, "TP_INT");

	SHTPS_LOG_DBG_PRINT("pinctrl_select_state(tp_int standby)\n");
	pinctrl_select_state(shtps_devctl_esd_regulator_info_p->pinctrl, shtps_devctl_esd_regulator_info_p->int_standby);

	ret = regulator_is_enabled(shtps_esd_vddh);
	if(ret == 0){
		ret = regulator_enable(shtps_esd_vddh);
		if (ret){
			SHTPS_LOG_ERR_PRINT("regulator vddh enable check Err[%d]\n",ret);
		}
	}

	ret = regulator_is_enabled(shtps_esd);
	if(ret == 0){
		ret = regulator_enable(shtps_esd);
		if (ret){
			SHTPS_LOG_ERR_PRINT("regulator enable check Err[%d]\n",ret);
		}
	}

	SHTPS_LOG_DBG_PRINT("regulator_disable()\n");
	ret = regulator_disable(shtps_esd);
	if (ret){
		SHTPS_LOG_ERR_PRINT("regulator disable Err[%d]\n",ret);
	}
	ret = regulator_disable(shtps_esd_vddh);
	if (ret){
		SHTPS_LOG_ERR_PRINT("regulator vddh disable Err[%d]\n",ret);
	}

	SHTPS_LOG_DBG_PRINT("msleep(%d) for regulator\n", SHTPS_ESD_REGULATOR_RESET_SLEEPTIME);
	msleep(SHTPS_ESD_REGULATOR_RESET_SLEEPTIME);

	SHTPS_LOG_DBG_PRINT("regulator_enable()\n");
	ret = regulator_enable(shtps_esd_vddh);
	if (ret){
		SHTPS_LOG_ERR_PRINT("regulator vddh enable Err[%d]\n",ret);
	}
	udelay(SHTPS_POWER_VDDH_WAIT_US);

	ret = regulator_enable(shtps_esd);
	if (ret){
		SHTPS_LOG_ERR_PRINT("regulator enable Err[%d]\n",ret);
	}
	udelay(SHTPS_POWER_VBUS_WAIT_US);

	SHTPS_LOG_DBG_PRINT("pinctrl_select_state(tp_int active)\n");
	pinctrl_select_state(shtps_devctl_esd_regulator_info_p->pinctrl, shtps_devctl_esd_regulator_info_p->int_active);

	gpio_free(shtps_devctl_esd_regulator_info_p->tp_int);

	SHTPS_LOG_DBG_PRINT("pinctrl_select_state(spi)\n");
	pinctrl_select_state(shtps_devctl_esd_regulator_info_p->pinctrl, shtps_devctl_esd_regulator_info_p->esd_spi);

	gpio_free(shtps_devctl_esd_regulator_info_p->tp_spi_mosi);
	gpio_free(shtps_devctl_esd_regulator_info_p->tp_spi_miso);
	gpio_free(shtps_devctl_esd_regulator_info_p->tp_spi_cs_n);
	gpio_free(shtps_devctl_esd_regulator_info_p->tp_spi_clk);

	pinctrl_free_state(shtps_devctl_esd_regulator_info_p->pinctrl);
}

void shtps_esd_regulator_get(void)
{
	SHTPS_LOG_FUNC_CALL();

	if(shtps_esd != NULL || shtps_esd_vddh != NULL || shtps_devctl_esd_regulator_info_p == NULL){
		SHTPS_LOG_ERR_PRINT("shtps_esd_regulator_get Err shtps_esd=%p shtps_esd_vddh=%p shtps_devctl_esd_regulator_info_p=%p\n", shtps_esd, shtps_esd_vddh, shtps_devctl_esd_regulator_info_p);
		return ;
	}

	shtps_esd = devm_regulator_get(shtps_devctl_esd_regulator_info_p->dev, SHTPS_VREG_ID_VBUS);

	if (IS_ERR(shtps_esd)) {
		SHTPS_LOG_ERR_PRINT("regulator_get Err\n");
		shtps_esd = NULL;
		return;
	}

	shtps_esd_vddh = devm_regulator_get(shtps_devctl_esd_regulator_info_p->dev, SHTPS_VREG_ID_VDDH);

	if (IS_ERR(shtps_esd_vddh)) {
		SHTPS_LOG_ERR_PRINT("regulator_get vddh Err\n");
		regulator_put(shtps_esd);
		shtps_esd = NULL;
		shtps_esd_vddh = NULL;
		return;
	}
}

void shtps_esd_regulator_put(void)
{
	SHTPS_LOG_FUNC_CALL();

	if(shtps_esd == NULL){
		SHTPS_LOG_ERR_PRINT("shtps_esd_regulator_put Err shtps_esd=%p\n", shtps_esd);
	}
	else{
		regulator_put(shtps_esd);
		shtps_esd = NULL;
	}

	if(shtps_esd_vddh == NULL){
		SHTPS_LOG_ERR_PRINT("shtps_esd_regulator_put vddh Err shtps_esd_vddh=%p\n", shtps_esd_vddh);
	}
	else{
		regulator_put(shtps_esd_vddh);
		shtps_esd_vddh = NULL;
	}
}

void shtps_esd_regulator_init(struct shtps_esd_regulator_info *esd_regulator_info)
{
	int ret;

	SHTPS_LOG_FUNC_CALL();

	if(shtps_esd || shtps_esd_vddh)
	{
		return ;
	}

	if( esd_regulator_info == NULL ||
		esd_regulator_info->dev == NULL ||
		IS_ERR_OR_NULL(esd_regulator_info->pinctrl) ||
		IS_ERR_OR_NULL(esd_regulator_info->esd_spi) ||
		IS_ERR_OR_NULL(esd_regulator_info->esd_gpio) )
	{
		SHTPS_LOG_ERR_PRINT("esd_regulator_info param init error\n");
		return ;
	}
	memcpy(&shtps_devctl_esd_regulator_info, esd_regulator_info, sizeof(shtps_devctl_esd_regulator_info));
	shtps_devctl_esd_regulator_info_p = &shtps_devctl_esd_regulator_info;

	shtps_esd_vddh = devm_regulator_get(shtps_devctl_esd_regulator_info_p->dev, SHTPS_VREG_ID_VDDH);

	if (IS_ERR(shtps_esd_vddh)) {
		SHTPS_LOG_ERR_PRINT("regulator_init vddh Err\n");
		shtps_esd_vddh = NULL;
		return;
	}

	ret = regulator_enable(shtps_esd_vddh);
	if (ret){
		SHTPS_LOG_ERR_PRINT("regulator_enable vddh Err[%d]\n",ret);
	}

	regulator_put(shtps_esd_vddh);
	shtps_esd_vddh = NULL;

	shtps_esd = devm_regulator_get(shtps_devctl_esd_regulator_info_p->dev, SHTPS_VREG_ID_VBUS);

	if (IS_ERR(shtps_esd)) {
		SHTPS_LOG_ERR_PRINT("regulator_init Err\n");
		shtps_esd = NULL;
		return;
	}

	ret = regulator_enable(shtps_esd);
	if (ret){
		SHTPS_LOG_ERR_PRINT("regulator_enable Err[%d]\n",ret);
	}

#if 0
	ret = regulator_set_mode(shtps_esd, REGULATOR_MODE_NORMAL);
	if (ret != 0) {
		SHTPS_LOG_ERR_PRINT("regulator_set_mode Err[%d]\n",ret);
	}
#endif

	regulator_put(shtps_esd);
	shtps_esd = NULL;


	gpio_request(shtps_devctl_esd_regulator_info_p->tp_int, "TP_INT");

	SHTPS_LOG_DBG_PRINT("pinctrl_select_state(tp_int active)\n");
	pinctrl_select_state(shtps_devctl_esd_regulator_info_p->pinctrl, shtps_devctl_esd_regulator_info_p->int_active);

	gpio_free(shtps_devctl_esd_regulator_info_p->tp_int);


	gpio_request(shtps_devctl_esd_regulator_info_p->tp_spi_mosi, "TP_SPI_MOSI");
	gpio_request(shtps_devctl_esd_regulator_info_p->tp_spi_miso, "TP_SPI_MISO");
	gpio_request(shtps_devctl_esd_regulator_info_p->tp_spi_cs_n, "TP_SPI_CS_N");
	gpio_request(shtps_devctl_esd_regulator_info_p->tp_spi_clk, "TP_SPI_CLK");

	SHTPS_LOG_DBG_PRINT("pinctrl_select_state(spi)\n");
	pinctrl_select_state(shtps_devctl_esd_regulator_info_p->pinctrl, shtps_devctl_esd_regulator_info_p->esd_spi);

	gpio_free(shtps_devctl_esd_regulator_info_p->tp_spi_mosi);
	gpio_free(shtps_devctl_esd_regulator_info_p->tp_spi_miso);
	gpio_free(shtps_devctl_esd_regulator_info_p->tp_spi_cs_n);
	gpio_free(shtps_devctl_esd_regulator_info_p->tp_spi_clk);

	pinctrl_free_state(shtps_devctl_esd_regulator_info_p->pinctrl);
}

void shtps_esd_regulator_remove(void)
{
	int ret;

	SHTPS_LOG_FUNC_CALL();

	if(shtps_esd != NULL){
		ret = regulator_disable(shtps_esd);
		if (ret){
			SHTPS_LOG_ERR_PRINT("regulator_disable Err[%d]\n",ret);
		}

		regulator_put(shtps_esd);
		shtps_esd = NULL;
	}

	if(shtps_esd_vddh != NULL){
		ret = regulator_disable(shtps_esd_vddh);
		if (ret){
			SHTPS_LOG_ERR_PRINT("regulator_disable vddh Err[%d]\n",ret);
		}

		regulator_put(shtps_esd_vddh);
		shtps_esd_vddh = NULL;
	}
}
#endif /* SHTPS_DYNAMIC_RESET_ESD_ENABLE */

MODULE_DESCRIPTION("SHARP TOUCHPANEL DRIVER MODULE");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.00");
