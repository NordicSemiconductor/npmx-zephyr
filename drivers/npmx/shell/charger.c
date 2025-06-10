/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "shell_common.h"
#include <npmx_driver.h>

npmx_adc_t *adc_instance_get(const struct shell *shell);

npmx_charger_t *charger_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);

	return npmx_instance ? npmx_charger_get(npmx_instance, 0) : NULL;
}

static bool charger_charging_current_set_helper(const struct shell *shell,
						npmx_charger_t *charger_instance, uint32_t current,
						uint32_t *p_charging_current)
{
	npmx_error_t err_code = npmx_charger_charging_current_set(charger_instance, current);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "charging current");
		return false;
	}

	err_code = npmx_charger_charging_current_get(charger_instance, p_charging_current);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "charging current");
		return false;
	}

	value_difference_info(shell, SHELL_ARG_TYPE_UINT32_VALUE, (uint32_t)current,
			      (uint32_t)*p_charging_current);

	return true;
}

static int cmd_charger_charging_current_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE, "charging current" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "charging current")) {
		return 0;
	}

	if (!range_check(shell, args_info.arg[0].result.uvalue,
			 NPM_BCHARGER_CHARGING_CURRENT_MIN_UA, NPM_BCHARGER_CHARGING_CURRENT_MAX_UA,
			 "charging current")) {
		return 0;
	}

	uint32_t charging_current_ua = (uint32_t)args_info.arg[0].result.uvalue;
	uint32_t current_actual;
	if (charger_charging_current_set_helper(shell, charger_instance, charging_current_ua,
						&current_actual)) {
		print_success(shell, current_actual, UNIT_TYPE_MICROAMPERE);
	}
	return 0;
}

static int cmd_charger_charging_current_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	uint32_t charging_current_ua;
	npmx_error_t err_code =
		npmx_charger_charging_current_get(charger_instance, &charging_current_ua);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "charging current");
		return 0;
	}

	print_value(shell, charging_current_ua, UNIT_TYPE_MICROAMPERE);
	return 0;
}

static bool charger_die_temp_set_helper(const struct shell *shell, npmx_charger_t *charger_instance,
					int16_t temperature, int16_t *p_temperature,
					npmx_error_t (*func_set)(npmx_charger_t const *p_instance,
								 int16_t temperature),
					npmx_error_t (*func_get)(npmx_charger_t const *p_instance,
								 int16_t *p_temperature))
{
	npmx_error_t err_code = func_set(charger_instance, temperature);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "die temperature threshold");
		return false;
	}

	err_code = func_get(charger_instance, p_temperature);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "die temperature threshold");
		return false;
	}

	value_difference_info(shell, SHELL_ARG_TYPE_INT32_VALUE, (int32_t)temperature,
			      (int32_t)*p_temperature);
	return true;
}

static int charger_die_temp_set(const struct shell *shell, size_t argc, char **argv,
				npmx_error_t (*func_set)(npmx_charger_t const *p_instance,
							 int16_t temperature),
				npmx_error_t (*func_get)(npmx_charger_t const *p_instance,
							 int16_t *p_temperature))
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_INT32_VALUE,
						  "die temperature threshold" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "die temperature threshold")) {
		return 0;
	}

	if (!range_check(shell, args_info.arg[0].result.ivalue,
			 NPM_BCHARGER_DIE_TEMPERATURE_MIN_VAL, NPM_BCHARGER_DIE_TEMPERATURE_MAX_VAL,
			 "die temperature threshold")) {
		return 0;
	}

	int16_t temperature = (int16_t)args_info.arg[0].result.ivalue;
	int16_t temperature_actual;
	if (charger_die_temp_set_helper(shell, charger_instance, temperature, &temperature_actual,
					func_set, func_get)) {
		print_success(shell, temperature_actual, UNIT_TYPE_CELSIUS);
	}
	return 0;
}

static int charger_die_temp_get(const struct shell *shell,
				npmx_error_t (*func)(npmx_charger_t const *p_instance,
						     int16_t *temperature))
{
	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	int16_t temperature;
	npmx_error_t err_code = func(charger_instance, &temperature);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "die temperature threshold");
		return 0;
	}

	print_value(shell, temperature, UNIT_TYPE_CELSIUS);
	return 0;
}

