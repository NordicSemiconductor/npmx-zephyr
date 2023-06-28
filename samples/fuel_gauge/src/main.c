/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <npmx_driver.h>

#include "nrf_fuel_gauge.h"
#include "fuel_gauge.h"

#define LOG_MODULE_NAME main
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

/**
 * @brief Callback function to be used when VBUSIN VOLTAGE event occurs.
 *
 * @param[in] p_pm Pointer to the instance of PMIC device.
 * @param[in] type Callback type. Should be @ref NPMX_CALLBACK_TYPE_EVENT_VBUSIN_VOLTAGE.
 * @param[in] mask Received event interrupt mask.
 */
void vbusin_voltage_callback(npmx_instance_t *p_pm, npmx_callback_type_t type, uint8_t mask)
{
	/* Current limit have to be applied each time when USB is (re)connected. */
	if (mask & NPMX_EVENT_GROUP_VBUSIN_DETECTED_MASK) {
		npmx_vbusin_task_trigger(npmx_vbusin_get(p_pm, 0),
					 NPMX_VBUSIN_TASK_APPLY_CURRENT_LIMIT);
	}
}

void main(void)
{
	const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

	if (!device_is_ready(pmic_dev)) {
		LOG_ERR("PMIC device is not ready.");
		return;
	}

	LOG_INF("PMIC device OK.");

	/* Get pointer to npmx instance and other required instances. */
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);
	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	npmx_vbusin_t *vbusin_instance = npmx_vbusin_get(npmx_instance, 0);
	npmx_adc_t *adc_instance = npmx_adc_get(npmx_instance, 0);

	/* Register callback for VBUSIN VOLTAGE event. */
	npmx_core_register_cb(npmx_instance, vbusin_voltage_callback,
			      NPMX_CALLBACK_TYPE_EVENT_VBUSIN_VOLTAGE);

	/* Enable detecting connected USB. */
	npmx_core_event_interrupt_enable(npmx_instance, NPMX_EVENT_GROUP_VBUSIN_VOLTAGE,
					 NPMX_EVENT_GROUP_VBUSIN_DETECTED_MASK);

	/* Disable charger before changing charge current and termination voltage. */
	npmx_charger_module_disable_set(charger_instance, NPMX_CHARGER_MODULE_CHARGER_MASK);

	/* Set charging current. */
	npmx_charger_charging_current_set(charger_instance, CONFIG_CHARGING_CURRENT);

	/* Set maximum discharging current. */
	npmx_charger_discharging_current_set(charger_instance, CONFIG_DISCHARGING_CURRENT);

	/* Set battery termination voltage in normal temperature. */
	npmx_charger_termination_normal_voltage_set(
		charger_instance, npmx_charger_voltage_convert(CONFIG_TERMINATION_VOLTAGE_NORMAL));

	/* Set battery termination voltage in warm temperature. */
	npmx_charger_termination_warm_voltage_set(
		charger_instance, npmx_charger_voltage_convert(CONFIG_TERMINATION_VOLTAGE_WARM));

	/* Enable charger. */
	npmx_charger_module_enable_set(charger_instance, NPMX_CHARGER_MODULE_CHARGER_MASK);

	/* Set the current limit for the USB port.
	 * Current limit needs to be applied after each USB (re)connection.
	 */
	npmx_vbusin_current_limit_set(vbusin_instance,
				      npmx_vbusin_current_convert(CONFIG_CURRENT_LIMIT));

	/* Apply current limit. */
	npmx_vbusin_task_trigger(vbusin_instance, NPMX_VBUSIN_TASK_APPLY_CURRENT_LIMIT);

	/* Set NTC type for ADC measurements. */
	npmx_adc_ntc_set(adc_instance, npmx_adc_ntc_type_convert(CONFIG_THERMISTOR_RESISTANCE));

	/* Enable auto measurement of the battery current after the battery voltage measurement. */
	npmx_adc_ibat_meas_enable_set(adc_instance, true);

	/* Trigger required ADC measurements. */
	npmx_adc_task_trigger(adc_instance, NPMX_ADC_TASK_SINGLE_SHOT_VBAT);
	npmx_adc_task_trigger(adc_instance, NPMX_ADC_TASK_SINGLE_SHOT_NTC);

	if (fuel_gauge_init(npmx_instance) < 0) {
		LOG_ERR("Fuel gauge initialization failed.");
		return;
	}

	LOG_INF("Fuel gauge OK.");

	while (1) {
		k_msleep(1000);
		fuel_gauge_update(npmx_instance);
	}
}
