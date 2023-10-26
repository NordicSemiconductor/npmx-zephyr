/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <shell/shell_common.h>
#include <npmx.h>
#include <npmx_driver.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>

/** @brief POF config parameter. */
typedef enum {
	POF_CONFIG_PARAM_POLARITY, /* POF polarity. */
	POF_CONFIG_PARAM_STATUS, /* Enable POF. */
	POF_CONFIG_PARAM_THRESHOLD, /* POF threshold value. */
} pof_config_param_t;

/** @brief SHIP config parameter. */
typedef enum {
	SHIP_CONFIG_PARAM_INV_POLARITY, /* Device is to invert the SHPHLD button active status. */
	SHIP_CONFIG_PARAM_TIME, /* Time required to exit from the ship or the hibernate mode. */
} ship_config_param_t;

/** @brief SHIP reset config parameter. */
typedef enum {
	SHIP_RESET_CONFIG_PARAM_LONG_PRESS, /* Use long press (10 s) button. */
	SHIP_RESET_CONFIG_PARAM_TWO_BUTTONS /* Use two buttons (SHPHLD and GPIO0). */
} ship_reset_config_param_t;

/** @brief Timer config parameter. */
typedef enum {
	TIMER_CONFIG_PARAM_COMPARE, /* Configure timer compare value. */
	TIMER_CONFIG_PARAM_MODE, /* Configure timer mode. */
	TIMER_CONFIG_PARAM_PRESCALER, /* Configure timer prescaler mode. */
} timer_config_param_t;

static const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

static npmx_pof_t *pof_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);

	return npmx_instance ? npmx_pof_get(npmx_instance, 0) : NULL;
}

static npmx_ship_t *ship_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);

	return npmx_instance ? npmx_ship_get(npmx_instance, 0) : NULL;
}

static npmx_timer_t *timer_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);

	return npmx_instance ? npmx_timer_get(npmx_instance, 0) : NULL;
}

static npmx_vbusin_t *vbusin_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);

	return npmx_instance ? npmx_vbusin_get(npmx_instance, 0) : NULL;
}

static int pof_config_set(const struct shell *shell, size_t argc, char **argv,
			  pof_config_param_t config_type)
{
	char *config_name;
	shell_arg_type_t arg_type = SHELL_ARG_TYPE_UINT32_VALUE;
	unit_type_t unit_type = UNIT_TYPE_NONE;
	switch (config_type) {
	case POF_CONFIG_PARAM_POLARITY:
		config_name = "polarity";
		break;
	case POF_CONFIG_PARAM_STATUS:
		config_name = "status";
		arg_type = SHELL_ARG_TYPE_BOOL_VALUE;
		break;
	case POF_CONFIG_PARAM_THRESHOLD:
		config_name = "threshold";
		unit_type = UNIT_TYPE_MILLIVOLT;
		break;
	}

	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { arg_type, config_name },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_pof_t *pof_instance = pof_instance_get(shell);
	if (pof_instance == NULL) {
		return 0;
	}

	npmx_pof_config_t pof_config;
	npmx_error_t err_code = npmx_pof_config_get(pof_instance, &pof_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "POF config");
		return 0;
	}

	shell_arg_result_t result = args_info.arg[0].result;
	switch (config_type) {
	case POF_CONFIG_PARAM_POLARITY:
		uint32_t polarity = result.uvalue;
		if (polarity >= NPMX_POF_POLARITY_COUNT) {
			shell_error(shell, "Error: Wrong polarity:");
			print_hint_error(shell, 0, "Active low");
			print_hint_error(shell, 1, "Active high");
			return 0;
		}
		pof_config.polarity =
			(polarity == 1) ? NPMX_POF_POLARITY_HIGH : NPMX_POF_POLARITY_LOW;
		break;
	case POF_CONFIG_PARAM_STATUS:
		bool status = result.bvalue;
		pof_config.status = status ? NPMX_POF_STATUS_ENABLE : NPMX_POF_STATUS_DISABLE;
		break;
	case POF_CONFIG_PARAM_THRESHOLD:
		uint32_t threshold = result.uvalue;
		pof_config.threshold = npmx_pof_threshold_convert(threshold);
		if (pof_config.threshold == NPMX_POF_THRESHOLD_INVALID) {
			print_convert_error(shell, "millivolts", "threshold");
			return 0;
		}
		break;
	}

	err_code = npmx_pof_config_set(pof_instance, &pof_config);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "POF config");
		return 0;
	}

	print_success(shell, result.ivalue, unit_type);
	return 0;
}

