/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "shell_common.h"
#include <npmx_driver.h>

/** @brief Load switch GPIO configuration parameter. */
typedef enum {
	LDSW_GPIO_PARAM_INDEX, /* Pin index. */
	LDSW_GPIO_PARAM_POLARITY, /* Pin polarity. */
} ldsw_gpio_param_t;

/** @brief Load switch soft-start configuration parameter. */
typedef enum {
	LDSW_SOFT_START_PARAM_ENABLE, /* Soft-start enable. */
	LDSW_SOFT_START_PARAM_CURRENT /* Soft-start current. */
} ldsw_soft_start_config_param_t;

static npmx_ldsw_t *ldsw_instance_get(const struct shell *shell, uint32_t index)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);
	bool status = check_instance_index(shell, "LDSW", index, NPM_LDSW_COUNT);

	return (npmx_instance && status) ? npmx_ldsw_get(npmx_instance, (uint8_t)index) : NULL;
}

static int cmd_ldsw_active_discharge_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
					  [1] = { SHELL_ARG_TYPE_BOOL_VALUE, "status" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	bool discharge_status = args_info.arg[1].result.bvalue;
	npmx_error_t err_code =
		npmx_ldsw_active_discharge_enable_set(ldsw_instance, discharge_status);

	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "LDSW active discharge status");
		return 0;
	}

	print_success(shell, discharge_status, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_ldsw_active_discharge_get(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	bool discharge_status;
	npmx_error_t err_code =
		npmx_ldsw_active_discharge_enable_get(ldsw_instance, &discharge_status);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "LDSW active discharge status");
		return 0;
	}

	print_value(shell, discharge_status, UNIT_TYPE_NONE);
	return 0;
}

static npmx_ldsw_gpio_t ldsw_gpio_index_convert(int32_t gpio_idx)
{
	switch (gpio_idx) {
	case (-1):
		return NPMX_LDSW_GPIO_NC;
	case (0):
		return NPMX_LDSW_GPIO_0;
	case (1):
		return NPMX_LDSW_GPIO_1;
	case (2):
		return NPMX_LDSW_GPIO_2;
	case (3):
		return NPMX_LDSW_GPIO_3;
	case (4):
		return NPMX_LDSW_GPIO_4;
	default:
		return NPMX_LDSW_GPIO_INVALID;
	}
}

static int ldsw_gpio_set(const struct shell *shell, size_t argc, char **argv,
			 ldsw_gpio_param_t config_type)
{
	char *config_name;
	shell_arg_type_t arg_type = SHELL_ARG_TYPE_INT32_VALUE;

	switch (config_type) {
	case LDSW_GPIO_PARAM_INDEX:
		config_name = "GPIO number";
		break;
	case LDSW_GPIO_PARAM_POLARITY:
		config_name = "GPIO polarity";
		arg_type = SHELL_ARG_TYPE_BOOL_VALUE;
		break;
	default:
		return 0;
	}

	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
					  [1] = { arg_type, config_name },
				  } };

	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	npmx_ldsw_gpio_config_t gpio_config;
	npmx_error_t err_code = npmx_ldsw_enable_gpio_get(ldsw_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "GPIO config");
		return 0;
	}

	switch (config_type) {
	case LDSW_GPIO_PARAM_INDEX:
		gpio_config.gpio = ldsw_gpio_index_convert(args_info.arg[1].result.ivalue);

		if (gpio_config.gpio == NPMX_LDSW_GPIO_INVALID) {
			shell_error(shell, "Error: wrong GPIO index.");
			return 0;
		}

		if (!check_pin_configuration_correctness(shell, args_info.arg[1].result.ivalue)) {
			return 0;
		}

		break;
	case LDSW_GPIO_PARAM_POLARITY:
		gpio_config.inverted = args_info.arg[1].result.bvalue;
		break;
	default:
		return 0;
	}

	err_code = npmx_ldsw_enable_gpio_set(ldsw_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "GPIO config");
		return 0;
	}

	int16_t value = (config_type == LDSW_GPIO_PARAM_INDEX) ? args_info.arg[1].result.ivalue :
								 gpio_config.inverted;

	print_success(shell, value, UNIT_TYPE_NONE);
	return 0;
}

