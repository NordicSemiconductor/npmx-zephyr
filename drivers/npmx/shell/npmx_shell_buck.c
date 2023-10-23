/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <npmx_shell_common.h>
#include <npmx_driver.h>
#include <math.h>

static const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

static int cmd_buck_vout_select_get(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: Missing buck instance index.");
		return 0;
	}

	int err = 0;
	uint8_t buck_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: buck instance has to be an integer.");
		return 0;
	}
	if (buck_indx >= NPM_BUCK_COUNT) {
		shell_error(shell, "Error: buck instance index is too high: no such instance.");
		return 0;
	}

	npmx_buck_t *buck_instance = npmx_buck_get(npmx_instance, buck_indx);
	npmx_buck_vout_select_t selection;
	npmx_error_t err_code = npmx_buck_vout_select_get(buck_instance, &selection);

	if (err_code == NPMX_SUCCESS) {
		shell_print(shell, "Value: %d.", selection);
	} else {
		shell_error(shell, "Error: unable to read vout select.");
	}

	return 0;
}

static int cmd_buck_vout_select_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(
			shell,
			"Error: missing buck instance index and buck output voltage select value.");
		return 0;
	}

	if (argc < 3) {
		shell_error(shell, "Error: missing output voltage select value.");
		return 0;
	}

	int err = 0;
	uint8_t buck_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	npmx_buck_vout_select_t selection =
		(npmx_buck_vout_select_t)CLAMP(shell_strtoul(argv[2], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: instance index and voltage must be integers.");
		return 0;
	}

	if (buck_indx >= NPM_BUCK_COUNT) {
		shell_error(shell, "Error: buck instance index is too high: no such instance.");
		return 0;
	}

	if (selection != NPMX_BUCK_VOUT_SELECT_VSET_PIN &&
	    selection != NPMX_BUCK_VOUT_SELECT_SOFTWARE) {
		shell_error(shell, "Error: invalid buck vout select value.");
		return 0;
	}

	npmx_buck_t *buck_instance = npmx_buck_get(npmx_instance, buck_indx);
	npmx_error_t err_code = npmx_buck_vout_select_set(buck_instance, selection);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %s.", argv[2]);
	} else {
		shell_error(shell, "Error: unable to read vout select.");
	}

	return 0;
}

static int buck_voltage_get(const struct shell *shell, size_t argc, char **argv,
			    npmx_error_t (*voltage_get)(npmx_buck_t const *, npmx_buck_voltage_t *))
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing buck instance index.");
		return 0;
	}

	int err = 0;
	uint8_t buck_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: buck instance has to be an integer.");
		return 0;
	}

	if (buck_indx >= NPM_BUCK_COUNT) {
		shell_error(shell, "Error: buck instance index is too high: no such instance.");
		return 0;
	}

	npmx_buck_t *buck_instance = npmx_buck_get(npmx_instance, buck_indx);

	npmx_error_t err_code;
	npmx_buck_voltage_t voltage;

	if (voltage_get == npmx_buck_normal_voltage_get) {
		npmx_buck_vout_select_t selection;

		err_code = npmx_buck_vout_select_get(buck_instance, &selection);

		if (!check_error_code(shell, err_code)) {
			shell_error(shell, "Error: unable to read VOUT reference selection.");
			return 0;
		}

		if (selection == NPMX_BUCK_VOUT_SELECT_VSET_PIN) {
			err_code = npmx_buck_status_voltage_get(buck_instance, &voltage);
		} else if (selection == NPMX_BUCK_VOUT_SELECT_SOFTWARE) {
			err_code = voltage_get(buck_instance, &voltage);
		} else {
			shell_error(shell, "Error: invalid VOUT reference selection.");
			return 0;
		}
	} else {
		err_code = voltage_get(buck_instance, &voltage);
	}

	if (check_error_code(shell, err_code)) {
		uint32_t voltage_mv;

		if (npmx_buck_voltage_convert_to_mv(voltage, &voltage_mv)) {
			shell_print(shell, "Value: %d mV.", voltage_mv);
		} else {
			shell_error(shell,
				    "Error: unable to convert buck voltage value to millivolts.");
		}
	} else {
		shell_error(shell, "Error: unable to read buck voltage.");
	}

	return 0;
}