static int cmd_charger_die_temp_resume_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_die_temp_set(shell, argc, argv, npmx_charger_die_temp_resume_set,
				    npmx_charger_die_temp_resume_get);
}

static int cmd_charger_die_temp_resume_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_die_temp_get(shell, npmx_charger_die_temp_resume_get);
}

static int cmd_charger_die_temp_status_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	bool status;
	npmx_error_t err_code = npmx_charger_die_temp_status_get(charger_instance, &status);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "charger die temperature comparator status");
		return 0;
	}

	print_value(shell, status, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_charger_die_temp_stop_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_die_temp_set(shell, argc, argv, npmx_charger_die_temp_stop_set,
				    npmx_charger_die_temp_stop_get);
}

static int cmd_charger_die_temp_stop_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_die_temp_get(shell, npmx_charger_die_temp_stop_get);
}

static bool charger_discharging_current_set_helper(const struct shell *shell,
						   npmx_charger_t *charger_instance,
						   uint16_t current, uint16_t *p_current)
{
	npmx_error_t err_code = npmx_charger_discharging_current_set(charger_instance, current);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "discharging current");
		return false;
	}

	err_code = npmx_charger_discharging_current_get(charger_instance, p_current);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "discharging current");
		return false;
	}

	value_difference_info(shell, SHELL_ARG_TYPE_UINT32_VALUE, (uint32_t)current,
			      (uint32_t)*p_current);

	return true;
}

static int cmd_charger_discharging_current_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE,
						  "discharging current" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "discharging current")) {
		return 0;
	}

	const uint16_t allowed_values[] = NPM_BCHARGER_DISCHARGING_CURRENTS_MA;
	int32_t allowed_min = allowed_values[0];
	int32_t allowed_max = allowed_values[ARRAY_SIZE(allowed_values) - 1];

	if (!range_check(shell, args_info.arg[0].result.uvalue,
			 allowed_min, allowed_max, "discharging current")) {
		return 0;
	}

	uint16_t discharging_current_ma = args_info.arg[0].result.uvalue;
	uint16_t current_actual;
	if (charger_discharging_current_set_helper(shell, charger_instance, discharging_current_ma,
						   &current_actual)) {
		print_success(shell, current_actual, UNIT_TYPE_MILLIAMPERE);
	}
	return 0;
}

static int cmd_charger_discharging_current_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	uint16_t charging_current_ma;
	npmx_error_t err_code =
		npmx_charger_discharging_current_get(charger_instance, &charging_current_ma);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "discharging current");
		return 0;
	}

	print_value(shell, charging_current_ma, UNIT_TYPE_MILLIAMPERE);
	return 0;
}

static int charger_module_set(const struct shell *shell, size_t argc, char **argv,
			      npmx_charger_module_mask_t mask)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_BOOL_VALUE, "status" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	bool charger_set = args_info.arg[0].result.bvalue;

	npmx_error_t (*change_status_function)(const npmx_charger_t *p_instance,
					       uint32_t module_mask) =
		charger_set ? npmx_charger_module_enable_set : npmx_charger_module_disable_set;

	npmx_error_t err_code = change_status_function(charger_instance, mask);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "charging module status");
		return 0;
	}

	print_success(shell, charger_set, UNIT_TYPE_NONE);
	return 0;
}

static int charger_module_get(const struct shell *shell, npmx_charger_module_mask_t mask)
{
	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	uint32_t module_mask;
	npmx_error_t err_code = npmx_charger_module_get(charger_instance, &module_mask);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "charging module status");
		return 0;
	}

	print_value(shell, !!(module_mask & (uint32_t)mask), UNIT_TYPE_NONE);
	return 0;
}

static int cmd_charger_module_charger_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_module_set(shell, argc, argv, NPMX_CHARGER_MODULE_CHARGER_MASK);
}

static int cmd_charger_module_charger_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_module_get(shell, NPMX_CHARGER_MODULE_CHARGER_MASK);
}

static int cmd_charger_module_full_cool_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_module_set(shell, argc, argv, NPMX_CHARGER_MODULE_FULL_COOL_MASK);
}

static int cmd_charger_module_full_cool_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_module_get(shell, NPMX_CHARGER_MODULE_FULL_COOL_MASK);
}