static int ldsw_gpio_get(const struct shell *shell, size_t argc, char **argv,
			 ldsw_gpio_param_t config_type)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	npmx_ldsw_gpio_config_t gpio_config;
	npmx_error_t err_code = npmx_ldsw_enable_gpio_get(ldsw_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "GPIO config");
		return 0;
	}

	switch (config_type) {
	case LDSW_GPIO_PARAM_INDEX:
		int8_t gpio_index = (gpio_config.gpio == NPMX_LDSW_GPIO_NC) ?
					    -1 :
					    ((int8_t)gpio_config.gpio - 1);

		print_value(shell, gpio_index, UNIT_TYPE_NONE);
		break;
	case LDSW_GPIO_PARAM_POLARITY:
		print_value(shell, gpio_config.inverted, UNIT_TYPE_NONE);
		break;
	default:
		return 0;
	}

	return 0;
}

static int cmd_ldsw_gpio_index_set(const struct shell *shell, size_t argc, char **argv)
{
	return ldsw_gpio_set(shell, argc, argv, LDSW_GPIO_PARAM_INDEX);
}

static int cmd_ldsw_gpio_index_get(const struct shell *shell, size_t argc, char **argv)
{
	return ldsw_gpio_get(shell, argc, argv, LDSW_GPIO_PARAM_INDEX);
}

static int cmd_ldsw_gpio_polarity_set(const struct shell *shell, size_t argc, char **argv)
{
	return ldsw_gpio_set(shell, argc, argv, LDSW_GPIO_PARAM_POLARITY);
}

static int cmd_ldsw_gpio_polarity_get(const struct shell *shell, size_t argc, char **argv)
{
	return ldsw_gpio_get(shell, argc, argv, LDSW_GPIO_PARAM_POLARITY);
}
static int cmd_ldsw_ldo_voltage_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
					  [1] = { SHELL_ARG_TYPE_UINT32_VALUE, "voltage" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	uint32_t voltage_mv = args_info.arg[1].result.uvalue;
	npmx_ldsw_voltage_t ldsw_voltage = npmx_ldsw_voltage_convert(voltage_mv);
	if (ldsw_voltage == NPMX_LDSW_VOLTAGE_INVALID) {
		print_convert_error(shell, "millivolts", "LDSW voltage");
		return 0;
	}

	npmx_error_t err_code = npmx_ldsw_ldo_voltage_set(ldsw_instance, ldsw_voltage);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "LDSW voltage");
		return 0;
	}

	print_success(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

static int cmd_ldsw_ldo_voltage_get(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	npmx_ldsw_voltage_t ldsw_voltage;
	npmx_error_t err_code = npmx_ldsw_ldo_voltage_get(ldsw_instance, &ldsw_voltage);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "LDSW voltage");
		return 0;
	}

	uint32_t voltage_mv;
	if (!npmx_ldsw_voltage_convert_to_mv(ldsw_voltage, &voltage_mv)) {
		print_convert_error(shell, "LDSW voltage", "millivolts");
		return 0;
	}

	print_value(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

static int cmd_ldsw_mode_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = {
		.expected_args = 2,
		.arg = { [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
			 [1] = { SHELL_ARG_TYPE_UINT32_VALUE, "mode" }, },
	};
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	uint32_t mode = args_info.arg[1].result.uvalue;
	npmx_ldsw_mode_t ldsw_mode;
	switch (mode) {
	case 0:
		ldsw_mode = NPMX_LDSW_MODE_LOAD_SWITCH;
		break;
	case 1:
		ldsw_mode = NPMX_LDSW_MODE_LDO;
		break;
	default:
		shell_error(shell, "Error: Wrong mode:");
		print_hint_error(shell, 0, "LOADSW");
		print_hint_error(shell, 1, "LDO");
		return 0;
	}

	npmx_error_t err_code = npmx_ldsw_mode_set(ldsw_instance, ldsw_mode);
	if (err_code != NPMX_SUCCESS) {
		print_set_error(shell, "LDSW mode");
		return 0;
	}

	/* LDSW reset is required to apply mode change. */
	err_code = npmx_ldsw_task_trigger(ldsw_instance, NPMX_LDSW_TASK_DISABLE);
	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: reset error while disabling LDSW to change mode.");
		return 0;
	}

	err_code = npmx_ldsw_task_trigger(ldsw_instance, NPMX_LDSW_TASK_ENABLE);
	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: reset error while enabling LDSW to change mode.");
		return 0;
	}

	print_success(shell, mode, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_ldsw_mode_get(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	npmx_ldsw_mode_t mode;
	npmx_error_t err_code = npmx_ldsw_mode_get(ldsw_instance, &mode);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "LDSW mode");
		return 0;
	}

	print_value(shell, (int)mode, UNIT_TYPE_NONE);
	return 0;
}

