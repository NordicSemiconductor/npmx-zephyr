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

#define LOG_MODULE_NAME buck_retention
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

/* Values used to refer to the specified buck converter instance. */
#define BUCK1_IDX 0
#define BUCK2_IDX 1

/**
 * @brief Function for testing the retention voltage with the selected external pin.
 *
 * @param[in] p_buck Pointer to the instance of buck converter.
 * @param[in] p_gpio Pointer to the instance of GPIO used for controlling retention mode.
 */
static void test_retention_voltage(npmx_buck_t *p_buck, npmx_gpio_t *p_gpio)
{
	LOG_INF("Test retention voltage.");

	npmx_error_t status;

	/* Switch GPIO1 to input mode. */
	status = npmx_gpio_mode_set(p_gpio, NPMX_GPIO_MODE_INPUT);
	if (status != NPMX_SUCCESS) {
		LOG_ERR("Unable to switch GPIO1 to input mode.");
		return;
	}

	/* Select voltages: in normal mode should be 1.7 V, in retention mode should be 3.3 V. */
	status = npmx_buck_normal_voltage_set(p_buck, NPMX_BUCK_VOLTAGE_1V7);
	if (status != NPMX_SUCCESS) {
		LOG_ERR("Unable to set normal voltage.");
		return;
	}

	status = npmx_buck_retention_voltage_set(p_buck, NPMX_BUCK_VOLTAGE_3V3);
	if (status != NPMX_SUCCESS) {
		LOG_ERR("Unable to set retention voltage.");
		return;
	}

	/* Apply voltages. */
	status = npmx_buck_vout_select_set(p_buck, NPMX_BUCK_VOUT_SELECT_SOFTWARE);
	if (status != NPMX_SUCCESS) {
		LOG_ERR("Unable to select vout reference.");
		return;
	}

	/* Configuration for external pin. If inversion is false,
	 * retention is active when GPIO1 is in high state.
	 */
	npmx_buck_gpio_config_t config = { .gpio = NPMX_BUCK_GPIO_1, .inverted = false };

	/* Set GPIO configuration to buck instance. */
	status = npmx_buck_retention_gpio_config_set(p_buck, &config);
	if (status != NPMX_SUCCESS) {
		LOG_ERR("Unable to select retention GPIO.");
		return;
	}

	LOG_INF("Test retention voltage OK.");
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

	test_retention_voltage(bucks[BUCK1_IDX], npmx_gpio_get(npmx_instance, 1));

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