static int pof_config_get(const struct shell *shell, pof_config_param_t config_type)
{
	npmx_pof_t *pof_instance = pof_instance_get(shell);
	if (pof_instance == NULL) {
		return 0;
	}

	npmx_pof_config_t pof_config;
	npmx_error_t err_code = npmx_pof_config_get(pof_instance, &pof_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "POF config");
		return 0;
	}

	switch (config_type) {
	case POF_CONFIG_PARAM_POLARITY:
		print_value(shell, pof_config.polarity, UNIT_TYPE_NONE);
		break;
	case POF_CONFIG_PARAM_STATUS:
		print_value(shell, pof_config.status, UNIT_TYPE_NONE);
		break;
	case POF_CONFIG_PARAM_THRESHOLD:
		uint32_t voltage_mv;
		if (!npmx_pof_threshold_convert_to_mv(pof_config.threshold, &voltage_mv)) {
			print_convert_error(shell, "threshold", "millivolts");
			return 0;
		}
		print_value(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
		break;
	}
	return 0;
}

static int cmd_pof_polarity_set(const struct shell *shell, size_t argc, char **argv)
{
	return pof_config_set(shell, argc, argv, POF_CONFIG_PARAM_POLARITY);
}

static int cmd_pof_polarity_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return pof_config_get(shell, POF_CONFIG_PARAM_POLARITY);
}

static int cmd_pof_status_set(const struct shell *shell, size_t argc, char **argv)
{
	return pof_config_set(shell, argc, argv, POF_CONFIG_PARAM_STATUS);
}

static int cmd_pof_status_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return pof_config_get(shell, POF_CONFIG_PARAM_STATUS);
}

static int cmd_pof_threshold_set(const struct shell *shell, size_t argc, char **argv)
{
	return pof_config_set(shell, argc, argv, POF_CONFIG_PARAM_THRESHOLD);
}

static int cmd_pof_threshold_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return pof_config_get(shell, POF_CONFIG_PARAM_THRESHOLD);
}

static int cmd_reset(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);
	if (npmx_instance == NULL) {
		return 0;
	}

	npmx_error_t err_code = npmx_core_task_trigger(npmx_instance, NPMX_CORE_TASK_RESET);
	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to reset device.");
		return 0;
	}

	shell_print(shell, "Success: resetting.");
	return 0;
}

static int ship_config_set(const struct shell *shell, size_t argc, char **argv,
			   ship_config_param_t config_type)
{
	shell_arg_type_t arg_type;
	const char *config_name;
	switch (config_type) {
	case SHIP_CONFIG_PARAM_TIME:
		arg_type = SHELL_ARG_TYPE_UINT32_VALUE;
		config_name = "time";
		break;
	case SHIP_CONFIG_PARAM_INV_POLARITY:
		arg_type = SHELL_ARG_TYPE_BOOL_VALUE;
		config_name = "polarity";
		break;
	}

	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { arg_type, config_name },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ship_t *ship_instance = ship_instance_get(shell);
	if (ship_instance == NULL) {
		return 0;
	}

	npmx_ship_config_t config;
	npmx_error_t err_code = npmx_ship_config_get(ship_instance, &config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "ship config");
		return 0;
	}

	shell_arg_result_t config_value = args_info.arg[0].result;
	switch (config_type) {
	case SHIP_CONFIG_PARAM_TIME:
		config.time = npmx_ship_time_convert(config_value.uvalue);
		if (config.time == NPMX_SHIP_TIME_INVALID) {
			print_convert_error(shell, "milliseconds", "ship time");
			return 0;
		}
		break;
	case SHIP_CONFIG_PARAM_INV_POLARITY:
		config.inverted_polarity = config_value.bvalue;
		break;
	}

	err_code = npmx_ship_config_set(ship_instance, &config);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "ship config");
		return 0;
	}

	print_success(shell, config_value.uvalue, UNIT_TYPE_NONE);
	return 0;
}

