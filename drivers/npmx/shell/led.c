/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "shell_common.h"
#include <npmx_driver.h>

static npmx_led_t *led_instance_get(const struct shell *shell, uint32_t index)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);
	bool status = check_instance_index(shell, "LED", index, NPM_LEDDRV_COUNT);

	return (npmx_instance && status) ? npmx_led_get(npmx_instance, (uint8_t)index) : NULL;
}

static int cmd_led_mode_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LED" },
					  [1] = { SHELL_ARG_TYPE_UINT32_VALUE, "mode" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_led_t *led_instance = led_instance_get(shell, args_info.arg[0].result.uvalue);
	if (led_instance == NULL) {
		return 0;
	}

	uint32_t mode = args_info.arg[1].result.uvalue;
	npmx_led_mode_t led_mode;
	switch (mode) {
	case 0:
		led_mode = NPMX_LED_MODE_ERROR;
		break;
	case 1:
		led_mode = NPMX_LED_MODE_CHARGING;
		break;
	case 2:
		led_mode = NPMX_LED_MODE_HOST;
		break;
	case 3:
		led_mode = NPMX_LED_MODE_NOTUSED;
		break;
	default:
		shell_error(shell, "Error: Wrong mode:");
		print_hint_error(shell, 0, "Charger error");
		print_hint_error(shell, 1, "Charging");
		print_hint_error(shell, 2, "Host");
		print_hint_error(shell, 3, "Not used");
		return 0;
	}

	npmx_error_t err_code = npmx_led_mode_set(led_instance, led_mode);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "LED mode");
		return 0;
	}

	print_success(shell, mode, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_led_mode_get(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LED" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_led_t *led_instance = led_instance_get(shell, args_info.arg[0].result.uvalue);
	if (led_instance == NULL) {
		return 0;
	}

	npmx_led_mode_t mode;
	npmx_error_t err_code = npmx_led_mode_get(led_instance, &mode);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "LED mode");
		return 0;
	}

	print_value(shell, (int)mode, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_led_state_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LED" },
					  [1] = { SHELL_ARG_TYPE_BOOL_VALUE, "state" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_led_t *led_instance = led_instance_get(shell, args_info.arg[0].result.uvalue);
	if (led_instance == NULL) {
		return 0;
	}

	bool led_state = args_info.arg[1].result.bvalue;
	npmx_error_t err_code = npmx_led_state_set(led_instance, led_state);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "LED state");
		return 0;
	}

	print_success(shell, led_state, UNIT_TYPE_NONE);
	return 0;
}

/* Creating subcommands (level 3 command) array for command "led mode". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_led_mode, SHELL_CMD(set, NULL, "Set LED mode", cmd_led_mode_set),
			       SHELL_CMD(get, NULL, "Get LED mode", cmd_led_mode_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "led state". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_led_state,
			       SHELL_CMD(set, NULL, "Set LED status", cmd_led_state_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "led". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_led, SHELL_CMD(mode, &sub_led_mode, "LED mode", NULL),
			       SHELL_CMD(state, &sub_led_state, "LED state", NULL),
			       SHELL_SUBCMD_SET_END);

SHELL_SUBCMD_ADD((npmx), led, &sub_led, "LED", NULL, 1, 0);
