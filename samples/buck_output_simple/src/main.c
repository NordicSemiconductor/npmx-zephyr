/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <npmx_buck.h>
#include <npmx_driver.h>

#define LOG_MODULE_NAME buck_output_simple
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
 * @brief Function for testing simple use-case of buck voltage.
 *
 * @param[in] p_buck Pointer to the instance of buck converter.
 */
static void test_set_buck_voltage(npmx_buck_t *p_buck)
{
	LOG_INF("Test setting buck voltage.");

	/* Set output voltage for buck converter. */
	if (set_buck_voltage(p_buck, NPMX_BUCK_VOLTAGE_2V4) != NPMX_SUCCESS) {
		LOG_ERR("Unable to set buck voltage.");
		return;
	}

	LOG_INF("Test setting buck voltage OK.");
}

void main(void)
{
	const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

	if (!device_is_ready(pmic_dev)) {
		LOG_INF("PMIC device is not ready.");
		return;
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

	test_set_buck_voltage(bucks[BUCK1_IDX]);

	while (1) {
		k_sleep(K_FOREVER);
	}
}