static int ship_config_get(const struct shell *shell, ship_config_param_t config_type)
{
	npmx_ship_t *ship_instance = ship_instance_get(shell);
	if (ship_instance == NULL) {
		return 0;
	}

	npmx_ship_config_t config;
	npmx_error_t err_code = npmx_ship_config_get(ship_instance, &config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "ship config");
		return 0;
	}

	switch (config_type) {
	case SHIP_CONFIG_PARAM_TIME:
		uint32_t time;
		if (!npmx_ship_time_convert_to_ms(config.time, &time)) {
			print_convert_error(shell, "ship time", "milliseconds");
			return 0;
		}
		print_value(shell, time, UNIT_TYPE_NONE);
		break;
	case SHIP_CONFIG_PARAM_INV_POLARITY:
		print_value(shell, config.inverted_polarity, UNIT_TYPE_NONE);
		break;
	}
	return 0;
}

static int cmd_ship_config_inv_polarity_set(const struct shell *shell, size_t argc, char **argv)
{
	return ship_config_set(shell, argc, argv, SHIP_CONFIG_PARAM_INV_POLARITY);
}

static int cmd_ship_config_inv_polarity_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return ship_config_get(shell, SHIP_CONFIG_PARAM_INV_POLARITY);
}

static int cmd_ship_config_time_set(const struct shell *shell, size_t argc, char **argv)
{
	return ship_config_set(shell, argc, argv, SHIP_CONFIG_PARAM_TIME);
}

static int cmd_ship_config_time_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return ship_config_get(shell, SHIP_CONFIG_PARAM_TIME);
}

static int ship_mode_set(const struct shell *shell, npmx_ship_task_t ship_task)
{
	npmx_ship_t *ship_instance = ship_instance_get(shell);
	if (ship_instance == NULL) {
		return 0;
	}

	npmx_error_t err_code = npmx_ship_task_trigger(ship_instance, ship_task);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "ship mode");
		return 0;
	}

	print_success(shell, true, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_ship_mode_hibernate_set(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return ship_mode_set(shell, NPMX_SHIP_TASK_HIBERNATE);
}

static int cmd_ship_mode_ship_set(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return ship_mode_set(shell, NPMX_SHIP_TASK_SHIPMODE);
}

static int ship_reset_config_set(const struct shell *shell, size_t argc, char **argv,
				 ship_reset_config_param_t config_type)
{
	const char *config_name;
	switch (config_type) {
	case SHIP_RESET_CONFIG_PARAM_LONG_PRESS:
		config_name = "long press";
		break;
	case SHIP_RESET_CONFIG_PARAM_TWO_BUTTONS:
		config_name = "two buttons";
		break;
	}

	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_BOOL_VALUE, config_name },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ship_t *ship_instance = ship_instance_get(shell);
	if (ship_instance == NULL) {
		return 0;
	}

	npmx_ship_reset_config_t reset_config;
	npmx_error_t err_code = npmx_ship_reset_config_get(ship_instance, &reset_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "reset config");
		return 0;
	}

	bool config_value = args_info.arg[0].result.bvalue;
	switch (config_type) {
	case SHIP_RESET_CONFIG_PARAM_LONG_PRESS:
		reset_config.long_press = config_value;
		break;
	case SHIP_RESET_CONFIG_PARAM_TWO_BUTTONS:
		reset_config.two_buttons = config_value;
		break;
	}

	err_code = npmx_ship_reset_config_set(ship_instance, &reset_config);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "reset config");
		return 0;
	}

	print_success(shell, config_value, UNIT_TYPE_NONE);
	return 0;
}

static int ship_reset_config_get(const struct shell *shell, ship_reset_config_param_t config_type)
{
	npmx_ship_t *ship_instance = ship_instance_get(shell);
	if (ship_instance == NULL) {
		return 0;
	}

	npmx_ship_reset_config_t reset_config;
	npmx_error_t err_code = npmx_ship_reset_config_get(ship_instance, &reset_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "reset config");
	}

	switch (config_type) {
	case SHIP_RESET_CONFIG_PARAM_LONG_PRESS:
		print_value(shell, reset_config.long_press, UNIT_TYPE_NONE);
		break;
	case SHIP_RESET_CONFIG_PARAM_TWO_BUTTONS:
		print_value(shell, reset_config.two_buttons, UNIT_TYPE_NONE);
		break;
	}
	return 0;
}