static int buck_voltage_set(const struct shell *shell, size_t argc, char **argv,
			    npmx_error_t (*voltage_set)(npmx_buck_t const *, npmx_buck_voltage_t))
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing buck instance index and buck voltage value.");
		return 0;
	}

	if (argc < 3) {
		shell_error(shell, "Error: missing voltage value.");
		return 0;
	}

	int err = 0;
	uint8_t buck_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	uint32_t buck_set = (uint32_t)shell_strtoul(argv[2], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: instance index and voltage must be integers.");
		return 0;
	}

	if (buck_indx >= NPM_BUCK_COUNT) {
		shell_error(shell, "Buck instance index is too high: no such instance.");
		return 0;
	}

	npmx_buck_t *buck_instance = npmx_buck_get(npmx_instance, buck_indx);
	npmx_buck_voltage_t voltage = npmx_buck_voltage_convert(buck_set);

	if (voltage == NPMX_BUCK_VOLTAGE_INVALID) {
		shell_error(shell, "Error: wrong buck voltage value.");
		return 0;
	}

	npmx_error_t err_code = voltage_set(buck_instance, voltage);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %s mV.", argv[2]);
	} else {
		shell_error(shell, "Error: unable to set buck voltage.");
	}

	return 0;
}

static int cmd_buck_voltage_normal_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_voltage_get(shell, argc, argv, npmx_buck_normal_voltage_get);
}

static int cmd_buck_voltage_normal_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_voltage_set(shell, argc, argv, npmx_buck_normal_voltage_set);
}

static int cmd_buck_voltage_retention_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_voltage_get(shell, argc, argv, npmx_buck_retention_voltage_get);
}

static int cmd_buck_voltage_retention_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_voltage_set(shell, argc, argv, npmx_buck_retention_voltage_set);
}

static int buck_gpio_get(const struct shell *shell, size_t argc, char **argv,
			 npmx_error_t (*gpio_config_get)(npmx_buck_t const *,
							 npmx_buck_gpio_config_t *))
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing buck instance index.");
		return 0;
	}

	int err = 0;
	uint8_t buck_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: buck instance has to be an integer.");
		return 0;
	}

	if (buck_indx >= NPM_BUCK_COUNT) {
		shell_error(shell, "Error: buck instance index is too high: no such instance.");
		return 0;
	}

	npmx_buck_t *buck_instance = npmx_buck_get(npmx_instance, buck_indx);

	npmx_buck_gpio_config_t gpio_config;
	npmx_error_t err_code = gpio_config_get(buck_instance, &gpio_config);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read GPIO config.");
		return 0;
	}

	int val = ((gpio_config.gpio == NPMX_BUCK_GPIO_NC) ||
		   (gpio_config.gpio == NPMX_BUCK_GPIO_NC1)) ?
			  -1 :
			  ((int)gpio_config.gpio - 1);

	shell_print(shell, "Value: %d %d.", val, (int)gpio_config.inverted);

	return 0;
}

