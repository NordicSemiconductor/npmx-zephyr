/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <npmx_driver.h>

#define LOG_MODULE_NAME shell
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

/**
 * @brief Function called when battery voltage drops below the configured threshold.
 *
 * @param[in] p_pm Pointer to the instance of PMIC device.
 */
static void pof_callback(npmx_instance_t *instance)
{
	LOG_INF("POF callback");
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

	npmx_ldsw_active_discharge_enable_set(npmx_ldsw_get(npmx_instance, 0), true);
	npmx_ldsw_active_discharge_enable_set(npmx_ldsw_get(npmx_instance, 1), true);

	/* Register callback so that POF warnings can be printed. */
	npmx_pof_config_t pof_config = { .status = NPMX_POF_STATUS_ENABLE,
					 .threshold = NPMX_POF_THRESHOLD_3V5,
					 .polarity = NPMX_POF_POLARITY_HIGH };

	npmx_driver_register_pof_cb(pmic_dev, &pof_config, pof_callback);

	while (1) {
		k_sleep(K_FOREVER);
	}
}
