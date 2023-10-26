/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "shell_common.h"
#include <npmx_driver.h>

static const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

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

SHELL_SUBCMD_ADD((npmx), reset, NULL, "Reset device", cmd_reset, 1, 0);

/* Creating subcommands (level 1 command) array for command "npmx". */
SHELL_SUBCMD_SET_CREATE(sub_npmx, (npmx));

/* Creating root (level 0) command "npmx" without a handler. */
SHELL_CMD_REGISTER(npmx, &sub_npmx, "npmx", NULL);
