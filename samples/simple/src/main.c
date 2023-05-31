/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <npmx_adc.h>
#include <npmx_charger.h>
#include <npmx_core.h>
#include <npmx_driver.h>

#define LOG_MODULE_NAME simple
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

/**
 * @brief Callback function to be used when VBUSIN THERMAL event occurs.
 *
 * @param[in] p_pm Pointer to the instance of PMIC device.
 * @param[in] type Callback type. Should be @ref NPMX_CALLBACK_TYPE_EVENT_VBUSIN_THERMAL_USB.
 * @param[in] mask Received event interrupt mask.
 */
void vbusin_thermal_callback(npmx_instance_t *p_pm, npmx_callback_type_t type, uint8_t mask)
{
	npmx_vbusin_cc_t cc1;
	npmx_vbusin_cc_t cc2;
	npmx_vbusin_t *p_vbusin = npmx_vbusin_get(p_pm, 0);

	/* Get the status of CC lines. */
	if (npmx_vbusin_cc_status_get(p_vbusin, &cc1, &cc2) == NPMX_SUCCESS) {
		static npmx_vbusin_current_t vbusin_current_limits[] = {
			[NPMX_VBUSIN_CC_DEFAULT] = NPMX_VBUSIN_CURRENT_500_MA,
			[NPMX_VBUSIN_CC_HIGH_POWER_1A5] = NPMX_VBUSIN_CURRENT_1500_MA,
			[NPMX_VBUSIN_CC_HIGH_POWER_3A0] = NPMX_VBUSIN_CURRENT_1500_MA,
		};

		if (cc1 != NPMX_VBUSIN_CC_NOT_CONNECTED) {
			npmx_vbusin_current_limit_set(p_vbusin, vbusin_current_limits[cc1]);

		} else if (cc2 != NPMX_VBUSIN_CC_NOT_CONNECTED) {
			npmx_vbusin_current_limit_set(p_vbusin, vbusin_current_limits[cc2]);
		}
		npmx_vbusin_task_trigger(p_vbusin, NPMX_VBUSIN_TASK_APPLY_CURRENT_LIMIT);

		LOG_INF("CC1: %s.", npmx_vbusin_cc_status_map_to_string(cc1));
		LOG_INF("CC2: %s.", npmx_vbusin_cc_status_map_to_string(cc2));
	} else {
		LOG_ERR("Unable to read CC lines status.");
	}
}

void main(void)
{
	const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

	if (!device_is_ready(pmic_dev)) {
		LOG_INF("PMIC device is not ready.");
		return;
	}

	LOG_INF("PMIC device OK.");

	/* Get pointer to npmx device. */
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	/* Get pointer to charger instance. */
	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);

	/* Register callback for VBUSIN thermal event used for detecting CC lines status. */
	npmx_core_register_cb(npmx_instance, vbusin_thermal_callback,
			      NPMX_EVENT_GROUP_VBUSIN_THERMAL);

	/* Enable detecting if CC status changed. */
	npmx_core_event_interrupt_enable(npmx_instance, NPMX_EVENT_GROUP_VBUSIN_THERMAL,
					 NPMX_EVENT_GROUP_USB_CC1_MASK |
						 NPMX_EVENT_GROUP_USB_CC2_MASK);

	/* Disable charger before changing charging current. */
	npmx_charger_module_disable_set(charger_instance, NPMX_CHARGER_MODULE_CHARGER_MASK);

	/* Set termination voltage and charging current. */
	npmx_charger_termination_normal_voltage_set(charger_instance, NPMX_CHARGER_VOLTAGE_4V20);
	npmx_charger_charging_current_set(charger_instance, 800);

	/* Enable charger for events handling. */
	npmx_charger_module_enable_set(charger_instance,
				       NPMX_CHARGER_MODULE_CHARGER_MASK |
					       NPMX_CHARGER_MODULE_RECHARGE_MASK |
					       NPMX_CHARGER_MODULE_NTC_LIMITS_MASK);

	/* Set LEDs. */
	npmx_led_mode_set(npmx_led_get(npmx_instance, 0), NPMX_LED_MODE_CHARGING);
	npmx_led_mode_set(npmx_led_get(npmx_instance, 1), NPMX_LED_MODE_ERROR);
	npmx_led_mode_set(npmx_led_get(npmx_instance, 2), NPMX_LED_MODE_NOTUSED);

	/* Set NTC type for ADC measurements. */
	npmx_adc_ntc_set(npmx_adc_get(npmx_instance, 0), NPMX_ADC_NTC_TYPE_10_K);

	while (1) {
		k_sleep(K_FOREVER);
	}
}
