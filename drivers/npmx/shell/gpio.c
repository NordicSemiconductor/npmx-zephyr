/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "shell_common.h"
#include <npmx_driver.h>

/** @brief GPIO configuration parameter. */
typedef enum {
	GPIO_CONFIG_PARAM_DEBOUNCE, /* GPIO debounce. */
	GPIO_CONFIG_PARAM_DRIVE, /* GPIO drive. */
	GPIO_CONFIG_PARAM_MODE, /* GPIO mode. */
	GPIO_CONFIG_PARAM_OPEN_DRAIN, /* GPIO open drain. */
	GPIO_CONFIG_PARAM_PULL, /* GPIO pull. */
	GPIO_CONFIG_PARAM_TYPE, /* Helper config to check if GPIO is output or input. */
} gpio_config_param_t;

static npmx_gpio_t *gpio_instance_get(const struct shell *shell, uint32_t index)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);
	bool status = check_instance_index(shell, "GPIO", index, NPM_GPIOS_COUNT);

	return (npmx_instance && status) ? npmx_gpio_get(npmx_instance, (uint8_t)index) : NULL;
}

static npmx_gpio_mode_t gpio_mode_convert(const struct shell *shell, uint32_t mode)
{
	switch (mode) {
	case 0:
		return NPMX_GPIO_MODE_INPUT;
	case 1:
		return NPMX_GPIO_MODE_INPUT_OVERRIDE_1;
	case 2:
		return NPMX_GPIO_MODE_INPUT_OVERRIDE_0;
	case 3:
		return NPMX_GPIO_MODE_INPUT_RISING_EDGE;
	case 4:
		return NPMX_GPIO_MODE_INPUT_FALLING_EDGE;
	case 5:
		return NPMX_GPIO_MODE_OUTPUT_IRQ;
	case 6:
		return NPMX_GPIO_MODE_OUTPUT_RESET;
	case 7:
		return NPMX_GPIO_MODE_OUTPUT_PLW;
	case 8:
		return NPMX_GPIO_MODE_OUTPUT_OVERRIDE_1;
	case 9:
		return NPMX_GPIO_MODE_OUTPUT_OVERRIDE_0;
	default:
		shell_error(shell, "Error: Wrong mode:");
		print_hint_error(shell, 0, "Input");
		print_hint_error(shell, 1, "Input logic 1");
		print_hint_error(shell, 2, "Input logic 0");
		print_hint_error(shell, 3, "Input rising edge event");
		print_hint_error(shell, 4, "Input falling edge event");
		print_hint_error(shell, 5, "Output interrupt");
		print_hint_error(shell, 6, "Output reset");
		print_hint_error(shell, 7, "Output power loss warning");
		print_hint_error(shell, 8, "Output logic 1");
		print_hint_error(shell, 9, "Output logic 0");
		return NPMX_GPIO_MODE_INVALID;
	}
}

static npmx_gpio_pull_t gpio_pull_convert(const struct shell *shell, uint32_t pull)
{
	switch (pull) {
	case 0:
		return NPMX_GPIO_PULL_DOWN;
	case 1:
		return NPMX_GPIO_PULL_UP;
	case 2:
		return NPMX_GPIO_PULL_NONE;
	default:
		shell_error(shell, "Error: Wrong pull:");
		print_hint_error(shell, 0, "Pull down");
		print_hint_error(shell, 1, "Pull up");
		print_hint_error(shell, 2, "Pull disable");
		return NPMX_GPIO_PULL_INVALID;
	}
}