static int cmd_ship_reset_long_press_set(const struct shell *shell, size_t argc, char **argv)
{
	return ship_reset_config_set(shell, argc, argv, SHIP_RESET_CONFIG_PARAM_LONG_PRESS);
}

static int cmd_ship_reset_long_press_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return ship_reset_config_get(shell, SHIP_RESET_CONFIG_PARAM_LONG_PRESS);
}

static int cmd_ship_reset_two_buttons_set(const struct shell *shell, size_t argc, char **argv)
{
	return ship_reset_config_set(shell, argc, argv, SHIP_RESET_CONFIG_PARAM_TWO_BUTTONS);
}

static int cmd_ship_reset_two_buttons_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return ship_reset_config_get(shell, SHIP_RESET_CONFIG_PARAM_TWO_BUTTONS);
}

static npmx_timer_mode_t timer_mode_int_convert_to_enum(const struct shell *shell,
							uint32_t timer_mode)
{
	switch (timer_mode) {
	case 0:
		return NPMX_TIMER_MODE_BOOT_MONITOR;
	case 1:
		return NPMX_TIMER_MODE_WATCHDOG_WARNING;
	case 2:
		return NPMX_TIMER_MODE_WATCHDOG_RESET;
	case 3:
		return NPMX_TIMER_MODE_GENERAL_PURPOSE;
	case 4:
		return NPMX_TIMER_MODE_WAKEUP;
	default:
		shell_error(shell, "Error: Wrong mode:");
		print_hint_error(shell, 0, "Boot monitor");
		print_hint_error(shell, 1, "Watchdog warning");
		print_hint_error(shell, 2, "Watchdog reset");
		print_hint_error(shell, 3, "General purpose");
		print_hint_error(shell, 4, "Wakeup");
		return NPMX_TIMER_MODE_INVALID;
	}
}

static int timer_config_set(const struct shell *shell, size_t argc, char **argv,
			    timer_config_param_t config_type)
{
	char *config_name;
	switch (config_type) {
	case TIMER_CONFIG_PARAM_MODE:
		config_name = "mode";
		break;
	case TIMER_CONFIG_PARAM_PRESCALER:
		config_name = "prescaler";
		break;
	case TIMER_CONFIG_PARAM_COMPARE:
		config_name = "compare";
		break;
	}

	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE, config_name },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_timer_t *timer_instance = timer_instance_get(shell);
	if (timer_instance == NULL) {
		return 0;
	}

	npmx_timer_config_t timer_config;
	npmx_error_t err_code = npmx_timer_config_get(timer_instance, &timer_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "timer config");
		return 0;
	}

	uint32_t config_value = args_info.arg[0].result.uvalue;
	switch (config_type) {
	case TIMER_CONFIG_PARAM_MODE:
		timer_config.mode = timer_mode_int_convert_to_enum(shell, config_value);
		if (timer_config.mode == NPMX_TIMER_MODE_INVALID) {
			return 0;
		}
		break;
	case TIMER_CONFIG_PARAM_PRESCALER:
		if (config_value >= NPMX_TIMER_PRESCALER_COUNT) {
			shell_error(shell, "Error: Wrong prescaler:");
			print_hint_error(shell, 0, "Slow");
			print_hint_error(shell, 1, "Fast");
			return 0;
		}

		timer_config.prescaler =
			(config_value ? NPMX_TIMER_PRESCALER_FAST : NPMX_TIMER_PRESCALER_SLOW);
		break;
	case TIMER_CONFIG_PARAM_COMPARE:
		if (!range_check(shell, config_value, 0, NPM_TIMER_COUNTER_COMPARE_VALUE_MAX,
				 "compare")) {
			return 0;
		}

		timer_config.compare_value = config_value;
		break;
	}

	err_code = npmx_timer_config_set(timer_instance, &timer_config);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "timer config");
		return 0;
	}

	print_success(shell, config_value, UNIT_TYPE_NONE);
	return 0;
}