static int cmd_charger_module_ntc_limits_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_module_set(shell, argc, argv, NPMX_CHARGER_MODULE_NTC_LIMITS_MASK);
}

static int cmd_charger_module_ntc_limits_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_module_get(shell, NPMX_CHARGER_MODULE_NTC_LIMITS_MASK);
}

static int cmd_charger_module_recharge_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_module_set(shell, argc, argv, NPMX_CHARGER_MODULE_RECHARGE_MASK);
}

static int cmd_charger_module_recharge_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_module_get(shell, NPMX_CHARGER_MODULE_RECHARGE_MASK);
}

static bool charger_ntc_resistance_set_helper(
	const struct shell *shell, npmx_charger_t const *p_instance, uint32_t resistance,
	uint32_t *p_resistance,
	npmx_error_t (*func_set)(npmx_charger_t const *p_instance, uint32_t resistance),
	npmx_error_t (*func_get)(npmx_charger_t const *p_instance, uint32_t *p_resistance))
{
	npmx_error_t err_code = func_set(p_instance, resistance);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "NTC resistance");
		return false;
	}

	err_code = func_get(p_instance, p_resistance);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "NTC resistance");
		return false;
	}

	value_difference_info(shell, SHELL_ARG_TYPE_UINT32_VALUE, resistance, *p_resistance);

	return true;
}

static int charger_ntc_resistance_set(const struct shell *shell, size_t argc, char **argv,
				      npmx_error_t (*func_set)(npmx_charger_t const *p_instance,
							       uint32_t resistance),
				      npmx_error_t (*func_get)(npmx_charger_t const *p_instance,
							       uint32_t *p_resistance))
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE, "NTC resistance" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "NTC resistance")) {
		return 0;
	}

	uint32_t resistance = args_info.arg[0].result.uvalue;
	uint32_t resistance_actual;
	if (charger_ntc_resistance_set_helper(shell, charger_instance, resistance,
					      &resistance_actual, func_set, func_get)) {
		print_success(shell, resistance_actual, UNIT_TYPE_OHM);
	}
	return 0;
}

static int charger_ntc_resistance_get(const struct shell *shell,
				      npmx_error_t (*func)(npmx_charger_t const *p_instance,
							   uint32_t *p_resistance))
{
	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	uint32_t resistance;
	npmx_error_t err_code = func(charger_instance, &resistance);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "NTC resistance");
		return 0;
	}

	print_value(shell, resistance, UNIT_TYPE_OHM);
	return 0;
}

static int cmd_charger_ntc_resistance_cold_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_resistance_set(shell, argc, argv, npmx_charger_cold_resistance_set,
					  npmx_charger_cold_resistance_get);
}

static int cmd_charger_ntc_resistance_cold_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_resistance_get(shell, npmx_charger_cold_resistance_get);
}

static int cmd_charger_ntc_resistance_cool_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_resistance_set(shell, argc, argv, npmx_charger_cool_resistance_set,
					  npmx_charger_cool_resistance_get);
}

static int cmd_charger_ntc_resistance_cool_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_resistance_get(shell, npmx_charger_cool_resistance_get);
}

static int cmd_charger_ntc_resistance_warm_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_resistance_set(shell, argc, argv, npmx_charger_warm_resistance_set,
					  npmx_charger_warm_resistance_get);
}

static int cmd_charger_ntc_resistance_warm_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_resistance_get(shell, npmx_charger_warm_resistance_get);
}

static int cmd_charger_ntc_resistance_hot_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_resistance_set(shell, argc, argv, npmx_charger_hot_resistance_set,
					  npmx_charger_hot_resistance_get);
}

static int cmd_charger_ntc_resistance_hot_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_resistance_get(shell, npmx_charger_hot_resistance_get);
}

static int charger_ntc_temperature_set(const struct shell *shell, size_t argc, char **argv,
				       npmx_error_t (*func)(npmx_charger_t const *p_instance,
							    int16_t temperature))
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_INT32_VALUE, "NTC temperature" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "NTC temperature")) {
		return 0;
	}

	int32_t temperature = args_info.arg[0].result.ivalue;
	if (!range_check(shell, temperature, -20, 60, "NTC temperature")) {
		return 0;
	}

	npmx_error_t err_code = func(charger_instance, temperature);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "NTC temperature");
		return 0;
	}

	print_success(shell, temperature, UNIT_TYPE_CELSIUS);
	return 0;
}

