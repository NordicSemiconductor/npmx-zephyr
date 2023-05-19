/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <npmx.h>
#include <npmx_driver.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

static const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

static bool check_error_code(const struct shell *shell, npmx_error_t err_code)
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
	}

	return false;
}

static int cmd_charger_termination_voltage_normal_get(const struct shell *shell, size_t argc,
						      char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	npmx_charger_voltage_t voltage;

	npmx_error_t err_code =
		npmx_charger_termination_normal_voltage_get(charger_instance, &voltage);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d mV.", npmx_charger_voltage_convert_to_mv(voltage));
	} else {
		shell_error(shell,
			    "Error: unable to read battery normal termination voltage value.");
	}

	return 0;
}

static int cmd_charger_termination_voltage_normal_set(const struct shell *shell, size_t argc,
						      char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing termination voltage value.");
		return 0;
	}

	int err = 0;
	uint32_t volt_set = (uint32_t)shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: voltage has to be an integer.");
		return 0;
	}

	npmx_charger_voltage_t voltage = npmx_charger_voltage_convert(volt_set);

	if (voltage == NPMX_CHARGER_VOLTAGE_INVALID) {
		shell_error(shell, "Error: wrong termination voltage value.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	uint32_t modules_mask;
	npmx_error_t err_code = npmx_charger_module_get(charger_instance, &modules_mask);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to get charger module status.");
		return 0;
	}

	if ((modules_mask & NPMX_CHARGER_MODULE_CHARGER_MASK) != 0) {
		shell_error(shell, "Error: charger must be disabled to set termination voltage.");
		return 0;
	}

	err_code = npmx_charger_termination_normal_voltage_set(charger_instance, voltage);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %s mV.", argv[1]);
	} else {
		shell_error(shell,
			    "Error: unable to set battery normal termination voltage value.");
	}

	return 0;
}

static int cmd_charger_termination_current_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	npmx_charger_iterm_t current;

	npmx_error_t err_code = npmx_charger_termination_current_get(charger_instance, &current);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d%%.", npmx_charger_iterm_convert_to_pct(current));
	} else {
		shell_error(shell, "Error: unable to read battery termination current value.");
	}

	return 0;
}

static int cmd_charger_termination_current_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing termination current value.");
		return 0;
	}

	int err = 0;
	uint32_t curr_set = (uint32_t)shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: current has to be an integer.");
		return 0;
	}

	npmx_charger_iterm_t current = npmx_charger_iterm_convert(curr_set);

	if (current == NPMX_CHARGER_ITERM_INVALID) {
		shell_error(
			shell,
			"Error: wrong termination current value, it should be equal to 10 or 20.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	uint32_t modules_mask;
	npmx_error_t err_code = npmx_charger_module_get(charger_instance, &modules_mask);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to get charger module status.");
		return 0;
	}

	if ((modules_mask & NPMX_CHARGER_MODULE_CHARGER_MASK) != 0) {
		shell_error(shell, "Error: charger must be disabled to set termination current.");
		return 0;
	}

	err_code = npmx_charger_termination_current_set(charger_instance, current);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %s%%.", argv[1]);
	} else {
		shell_error(shell, "Error: unable to set charger termination current value.");
	}

	return 0;
}

static int cmd_charger_charging_current_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);

	uint16_t current;
	npmx_error_t err_code = npmx_charger_charging_current_get(charger_instance, &current);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d mA.", current);
	} else {
		shell_error(shell, "Error: unable to read charging current value.");
	}

	return 0;
}

static int cmd_charger_charging_current_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing charging current value.");
		return 0;
	}

	int err = 0;
	uint16_t current = (uint16_t)shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: current has to be an integer.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	uint32_t modules_mask;
	npmx_error_t err_code = npmx_charger_module_get(charger_instance, &modules_mask);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to get charger module status.");
		return 0;
	}

	if ((modules_mask & NPMX_CHARGER_MODULE_CHARGER_MASK) != 0) {
		shell_error(shell, "Error: charger must be disabled to set charging current.");
		return 0;
	}

	err_code = npmx_charger_charging_current_set(charger_instance, current);
	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %s mA.", argv[1]);
	} else {
		shell_error(shell, "Error: unable to set charging current value.");
	}

	return 0;
}