static int timer_config_get(const struct shell *shell, timer_config_param_t config_type)
{
	npmx_timer_t *timer_instance = timer_instance_get(shell);
	if (timer_instance == NULL) {
		return 0;
	}

	npmx_timer_config_t timer_config;
	npmx_error_t err_code = npmx_timer_config_get(timer_instance, &timer_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "timer config");
		return 0;
	}

	switch (config_type) {
	case TIMER_CONFIG_PARAM_MODE:
		print_value(shell, (int)timer_config.mode, UNIT_TYPE_NONE);
		break;
	case TIMER_CONFIG_PARAM_PRESCALER:
		print_value(shell, (int)timer_config.prescaler, UNIT_TYPE_NONE);
		break;
	case TIMER_CONFIG_PARAM_COMPARE:
		print_value(shell, timer_config.compare_value, UNIT_TYPE_NONE);
		break;
	}
	return 0;
}

static int cmd_timer_config_compare_set(const struct shell *shell, size_t argc, char **argv)
{
	return timer_config_set(shell, argc, argv, TIMER_CONFIG_PARAM_COMPARE);
}

static int cmd_timer_config_compare_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return timer_config_get(shell, TIMER_CONFIG_PARAM_COMPARE);
}

static int cmd_timer_config_mode_set(const struct shell *shell, size_t argc, char **argv)
{
	return timer_config_set(shell, argc, argv, TIMER_CONFIG_PARAM_MODE);
}

static int cmd_timer_config_mode_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return timer_config_get(shell, TIMER_CONFIG_PARAM_MODE);
}

static int cmd_timer_config_prescaler_set(const struct shell *shell, size_t argc, char **argv)
{
	return timer_config_set(shell, argc, argv, TIMER_CONFIG_PARAM_PRESCALER);
}

static int cmd_timer_config_prescaler_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return timer_config_get(shell, TIMER_CONFIG_PARAM_PRESCALER);
}
static int timer_trigger_task(const struct shell *shell, npmx_timer_task_t task)
{
	npmx_timer_t *timer_instance = timer_instance_get(shell);
	if (timer_instance == NULL) {
		return 0;
	}

	npmx_error_t err_code = npmx_timer_task_trigger(timer_instance, task);

	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "timer task");
		return 0;
	}

	print_success(shell, true, UNIT_TYPE_NONE);

	return 0;
}

static int cmd_timer_config_strobe(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return timer_trigger_task(shell, NPMX_TIMER_TASK_STROBE);
}

static int cmd_timer_disable(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return timer_trigger_task(shell, NPMX_TIMER_TASK_DISABLE);
}

static int cmd_timer_enable(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return timer_trigger_task(shell, NPMX_TIMER_TASK_ENABLE);
}

static int cmd_timer_watchdog_kick(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return timer_trigger_task(shell, NPMX_TIMER_TASK_KICK);
}

static int cmd_vbusin_current_limit_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE, "current limit" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_vbusin_t *vbusin_instance = vbusin_instance_get(shell);
	if (vbusin_instance == NULL) {
		return 0;
	}

	uint32_t current_limit_ma = args_info.arg[0].result.uvalue;
	npmx_vbusin_current_t vbusin_current = npmx_vbusin_current_convert(current_limit_ma);
	if (vbusin_current == NPMX_VBUSIN_CURRENT_INVALID) {
		print_convert_error(shell, "milliamperes", "current limit");
		return 0;
	}

	npmx_error_t err_code = npmx_vbusin_current_limit_set(vbusin_instance, vbusin_current);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "current limit");
		return 0;
	}

	print_success(shell, current_limit_ma, UNIT_TYPE_MILLIAMPERE);
	return 0;
}

static int cmd_vbusin_current_limit_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_vbusin_t *vbusin_instance = vbusin_instance_get(shell);
	if (vbusin_instance == NULL) {
		return 0;
	}

	npmx_vbusin_current_t vbusin_current;
	npmx_error_t err_code = npmx_vbusin_current_limit_get(vbusin_instance, &vbusin_current);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "current limit");
		return 0;
	}

	uint32_t current_limit_ma;
	if (!npmx_vbusin_current_convert_to_ma(vbusin_current, &current_limit_ma)) {
		print_convert_error(shell, "current limit", "milliamperes");
		return 0;
	}

	print_value(shell, current_limit_ma, UNIT_TYPE_MILLIAMPERE);
	return 0;
}

