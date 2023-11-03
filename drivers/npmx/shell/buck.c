/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "shell_common.h"
#include <npmx_driver.h>

/** @brief BUCK GPIO configuration parameter. */
typedef enum {
	BUCK_GPIO_PARAM_INDEX, /* Pin index. */
	BUCK_GPIO_PARAM_POLARITY, /* Pin polarity. */
} buck_gpio_param_t;

static npmx_buck_t *buck_instance_get(const struct shell *shell, uint32_t index)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);
	bool status = check_instance_index(shell, "buck", index, NPM_BUCK_COUNT);

	return (npmx_instance && status) ? npmx_buck_get(npmx_instance, (uint8_t)index) : NULL;
}

static int cmd_buck_active_discharge_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "buck" },
					  [1] = { SHELL_ARG_TYPE_BOOL_VALUE, "active discharge" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_buck_t *buck_instance = buck_instance_get(shell, args_info.arg[0].result.uvalue);
	if (buck_instance == NULL) {
		return 0;
	}

	bool active_discharge = args_info.arg[1].result.bvalue;
	npmx_error_t err_code =
		npmx_buck_active_discharge_enable_set(buck_instance, active_discharge);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "active discharge");
		return 0;
	}

	print_success(shell, active_discharge, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_buck_active_discharge_get(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "buck" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_buck_t *buck_instance = buck_instance_get(shell, args_info.arg[0].result.uvalue);
	if (buck_instance == NULL) {
		return 0;
	}

	bool discharge_enable;
	npmx_error_t err_code =
		npmx_buck_active_discharge_enable_get(buck_instance, &discharge_enable);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "active discharge");
		return 0;
	}

	print_value(shell, discharge_enable, UNIT_TYPE_NONE);
	return 0;
}

static npmx_buck_gpio_t buck_gpio_index_convert(int32_t gpio_index)
{
	switch (gpio_index) {
	case (-1):
		return NPMX_BUCK_GPIO_NC;
	case (0):
		return NPMX_BUCK_GPIO_0;
	case (1):
		return NPMX_BUCK_GPIO_1;
	case (2):
		return NPMX_BUCK_GPIO_2;
	case (3):
		return NPMX_BUCK_GPIO_3;
	case (4):
		return NPMX_BUCK_GPIO_4;
	default:
		return NPMX_BUCK_GPIO_INVALID;
	}
}

static int
buck_gpio_set(const struct shell *shell, size_t argc, char **argv,
	      npmx_error_t (*gpio_config_set)(npmx_buck_t const *, npmx_buck_gpio_config_t const *),
	      npmx_error_t (*gpio_config_get)(npmx_buck_t const *, npmx_buck_gpio_config_t *),
	      buck_gpio_param_t config_type)
{
	char *config_name;
	shell_arg_type_t arg_type = SHELL_ARG_TYPE_INT32_VALUE;

	switch (config_type) {
	case BUCK_GPIO_PARAM_INDEX:
		config_name = "GPIO number";
		break;
	case BUCK_GPIO_PARAM_POLARITY:
		arg_type = SHELL_ARG_TYPE_BOOL_VALUE;
		config_name = "GPIO polarity";
		break;
	default:
		return 0;
	}

	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "buck" },
					  [1] = { arg_type, config_name },
				  } };

	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_buck_t *buck_instance = buck_instance_get(shell, args_info.arg[0].result.uvalue);
	if (buck_instance == NULL) {
		return 0;
	}

	npmx_buck_gpio_config_t gpio_config;
	npmx_error_t err_code = gpio_config_get(buck_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "GPIO config");
		return 0;
	}

	switch (config_type) {
	case BUCK_GPIO_PARAM_INDEX:
		if (!check_pin_configuration_correctness(shell, args_info.arg[1].result.ivalue)) {
			return 0;
		}
		gpio_config.gpio = buck_gpio_index_convert(args_info.arg[1].result.ivalue);

		if (gpio_config.gpio == NPMX_BUCK_GPIO_INVALID) {
			shell_error(shell, "Error: wrong GPIO index.");
			return 0;
		}
		break;
	case BUCK_GPIO_PARAM_POLARITY:
		gpio_config.inverted = args_info.arg[1].result.bvalue;
		break;
	default:
		return 0;
	}

	err_code = gpio_config_set(buck_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "GPIO config");
		return 0;
	}

	int16_t value = (config_type == BUCK_GPIO_PARAM_INDEX) ? args_info.arg[1].result.ivalue :
								 gpio_config.inverted;

	print_success(shell, value, UNIT_TYPE_NONE);
	return 0;
}

