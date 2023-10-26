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

/** @brief Timer config parameter. */
typedef enum {
	TIMER_CONFIG_PARAM_COMPARE, /* Configure timer compare value. */
	TIMER_CONFIG_PARAM_MODE, /* Configure timer mode. */
	TIMER_CONFIG_PARAM_PRESCALER, /* Configure timer prescaler mode. */
} timer_config_param_t;

static const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

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

SHELL_SUBCMD_ADD((npmx), reset, NULL, "Reset device", cmd_reset, 1, 0);
SHELL_SUBCMD_ADD((npmx), timer, &sub_timer, "Timer", NULL, 1, 0);
SHELL_SUBCMD_ADD((npmx), vbusin, &sub_vbusin, "VBUSIN", NULL, 1, 0);

/* Creating subcommands (level 1 command) array for command "npmx". */
SHELL_SUBCMD_SET_CREATE(sub_npmx, (npmx));

/* Creating root (level 0) command "npmx" without a handler. */
SHELL_CMD_REGISTER(npmx, &sub_npmx, "npmx", NULL);
