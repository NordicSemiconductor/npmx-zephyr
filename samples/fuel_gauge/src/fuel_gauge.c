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
#include <math.h>

#define LOG_MODULE_NAME fuel_gauge
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

static const struct battery_model battery_model = {
#include "battery_model.inc"
};

static float max_charge_current;
static float term_charge_current;
static int64_t ref_time;

static int read_sensors(npmx_instance_t *const p_pm, float *voltage, float *current, float *temp,
			bool *cc_charging)
{
	npmx_adc_t *adc_instance = npmx_adc_get(p_pm, 0);
	npmx_adc_meas_all_t meas;

	if (npmx_adc_meas_all_get(adc_instance, &meas) != NPMX_SUCCESS) {
		LOG_ERR("Reading ADC measurements failed.");
		return -EIO;
	}

	if (npmx_adc_task_trigger(adc_instance, NPMX_ADC_TASK_SINGLE_SHOT_VBAT) != NPMX_SUCCESS) {
		LOG_ERR("Triggering VBAT measurement failed.");
		return -EIO;
	}

	if (npmx_adc_task_trigger(adc_instance, NPMX_ADC_TASK_SINGLE_SHOT_NTC) != NPMX_SUCCESS) {
		LOG_ERR("Triggering NTC measurement failed.");
		return -EIO;
	}

	/* Convert current in microamperes to current in amperes. */
	*current = (float)meas.values[NPMX_ADC_MEAS_VBAT2_IBAT] / 1000000.0f;

	/* Convert temperature in millidegrees Celsius to temperature in Celsius */
	*temp = (float)meas.values[NPMX_ADC_MEAS_BAT_TEMP] / 1000.0f;

	/* Convert voltage in millivolts to voltage in volts. */
	*voltage = (float)meas.values[NPMX_ADC_MEAS_VBAT] / 1000.0f;

	/* Get charger status */
	npmx_charger_t *charger_instance = npmx_charger_get(p_pm, 0);
	uint8_t chg_status;

	if (npmx_charger_status_get(charger_instance, &chg_status) != NPMX_SUCCESS) {
		LOG_ERR("Reading charger status failed.");
		return -EIO;
	}

	*cc_charging = (chg_status & NPMX_CHARGER_STATUS_CONSTANT_CURRENT_MASK) != 0U;

	return 0;
}

int fuel_gauge_init(npmx_instance_t *const p_pm)
{
	struct nrf_fuel_gauge_init_parameters parameters = {
		.model = &battery_model,
		.opt_params = NULL,
	};
	bool cc_charging;
	int ret;

	ret = read_sensors(p_pm, &parameters.v0, &parameters.i0, &parameters.t0, &cc_charging);
	if (ret < 0) {
		return ret;
	}

	/* Store charge nominal and termination current, needed for ttf calculation */
	max_charge_current = CONFIG_CHARGING_CURRENT / 1000.0f;
	term_charge_current = max_charge_current / 10.f;

	nrf_fuel_gauge_init(&parameters, NULL);

	ref_time = k_uptime_get();

	return 0;
}

int fuel_gauge_update(npmx_instance_t *const p_pm)
{
	float voltage;
	float current;
	float temp;
	float soc;
	float tte;
	float ttf;
	float delta;
	bool cc_charging;
	int ret;

	ret = read_sensors(p_pm, &voltage, &current, &temp, &cc_charging);
	if (ret < 0) {
		LOG_ERR("Error: Could not read data from charger device.");
		return ret;
	}

	delta = (float)k_uptime_delta(&ref_time) / 1000.f;

	soc = nrf_fuel_gauge_process(voltage, current, temp, delta, NULL);
	tte = nrf_fuel_gauge_tte_get();
	ttf = nrf_fuel_gauge_ttf_get(cc_charging, -term_charge_current);

	LOG_INF("V: %.3f, I: %.3f, T: %.2f, SoC: %.2f, TTE: %.0f, TTF: %.0f", voltage, current,
		temp, soc, tte, ttf);

	return 0;
}
