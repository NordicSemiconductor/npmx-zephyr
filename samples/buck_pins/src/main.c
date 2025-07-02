/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <npmx_buck.h>
#include <npmx_driver.h>
#include <npmx_gpio.h>

#define LOG_MODULE_NAME buck_pins
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

/* Values used to refer to the specified buck converter instance. */
#define BUCK1_IDX 0
#define BUCK2_IDX 1

/**
 * @brief Function for setting the buck output voltage for the specified buck converter.
 *
 * @param[in] p_buck  Pointer to the instance of buck converter.
 * @param[in] voltage Selected voltage.
 *
 * @retval NPMX_SUCCESS  Operation performed successfully.
 * @retval NPMX_ERROR_IO Error using IO bus line.
 */
static npmx_error_t set_buck_voltage(npmx_buck_t *p_buck, npmx_buck_voltage_t voltage)
{
	npmx_error_t status;

	/* Set the output voltage. */
	status = npmx_buck_normal_voltage_set(p_buck, voltage);
	if (status != NPMX_SUCCESS) {
		LOG_ERR("Unable to set normal voltage.");
		return status;
	}

	/* Have to be called each time to change output voltage. */
	status = npmx_buck_vout_select_set(p_buck, NPMX_BUCK_VOUT_SELECT_SOFTWARE);
	if (status != NPMX_SUCCESS) {
		LOG_ERR("Unable to select vout reference.");
		return status;
	}
	return NPMX_SUCCESS;
}

/**
 * @brief Function for testing enable mode with the selected external pin.
 *
 * @param[in] p_buck Pointer to the instance of buck converter.
 * @param[in] p_gpio Pointer to the instance of GPIO used to enable buck.
 */
static void test_enable_bucks_using_pin(npmx_buck_t *p_buck, npmx_gpio_t *p_gpio)
{
	LOG_INF("Test enable buck using connected pin.");

	/* Disable buck converter for testing. */
	if (npmx_buck_task_trigger(p_buck, NPMX_BUCK_TASK_DISABLE) != NPMX_SUCCESS) {
		LOG_ERR("Unable to disable buck.");
		return;
	}

	/* Switch GPIO3 to input mode. */
	if (npmx_gpio_mode_set(p_gpio, NPMX_GPIO_MODE_INPUT) != NPMX_SUCCESS) {
		LOG_ERR("Unable to switch GPIO3 to input mode.");
		return;
	}

	/* Select output voltage to be 3.3 V. */
	set_buck_voltage(p_buck, NPMX_BUCK_VOLTAGE_3V3);

	/* Configuration for external pin. If inversion is false, */
	/* BUCK1 is active when GPIO1 is in high state. */
	npmx_buck_gpio_config_t config = { .gpio = NPMX_BUCK_GPIO_3, .inverted = false };

	/* When GPIO3 changes to high state, BUCK1 will start working. */
	if (npmx_buck_enable_gpio_config_set(p_buck, &config) != NPMX_SUCCESS) {
		LOG_ERR("Unable to connect GPIO3 to BUCK1.");
		return;
	}

	/* Enable active discharge, so that the capacitor discharges faster,
	 * when there is no load connected to PMIC.
	 */
	if (npmx_buck_active_discharge_enable_set(p_buck, true) != NPMX_SUCCESS) {
		LOG_ERR("Unable to activate auto discharge mode.");
		return;
	}

	LOG_INF("Test enable buck using connected pin OK.");
}

int main(void)
{
	const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

	if (!device_is_ready(pmic_dev)) {
		LOG_INF("PMIC device is not ready.");
		return 0;
	}

	LOG_INF("PMIC device OK.");

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	npmx_buck_t *bucks[] = { npmx_buck_get(npmx_instance, BUCK1_IDX),
				 npmx_buck_get(npmx_instance, BUCK2_IDX) };

	/* After reset, buck converters are enabled by default,
	 * but to be sure, enable both at the beginning of the testing.
	 */
	npmx_buck_task_trigger(bucks[BUCK1_IDX], NPMX_BUCK_TASK_ENABLE);
	npmx_buck_task_trigger(bucks[BUCK2_IDX], NPMX_BUCK_TASK_ENABLE);

	test_enable_bucks_using_pin(bucks[BUCK1_IDX], npmx_gpio_get(npmx_instance, 3));

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
