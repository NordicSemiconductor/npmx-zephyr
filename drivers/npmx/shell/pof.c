/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "shell_common.h"
#include <npmx_driver.h>

/** @brief POF config parameter. */
typedef enum {
	POF_CONFIG_PARAM_POLARITY, /* POF polarity. */
	POF_CONFIG_PARAM_STATUS, /* Enable POF. */
	POF_CONFIG_PARAM_THRESHOLD, /* POF threshold value. */
} pof_config_param_t;

static npmx_pof_t *pof_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);

	return npmx_instance ? npmx_pof_get(npmx_instance, 0) : NULL;
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

SHELL_SUBCMD_ADD((npmx), pof, &sub_pof, "POF", NULL, 1, 0);