static int cmd_charger_status_charging_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);

	uint8_t status_mask;
	uint8_t charging_mask = NPMX_CHARGER_STATUS_TRICKLE_CHARGE_MASK |
				NPMX_CHARGER_STATUS_CONSTANT_CURRENT_MASK |
				NPMX_CHARGER_STATUS_CONSTANT_VOLTAGE_MASK;
	npmx_error_t err_code = npmx_charger_status_get(charger_instance, &status_mask);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d.", !!(status_mask & charging_mask));
	} else {
		shell_error(shell, "Error: unable to read charging status.");
	}

	return 0;
}

static int cmd_charger_status_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);

	uint8_t status_mask;
	npmx_error_t err_code = npmx_charger_status_get(charger_instance, &status_mask);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d.", status_mask);
	} else {
		shell_error(shell, "Error: unable to read charger status.");
	}

	return 0;
}

static int cmd_charger_module_set(const struct shell *shell, size_t argc, char **argv,
				  npmx_charger_module_mask_t mask)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing new status value.");
		return 0;
	}

	int err = 0;
	uint8_t charger_set = (uint8_t)shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: status has to be an integer.");
		return 0;
	}

	if (charger_set > 1) {
		shell_error(shell, "Error: wrong new status value.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	npmx_error_t (*change_status_function)(const npmx_charger_t *p_instance,
					       uint32_t module_mask) =
		charger_set == 1 ? npmx_charger_module_enable_set : npmx_charger_module_disable_set;

	npmx_error_t err_code = change_status_function(charger_instance, mask);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d.", charger_set);
	} else {
		shell_error(shell, "Error: unable to set charging module status.");
	}

	return 0;
}

static int cmd_charger_module_get(const struct shell *shell, size_t argc, char **argv,
				  npmx_charger_module_mask_t mask)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	uint32_t module_mask;

	npmx_error_t err_code = npmx_charger_module_get(charger_instance, &module_mask);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d.", !!(module_mask & (uint32_t)mask));
	} else {
		shell_error(shell, "Error: unable to read charging module status.");
	}

	return 0;
}

static int cmd_charger_module_charger_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_charger_module_set(shell, argc, argv, NPMX_CHARGER_MODULE_CHARGER_MASK);
}

static int cmd_charger_module_charger_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_charger_module_get(shell, argc, argv, NPMX_CHARGER_MODULE_CHARGER_MASK);
}

static int cmd_charger_module_recharge_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_charger_module_set(shell, argc, argv, NPMX_CHARGER_MODULE_RECHARGE_MASK);
}

static int cmd_charger_module_recharge_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_charger_module_get(shell, argc, argv, NPMX_CHARGER_MODULE_RECHARGE_MASK);
}

static int cmd_charger_trickle_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	npmx_charger_trickle_t voltage;

	npmx_error_t err_code = npmx_charger_trickle_voltage_get(charger_instance, &voltage);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d mV.", npmx_charger_trickle_convert_to_mv(voltage));
	} else {
		shell_error(shell,
			    "Error: unable to read battery normal termination voltage value.");
	}

	return 0;
}

static int cmd_charger_trickle_set(const struct shell *shell, size_t argc, char **argv, void *data)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_charger_trickle_t charger_trickle = (npmx_charger_trickle_t)data;

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	uint32_t modules_mask;
	npmx_error_t err_code = npmx_charger_module_get(charger_instance, &modules_mask);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to get charger module status.");
		return 0;
	}

	if ((modules_mask & NPMX_CHARGER_MODULE_CHARGER_MASK) != 0) {
		shell_error(shell, "Error: charger must be disabled to set trickle voltage.");
		return 0;
	}

	err_code = npmx_charger_trickle_voltage_set(charger_instance, charger_trickle);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d mV.",
			    npmx_charger_trickle_convert_to_mv(charger_trickle));
	} else {
		shell_error(shell, "Error: unable to set trickle voltage value.");
	}

	return 0;
}