static int charger_ntc_temperature_get(const struct shell *shell,
				       npmx_error_t (*func)(npmx_charger_t const *p_instance,
							    int16_t *p_temperature))
{
	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	int16_t temperature;
	npmx_error_t err_code = func(charger_instance, &temperature);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "NTC temperature");
		return 0;
	}

	print_value(shell, temperature, UNIT_TYPE_CELSIUS);
	return 0;
}

static int cmd_charger_ntc_temperature_cold_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_temperature_set(shell, argc, argv, npmx_charger_cold_temperature_set);
}

static int cmd_charger_ntc_temperature_cold_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_temperature_get(shell, npmx_charger_cold_temperature_get);
}

static int cmd_charger_ntc_temperature_cool_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_temperature_set(shell, argc, argv, npmx_charger_cool_temperature_set);
}

static int cmd_charger_ntc_temperature_cool_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_temperature_get(shell, npmx_charger_cool_temperature_get);
}

static int cmd_charger_ntc_temperature_warm_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_temperature_set(shell, argc, argv, npmx_charger_warm_temperature_set);
}

static int cmd_charger_ntc_temperature_warm_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_temperature_get(shell, npmx_charger_warm_temperature_get);
}

static int cmd_charger_ntc_temperature_hot_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_temperature_set(shell, argc, argv, npmx_charger_hot_temperature_set);
}

static int cmd_charger_ntc_temperature_hot_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_temperature_get(shell, npmx_charger_hot_temperature_get);
}

static int cmd_charger_status_all_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	uint8_t status_mask;
	npmx_error_t err_code = npmx_charger_status_get(charger_instance, &status_mask);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "charger status");
		return 0;
	}

	print_value(shell, status_mask, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_charger_status_charging_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	uint8_t status_mask;
	uint8_t charging_mask = NPMX_CHARGER_STATUS_TRICKLE_CHARGE_MASK |
				NPMX_CHARGER_STATUS_CONSTANT_CURRENT_MASK |
				NPMX_CHARGER_STATUS_CONSTANT_VOLTAGE_MASK;
	npmx_error_t err_code = npmx_charger_status_get(charger_instance, &status_mask);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "charger status");
		return 0;
	}

	print_value(shell, !!(status_mask & charging_mask), UNIT_TYPE_NONE);
	return 0;
}

static int cmd_charger_termination_current_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE,
						  "termination current" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "termination current")) {
		return 0;
	}

	uint32_t current_pct = args_info.arg[0].result.uvalue;
	npmx_charger_iterm_t charger_iterm = npmx_charger_iterm_convert(current_pct);
	if (charger_iterm == NPMX_CHARGER_ITERM_INVALID) {
		print_convert_error(shell, "pct", "termination current");
		return 0;
	}

	npmx_error_t err_code =
		npmx_charger_termination_current_set(charger_instance, charger_iterm);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "termination current");
		return 0;
	}

	print_success(shell, current_pct, UNIT_TYPE_PCT);
	return 0;
}

static int cmd_charger_termination_current_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	npmx_charger_iterm_t charger_iterm;
	npmx_error_t err_code =
		npmx_charger_termination_current_get(charger_instance, &charger_iterm);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "termination current");
		return 0;
	}

	uint32_t current_pct;
	if (!npmx_charger_iterm_convert_to_pct(charger_iterm, &current_pct)) {
		print_convert_error(shell, "termination current", "pct");
		return 0;
	}

	print_value(shell, current_pct, UNIT_TYPE_PCT);
	return 0;
}