static int buck_gpio_get(const struct shell *shell, size_t argc, char **argv,
			 npmx_error_t (*gpio_config_get)(npmx_buck_t const *,
							 npmx_buck_gpio_config_t *),
			 buck_gpio_param_t config_type)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "buck" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_buck_t *buck_instance = buck_instance_get(shell, args_info.arg[0].result.uvalue);
	if (buck_instance == NULL) {
		return 0;
	}

	npmx_buck_gpio_config_t gpio_config;
	npmx_error_t err_code = gpio_config_get(buck_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "GPIO config");
		return 0;
	}

	switch (config_type) {
	case BUCK_GPIO_PARAM_INDEX:
		int8_t gpio_index = (gpio_config.gpio == NPMX_BUCK_GPIO_NC) ?
					    -1 :
					    ((int8_t)gpio_config.gpio - 1);

		print_value(shell, gpio_index, UNIT_TYPE_NONE);
		break;
	case BUCK_GPIO_PARAM_POLARITY:
		print_value(shell, gpio_config.inverted, UNIT_TYPE_NONE);
		break;
	default:
		return 0;
	}

	return 0;
}

static int cmd_buck_gpio_on_off_index_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_set(shell, argc, argv, npmx_buck_enable_gpio_config_set,
			     npmx_buck_enable_gpio_config_get, BUCK_GPIO_PARAM_INDEX);
}

static int cmd_buck_gpio_on_off_index_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_get(shell, argc, argv, npmx_buck_enable_gpio_config_get,
			     BUCK_GPIO_PARAM_INDEX);
}

static int cmd_buck_gpio_on_off_polarity_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_set(shell, argc, argv, npmx_buck_enable_gpio_config_set,
			     npmx_buck_enable_gpio_config_get, BUCK_GPIO_PARAM_POLARITY);
}

static int cmd_buck_gpio_on_off_polarity_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_get(shell, argc, argv, npmx_buck_enable_gpio_config_get,
			     BUCK_GPIO_PARAM_POLARITY);
}

static int cmd_buck_gpio_pwm_force_index_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_set(shell, argc, argv, npmx_buck_forced_pwm_gpio_config_set,
			     npmx_buck_forced_pwm_gpio_config_get, BUCK_GPIO_PARAM_INDEX);
}

static int cmd_buck_gpio_pwm_force_index_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_get(shell, argc, argv, npmx_buck_forced_pwm_gpio_config_get,
			     BUCK_GPIO_PARAM_INDEX);
}

static int cmd_buck_gpio_pwm_force_polarity_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_set(shell, argc, argv, npmx_buck_forced_pwm_gpio_config_set,
			     npmx_buck_forced_pwm_gpio_config_get, BUCK_GPIO_PARAM_POLARITY);
}

static int cmd_buck_gpio_pwm_force_polarity_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_get(shell, argc, argv, npmx_buck_forced_pwm_gpio_config_get,
			     BUCK_GPIO_PARAM_POLARITY);
}

static int cmd_buck_gpio_retention_index_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_set(shell, argc, argv, npmx_buck_retention_gpio_config_set,
			     npmx_buck_retention_gpio_config_get, BUCK_GPIO_PARAM_INDEX);
}

static int cmd_buck_gpio_retention_index_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_get(shell, argc, argv, npmx_buck_retention_gpio_config_get,
			     BUCK_GPIO_PARAM_INDEX);
}

static int cmd_buck_gpio_retention_polarity_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_set(shell, argc, argv, npmx_buck_retention_gpio_config_set,
			     npmx_buck_retention_gpio_config_get, BUCK_GPIO_PARAM_POLARITY);
}

static int cmd_buck_gpio_retention_polarity_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_get(shell, argc, argv, npmx_buck_retention_gpio_config_get,
			     BUCK_GPIO_PARAM_POLARITY);
}