static int cmd_buck_set(const struct shell *shell, size_t argc, char **argv)
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
	uint8_t buck_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);
	uint8_t buck_set = (uint8_t)shell_strtoul(argv[2], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: instance index and status have to be integers.");
		return 0;
	}

	if (buck_indx >= NPMX_PERIPH_BUCK_COUNT) {
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
	uint8_t buck_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: buck instance has to be an integer.");
		return 0;
	}
	if (buck_indx >= NPMX_PERIPH_BUCK_COUNT) {
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
	uint8_t buck_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);
	npmx_buck_vout_select_t selection =
		(npmx_buck_vout_select_t)shell_strtoul(argv[2], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: instance index and voltage must be integers.");
		return 0;
	}

	if (buck_indx >= NPMX_PERIPH_BUCK_COUNT) {
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
	uint8_t buck_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: buck instance has to be an integer.");
		return 0;
	}

	if (buck_indx >= NPMX_PERIPH_BUCK_COUNT) {
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
		shell_print(shell, "Value: %d mV.", npmx_buck_voltage_convert_to_mv(voltage));
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
	uint8_t buck_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);
	uint32_t buck_set = (uint32_t)shell_strtoul(argv[2], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: instance index and voltage must be integers.");
		return 0;
	}

	if (buck_indx >= NPMX_PERIPH_BUCK_COUNT) {
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
	uint8_t buck_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: buck instance has to be an integer.");
		return 0;
	}

	if (buck_indx >= NPMX_PERIPH_BUCK_COUNT) {
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
	uint8_t buck_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);
	int gpio_indx = shell_strtol(argv[2], 0, &err);

	npmx_buck_gpio_config_t gpio_config = {
		.inverted = !!shell_strtoul(argv[3], 0, &err),
	};

	if (err != 0) {
		shell_error(
			shell,
			"Error: instance index, GPIO number, GPIO inv state have to be integers.");
		return 0;
	}

	if (buck_indx >= NPMX_PERIPH_BUCK_COUNT) {
		shell_error(shell, "Error: Buck instance index is too high: no such instance.");
		return 0;
	}

	npmx_buck_t *buck_instance = npmx_buck_get(npmx_instance, buck_indx);

	static const npmx_buck_gpio_t gpios[] = {
		NPMX_BUCK_GPIO_0, NPMX_BUCK_GPIO_1, NPMX_BUCK_GPIO_2,
		NPMX_BUCK_GPIO_3, NPMX_BUCK_GPIO_4,
	};

	int pmic_int_pin = npmx_driver_int_pin_get(pmic_dev);
	int pmic_pof_pin = npmx_driver_pof_pin_get(pmic_dev);

	if (pmic_int_pin != -1) {
		if (pmic_int_pin == gpio_indx) {
			shell_error(shell, "Error: GPIO used as interrupt.");
			return 0;
		}
	}

	if (pmic_pof_pin != -1) {
		if (pmic_pof_pin == gpio_indx) {
			shell_error(shell, "Error: GPIO used as POF.");
			return 0;
		}
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

static int cmd_ldsw_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}
	if (argc < 2) {
		shell_error(shell, "Error: missing LDSW instance index and new status value.");
		return 0;
	}
	if (argc < 3) {
		shell_error(shell, "Error: missing new status value.");
		return 0;
	}

	int err = 0;
	uint8_t ldsw_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);
	uint8_t ldsw_set = (uint8_t)shell_strtoul(argv[2], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: instance index and status have to be integers.");
		return 0;
	}

	if (ldsw_indx >= NPMX_PERIPH_LDSW_COUNT) {
		shell_error(shell, "Error: LDSW instance index is too high: no such instance.");
		return 0;
	}

	if (ldsw_set > 1) {
		shell_error(shell, "Error: wrong new enable value.");
		return 0;
	}

	npmx_ldsw_task_t ldsw_task = ldsw_set == 1 ? NPMX_LDSW_TASK_ENABLE : NPMX_LDSW_TASK_DISABLE;
	npmx_ldsw_t *ldsw_instance = npmx_ldsw_get(npmx_instance, ldsw_indx);

	npmx_error_t err_code = npmx_ldsw_task_trigger(ldsw_instance, ldsw_task);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d.", ldsw_set);
	} else {
		shell_error(shell, "Error: unable to set LDSW status.");
	}

	return 0;
}

