/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "shell_common.h"
#include <npmx_driver.h>

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

static npmx_ship_t *ship_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);

	return npmx_instance ? npmx_ship_get(npmx_instance, 0) : NULL;
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

/* Creating subcommands (level 4 command) array for command "ship config inv_polarity". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_config_inv_polarity,
			       SHELL_CMD(set, NULL, "Set inverted polarity status",
					 cmd_ship_config_inv_polarity_set),
			       SHELL_CMD(get, NULL, "Get inverted polarity status",
					 cmd_ship_config_inv_polarity_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ship config time". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_config_time,
			       SHELL_CMD(set, NULL, "Set ship exit time", cmd_ship_config_time_set),
			       SHELL_CMD(get, NULL, "Get ship exit time", cmd_ship_config_time_get),
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

SHELL_SUBCMD_ADD((npmx), ship, &sub_ship, "SHIP", NULL, 1, 0);