static int charger_termination_voltage_set(const struct shell *shell, size_t argc, char **argv,
					   npmx_error_t (*func)(npmx_charger_t const *p_instance,
								npmx_charger_voltage_t voltage))
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE,
						  "termination voltage" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	uint32_t voltage_mv = args_info.arg[0].result.uvalue;
	npmx_charger_voltage_t charger_voltage = npmx_charger_voltage_convert(voltage_mv);
	if (charger_voltage == NPMX_CHARGER_VOLTAGE_INVALID) {
		print_convert_error(shell, "millivolts", "termination voltage");
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "termination voltage")) {
		return 0;
	}

	npmx_error_t err_code = func(charger_instance, charger_voltage);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "termination voltage");
		return 0;
	}

	print_success(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

static int charger_termination_voltage_get(const struct shell *shell,
					   npmx_error_t (*func)(npmx_charger_t const *p_instance,
								npmx_charger_voltage_t *voltage))
{
	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	npmx_charger_voltage_t charger_voltage;
	npmx_error_t err_code = func(charger_instance, &charger_voltage);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "termination voltage");
		return 0;
	}

	uint32_t voltage_mv;
	if (!npmx_charger_voltage_convert_to_mv(charger_voltage, &voltage_mv)) {
		print_convert_error(shell, "termination voltage", "millivolts");
		return 0;
	}

	print_value(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

static int cmd_charger_termination_voltage_normal_set(const struct shell *shell, size_t argc,
						      char **argv)
{
	return charger_termination_voltage_set(shell, argc, argv,
					       npmx_charger_termination_normal_voltage_set);
}

static int cmd_charger_termination_voltage_normal_get(const struct shell *shell, size_t argc,
						      char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_termination_voltage_get(shell, npmx_charger_termination_normal_voltage_get);
}

static int cmd_charger_termination_voltage_warm_set(const struct shell *shell, size_t argc,
						    char **argv)
{
	return charger_termination_voltage_set(shell, argc, argv,
					       npmx_charger_termination_warm_voltage_set);
}

static int cmd_charger_termination_voltage_warm_get(const struct shell *shell, size_t argc,
						    char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_termination_voltage_get(shell, npmx_charger_termination_warm_voltage_get);
}

static int cmd_charger_trickle_voltage_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE, "trickle voltage" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "trickle voltage")) {
		return 0;
	}

	uint32_t voltage_mv = args_info.arg[0].result.uvalue;
	npmx_charger_trickle_t charger_trickle = npmx_charger_trickle_convert(voltage_mv);
	if (charger_trickle == NPMX_CHARGER_TRICKLE_INVALID) {
		print_convert_error(shell, "millivolts", "trickle voltage");
		return 0;
	}

	npmx_error_t err_code = npmx_charger_trickle_voltage_set(charger_instance, charger_trickle);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "trickle voltage");
		return 0;
	}

	print_success(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

static int cmd_charger_trickle_voltage_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	npmx_charger_trickle_t charger_trickle;
	npmx_error_t err_code =
		npmx_charger_trickle_voltage_get(charger_instance, &charger_trickle);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "trickle voltage");
		return 0;
	}

	uint32_t voltage_mv;
	if (!npmx_charger_trickle_convert_to_mv(charger_trickle, &voltage_mv)) {
		print_convert_error(shell, "trickle voltage", "millivolts");
		return 0;
	}

	print_value(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

/* Creating subcommands (level 3 command) array for command "charger charging_current". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_charging_current,
			       SHELL_CMD(set, NULL, "Set charging current",
					 cmd_charger_charging_current_set),
			       SHELL_CMD(get, NULL, "Get charging current",
					 cmd_charger_charging_current_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger die_temp resume". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_die_temp_resume,
			       SHELL_CMD(set, NULL,
					 "Set die temperature threshold for resume charging",
					 cmd_charger_die_temp_resume_set),
			       SHELL_CMD(get, NULL,
					 "Get die temperature threshold for resume charging",
					 cmd_charger_die_temp_resume_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger die_temp status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_die_temp_status,
			       SHELL_CMD(get, NULL, "Get die temperature comparator status",
					 cmd_charger_die_temp_status_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger die_temp stop". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_die_temp_stop,
			       SHELL_CMD(set, NULL,
					 "Set die temperature threshold for stop charging",
					 cmd_charger_die_temp_stop_set),
			       SHELL_CMD(get, NULL,
					 "Get die temperature threshold for stop charging",
					 cmd_charger_die_temp_stop_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger die_temp". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_die_temp,
			       SHELL_CMD(resume, &sub_charger_die_temp_resume,
					 "Die temperature threshold for resume charging", NULL),
			       SHELL_CMD(status, &sub_charger_die_temp_status,
					 "Die temperature comparator status", NULL),
			       SHELL_CMD(stop, &sub_charger_die_temp_stop,
					 "Die temperature threshold for stop charging", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger discharging_current". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_discharging_current,
			       SHELL_CMD(set, NULL, "Set discharging current",
					 cmd_charger_discharging_current_set),
			       SHELL_CMD(get, NULL, "Get discharging current",
					 cmd_charger_discharging_current_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger module charger". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_module_charger,
			       SHELL_CMD(set, NULL, "Set charger status",
					 cmd_charger_module_charger_set),
			       SHELL_CMD(get, NULL, "Get charger status",
					 cmd_charger_module_charger_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger module full_cool". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_module_full_cool,
			       SHELL_CMD(set, NULL, "Set full cool status",
					 cmd_charger_module_full_cool_set),
			       SHELL_CMD(get, NULL, "Get full cool status",
					 cmd_charger_module_full_cool_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger module ntc_limits". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_module_ntc_limits,
			       SHELL_CMD(set, NULL, "Set NTC status",
					 cmd_charger_module_ntc_limits_set),
			       SHELL_CMD(get, NULL, "Get NTC status",
					 cmd_charger_module_ntc_limits_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger module recharge". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_module_recharge,
			       SHELL_CMD(set, NULL, "Set recharge status",
					 cmd_charger_module_recharge_set),
			       SHELL_CMD(get, NULL, "Get recharge status",
					 cmd_charger_module_recharge_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger module". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_charger_module, SHELL_CMD(charger, &sub_charger_module_charger, "Charger module", NULL),
	SHELL_CMD(full_cool, &sub_charger_module_full_cool, "Full charge in cool temp module",
		  NULL),
	SHELL_CMD(ntc_limits, &sub_charger_module_ntc_limits, "NTC limits module", NULL),
	SHELL_CMD(recharge, &sub_charger_module_recharge, "Recharge module", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_resistance cold". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_resistance_cold,
			       SHELL_CMD(set, NULL, "Set NTC resistance",
					 cmd_charger_ntc_resistance_cold_set),
			       SHELL_CMD(get, NULL, "Get NTC resistance",
					 cmd_charger_ntc_resistance_cold_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_resistance cool". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_resistance_cool,
			       SHELL_CMD(set, NULL, "Set NTC resistance",
					 cmd_charger_ntc_resistance_cool_set),
			       SHELL_CMD(get, NULL, "Get NTC resistance",
					 cmd_charger_ntc_resistance_cool_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_resistance warm". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_resistance_warm,
			       SHELL_CMD(set, NULL, "Set NTC resistance",
					 cmd_charger_ntc_resistance_warm_set),
			       SHELL_CMD(get, NULL, "Get NTC resistance",
					 cmd_charger_ntc_resistance_warm_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_resistance hot". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_resistance_hot,
			       SHELL_CMD(set, NULL, "Set NTC resistance",
					 cmd_charger_ntc_resistance_hot_set),
			       SHELL_CMD(get, NULL, "Get NTC resistance",
					 cmd_charger_ntc_resistance_hot_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger ntc_resistance". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_resistance,
			       SHELL_CMD(cold, &sub_charger_ntc_resistance_cold,
					 "NTC resistance when cold temperature", NULL),
			       SHELL_CMD(cool, &sub_charger_ntc_resistance_cool,
					 "NTC resistance when cool temperature", NULL),
			       SHELL_CMD(warm, &sub_charger_ntc_resistance_warm,
					 "NTC resistance when warm temperature", NULL),
			       SHELL_CMD(hot, &sub_charger_ntc_resistance_hot,
					 "NTC resistance when hot temperature", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_temperature cold". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_temperature_cold,
			       SHELL_CMD(set, NULL, "Set NTC temperature",
					 cmd_charger_ntc_temperature_cold_set),
			       SHELL_CMD(get, NULL, "Get NTC temperature",
					 cmd_charger_ntc_temperature_cold_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_temperature cool". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_temperature_cool,
			       SHELL_CMD(set, NULL, "Set NTC temperature",
					 cmd_charger_ntc_temperature_cool_set),
			       SHELL_CMD(get, NULL, "Get NTC temperature",
					 cmd_charger_ntc_temperature_cool_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_temperature warm". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_temperature_warm,
			       SHELL_CMD(set, NULL, "Set NTC temperature",
					 cmd_charger_ntc_temperature_warm_set),
			       SHELL_CMD(get, NULL, "Get NTC temperature",
					 cmd_charger_ntc_temperature_warm_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_temperature hot". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_temperature_hot,
			       SHELL_CMD(set, NULL, "Set NTC temperature at hot",
					 cmd_charger_ntc_temperature_hot_set),
			       SHELL_CMD(get, NULL, "Get NTC temperature at hot",
					 cmd_charger_ntc_temperature_hot_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger ntc_temperature". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_temperature,
			       SHELL_CMD(cold, &sub_charger_ntc_temperature_cold,
					 "NTC temperature when cold temperature", NULL),
			       SHELL_CMD(cool, &sub_charger_ntc_temperature_cool,
					 "NTC temperature when cool temperature", NULL),
			       SHELL_CMD(warm, &sub_charger_ntc_temperature_warm,
					 "NTC temperature when warm temperature", NULL),
			       SHELL_CMD(hot, &sub_charger_ntc_temperature_hot,
					 "NTC temperature when hot temperature", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger status all". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_status_all,
			       SHELL_CMD(get, NULL, "Get all status", cmd_charger_status_all_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger status charging". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_status_charging,
			       SHELL_CMD(get, NULL, "Get charging status",
					 cmd_charger_status_charging_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_status,
			       SHELL_CMD(all, &sub_charger_status_all, "All status", NULL),
			       SHELL_CMD(charging, &sub_charger_status_charging, "Charging status",
					 NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger termination_current". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_current,
			       SHELL_CMD(set, NULL, "Set charger termination current",
					 cmd_charger_termination_current_set),
			       SHELL_CMD(get, NULL, "Get charger termination current",
					 cmd_charger_termination_current_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger termination_voltage normal". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_voltage_normal,
			       SHELL_CMD(set, NULL, "Set charger normal termination voltage",
					 cmd_charger_termination_voltage_normal_set),
			       SHELL_CMD(get, NULL, "Get charger normal termination voltage",
					 cmd_charger_termination_voltage_normal_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger termination_voltage warm". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_voltage_warm,
			       SHELL_CMD(set, NULL, "Set charger warm termination voltage",
					 cmd_charger_termination_voltage_warm_set),
			       SHELL_CMD(get, NULL, "Get charger warm termination voltage",
					 cmd_charger_termination_voltage_warm_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger termination_voltage". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_voltage,
			       SHELL_CMD(normal, &sub_charger_termination_voltage_normal,
					 "Charger termination voltage normal", NULL),
			       SHELL_CMD(warm, &sub_charger_termination_voltage_warm,
					 "Charger termination voltage warm", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger trickle_voltage". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_trickle_voltage,
			       SHELL_CMD(set, NULL, "Set charger trickle voltage",
					 cmd_charger_trickle_voltage_set),
			       SHELL_CMD(get, NULL, "Get charger trickle voltage",
					 cmd_charger_trickle_voltage_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "charger". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_charger,
	SHELL_CMD(charging_current, &sub_charger_charging_current, "Charger current", NULL),
	SHELL_CMD(die_temp, &sub_charger_die_temp, "Charger die temperature", NULL),
	SHELL_CMD(discharging_current, &sub_charger_discharging_current,
		  "Maximum discharging current", NULL),
	SHELL_CMD(module, &sub_charger_module, "Charger module", NULL),
	SHELL_CMD(ntc_resistance, &sub_charger_ntc_resistance, "Battery NTC resistance values",
		  NULL),
	SHELL_CMD(ntc_temperature, &sub_charger_ntc_temperature, "Battery NTC temperature values",
		  NULL),
	SHELL_CMD(status, &sub_charger_status, "Charger status", NULL),
	SHELL_CMD(termination_current, &sub_charger_termination_current,
		  "Charger termination current", NULL),
	SHELL_CMD(termination_voltage, &sub_charger_termination_voltage,
		  "Charger termination voltage", NULL),
	SHELL_CMD(trickle_voltage, &sub_charger_trickle_voltage, "Charger trickle voltage", NULL),
	SHELL_SUBCMD_SET_END);

SHELL_SUBCMD_ADD((npmx), charger, &sub_charger, "Charger", NULL, 1, 0);