static int cmd_ldsw_get(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing LDSW instance index.");
		return 0;
	}

	int err = 0;
	uint8_t ldsw_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: instance index has to be an integer.");
		return 0;
	}

	if (ldsw_indx >= NPMX_PERIPH_LDSW_COUNT) {
		shell_error(shell, "Error: LDSW instance index is too high: no such instance.");
		return 0;
	}

	uint8_t status_mask;
	npmx_ldsw_t *ldsw_instance = npmx_ldsw_get(npmx_instance, ldsw_indx);
	npmx_error_t err_code = npmx_ldsw_status_get(ldsw_instance, &status_mask);
	uint8_t check_mask = ldsw_indx == 0 ? (NPMX_LDSW_STATUS_POWERUP_LDSW_1_MASK |
					       NPMX_LDSW_STATUS_POWERUP_LDO_1_MASK) :
					      (NPMX_LDSW_STATUS_POWERUP_LDSW_2_MASK |
					       NPMX_LDSW_STATUS_POWERUP_LDO_2_MASK);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d.", !!(status_mask & check_mask));
	} else {
		shell_error(shell, "Error: unable to get LDSW status.");
	}

	return 0;
}

static int cmd_ldsw_mode_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing LDSW instance index and LDSW mode.");
		return 0;
	}

	if (argc < 3) {
		shell_error(shell, "Error: missing LDSW mode (0 -> LOADSW, 1 -> LDO).");
		return 0;
	}

	int err = 0;
	uint8_t ldsw_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);
	npmx_ldsw_mode_t mode = (npmx_ldsw_mode_t)shell_strtoul(argv[2], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: LDSW instance index and LDSW mode must be integers.");
		return 0;
	}

	if (ldsw_indx >= NPMX_PERIPH_LDSW_COUNT) {
		shell_error(shell, "Error: LDSW instance index is too high: no such instance.");
		return 0;
	}

	if (mode != NPMX_LDSW_MODE_LOAD_SWITCH && mode != NPMX_LDSW_MODE_LDO) {
		shell_error(shell, "Error: invalid LDSW mode (0 -> LOADSW, 1 -> LDO).");
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = npmx_ldsw_get(npmx_instance, ldsw_indx);
	npmx_error_t err_code = npmx_ldsw_mode_set(ldsw_instance, mode);

	if (err_code != NPMX_SUCCESS) {
		shell_error(shell, "Error: unable to set LDSW mode.");
	}

	/* LDSW reset is required to apply mode change. */
	err_code = npmx_ldsw_task_trigger(ldsw_instance, NPMX_LDSW_TASK_DISABLE);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell,
			    "Error: reset error while disabling specified LDSW to change mode.");
	}

	err_code = npmx_ldsw_task_trigger(ldsw_instance, NPMX_LDSW_TASK_ENABLE);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d.", (uint8_t)mode);
	} else {
		shell_error(shell,
			    "Error: reset error while enabling specified LDSW to change mode.");
	}

	return 0;
}

static int cmd_ldsw_mode_get(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing LDSW instance index.");
		return 0;
	}

	int err = 0;
	uint8_t ldsw_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: LDSW instance must be an integer.");
		return 0;
	}

	if (ldsw_indx >= NPMX_PERIPH_LDSW_COUNT) {
		shell_error(shell, "Error: LDSW instance index is too high: no such instance.");
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = npmx_ldsw_get(npmx_instance, ldsw_indx);
	npmx_ldsw_mode_t mode;

	if (npmx_ldsw_mode_get(ldsw_instance, &mode) == NPMX_SUCCESS) {
		shell_print(shell, "Mode: %d.", (uint8_t)mode);
	} else {
		shell_error(shell, "Error: LDSW mode cannot be read.");
	}

	return 0;
}