static int cmd_buck_mode_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "buck" },
					  [1] = { SHELL_ARG_TYPE_UINT32_VALUE, "mode" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_buck_t *buck_instance = buck_instance_get(shell, args_info.arg[0].result.uvalue);
	if (buck_instance == NULL) {
		return 0;
	}

	uint32_t mode = args_info.arg[1].result.uvalue;
	npmx_buck_mode_t buck_mode;
	switch (mode) {
	case 0:
		buck_mode = NPMX_BUCK_MODE_AUTO;
		break;
	case 1:
		buck_mode = NPMX_BUCK_MODE_PFM;
		break;
	case 2:
		buck_mode = NPMX_BUCK_MODE_PWM;
		break;
	default:
		shell_error(shell, "Error: Wrong mode:");
		print_hint_error(shell, 0, "AUTO");
		print_hint_error(shell, 1, "PFM");
		print_hint_error(shell, 2, "PWM");
		return 0;
	}

	npmx_error_t err_code = npmx_buck_converter_mode_set(buck_instance, buck_mode);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "buck mode");
		return 0;
	}

	print_success(shell, mode, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_buck_status_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "buck" },
					  [1] = { SHELL_ARG_TYPE_BOOL_VALUE, "status" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_buck_t *buck_instance = buck_instance_get(shell, args_info.arg[0].result.uvalue);
	if (buck_instance == NULL) {
		return 0;
	}

	bool status = args_info.arg[1].result.bvalue;
	npmx_buck_task_t buck_task = status ? NPMX_BUCK_TASK_ENABLE : NPMX_BUCK_TASK_DISABLE;
	npmx_error_t err_code = npmx_buck_task_trigger(buck_instance, buck_task);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "buck status");
		return 0;
	}

	print_success(shell, status, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_buck_status_get(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "buck" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_buck_t *buck_instance = buck_instance_get(shell, args_info.arg[0].result.uvalue);
	if (buck_instance == NULL) {
		return 0;
	}

	npmx_buck_status_t buck_status;
	npmx_error_t err_code = npmx_buck_status_get(buck_instance, &buck_status);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "buck status");
		return 0;
	}

	print_value(shell, buck_status.powered, UNIT_TYPE_NONE);
	return 0;
}

static int buck_voltage_set(const struct shell *shell, size_t argc, char **argv,
			    npmx_error_t (*voltage_set)(npmx_buck_t const *, npmx_buck_voltage_t))
{
	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "buck" },
					  [1] = { SHELL_ARG_TYPE_UINT32_VALUE, "voltage" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_buck_t *buck_instance = buck_instance_get(shell, args_info.arg[0].result.uvalue);
	if (buck_instance == NULL) {
		return 0;
	}

	uint32_t voltage_mv = args_info.arg[1].result.uvalue;
	npmx_buck_voltage_t buck_voltage = npmx_buck_voltage_convert(voltage_mv);
	if (buck_voltage == NPMX_BUCK_VOLTAGE_INVALID) {
		print_convert_error(shell, "millivolts", "buck voltage");
		return 0;
	}

	npmx_error_t err_code = voltage_set(buck_instance, buck_voltage);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "buck voltage");
		return 0;
	}

	print_success(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

static int buck_voltage_get(const struct shell *shell, size_t argc, char **argv,
			    npmx_error_t (*voltage_get)(npmx_buck_t const *, npmx_buck_voltage_t *))
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "buck" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_buck_t *buck_instance = buck_instance_get(shell, args_info.arg[0].result.uvalue);
	if (buck_instance == NULL) {
		return 0;
	}

	npmx_error_t err_code;
	npmx_buck_voltage_t buck_voltage;
	if (voltage_get == npmx_buck_normal_voltage_get) {
		npmx_buck_vout_select_t vout_select;
		err_code = npmx_buck_vout_select_get(buck_instance, &vout_select);
		if (!check_error_code(shell, err_code)) {
			print_get_error(shell, "vout select");
			return 0;
		}

		if (vout_select == NPMX_BUCK_VOUT_SELECT_VSET_PIN) {
			err_code = npmx_buck_status_voltage_get(buck_instance, &buck_voltage);
		} else if (vout_select == NPMX_BUCK_VOUT_SELECT_SOFTWARE) {
			err_code = voltage_get(buck_instance, &buck_voltage);
		} else {
			shell_error(shell, "Error: invalid vout select.");
			return 0;
		}
	} else {
		err_code = voltage_get(buck_instance, &buck_voltage);
	}

	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "buck voltage");
		return 0;
	}

	uint32_t voltage_mv;
	if (!npmx_buck_voltage_convert_to_mv(buck_voltage, &voltage_mv)) {
		print_convert_error(shell, "buck voltage", "millivolts");
		return 0;
	}

	print_value(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

static int cmd_buck_voltage_normal_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_voltage_set(shell, argc, argv, npmx_buck_normal_voltage_set);
}

