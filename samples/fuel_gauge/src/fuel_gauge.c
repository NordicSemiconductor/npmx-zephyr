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

static int read_sensors(npmx_instance_t *const p_pm, float *voltage, float *current, float *temp)
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

	return 0;
}

static int process_vbus_state(npmx_instance_t *const p_pm)
{
	npmx_vbusin_t *vbus_instance = npmx_vbusin_get(p_pm, 0);
	enum nrf_fuel_gauge_ext_state_info_type state_info;
	uint8_t vbusin_status;

	if (npmx_vbusin_vbus_status_get(vbus_instance, &vbusin_status) != NPMX_SUCCESS) {
		LOG_ERR("Reading VBUS status failed.");
		return -EIO;
	}

	if (vbusin_status & NPMX_VBUSIN_STATUS_CONNECTED_MASK) {
		state_info = NRF_FUEL_GAUGE_EXT_STATE_INFO_VBUS_CONNECTED;
	} else {
		state_info = NRF_FUEL_GAUGE_EXT_STATE_INFO_VBUS_DISCONNECTED;
	}

	return nrf_fuel_gauge_ext_state_update(state_info, NULL);
}

static int process_charger_state(npmx_instance_t *const p_pm)
{
	npmx_charger_t *charger_instance = npmx_charger_get(p_pm, 0);
	union nrf_fuel_gauge_ext_state_info_data state_info;
	uint8_t charger_status;

	if (npmx_charger_status_get(charger_instance, &charger_status) != NPMX_SUCCESS) {
		LOG_ERR("Reading charger status failed.");
		return -EIO;
	}

	if (charger_status & NPMX_CHARGER_STATUS_COMPLETED_MASK) {
		LOG_INF("Charge complete");
		state_info.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_COMPLETE;
	} else if (charger_status & NPMX_CHARGER_STATUS_TRICKLE_CHARGE_MASK) {
		LOG_INF("Trickle charging");
		state_info.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_TRICKLE;
	} else if (charger_status & NPMX_CHARGER_STATUS_CONSTANT_CURRENT_MASK) {
		LOG_INF("Constant current charging");
		state_info.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_CC;
	} else if (charger_status & NPMX_CHARGER_STATUS_CONSTANT_VOLTAGE_MASK) {
		LOG_INF("Constant voltage charging");
		state_info.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_CV;
	} else {
		LOG_INF("Charger idle");
		state_info.charge_state = NRF_FUEL_GAUGE_CHARGE_STATE_IDLE;
	}

	return nrf_fuel_gauge_ext_state_update(NRF_FUEL_GAUGE_EXT_STATE_INFO_CHARGE_STATE_CHANGE,
					       &state_info);
}

int fuel_gauge_init(npmx_instance_t *const p_pm)
{
	struct nrf_fuel_gauge_init_parameters parameters = {
		.model = &battery_model,
		.opt_params = NULL,
		.state = NULL,
	};
	npmx_charger_t *charger_instance = npmx_charger_get(p_pm, 0);
	npmx_charger_iterm_t iterm;
	npmx_error_t npmx_err;
	uint32_t iterm_pct;
	uint32_t charge_current_limit_uA;
	float charge_current_limit;
	float term_current;
	int ret;

	ret = read_sensors(p_pm, &parameters.v0, &parameters.i0, &parameters.t0);
	if (ret < 0) {
		return ret;
	}

	/* Store charge nominal and termination current, needed for ttf calculation */
	max_charge_current = CONFIG_CHARGING_CURRENT / 1000.0f;
	term_charge_current = max_charge_current / 10.f;

	nrf_fuel_gauge_init(&parameters, NULL);

	ref_time = k_uptime_get();

	npmx_err = npmx_charger_charging_current_get(charger_instance, &charge_current_limit_uA);
	if (npmx_err != NPMX_SUCCESS) {
		return -EIO;
	}

	npmx_err = npmx_charger_termination_current_get(charger_instance, &iterm);
	if (npmx_err != NPMX_SUCCESS) {
		return -EIO;
	}

	if (!npmx_charger_iterm_convert_to_pct(iterm, &iterm_pct)) {
		return -EIO;
	}

	charge_current_limit = (float)charge_current_limit_uA / 1000000.f;
	term_current = charge_current_limit * (float)iterm_pct / 100.f;

	ret = nrf_fuel_gauge_ext_state_update(NRF_FUEL_GAUGE_EXT_STATE_INFO_CHARGE_CURRENT_LIMIT,
					      &(union nrf_fuel_gauge_ext_state_info_data){
						      .charge_current_limit = charge_current_limit});
	if (ret < 0) {
		return ret;
	}

	ret = nrf_fuel_gauge_ext_state_update(NRF_FUEL_GAUGE_EXT_STATE_INFO_TERM_CURRENT,
					      &(union nrf_fuel_gauge_ext_state_info_data){
						      .charge_term_current = term_current});
	if (ret < 0) {
		return ret;
	}

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
	int ret;

	ret = process_vbus_state(p_pm);
	if (ret < 0) {
		LOG_ERR("Error: Could not process VBUS state.");
		return ret;
	}

	ret = process_charger_state(p_pm);
	if (ret < 0) {
		LOG_ERR("Error: Could not process charger state.");
		return ret;
	}

	ret = read_sensors(p_pm, &voltage, &current, &temp);
	if (ret < 0) {
		LOG_ERR("Error: Could not read data from charger device.");
		return ret;
	}

	delta = (float)k_uptime_delta(&ref_time) / 1000.f;

	soc = nrf_fuel_gauge_process(voltage, current, temp, delta, NULL);
	tte = nrf_fuel_gauge_tte_get();
	ttf = nrf_fuel_gauge_ttf_get();

	LOG_INF("V: %.3f, I: %.3f, T: %.2f, SoC: %.2f, TTE: %.0f, TTF: %.0f", (double)voltage, (double)current,
		(double)temp, (double)soc, (double)tte, (double)ttf);

	return 0;
}