static int cmd_vbusin_status_cc_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_vbusin_t *vbusin_instance = vbusin_instance_get(shell);
	if (vbusin_instance == NULL) {
		return 0;
	}

	npmx_vbusin_cc_t cc1;
	npmx_vbusin_cc_t cc2;
	npmx_error_t err_code = npmx_vbusin_cc_status_get(vbusin_instance, &cc1, &cc2);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "VBUS CC line status");
		return 0;
	}

	if (cc1 == NPMX_VBUSIN_CC_NOT_CONNECTED) {
		print_value(shell, (int)cc2, UNIT_TYPE_NONE);
	} else {
		print_value(shell, (int)cc1, UNIT_TYPE_NONE);
	}
	return 0;
}

static int cmd_vbusin_status_connected_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_vbusin_t *vbusin_instance = vbusin_instance_get(shell);
	if (vbusin_instance == NULL) {
		return 0;
	}

	uint8_t status_mask;
	npmx_error_t err_code = npmx_vbusin_vbus_status_get(vbusin_instance, &status_mask);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "VBUS connected status");
		return 0;
	}

	print_value(shell, !!(status_mask & NPMX_VBUSIN_STATUS_CONNECTED_MASK), UNIT_TYPE_NONE);
	return 0;
}

/* Creating subcommands (level 3 command) array for command "pof polarity". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_pof_polarity, SHELL_CMD(set, NULL, "Set POF warning polarity", cmd_pof_polarity_set),
	SHELL_CMD(get, NULL, "Get POF warning polarity", cmd_pof_polarity_get),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "pof status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_pof_status,
			       SHELL_CMD(set, NULL, "Set POF status", cmd_pof_status_set),
			       SHELL_CMD(get, NULL, "Get POF status", cmd_pof_status_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "pof threshold". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_pof_threshold,
			       SHELL_CMD(set, NULL, "Set Vsys comparator threshold",
					 cmd_pof_threshold_set),
			       SHELL_CMD(get, NULL, "Get Vsys comparator threshold",
					 cmd_pof_threshold_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "pof". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_pof, SHELL_CMD(polarity, &sub_pof_polarity, "Power failure warning polarity", NULL),
	SHELL_CMD(status, &sub_pof_status, "Status power failure feature", NULL),
	SHELL_CMD(threshold, &sub_pof_threshold, "Vsys comparator threshold select", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ship config time". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_config_time,
			       SHELL_CMD(set, NULL, "Set ship exit time", cmd_ship_config_time_set),
			       SHELL_CMD(get, NULL, "Get ship exit time", cmd_ship_config_time_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ship config inv_polarity". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_config_inv_polarity,
			       SHELL_CMD(set, NULL, "Set inverted polarity status",
					 cmd_ship_config_inv_polarity_set),
			       SHELL_CMD(get, NULL, "Get inverted polarity status",
					 cmd_ship_config_inv_polarity_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "ship config". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_config,
			       SHELL_CMD(inv_polarity, &sub_ship_config_inv_polarity,
					 "Button invert polarity", NULL),
			       SHELL_CMD(time, &sub_ship_config_time, "Time", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "ship mode". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_mode,
			       SHELL_CMD(hibernate, NULL, "Enter hibernate mode",
					 cmd_ship_mode_hibernate_set),
			       SHELL_CMD(ship, NULL, "Enter ship mode", cmd_ship_mode_ship_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ship reset long_press". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_reset_long_press,
			       SHELL_CMD(set, NULL, "Set long press status",
					 cmd_ship_reset_long_press_set),
			       SHELL_CMD(get, NULL, "Get long press status",
					 cmd_ship_reset_long_press_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ship reset two_buttons". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_reset_two_buttons,
			       SHELL_CMD(set, NULL, "Set two buttons status",
					 cmd_ship_reset_two_buttons_set),
			       SHELL_CMD(get, NULL, "Get two buttons status",
					 cmd_ship_reset_two_buttons_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "ship reset". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_ship_reset, SHELL_CMD(long_press, &sub_ship_reset_long_press, "Long press", NULL),
	SHELL_CMD(two_buttons, &sub_ship_reset_two_buttons, "Two buttons", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "ship". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship, SHELL_CMD(config, &sub_ship_config, "Ship config", NULL),
			       SHELL_CMD(mode, &sub_ship_mode, "Set ship mode", NULL),
			       SHELL_CMD(reset, &sub_ship_reset, "Reset button config", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "timer config compare". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_timer_config_compare,
			       SHELL_CMD(set, NULL, "Set timer compare",
					 cmd_timer_config_compare_set),
			       SHELL_CMD(get, NULL, "Get timer compare",
					 cmd_timer_config_compare_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "timer config mode". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_timer_config_mode,
			       SHELL_CMD(set, NULL, "Set timer mode", cmd_timer_config_mode_set),
			       SHELL_CMD(get, NULL, "Get timer mode", cmd_timer_config_mode_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "timer config prescaler". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_timer_config_prescaler,
			       SHELL_CMD(set, NULL, "Set timer prescaler",
					 cmd_timer_config_prescaler_set),
			       SHELL_CMD(get, NULL, "Get timer prescaler",
					 cmd_timer_config_prescaler_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "timer config". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_timer_config,
	SHELL_CMD(compare, &sub_timer_config_compare, "Timer compare value", NULL),
	SHELL_CMD(mode, &sub_timer_config_mode, "Timer mode selection", NULL),
	SHELL_CMD(prescaler, &sub_timer_config_prescaler, "Timer prescaler selection", NULL),
	SHELL_CMD(strobe, NULL, "Timer strobe", cmd_timer_config_strobe), SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "timer watchdog". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_timer_watchdog,
			       SHELL_CMD(kick, NULL, "Kick watchdog timer",
					 cmd_timer_watchdog_kick),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "timer". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_timer,
			       SHELL_CMD(config, &sub_timer_config, "Timer config", NULL),
			       SHELL_CMD(disable, NULL, "Timer stop", cmd_timer_disable),
			       SHELL_CMD(enable, NULL, "Timer start", cmd_timer_enable),
			       SHELL_CMD(watchdog, &sub_timer_watchdog, "Timer watchdog", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "vbusin current_limit". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vbusin_current_limit,
			       SHELL_CMD(set, NULL, "Set VBUS current limit",
					 cmd_vbusin_current_limit_set),
			       SHELL_CMD(get, NULL, "Get VBUS current limit",
					 cmd_vbusin_current_limit_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "vbusin status cc". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vbusin_status_cc,
			       SHELL_CMD(get, NULL, "Get VBUS CC status", cmd_vbusin_status_cc_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "vbusin status connected". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vbusin_status_connected,
			       SHELL_CMD(get, NULL, "Get VBUS connected status",
					 cmd_vbusin_status_connected_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "vbusin status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vbusin_status,
			       SHELL_CMD(cc, &sub_vbusin_status_cc, "VBUS CC", NULL),
			       SHELL_CMD(connected, &sub_vbusin_status_connected, "VBUS connected",
					 NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "vbusin". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_vbusin, SHELL_CMD(current_limit, &sub_vbusin_current_limit, "Current limit", NULL),
	SHELL_CMD(status, &sub_vbusin_status, "Status", NULL), SHELL_SUBCMD_SET_END);

SHELL_SUBCMD_ADD((npmx), pof, &sub_pof, "POF", NULL, 1, 0);
SHELL_SUBCMD_ADD((npmx), reset, NULL, "Reset device", cmd_reset, 1, 0);
SHELL_SUBCMD_ADD((npmx), ship, &sub_ship, "SHIP", NULL, 1, 0);
SHELL_SUBCMD_ADD((npmx), timer, &sub_timer, "Timer", NULL, 1, 0);
SHELL_SUBCMD_ADD((npmx), vbusin, &sub_vbusin, "VBUSIN", NULL, 1, 0);

/* Creating subcommands (level 1 command) array for command "npmx". */
SHELL_SUBCMD_SET_CREATE(sub_npmx, (npmx));

/* Creating root (level 0) command "npmx" without a handler. */
SHELL_CMD_REGISTER(npmx, &sub_npmx, "npmx", NULL);