static int cmd_buck_voltage_normal_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_voltage_get(shell, argc, argv, npmx_buck_normal_voltage_get);
}

static int cmd_buck_voltage_retention_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_voltage_set(shell, argc, argv, npmx_buck_retention_voltage_set);
}

static int cmd_buck_voltage_retention_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_voltage_get(shell, argc, argv, npmx_buck_retention_voltage_get);
}

static int cmd_buck_vout_select_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "buck" },
					  [1] = { SHELL_ARG_TYPE_UINT32_VALUE, "vout select" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_buck_t *buck_instance = buck_instance_get(shell, args_info.arg[0].result.uvalue);
	if (buck_instance == NULL) {
		return 0;
	}

	uint32_t select = args_info.arg[1].result.uvalue;
	npmx_buck_vout_select_t vout_select;
	switch (select) {
	case 0:
		vout_select = NPMX_BUCK_VOUT_SELECT_VSET_PIN;
		break;
	case 1:
		vout_select = NPMX_BUCK_VOUT_SELECT_SOFTWARE;
		break;
	default:
		shell_error(shell, "Error: Wrong vout select:");
		print_hint_error(shell, 0, "Vset pin");
		print_hint_error(shell, 1, "Software");
		return 0;
	}

	npmx_error_t err_code = npmx_buck_vout_select_set(buck_instance, vout_select);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "vout select");
		return 0;
	}

	print_success(shell, select, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_buck_vout_select_get(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "buck" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_buck_t *buck_instance = buck_instance_get(shell, args_info.arg[0].result.uvalue);
	if (buck_instance == NULL) {
		return 0;
	}

	npmx_buck_vout_select_t vout_select;
	npmx_error_t err_code = npmx_buck_vout_select_get(buck_instance, &vout_select);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "vout select");
		return 0;
	}

	print_value(shell, (int)vout_select, UNIT_TYPE_NONE);
	return 0;
}