static int buck_gpio_set(const struct shell *shell, size_t argc, char **argv,
			 npmx_error_t (*gpio_config_set)(npmx_buck_t const *,
							 npmx_buck_gpio_config_t const *))
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell,
			    "Error: missing buck instance index, GPIO number and GPIO inv state.");
		return 0;
	}

	if (argc < 3) {
		shell_error(shell, "Error: missing GPIO number and GPIO inv state.");
		return 0;
	}

	if (argc < 4) {
		shell_error(shell, "Error: missing GPIO inv state.");
		return 0;
	}

	int err = 0;
	uint8_t buck_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	int8_t gpio_indx = CLAMP(shell_strtol(argv[2], 0, &err), INT8_MIN, INT8_MAX);

	npmx_buck_gpio_config_t gpio_config = {
		.inverted = !!shell_strtoul(argv[3], 0, &err),
	};

	if (err != 0) {
		shell_error(
			shell,
			"Error: instance index, GPIO number, GPIO inv state have to be integers.");
		return 0;
	}

	if (buck_indx >= NPM_BUCK_COUNT) {
		shell_error(shell, "Error: Buck instance index is too high: no such instance.");
		return 0;
	}

	npmx_buck_t *buck_instance = npmx_buck_get(npmx_instance, buck_indx);

	static const npmx_buck_gpio_t gpios[] = {
		NPMX_BUCK_GPIO_0, NPMX_BUCK_GPIO_1, NPMX_BUCK_GPIO_2,
		NPMX_BUCK_GPIO_3, NPMX_BUCK_GPIO_4,
	};

	if (!check_pin_configuration_correctness(shell, gpio_indx)) {
		return 0;
	}

	if (gpio_indx == -1) {
		gpio_config.gpio = NPMX_BUCK_GPIO_NC;
	} else {
		if ((gpio_indx >= 0) && (gpio_indx < ARRAY_SIZE(gpios))) {
			gpio_config.gpio = gpios[gpio_indx];
		} else {
			shell_error(shell, "Error: wrong GPIO index.");
			return 0;
		}
	}

	npmx_error_t err_code = gpio_config_set(buck_instance, &gpio_config);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d.", gpio_indx);
	} else {
		shell_error(shell, "Error: unable to set GPIO config.");
		return 0;
	}

	return 0;
}

static int cmd_buck_gpio_on_off_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_get(shell, argc, argv, npmx_buck_enable_gpio_config_get);
}

static int cmd_buck_gpio_on_off_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_set(shell, argc, argv, npmx_buck_enable_gpio_config_set);
}

static int cmd_buck_gpio_retention_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_get(shell, argc, argv, npmx_buck_retention_gpio_config_get);
}

static int cmd_buck_gpio_retention_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_set(shell, argc, argv, npmx_buck_retention_gpio_config_set);
}

static int cmd_buck_gpio_forced_pwm_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_get(shell, argc, argv, npmx_buck_forced_pwm_gpio_config_get);
}

static int cmd_buck_gpio_forced_pwm_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_set(shell, argc, argv, npmx_buck_forced_pwm_gpio_config_set);
}

static int cmd_buck_mode_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing buck instance index and buck mode.");
		return 0;
	}

	if (argc < 3) {
		shell_error(shell, "Error: missing buck mode.");
		return 0;
	}

	int err = 0;
	uint8_t buck_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	int buck_mode = shell_strtol(argv[2], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: instance index and mode have to be integers.");
		return 0;
	}

	if (buck_indx >= NPM_BUCK_COUNT) {
		shell_error(shell, "Error: Buck instance index is too high: no such instance.");
		return 0;
	}

	npmx_buck_t *buck_instance = npmx_buck_get(npmx_instance, buck_indx);
	npmx_buck_mode_t mode;

	switch (buck_mode) {
	case 0:
		mode = NPMX_BUCK_MODE_AUTO;
		break;
	case 1:
		mode = NPMX_BUCK_MODE_PFM;
		break;
	case 2:
		mode = NPMX_BUCK_MODE_PWM;
		break;
	default:
		shell_error(shell, "Error: Buck mode can be 0-AUTO, 1-PFM, 2-PWM.");
		return 0;
	}

	npmx_error_t err_code = npmx_buck_converter_mode_set(buck_instance, mode);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d.", buck_mode);
	} else {
		shell_error(shell, "Error: unable to set buck mode.");
	}

	return 0;
}

