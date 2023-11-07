/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "shell_common.h"
#include <zephyr/kernel.h>
#include <npmx_driver.h>

static const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

static const char *unit_str_get(unit_type_t unit_type)
{
	switch (unit_type) {
	case UNIT_TYPE_MILLIAMPERE:
		return " mA";
	case UNIT_TYPE_MILLIVOLT:
		return " mV";
	case UNIT_TYPE_CELSIUS:
		return "*C";
	case UNIT_TYPE_OHM:
		return " ohms";
	case UNIT_TYPE_PCT:
		return "%";
	default:
		return "";
	}
}

static const char *message_ending_get(shell_arg_type_t arg_type)
{
	switch (arg_type) {
	case SHELL_ARG_TYPE_INT32_VALUE:
	case SHELL_ARG_TYPE_UINT32_VALUE:
	case SHELL_ARG_TYPE_BOOL_VALUE:
		return "value";
	case SHELL_ARG_TYPE_UINT32_INDEX:
		return "instance index";
	}
	return "";
}

bool check_error_code(const struct shell *shell, npmx_error_t err_code)
{
	switch (err_code) {
	case NPMX_SUCCESS:
		return true;
	case NPMX_ERROR_INVALID_PARAM: {
		shell_error(shell, "Error: invalid parameter for npmx function.");
		return false;
	}
	case NPMX_ERROR_IO: {
		shell_error(shell, "Error: IO error.");
		return false;
	}
	case NPMX_ERROR_INVALID_MEAS:
		shell_error(shell, "Error: invalid measurement.");
		return false;
	}

	return false;
}

void print_value(const struct shell *shell, int value, unit_type_t unit_type)
{
	shell_print(shell, "Value: %d%s.", value, unit_str_get(unit_type));
}

void print_success(const struct shell *shell, int value, unit_type_t unit_type)
{
	shell_print(shell, "Success: %d%s.", value, unit_str_get(unit_type));
}

void print_hint_error(const struct shell *shell, int32_t index, const char *str)
{
	shell_error(shell, "%d-%s", index, str);
}

void print_set_error(const struct shell *shell, const char *str)
{
	shell_error(shell, "Error: unable to set %s.", str);
}

void print_get_error(const struct shell *shell, const char *str)
{
	shell_error(shell, "Error: unable to get %s.", str);
}

void print_convert_error(const struct shell *shell, const char *src, const char *dst)
{
	shell_error(shell, "Error: unable to convert %s to %s.", src, dst);
}

