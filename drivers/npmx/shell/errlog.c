/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "shell_common.h"
#include <npmx_driver.h>

static const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

static const char *shell_err_to_field(npmx_callback_type_t type, uint8_t bit)
{
	static const char * err_fieldnames[][8] =
	{
		[NPMX_CALLBACK_TYPE_RSTCAUSE] =
		{
			[0] = "SHIPMODEEXIT",    [1] = "BOOTMONITORTIMEOUT",
			[2] = "WATCHDOGTIMEOUT", [3] = "LONGPRESSTIMEOUT",
			[4] = "THERMALSHUTDOWN", [5] = "VSYSLOW",
			[6] = "SWRESET",
		},
		[NPMX_CALLBACK_TYPE_CHARGER_ERROR] =
		{
			[0] = "NTCSENSORERR",  [1] = "VBATSENSORERR",
			[2] = "VBATLOW",       [3] = "VTRICKLE",
			[4] = "MEASTIMEOUT",   [5] = "CHARGETIMEOUT",
			[6] = "TRICKLETIMEOUT",
		},
		[NPMX_CALLBACK_TYPE_SENSOR_ERROR] =
		{
			[0] = "SENSORNTCCOLD",  [1] = "SENSORNTCCOOL",
			[2] = "SENSORNTCWARM",  [3] = "SENSORNTCHOT",
			[4] = "SENSORVTERM",    [5] = "SENSORRECHARGE",
			[6] = "SENSORVTRICKLE", [7] = "SENSORVBATLOW",
		},
	};

	return err_fieldnames[type][bit];
}

static void print_errlog(npmx_instance_t *p_pm, npmx_callback_type_t type, uint8_t mask)
{
	const struct shell *shell = (struct shell *)npmx_core_context_get(p_pm);

	shell_print(shell, "%s:", npmx_callback_to_str(type));
	for (uint8_t i = 0; i < 8; i++) {
		if ((1U << i) & mask) {
			shell_print(shell, "\t%s", shell_err_to_field(type, i));
		}
	}
}

static int cmd_errlog_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);
	if (npmx_instance == NULL) {
		return 0;
	}

	npmx_core_context_set(npmx_instance, (void *)shell);

	npmx_core_register_cb(npmx_instance, print_errlog, NPMX_CALLBACK_TYPE_RSTCAUSE);
	npmx_core_register_cb(npmx_instance, print_errlog, NPMX_CALLBACK_TYPE_CHARGER_ERROR);
	npmx_core_register_cb(npmx_instance, print_errlog, NPMX_CALLBACK_TYPE_SENSOR_ERROR);

	npmx_errlog_t *errlog_instance = npmx_errlog_get(npmx_instance, 0);
	npmx_error_t err_code = npmx_errlog_reset_errors_check(errlog_instance);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "error log");
	}
	return 0;
}

/* Creating subcommands (level 2 command) array for command "errlog". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_errlog, SHELL_CMD(get, NULL, "Get error logs", cmd_errlog_get),
			       SHELL_SUBCMD_SET_END);

SHELL_SUBCMD_ADD((npmx), errlog, &sub_errlog, "Reset errors logs", NULL, 1, 0);