static int gpio_config_set(const struct shell *shell, size_t argc, char **argv,
			   gpio_config_param_t config_type)
{
	char *config_name;
	shell_arg_type_t arg_type = SHELL_ARG_TYPE_UINT32_VALUE;
	switch (config_type) {
	case GPIO_CONFIG_PARAM_DEBOUNCE:
		config_name = "debounce";
		arg_type = SHELL_ARG_TYPE_BOOL_VALUE;
		break;
	case GPIO_CONFIG_PARAM_DRIVE:
		config_name = "drive current";
		break;
	case GPIO_CONFIG_PARAM_MODE:
		config_name = "mode";
		break;
	case GPIO_CONFIG_PARAM_OPEN_DRAIN:
		config_name = "open drain";
		arg_type = SHELL_ARG_TYPE_BOOL_VALUE;
		break;
	case GPIO_CONFIG_PARAM_PULL:
		config_name = "pull";
		break;
	case GPIO_CONFIG_PARAM_TYPE:
		return 0;
	}

	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "GPIO" },
					  [1] = { arg_type, config_name },
				  } };

	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_gpio_t *gpio_instance = gpio_instance_get(shell, args_info.arg[0].result.uvalue);
	if (gpio_instance == NULL) {
		return 0;
	}

	if (!check_pin_configuration_correctness(shell, (int8_t)(args_info.arg[0].result.uvalue))) {
		return 0;
	}

	npmx_gpio_config_t gpio_config;
	npmx_error_t err_code = npmx_gpio_config_get(gpio_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "GPIO config");
		return 0;
	}

	shell_arg_result_t result = args_info.arg[1].result;
	switch (config_type) {
	case GPIO_CONFIG_PARAM_DEBOUNCE:
		gpio_config.debounce = result.bvalue;
		break;
	case GPIO_CONFIG_PARAM_DRIVE:
		gpio_config.drive = npmx_gpio_drive_convert(result.uvalue);
		if (gpio_config.drive == NPMX_GPIO_DRIVE_INVALID) {
			shell_error(shell, "Error: Wrong drive current:");
			print_hint_error(shell, 1, "1 mA");
			print_hint_error(shell, 6, "6 mA");
			return 0;
		}
		break;
	case GPIO_CONFIG_PARAM_MODE:
		gpio_config.mode = gpio_mode_convert(shell, result.uvalue);
		if (gpio_config.mode == NPMX_GPIO_MODE_INVALID) {
			return 0;
		}
		break;
	case GPIO_CONFIG_PARAM_OPEN_DRAIN:
		gpio_config.open_drain = result.bvalue;
		break;
	case GPIO_CONFIG_PARAM_PULL:
		gpio_config.pull = gpio_pull_convert(shell, result.uvalue);
		if (gpio_config.pull == NPMX_GPIO_PULL_INVALID) {
			return 0;
		}
		break;
	default:
		return 0;
	}

	err_code = npmx_gpio_config_set(gpio_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "GPIO config");
		return 0;
	}

	switch (config_type) {
	case GPIO_CONFIG_PARAM_DRIVE:
		print_success(shell, result.uvalue, UNIT_TYPE_MILLIAMPERE);
		break;
	default:
		print_success(shell, result.uvalue, UNIT_TYPE_NONE);
		break;
	}

	return 0;
}

static int gpio_config_get(const struct shell *shell, size_t argc, char **argv,
			   gpio_config_param_t config_type)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "GPIO" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_gpio_t *gpio_instance = gpio_instance_get(shell, args_info.arg[0].result.uvalue);
	if (gpio_instance == NULL) {
		return 0;
	}

	npmx_gpio_config_t gpio_config;
	npmx_error_t err_code = npmx_gpio_config_get(gpio_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "GPIO config");
		return 0;
	}

	switch (config_type) {
	case GPIO_CONFIG_PARAM_DEBOUNCE:
		print_value(shell, gpio_config.debounce, UNIT_TYPE_NONE);
		break;
	case GPIO_CONFIG_PARAM_DRIVE:
		uint32_t current_ma = 0;
		if (!npmx_gpio_drive_convert_to_ma(gpio_config.drive, &current_ma)) {
			print_convert_error(shell, "gpio drive", "milliamperes");
			return 0;
		}
		print_value(shell, current_ma, UNIT_TYPE_MILLIAMPERE);
		break;
	case GPIO_CONFIG_PARAM_MODE:
		print_value(shell, (int)gpio_config.mode, UNIT_TYPE_NONE);
		break;
	case GPIO_CONFIG_PARAM_OPEN_DRAIN:
		print_value(shell, gpio_config.open_drain, UNIT_TYPE_NONE);
		break;
	case GPIO_CONFIG_PARAM_PULL:
		print_value(shell, (int)gpio_config.pull, UNIT_TYPE_NONE);
		break;
	case GPIO_CONFIG_PARAM_TYPE:
		if (gpio_config.mode <= NPMX_GPIO_MODE_INPUT_FALLING_EDGE) {
			shell_print(shell, "Value: input.");
		} else {
			shell_print(shell, "Value: output.");
		}
		break;
	}
	return 0;
}