static int cmd_ldsw_ldo_voltage_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}
	if (argc < 2) {
		shell_error(shell, "Error: missing LDSW instance index and LDO voltage value.");
		return 0;
	}
	if (argc < 3) {
		shell_error(shell, "Error: missing voltage value.");
		return 0;
	}

	int err = 0;
	uint8_t ldsw_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);
	uint32_t ldo_set = (uint32_t)shell_strtoul(argv[2], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: instance index and voltage must be integers.");
		return 0;
	}

	if (ldsw_indx >= NPMX_PERIPH_LDSW_COUNT) {
		shell_error(shell, "Error: LDSW instance index is too high: no such instance.");
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = npmx_ldsw_get(npmx_instance, ldsw_indx);
	npmx_ldsw_voltage_t voltage = npmx_ldsw_voltage_convert(ldo_set);

	if (voltage == NPMX_LDSW_VOLTAGE_INVALID) {
		shell_error(shell, "Error: wrong LDO voltage value.");
		return 0;
	}

	npmx_error_t err_code = npmx_ldsw_ldo_voltage_set(ldsw_instance, voltage);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %s mV.", argv[2]);
	} else {
		shell_error(shell, "Error: unable to set LDO voltage.");
	}

	return 0;
}

static int cmd_ldsw_ldo_voltage_get(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing LDSW instance index.");
		return 0;
	}

	int err = 0;
	uint8_t ldsw_indx = (uint8_t)shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: LDSW instance must be an integer.");
		return 0;
	}

	if (ldsw_indx >= NPMX_PERIPH_LDSW_COUNT) {
		shell_error(shell, "Error: LDSW instance index is too high: no such instance.");
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = npmx_ldsw_get(npmx_instance, ldsw_indx);

	npmx_error_t err_code;
	npmx_ldsw_voltage_t voltage;

	err_code = npmx_ldsw_ldo_voltage_get(ldsw_instance, &voltage);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d mV.", npmx_ldsw_voltage_convert_to_mv(voltage));
	} else {
		shell_error(shell, "Error: unable to read LDO voltage.");
	}

	return 0;
}

static int cmd_adc_ntc_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_adc_t *adc_instance = npmx_adc_get(npmx_instance, 0);

	npmx_adc_ntc_type_t ntc_type;
	npmx_error_t err_code = npmx_adc_ntc_get(adc_instance, &ntc_type);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %s.", npmx_adc_ntc_type_map_to_string(ntc_type));
	} else {
		shell_error(shell, "Error: unable to read ADC NTC value.");
	}

	return 0;
}

static int cmd_adc_ntc_set(const struct shell *shell, size_t argc, char **argv, void *data)
{
	ARG_UNUSED(argc);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_adc_t *adc_instance = npmx_adc_get(npmx_instance, 0);
	npmx_adc_ntc_type_t type = (npmx_adc_ntc_type_t)data;

	npmx_error_t err_code = npmx_adc_ntc_set(adc_instance, type);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %s.", argv[0]);
	} else {
		shell_error(shell, "Error: unable to set ADC NTC value.");
	}

	return 0;
}

static int cmd_adc_meas_take_vbat(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_adc_t *adc_instance = npmx_adc_get(npmx_instance, 0);

	npmx_error_t err_code = npmx_adc_task_trigger(adc_instance, NPMX_ADC_TASK_SINGLE_SHOT_VBAT);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read getting measurement.");
		return 0;
	}

	uint16_t battery_voltage_millivolts;
	bool meas_ready = false;

	err_code = NPMX_ERROR_INVALID_PARAM;

	while (!meas_ready) {
		err_code = npmx_adc_meas_check(adc_instance, NPMX_ADC_MEAS_VBAT, &meas_ready);
		if (!check_error_code(shell, err_code)) {
			shell_error(shell, "Error: unable to read measurement's status.");
			return 0;
		}
	}

	err_code = npmx_adc_meas_get(adc_instance, NPMX_ADC_MEAS_VBAT, &battery_voltage_millivolts);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d mV.", battery_voltage_millivolts);
	} else {
		shell_error(shell, "Error: unable to read measurement's result.");
	}

	return 0;
}

static void print_errlog(npmx_instance_t *p_pm, npmx_callback_type_t type, uint8_t mask)
{
	const struct shell *shell = (struct shell *)npmx_core_context_get(p_pm);

	shell_print(shell, "%s:", npmx_callback_to_str(type));
	for (uint8_t i = 0; i < 8; i++) {
		if ((1U << i) & mask) {
			shell_print(shell, "\t%s", npmx_callback_bit_to_str(type, i));
		}
	}
}