static int cmd_buck_active_discharge_get(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing buck instance index.");
		return 0;
	}

	int err = 0;
	uint8_t buck_index = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: instance index has to be an integer.");
		return 0;
	}

	if (buck_index >= NPM_BUCK_COUNT) {
		shell_error(shell, "Error: Buck instance index is too high: no such instance.");
		return 0;
	}

	npmx_buck_t *buck_instance = npmx_buck_get(npmx_instance, buck_index);
	bool discharge_enable;

	npmx_error_t err_code =
		npmx_buck_active_discharge_enable_get(buck_instance, &discharge_enable);

	if (err_code == NPMX_SUCCESS) {
		shell_print(shell, "Value: %d.", discharge_enable);
	} else {
		shell_error(shell, "Error: unable to read buck active discharge status.");
	}

	return 0;
}

static int cmd_buck_active_discharge_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing buck instance index and new status value.");
		return 0;
	}

	if (argc < 3) {
		shell_error(shell, "Error: missing new status value.");
		return 0;
	}

	int err = 0;
	uint8_t buck_index = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	uint8_t discharge_enable = CLAMP(shell_strtoul(argv[2], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: instance index has to be an integer.");
		return 0;
	}

	if (buck_index >= NPM_BUCK_COUNT) {
		shell_error(shell, "Error: Buck instance index is too high: no such instance.");
		return 0;
	}

	if (discharge_enable > 1) {
		shell_error(
			shell,
			"Error: invalid active discharge status. Accepted values: 0 = disabled, 1 = enabled.");
		return 0;
	}

	npmx_buck_t *buck_instance = npmx_buck_get(npmx_instance, buck_index);

	npmx_error_t err_code =
		npmx_buck_active_discharge_enable_set(buck_instance, discharge_enable == 1);

	if (err_code == NPMX_SUCCESS) {
		shell_print(shell, "Success: %d.", discharge_enable);
	} else {
		shell_error(shell, "Error: unable to set buck active discharge status.");
	}

	return 0;
}

static int cmd_buck_status_power_get(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing buck instance index.");
		return 0;
	}

	int err = 0;
	uint8_t buck_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: instance index have to be an integer.");
		return 0;
	}

	if (buck_indx >= NPM_BUCK_COUNT) {
		shell_error(shell, "Error: Buck instance index is too high: no such instance.");
		return 0;
	}

	npmx_buck_t *buck_instance = npmx_buck_get(npmx_instance, buck_indx);
	npmx_buck_status_t buck_status;

	npmx_error_t err_code = npmx_buck_status_get(buck_instance, &buck_status);

	if (err_code == NPMX_SUCCESS) {
		shell_print(shell, "Buck power status: %d.", buck_status.powered);
	} else {
		shell_error(shell, "Error: unable to read buck power status.");
	}

	return 0;
}

static int cmd_buck_status_power_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing buck instance index and new status value.");
		return 0;
	}

	if (argc < 3) {
		shell_error(shell, "Error: missing new status value.");
		return 0;
	}

	int err = 0;
	uint8_t buck_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	uint8_t buck_set = CLAMP(shell_strtoul(argv[2], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: instance index and status have to be integers.");
		return 0;
	}

	if (buck_indx >= NPM_BUCK_COUNT) {
		shell_error(shell, "Error: buck instance index is too high: no such instance.");
		return 0;
	}

	if (buck_set > 1) {
		shell_error(shell, "Error: wrong new status value.");
		return 0;
	}

	npmx_buck_task_t buck_task = buck_set == 1 ? NPMX_BUCK_TASK_ENABLE : NPMX_BUCK_TASK_DISABLE;
	npmx_buck_t *buck_instance = npmx_buck_get(npmx_instance, buck_indx);

	npmx_error_t err_code = npmx_buck_task_trigger(buck_instance, buck_task);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d.", buck_set);
	} else {
		shell_error(shell, "Error: unable to set buck state.");
	}

	return 0;
}