static int ldsw_soft_start_config_set(const struct shell *shell, size_t argc, char **argv,
				      ldsw_soft_start_config_param_t config_type)
{
	shell_arg_type_t arg_type;
	switch (config_type) {
	case LDSW_SOFT_START_PARAM_ENABLE:
		arg_type = SHELL_ARG_TYPE_BOOL_VALUE;
		break;
	case LDSW_SOFT_START_PARAM_CURRENT:
		arg_type = SHELL_ARG_TYPE_UINT32_VALUE;
		break;
	}

	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
					  [1] = { arg_type, "config" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	npmx_ldsw_soft_start_config_t soft_start_config;
	npmx_error_t err_code = npmx_ldsw_soft_start_config_get(ldsw_instance, &soft_start_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "soft-start config");
		return 0;
	}

	shell_arg_result_t result = args_info.arg[1].result;
	switch (config_type) {
	case LDSW_SOFT_START_PARAM_ENABLE:
		soft_start_config.enable = result.bvalue;
		break;
	case LDSW_SOFT_START_PARAM_CURRENT:
		soft_start_config.current = npmx_ldsw_soft_start_current_convert(result.uvalue);
		if (soft_start_config.current == NPMX_LDSW_SOFT_START_CURRENT_INVALID) {
			print_convert_error(shell, "milliamperes", "soft-start current");
			return 0;
		}
		break;
	}

	err_code = npmx_ldsw_soft_start_config_set(ldsw_instance, &soft_start_config);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "soft-start config");
		return 0;
	}

	switch (config_type) {
	case LDSW_SOFT_START_PARAM_ENABLE:
		print_success(shell, result.bvalue, UNIT_TYPE_NONE);
		break;
	case LDSW_SOFT_START_PARAM_CURRENT:
		print_success(shell, result.uvalue, UNIT_TYPE_MILLIAMPERE);
		break;
	}
	return 0;
}

static int ldsw_soft_start_config_get(const struct shell *shell, size_t argc, char **argv,
				      ldsw_soft_start_config_param_t config_type)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	npmx_ldsw_soft_start_config_t soft_start_config;
	npmx_error_t err_code = npmx_ldsw_soft_start_config_get(ldsw_instance, &soft_start_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "soft-start config");
		return 0;
	}

	switch (config_type) {
	case LDSW_SOFT_START_PARAM_ENABLE:
		print_value(shell, soft_start_config.enable, UNIT_TYPE_NONE);
		break;
	case LDSW_SOFT_START_PARAM_CURRENT:
		uint32_t soft_start_current_ma;
		if (!npmx_ldsw_soft_start_current_convert_to_ma(soft_start_config.current,
								&soft_start_current_ma)) {
			print_convert_error(shell, "soft-start current", "milliamperes");
			return 0;
		}
		print_value(shell, soft_start_current_ma, UNIT_TYPE_MILLIAMPERE);
		break;
	}
	return 0;
}

static int cmd_ldsw_soft_start_current_set(const struct shell *shell, size_t argc, char **argv)
{
	return ldsw_soft_start_config_set(shell, argc, argv, LDSW_SOFT_START_PARAM_CURRENT);
}

static int cmd_ldsw_soft_start_current_get(const struct shell *shell, size_t argc, char **argv)
{
	return ldsw_soft_start_config_get(shell, argc, argv, LDSW_SOFT_START_PARAM_CURRENT);
}

static int cmd_ldsw_soft_start_enable_set(const struct shell *shell, size_t argc, char **argv)
{
	return ldsw_soft_start_config_set(shell, argc, argv, LDSW_SOFT_START_PARAM_ENABLE);
}

static int cmd_ldsw_soft_start_enable_get(const struct shell *shell, size_t argc, char **argv)
{
	return ldsw_soft_start_config_get(shell, argc, argv, LDSW_SOFT_START_PARAM_ENABLE);
}