bool arguments_check(const struct shell *shell, size_t argc, char **argv, args_info_t *args_info)
{
	__ASSERT_NO_MSG(args_info);
	__ASSERT_NO_MSG(args_info->expected_args <= SHELL_ARG_MAX_COUNT);

	/* argc and argv have also an information about command name, here it should be skipped. */
	argc--;
	/* Only arguments passed to commands are used. Command name is skipped. */
	argv++;

	/* Check if all argument are passed. */
	if (argc < args_info->expected_args) {
		shell_arg_t *missing_args = &args_info->arg[argc];
		int missing_count = args_info->expected_args - argc;
		switch (missing_count) {
		case 1:
			shell_error(shell, "Error: missing %s %s.", missing_args[0].name,
				    message_ending_get(missing_args[0].type));
			break;
		case 2:
			shell_error(shell, "Error: missing %s %s and %s %s.", missing_args[0].name,
				    message_ending_get(missing_args[0].type), missing_args[1].name,
				    message_ending_get(missing_args[1].type));
			break;
		case 3:
			shell_error(shell, "Error: missing %s %s, %s %s and %s %s.",
				    missing_args[0].name, message_ending_get(missing_args[0].type),
				    missing_args[1].name, message_ending_get(missing_args[1].type),
				    missing_args[2].name, message_ending_get(missing_args[2].type));
			break;
		}
		return false;
	}

	for (int i = 0; i < args_info->expected_args; i++) {
		shell_arg_t *arg_info = &args_info->arg[i];
		__ASSERT_NO_MSG(arg_info);

		int err = 0;
		switch (arg_info->type) {
		case SHELL_ARG_TYPE_INT32_VALUE:
			arg_info->result.ivalue = shell_strtol(argv[i], 0, &err);
			if (err != 0) {
				shell_error(shell, "Error: %s has to be an integer.",
					    arg_info->name);
				return false;
			}
			break;
		case SHELL_ARG_TYPE_UINT32_VALUE:
		case SHELL_ARG_TYPE_UINT32_INDEX:
			arg_info->result.uvalue = shell_strtoul(argv[i], 0, &err);
			if (err != 0) {
				shell_error(shell, "Error: %s has to be a non-negative integer.",
					    arg_info->name);
				return false;
			}
			break;
		case SHELL_ARG_TYPE_BOOL_VALUE:
			const char *str = argv[i];
			/* shell_strtobool can't be used due to accepting any big value as true. */
			if ((strcmp(str, "on") == 0) || (strcmp(str, "enable") == 0) ||
			    (strcmp(str, "true") == 0)) {
				arg_info->result.bvalue = true;
				break;
			}
			if ((strcmp(str, "off") == 0) || (strcmp(str, "disable") == 0) ||
			    (strcmp(str, "false") == 0)) {
				arg_info->result.bvalue = false;
				break;
			}

			uint32_t bool_value = shell_strtoul(str, 0, &err);
			if ((err != 0) || (bool_value > 1)) {
				shell_error(shell, "Error: invalid %s value.", arg_info->name);
				return false;
			}

			arg_info->result.bvalue = bool_value == 1;
			break;
		}
	}

	return true;
}

bool check_instance_index(const struct shell *shell, const char *instance_name, uint32_t index,
			  uint32_t max_index)
{
	if (index >= max_index) {
		shell_error(shell, "Error: %s instance index is too high: no such instance.",
			    instance_name);
		return false;
	}

	return true;
}

npmx_instance_t *npmx_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);
	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return NULL;
	}

	return npmx_instance;
}

bool range_check(const struct shell *shell, int32_t value, int32_t min, int32_t max,
		 const char *name)
{
	if ((value >= min) && (value <= max)) {
		return true;
	}

	shell_error(shell, "Error: %s value out of range.", name);
	return false;
}

bool check_pin_configuration_correctness(const struct shell *shell, int32_t gpio_idx)
{
	int32_t pmic_int_pin = npmx_driver_int_pin_get(pmic_dev);
	int32_t pmic_pof_pin = npmx_driver_pof_pin_get(pmic_dev);

	if ((pmic_int_pin != -1) && (pmic_int_pin == gpio_idx)) {
		shell_error(shell, "Error: GPIO used as interrupt.");
		return false;
	}

	if ((pmic_pof_pin != -1) && (pmic_pof_pin == gpio_idx)) {
		shell_error(shell, "Error: GPIO used as POF.");
		return false;
	}

	return true;
}

bool charger_disabled_check(const struct shell *shell, npmx_charger_t *charger_instance,
			    const char *help)
{
	uint32_t modules_mask;
	npmx_error_t err_code = npmx_charger_module_get(charger_instance, &modules_mask);

	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "charger module status");
		return false;
	}

	if ((modules_mask & NPMX_CHARGER_MODULE_CHARGER_MASK) != 0) {
		shell_error(shell, "Error: charger must be disabled to set %s.", help);
		return false;
	}
	return true;
}

void value_difference_info(const struct shell *shell, shell_arg_type_t arg_type, uint32_t value_set,
			   uint32_t value_get)
{
	if (value_set != value_get) {
		if (arg_type == SHELL_ARG_TYPE_INT32_VALUE) {
			shell_info(
				shell,
				"Info: Requested value was %i but reading will return %i due to approximations.",
				(int32_t)value_set, (int32_t)value_get);
		} else {
			shell_info(
				shell,
				"Info: Requested value was %u but reading will return %u due to approximations.",
				(uint32_t)value_set, (uint32_t)value_get);
		}
	}
}