/* Creating dictionary subcommands (level 4 command) array for command "buck vout select". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_buck_vout_select,
	SHELL_CMD(get, NULL, "Get buck output voltage reference source", cmd_buck_vout_select_get),
	SHELL_CMD(set, NULL, "Set buck output voltage reference source, 0-vset pin, 1-software",
		  cmd_buck_vout_select_set),
	SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "buck vout". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_vout,
			       SHELL_CMD(select, &sub_buck_vout_select, "Buck reference source",
					 NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck voltage normal". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_voltage_normal,
			       SHELL_CMD(get, NULL, "Get buck normal voltage",
					 cmd_buck_voltage_normal_get),
			       SHELL_CMD(set, NULL, "Set buck normal voltage",
					 cmd_buck_voltage_normal_set),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck voltage retention". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_voltage_retention,
			       SHELL_CMD(get, NULL, "Get buck retention voltage",
					 cmd_buck_voltage_retention_get),
			       SHELL_CMD(set, NULL, "Set buck retention voltage",
					 cmd_buck_voltage_retention_set),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "buck voltage". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_buck_voltage, SHELL_CMD(normal, &sub_buck_voltage_normal, "Buck normal voltage", NULL),
	SHELL_CMD(retention, &sub_buck_voltage_retention, "Buck retention voltage", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck gpio on/off". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_on_off,
			       SHELL_CMD(get, NULL, "Get buck GPIO on/off",
					 cmd_buck_gpio_on_off_get),
			       SHELL_CMD(set, NULL, "Set buck GPIO on/off",
					 cmd_buck_gpio_on_off_set),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck gpio retention". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_retention,
			       SHELL_CMD(get, NULL, "Get buck GPIO retention",
					 cmd_buck_gpio_retention_get),
			       SHELL_CMD(set, NULL, "Set buck GPIO retention",
					 cmd_buck_gpio_retention_set),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck gpio pwm_force". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_forced_pwm,
			       SHELL_CMD(get, NULL, "Get buck GPIO PWM forcing",
					 cmd_buck_gpio_forced_pwm_get),
			       SHELL_CMD(set, NULL, "Set buck GPIO PWM forcing",
					 cmd_buck_gpio_forced_pwm_set),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "buck gpio". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio,
			       SHELL_CMD(on_off, &sub_buck_gpio_on_off,
					 "Select GPIO used as buck's on/off", NULL),
			       SHELL_CMD(retention, &sub_buck_gpio_retention,
					 "Select GPIO used as buck's retention", NULL),
			       SHELL_CMD(pwm_force, &sub_buck_gpio_forced_pwm,
					 "Select GPIO used as buck's PWM forcing", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "buck active_discharge". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_active_discharge,
			       SHELL_CMD(get, NULL, "Get active discharge status",
					 cmd_buck_active_discharge_get),
			       SHELL_CMD(set, NULL, "Set active discharge status",
					 cmd_buck_active_discharge_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "buck status power". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_status_power,
			       SHELL_CMD(get, NULL, "Get buck power status",
					 cmd_buck_status_power_get),
			       SHELL_CMD(set, NULL, "Set buck power status",
					 cmd_buck_status_power_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "buck status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_status,
			       SHELL_CMD(power, &sub_buck_status_power, "Buck power status", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "buck". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_buck, SHELL_CMD(vout, &sub_buck_vout, "Buck output voltage reference source", NULL),
	SHELL_CMD(voltage, &sub_buck_voltage, "Buck voltage", NULL),
	SHELL_CMD(gpio, &sub_buck_gpio, "Buck GPIOs", NULL),
	SHELL_CMD(mode, NULL, "Set buck mode", cmd_buck_mode_set),
	SHELL_CMD(active_discharge, &sub_buck_active_discharge,
		  "Enable or disable buck active discharge", NULL),
	SHELL_CMD(status, &sub_buck_status, "Buck status", NULL), SHELL_SUBCMD_SET_END);

void dynamic_cmd_buck(size_t index, struct shell_static_entry *entry)
{
	if (index < 6) {
		entry->syntax = sub_buck.entry[index].syntax;
		entry->handler = sub_buck.entry[index].handler;
		entry->subcmd = sub_buck.entry[index].subcmd;
		entry->help = sub_buck.entry[index].help;
	} else {
		entry->syntax = NULL;
	}
}