/* Creating subcommands (level 3 command) array for command "buck active_discharge". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_active_discharge,
			       SHELL_CMD(set, NULL, "Set active discharge status",
					 cmd_buck_active_discharge_set),
			       SHELL_CMD(get, NULL, "Get active discharge status",
					 cmd_buck_active_discharge_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 5 command) array for command "buck gpio on_off index". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_on_off_index,
			       SHELL_CMD(set, NULL, "Set buck on/off GPIO index",
					 cmd_buck_gpio_on_off_index_set),
			       SHELL_CMD(get, NULL, "Get buck on/off GPIO index",
					 cmd_buck_gpio_on_off_index_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 5 command) array for command "buck gpio on_off polarity". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_on_off_polarity,
			       SHELL_CMD(set, NULL, "Set buck on/off GPIO polarity inversion",
					 cmd_buck_gpio_on_off_polarity_set),
			       SHELL_CMD(get, NULL, "Get buck on/off GPIO polarity inversion",
					 cmd_buck_gpio_on_off_polarity_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck gpio on_off". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_on_off,
			       SHELL_CMD(index, &sub_buck_gpio_on_off_index,
					 "Buck on/off GPIO index", NULL),
			       SHELL_CMD(polarity, &sub_buck_gpio_on_off_polarity,
					 "Buck on/off GPIO polarity", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 5 command) array for command "buck gpio pwm_force index". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_pwm_force_index,
			       SHELL_CMD(set, NULL, "Set buck PWM force GPIO index",
					 cmd_buck_gpio_pwm_force_index_set),
			       SHELL_CMD(get, NULL, "Get buck PWM force GPIO index",
					 cmd_buck_gpio_pwm_force_index_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 5 command) array for command "buck gpio pwm_force polarity". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_pwm_force_polarity,
			       SHELL_CMD(set, NULL, "Set buck PWM force GPIO polarity inversion",
					 cmd_buck_gpio_pwm_force_polarity_set),
			       SHELL_CMD(get, NULL, "Get buck PWM force GPIO polarity inversion",
					 cmd_buck_gpio_pwm_force_polarity_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck gpio pwm_force". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_pwm_force,
			       SHELL_CMD(index, &sub_buck_gpio_pwm_force_index,
					 "Buck PWM force GPIO index", NULL),
			       SHELL_CMD(polarity, &sub_buck_gpio_pwm_force_polarity,
					 "Buck PWM force GPIO polarity", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 5 command) array for command "buck gpio retention index". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_retention_index,
			       SHELL_CMD(set, NULL, "Set buck retention GPIO index",
					 cmd_buck_gpio_retention_index_set),
			       SHELL_CMD(get, NULL, "Get buck retention GPIO index",
					 cmd_buck_gpio_retention_index_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 5 command) array for command "buck gpio retention polarity". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_retention_polarity,
			       SHELL_CMD(set, NULL, "Set buck retention GPIO polarity inversion",
					 cmd_buck_gpio_retention_polarity_set),
			       SHELL_CMD(get, NULL, "Get buck retention GPIO polarity inversion",
					 cmd_buck_gpio_retention_polarity_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck gpio retention". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_retention,
			       SHELL_CMD(index, &sub_buck_gpio_retention_index,
					 "Buck retention GPIO index", NULL),
			       SHELL_CMD(polarity, &sub_buck_gpio_retention_polarity,
					 "Buck retention GPIO polarity", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "buck gpio". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio,
			       SHELL_CMD(on_off, &sub_buck_gpio_on_off,
					 "Select GPIO used as buck's on/off", NULL),
			       SHELL_CMD(pwm_force, &sub_buck_gpio_pwm_force,
					 "Select GPIO used as buck's PWM forcing", NULL),
			       SHELL_CMD(retention, &sub_buck_gpio_retention,
					 "Select GPIO used as buck's retention", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "buck status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_status,
			       SHELL_CMD(set, NULL, "Set buck status", cmd_buck_status_set),
			       SHELL_CMD(get, NULL, "Get buck status", cmd_buck_status_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck voltage normal". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_voltage_normal,
			       SHELL_CMD(set, NULL, "Set buck normal voltage",
					 cmd_buck_voltage_normal_set),
			       SHELL_CMD(get, NULL, "Get buck normal voltage",
					 cmd_buck_voltage_normal_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck voltage retention". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_voltage_retention,
			       SHELL_CMD(set, NULL, "Set buck retention voltage",
					 cmd_buck_voltage_retention_set),
			       SHELL_CMD(get, NULL, "Get buck retention voltage",
					 cmd_buck_voltage_retention_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "buck voltage". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_buck_voltage, SHELL_CMD(normal, &sub_buck_voltage_normal, "Buck normal voltage", NULL),
	SHELL_CMD(retention, &sub_buck_voltage_retention, "Buck retention voltage", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "buck vout_select". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_vout_select,
			       SHELL_CMD(set, NULL, "Set buck voltage reference source",
					 cmd_buck_vout_select_set),
			       SHELL_CMD(get, NULL, "Get buck voltage reference source",
					 cmd_buck_vout_select_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "buck". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck,
			       SHELL_CMD(active_discharge, &sub_buck_active_discharge,
					 "Buck active discharge", NULL),
			       SHELL_CMD(gpio, &sub_buck_gpio, "Buck GPIO", NULL),
			       SHELL_CMD(mode, NULL, "Set buck mode", cmd_buck_mode_set),
			       SHELL_CMD(status, &sub_buck_status, "Buck status", NULL),
			       SHELL_CMD(voltage, &sub_buck_voltage, "Buck voltage", NULL),
			       SHELL_CMD(vout_select, &sub_buck_vout_select,
					 "Buck output voltage reference source", NULL),
			       SHELL_SUBCMD_SET_END);

SHELL_SUBCMD_ADD((npmx), buck, &sub_buck, "Buck", NULL, 1, 0);