static int cmd_gpio_config_debounce_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_PARAM_DEBOUNCE);
}

static int cmd_gpio_config_debounce_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_PARAM_DEBOUNCE);
}

static int cmd_gpio_config_drive_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_PARAM_DRIVE);
}

static int cmd_gpio_config_drive_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_PARAM_DRIVE);
}

static int cmd_gpio_config_mode_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_PARAM_MODE);
}

static int cmd_gpio_config_mode_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_PARAM_MODE);
}

static int cmd_gpio_config_open_drain_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_PARAM_OPEN_DRAIN);
}

static int cmd_gpio_config_open_drain_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_PARAM_OPEN_DRAIN);
}

static int cmd_gpio_config_pull_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_PARAM_PULL);
}

static int cmd_gpio_config_pull_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_PARAM_PULL);
}

static int cmd_gpio_status_get(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "GPIO" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_gpio_t *gpio_instance = gpio_instance_get(shell, args_info.arg[0].result.uvalue);
	if (gpio_instance == NULL) {
		return 0;
	}

	bool gpio_status;
	npmx_error_t err_code = npmx_gpio_status_check(gpio_instance, &gpio_status);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "GPIO status");
		return 0;
	}

	print_value(shell, gpio_status, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_gpio_type_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_PARAM_TYPE);
}

/* Creating subcommands (level 4 command) array for command "gpio config debounce". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_config_debounce,
			       SHELL_CMD(set, NULL, "Set debounce status",
					 cmd_gpio_config_debounce_set),
			       SHELL_CMD(get, NULL, "Get debounce status",
					 cmd_gpio_config_debounce_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "gpio config drive". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_config_drive,
			       SHELL_CMD(set, NULL, "Set drive status", cmd_gpio_config_drive_set),
			       SHELL_CMD(get, NULL, "Get drive status", cmd_gpio_config_drive_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "gpio config mode". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_config_mode,
			       SHELL_CMD(set, NULL, "Set GPIO mode", cmd_gpio_config_mode_set),
			       SHELL_CMD(get, NULL, "Get GPIO mode", cmd_gpio_config_mode_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "gpio config open_drain". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_config_open_drain,
			       SHELL_CMD(set, NULL, "Set open drain status",
					 cmd_gpio_config_open_drain_set),
			       SHELL_CMD(get, NULL, "Get open drain status",
					 cmd_gpio_config_open_drain_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio config pull". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_config_pull,
			       SHELL_CMD(set, NULL, "Set pull status", cmd_gpio_config_pull_set),
			       SHELL_CMD(get, NULL, "Get pull status", cmd_gpio_config_pull_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio config". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_gpio_config, SHELL_CMD(debounce, &sub_gpio_config_debounce, "Debounce config", NULL),
	SHELL_CMD(drive, &sub_gpio_config_drive, "Drive current config", NULL),
	SHELL_CMD(mode, &sub_gpio_config_mode, "GPIO mode config", NULL),
	SHELL_CMD(open_drain, &sub_gpio_config_open_drain, "Open drain config", NULL),
	SHELL_CMD(pull, &sub_gpio_config_pull, "Pull type config", NULL), SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_status,
			       SHELL_CMD(get, NULL, "Get GPIO status", cmd_gpio_status_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio type". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_type,
			       SHELL_CMD(get, NULL, "Get GPIO type", cmd_gpio_type_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "gpio". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio, SHELL_CMD(config, &sub_gpio_config, "GPIO config", NULL),
			       SHELL_CMD(status, &sub_gpio_status, "GPIO status", NULL),
			       SHELL_CMD(type, &sub_gpio_type, "GPIO type", NULL),
			       SHELL_SUBCMD_SET_END);

SHELL_SUBCMD_ADD((npmx), gpio, &sub_gpio, "GPIO", NULL, 1, 0);