static int cmd_errlog_check(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_core_context_set(npmx_instance, (void *)shell);

	npmx_core_register_cb(npmx_instance, print_errlog, NPMX_CALLBACK_TYPE_RSTCAUSE);
	npmx_core_register_cb(npmx_instance, print_errlog, NPMX_CALLBACK_TYPE_CHARGER_ERROR);
	npmx_core_register_cb(npmx_instance, print_errlog, NPMX_CALLBACK_TYPE_SENSOR_ERROR);

	npmx_errlog_t *errlog_instance = npmx_errlog_get(npmx_instance, 0);

	npmx_error_t err_code = npmx_errlog_reset_errors_check(errlog_instance);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read errors log.");
	}

	return 0;
}

static int cmd_vbusin_status_connected_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	uint8_t status_mask;
	npmx_vbusin_t *vbusin_instance = npmx_vbusin_get(npmx_instance, 0);
	npmx_error_t err_code = npmx_vbusin_vbus_status_get(vbusin_instance, &status_mask);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d.",
			    !!(status_mask & NPMX_VBUSIN_STATUS_CONNECTED_MASK));
	} else {
		shell_error(shell, "Error: unable to read VBUS connected status.");
	}

	return 0;
}

/* Creating subcommands (level 4 command) array for command "charger termination_voltage normal". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_voltage_normal,
			       SHELL_CMD(get, NULL, "Get charger normal termination voltage",
					 cmd_charger_termination_voltage_normal_get),
			       SHELL_CMD(set, NULL, "Set charger normal termination voltage",
					 cmd_charger_termination_voltage_normal_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger termination_voltage". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_voltage,
			       SHELL_CMD(normal, &sub_charger_termination_voltage_normal,
					 "Charger termination voltage", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger termination_current". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_current,
			       SHELL_CMD(get, NULL, "Get charger termination current",
					 cmd_charger_termination_current_get),
			       SHELL_CMD(set, NULL, "Set charger termination current",
					 cmd_charger_termination_current_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger charging_current". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_charging_current,
			       SHELL_CMD(get, NULL, "Get charging current",
					 cmd_charger_charging_current_get),
			       SHELL_CMD(set, NULL, "Set charging current",
					 cmd_charger_charging_current_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger status charging". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_status_charging,
			       SHELL_CMD(get, NULL, "Get charging status",
					 cmd_charger_status_charging_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_status,
			       SHELL_CMD(get, NULL, "Get charger status", cmd_charger_status_get),
			       SHELL_CMD(charging, &sub_charger_status_charging, "Charging status",
					 NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger module charger". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_module_charger,
			       SHELL_CMD(set, NULL, "Enable or disable charging",
					 cmd_charger_module_charger_set),
			       SHELL_CMD(get, NULL, "Get charging status",
					 cmd_charger_module_charger_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger module recharge". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_module_recharge,
			       SHELL_CMD(set, NULL, "Enable or disable recharging",
					 cmd_charger_module_recharge_set),
			       SHELL_CMD(get, NULL, "Get recharging status",
					 cmd_charger_module_recharge_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger module". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_charger_module, SHELL_CMD(charger, &sub_charger_module_charger, "Charger module", NULL),
	SHELL_CMD(recharge, &sub_charger_module_recharge, "Recharge module", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "charger trickle set". */
SHELL_SUBCMD_DICT_SET_CREATE(charger_trickle_type, cmd_charger_trickle_set,
			     (2500, NPMX_CHARGER_TRICKLE_2V5, "2500 mV"),
			     (2900, NPMX_CHARGER_TRICKLE_2V9, "2900 mV"));

