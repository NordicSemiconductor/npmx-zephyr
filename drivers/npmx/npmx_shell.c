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

static const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

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
SHELL_SUBCMD_ADD((npmx), vbusin, &sub_vbusin, "VBUSIN", NULL, 1, 0);

/* Creating subcommands (level 1 command) array for command "npmx". */
SHELL_SUBCMD_SET_CREATE(sub_npmx, (npmx));

/* Creating root (level 0) command "npmx" without a handler. */
SHELL_CMD_REGISTER(npmx, &sub_npmx, "npmx", NULL);