static int cmd_ldsw_status_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
					  [1] = { SHELL_ARG_TYPE_BOOL_VALUE, "status" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	bool ldsw_status = args_info.arg[1].result.bvalue;
	npmx_ldsw_task_t ldsw_task = ldsw_status ? NPMX_LDSW_TASK_ENABLE : NPMX_LDSW_TASK_DISABLE;
	npmx_error_t err_code = npmx_ldsw_task_trigger(ldsw_instance, ldsw_task);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "LDSW status");
		return 0;
	}

	print_success(shell, ldsw_status, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_ldsw_status_get(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	uint8_t ldsw_idx = (uint8_t)args_info.arg[0].result.uvalue;
	uint8_t status_mask;
	npmx_error_t err_code = npmx_ldsw_status_get(ldsw_instance, &status_mask);
	uint8_t check_mask = ldsw_idx == 0 ? (NPMX_LDSW_STATUS_POWERUP_LDSW_1_MASK |
					      NPMX_LDSW_STATUS_POWERUP_LDO_1_MASK) :
					     (NPMX_LDSW_STATUS_POWERUP_LDSW_2_MASK |
					      NPMX_LDSW_STATUS_POWERUP_LDO_2_MASK);

	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "LDSW status");
		return 0;
	}

	print_value(shell, !!(status_mask & check_mask), UNIT_TYPE_NONE);
	return 0;
}

/* Creating subcommands (level 3 command) array for command "ldsw active_discharge". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_active_discharge,
			       SHELL_CMD(set, NULL, "Set active discharge status",
					 cmd_ldsw_active_discharge_set),
			       SHELL_CMD(get, NULL, "Get active discharge status",
					 cmd_ldsw_active_discharge_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "ldsw gpio index". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_gpio_index,
			       SHELL_CMD(set, NULL, "Set GPIO pin index", cmd_ldsw_gpio_index_set),
			       SHELL_CMD(get, NULL, "Get GPIO pin index", cmd_ldsw_gpio_index_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "ldsw gpio polarity". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_gpio_polarity,
			       SHELL_CMD(set, NULL, "Set GPIO pin polarity inversion",
					 cmd_ldsw_gpio_polarity_set),
			       SHELL_CMD(get, NULL, "Get GPIO pin polarity inversion",
					 cmd_ldsw_gpio_polarity_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "ldsw gpio". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_gpio,
			       SHELL_CMD(index, &sub_ldsw_gpio_index, "LDSW GPIO pin index", NULL),
			       SHELL_CMD(polarity, &sub_ldsw_gpio_polarity,
					 "LDSW GPIO pin polarity", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "ldsw ldo_voltage". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_ldo_voltage,
			       SHELL_CMD(set, NULL, "Set LDO voltage", cmd_ldsw_ldo_voltage_set),
			       SHELL_CMD(get, NULL, "Get LDO voltage", cmd_ldsw_ldo_voltage_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "ldsw mode". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_mode,
			       SHELL_CMD(set, NULL, "Set LDSW mode", cmd_ldsw_mode_set),
			       SHELL_CMD(get, NULL, "Get LDSW mode", cmd_ldsw_mode_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ldsw soft_start current". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_soft_start_current,
			       SHELL_CMD(set, NULL, "Set soft-start current",
					 cmd_ldsw_soft_start_current_set),
			       SHELL_CMD(get, NULL, "Get soft-start current",
					 cmd_ldsw_soft_start_current_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ldsw soft_start enable". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_soft_start_enable,
			       SHELL_CMD(set, NULL, "Set soft-start enable",
					 cmd_ldsw_soft_start_enable_set),
			       SHELL_CMD(get, NULL, "Get soft-start enable",
					 cmd_ldsw_soft_start_enable_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "ldsw soft_start". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_soft_start,
			       SHELL_CMD(current, &sub_ldsw_soft_start_current,
					 "Soft-start current", NULL),
			       SHELL_CMD(enable, &sub_ldsw_soft_start_enable, "Soft-start enable",
					 NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "ldsw status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_status,
			       SHELL_CMD(set, NULL, "Set LDSW status", cmd_ldsw_status_set),
			       SHELL_CMD(get, NULL, "Get LDSW status", cmd_ldsw_status_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "ldsw". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_ldsw, SHELL_CMD(active_discharge, &sub_ldsw_active_discharge, "Active discharge", NULL),
	SHELL_CMD(gpio, &sub_ldsw_gpio, "Select GPIO used as LDSW's on/off", NULL),
	SHELL_CMD(ldo_voltage, &sub_ldsw_ldo_voltage, "LDO voltage", NULL),
	SHELL_CMD(mode, &sub_ldsw_mode, "LDSW mode", NULL),
	SHELL_CMD(soft_start, &sub_ldsw_soft_start, "LDSW soft-start", NULL),
	SHELL_CMD(status, &sub_ldsw_status, "LDSW status", NULL), SHELL_SUBCMD_SET_END);

SHELL_SUBCMD_ADD((npmx), ldsw, &sub_ldsw, "LDSW", NULL, 1, 0);