/* Creating subcommands (level 3 command) array for command "charger trickle". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_trickle,
			       SHELL_CMD(get, NULL, "Get charger trickle voltage",
					 cmd_charger_trickle_get),
			       SHELL_CMD(set, &charger_trickle_type, "Set charger trickle voltage",
					 NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "charger". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger,
			       SHELL_CMD(termination_voltage, &sub_charger_termination_voltage,
					 "Charger termination voltage", NULL),
			       SHELL_CMD(termination_current, &sub_charger_termination_current,
					 "Charger termination current", NULL),
			       SHELL_CMD(charger_current, &sub_charger_charging_current,
					 "Charger current", NULL),
			       SHELL_CMD(status, &sub_charger_status, "Charger status", NULL),
			       SHELL_CMD(module, &sub_charger_module, "Charger module", NULL),
			       SHELL_CMD(trickle, &sub_charger_trickle, "Charger trickle voltage",
					 NULL),
			       SHELL_SUBCMD_SET_END);

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

/* Creating subcommands (level 2 command) array for command "buck". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_buck, SHELL_CMD(set, NULL, "Enable or disable buck", cmd_buck_set),
	SHELL_CMD(vout, &sub_buck_vout, "Buck output voltage reference source", NULL),
	SHELL_CMD(voltage, &sub_buck_voltage, "Buck voltage", NULL),
	SHELL_CMD(gpio, &sub_buck_gpio, "Buck GPIOs", NULL), SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "sub_ldsw_mode". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_mode,
			       SHELL_CMD(set, NULL,
					 "Configure LDSW to work as LOAD SWITCH (0) or LDO (1)",
					 cmd_ldsw_mode_set),
			       SHELL_CMD(get, NULL, "Check LDSW mode", cmd_ldsw_mode_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "sub_ldsw_ldo_voltage". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_ldo_voltage,
			       SHELL_CMD(set, NULL, "Set LDO voltage", cmd_ldsw_ldo_voltage_set),
			       SHELL_CMD(get, NULL, "Get LDO voltage", cmd_ldsw_ldo_voltage_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "ldsw". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw,
			       SHELL_CMD(set, NULL, "Enable or disable LDSW", cmd_ldsw_set),
			       SHELL_CMD(get, NULL, "Check if LDSW is enabled", cmd_ldsw_get),
			       SHELL_CMD(mode, &sub_ldsw_mode, "LDSW mode", NULL),
			       SHELL_CMD(ldo_voltage, &sub_ldsw_ldo_voltage, "LDO voltage", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "adc ntc set". */
SHELL_SUBCMD_DICT_SET_CREATE(adc_ntc_type, cmd_adc_ntc_set,
			     (ntc_hi_z, NPMX_ADC_NTC_TYPE_HI_Z, "HIGH Z"),
			     (ntc_10k, NPMX_ADC_NTC_TYPE_10_K, "10k"),
			     (ntc_47k, NPMX_ADC_NTC_TYPE_47_K, "47k"),
			     (ntc_100k, NPMX_ADC_NTC_TYPE_100_K, "100k"));

/* Creating subcommands (level 3 command) array for command "adc ntc". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_ntc,
			       SHELL_CMD(get, NULL, "Get ADC NTC value", cmd_adc_ntc_get),
			       SHELL_CMD(set, &adc_ntc_type, "Set ADC NTC value", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "adc take meas". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_meas_take,
			       SHELL_CMD(vbat, NULL, "Read battery voltage",
					 cmd_adc_meas_take_vbat),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "adc meas". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_meas,
			       SHELL_CMD(take, &sub_adc_meas_take, "Take ADC measurement", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "adc". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc, SHELL_CMD(ntc, &sub_adc_ntc, "ADC NTC value", NULL),
			       SHELL_CMD(meas, &sub_adc_meas, "ADC measurement", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "errlog". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_errlog,
			       SHELL_CMD(check, NULL, "Check reset errors logs", cmd_errlog_check),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "vbusin status connected". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vbusin_status_connected,
			       SHELL_CMD(get, NULL, "Get VBUS connected status",
					 cmd_vbusin_status_connected_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "vbusin status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vbusin_status,
			       SHELL_CMD(status, &sub_vbusin_status_connected, "VBUS connected",
					 NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "vbusin". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vbusin, SHELL_CMD(vbus, &sub_vbusin_status, "Status", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 1 command) array for command "npmx". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_npmx, SHELL_CMD(charger, &sub_charger, "Charger", NULL),
			       SHELL_CMD(buck, &sub_buck, "Buck", NULL),
			       SHELL_CMD(ldsw, &sub_ldsw, "LDSW", NULL),
			       SHELL_CMD(adc, &sub_adc, "ADC", NULL),
			       SHELL_CMD(errlog, &sub_errlog, "Reset errors logs", NULL),
			       SHELL_CMD(vbusin, &sub_vbusin, "VBUSIN", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating root (level 0) command "npmx" without a handler. */
SHELL_CMD_REGISTER(npmx, &sub_npmx, "npmx", NULL);
