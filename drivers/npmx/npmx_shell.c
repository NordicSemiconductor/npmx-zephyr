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
#include <math.h>

/** @brief The difference in centigrade scale between 0*C to absolute zero temperature. */
#define ABSOLUTE_ZERO_DIFFERENCE 273.15f

/**
 * @brief All thermistors values are defined in temperature 25*C.
 *        For calculations Kelvin scale is required.
 */
#define THERMISTOR_NOMINAL_TEMPERATURE (25.0f + ABSOLUTE_ZERO_DIFFERENCE)

/** @brief GPIO configuration type. */
typedef enum {
	GPIO_CONFIG_TYPE_MODE, /* GPIO mode. */
	GPIO_CONFIG_TYPE_TYPE, /* GPIO type. */
	GPIO_CONFIG_TYPE_PULL, /* GPIO pull. */
	GPIO_CONFIG_TYPE_DRIVE, /* GPIO drive. */
	GPIO_CONFIG_TYPE_OPEN_DRAIN, /* GPIO open drain. */
	GPIO_CONFIG_TYPE_DEBOUNCE, /* GPIO debounce. */
} gpio_config_type_t;

/** @brief POF configuration type. */
typedef enum {
	POF_CONFIG_TYPE_ENABLE, /* Enable POF. */
	POF_CONFIG_TYPE_POLARITY, /* POF polarity. */
	POF_CONFIG_TYPE_THRESHOLD, /* POF threshold value. */
} pof_config_type_t;

/** @brief Timer configuration type. */
typedef enum {
	TIMER_CONFIG_TYPE_MODE, /* Configure timer mode. */
	TIMER_CONFIG_TYPE_PRESCALER, /* Configure timer prescaler mode. */
	TIMER_CONFIG_TYPE_COMPARE, /* Configure timer compare value. */
} timer_config_type_t;

/** @brief SHIP configuration type. */
typedef enum {
	SHIP_CONFIG_TYPE_TIME, /* Time required to exit from the ship or the hibernate mode. */
	SHIP_CONFIG_TYPE_INV_POLARITY /* Device is to invert the SHPHLD button active status. */
} ship_config_type_t;

/** @brief SHIP reset configuration type. */
typedef enum {
	SHIP_RESET_CONFIG_TYPE_LONG_PRESS, /* Use long press (10 s) button. */
	SHIP_RESET_CONFIG_TYPE_TWO_BUTTONS /* Use two buttons (SHPHLD and GPIO0). */
} ship_reset_config_type_t;

/** @brief Load switch soft-start configuration type. */
typedef enum {
	LDSW_SOFT_START_TYPE_ENABLE, /* Soft-start enable. */
	LDSW_SOFT_START_TYPE_CURRENT /* Soft-start current. */
} ldsw_soft_start_config_type_t;

/** @brief NTC thermistor configuration parameter. */
typedef enum {
	ADC_NTC_CONFIG_PARAM_TYPE, /* Battery NTC type. */
	ADC_NTC_CONFIG_PARAM_BETA /* Battery NTC beta value. */
} adc_ntc_config_param_t;

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
	case NPMX_ERROR_INVALID_MEAS:
		shell_error(shell, "Error: invalid measurement.");
		return false;
	}

	return false;
}

static int cmd_charger_termination_voltage_get(
	const struct shell *shell, size_t argc, char **argv,
	npmx_error_t (*func)(npmx_charger_t const *p_instance, npmx_charger_voltage_t *voltage))
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

	npmx_error_t err_code = func(charger_instance, &voltage);

	if (check_error_code(shell, err_code)) {
		uint32_t voltage_mv;

		if (npmx_charger_voltage_convert_to_mv(voltage, &voltage_mv)) {
			shell_print(shell, "Value: %d mV.", voltage_mv);
		} else {
			shell_error(
				shell,
				"Error: unable to convert battery termination voltage value to millivolts.");
		}
	} else {
		shell_error(shell, "Error: unable to read battery termination voltage value.");
	}

	return 0;
}

static int cmd_charger_termination_voltage_set(
	const struct shell *shell, size_t argc, char **argv,
	npmx_error_t (*func)(npmx_charger_t const *p_instance, npmx_charger_voltage_t voltage))
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
		shell_error(shell, "Error: voltage value has to be an integer.");
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
		shell_error(shell,
			    "Error: charger must be disabled to set battery termination voltage.");
		return 0;
	}

	err_code = func(charger_instance, voltage);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %s mV.", argv[1]);
	} else {
		shell_error(shell, "Error: unable to set battery termination voltage value.");
	}

	return 0;
}

static int cmd_charger_termination_voltage_normal_get(const struct shell *shell, size_t argc,
						      char **argv)
{
	return cmd_charger_termination_voltage_get(shell, argc, argv,
						   npmx_charger_termination_normal_voltage_get);
}

static int cmd_charger_termination_voltage_normal_set(const struct shell *shell, size_t argc,
						      char **argv)
{
	return cmd_charger_termination_voltage_set(shell, argc, argv,
						   npmx_charger_termination_normal_voltage_set);
}

static int cmd_charger_termination_voltage_warm_get(const struct shell *shell, size_t argc,
						    char **argv)
{
	return cmd_charger_termination_voltage_get(shell, argc, argv,
						   npmx_charger_termination_warm_voltage_get);
}

static int cmd_charger_termination_voltage_warm_set(const struct shell *shell, size_t argc,
						    char **argv)
{
	return cmd_charger_termination_voltage_set(shell, argc, argv,
						   npmx_charger_termination_warm_voltage_set);
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
		uint32_t current_pct;

		if (npmx_charger_iterm_convert_to_pct(current, &current_pct)) {
			shell_print(shell, "Value: %d%%.", current_pct);
		} else {
			shell_error(
				shell,
				"Error: unable to convert battery termination current value to pct.");
		}
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
	uint16_t current = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT16_MAX);

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
	uint8_t charger_set = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

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

static int cmd_charger_module_ntc_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_charger_module_set(shell, argc, argv, NPMX_CHARGER_MODULE_NTC_LIMITS_MASK);
}

static int cmd_charger_module_ntc_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_charger_module_get(shell, argc, argv, NPMX_CHARGER_MODULE_NTC_LIMITS_MASK);
}

static int cmd_charger_module_full_cool_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_charger_module_set(shell, argc, argv, NPMX_CHARGER_MODULE_FULL_COOL_MASK);
}

static int cmd_charger_module_full_cool_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_charger_module_get(shell, argc, argv, NPMX_CHARGER_MODULE_FULL_COOL_MASK);
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
		uint32_t voltage_mv;

		if (npmx_charger_trickle_convert_to_mv(voltage, &voltage_mv)) {
			shell_print(shell, "Value: %d mV.", voltage_mv);
		} else {
			shell_error(
				shell,
				"Error: unable to convert trickle voltage value to millivolts.");
		}
	} else {
		shell_error(shell, "Error: unable to read trickle voltage value.");
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

	uint32_t trickle_mv;

	if (!npmx_charger_trickle_convert_to_mv(charger_trickle, &trickle_mv)) {
		shell_error(shell, "Error: unable to convert trickle voltage value to millivolts.");
	}

	err_code = npmx_charger_trickle_voltage_set(charger_instance, charger_trickle);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d mV.", trickle_mv);
	} else {
		shell_error(shell, "Error: unable to set trickle voltage value.");
	}

	return 0;
}

static int cmd_die_temp_set(const struct shell *shell, size_t argc, char **argv,
			    npmx_error_t (*func)(npmx_charger_t const *p_instance,
						 int16_t temperature))
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing die temperature threshold value.");
		return 0;
	}

	int err = 0;
	int16_t temperature =
		(int16_t)CLAMP(shell_strtol(argv[1], 0, &err), NPM_BCHARGER_DIE_TEMPERATURE_MIN_VAL,
			       NPM_BCHARGER_DIE_TEMPERATURE_MAX_VAL);

	if (err != 0) {
		shell_error(shell, "Error: temperature has to be an integer.");
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
		shell_error(shell, "Error: charger must be disabled to set threshold value.");
		return 0;
	}

	err_code = func(charger_instance, temperature);
	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d *C.", temperature);
	} else {
		shell_error(shell, "Error: unable to set die temperature threshold.");
	}

	return 0;
}

static int cmd_die_temp_get(const struct shell *shell, size_t argc, char **argv,
			    npmx_error_t (*func)(npmx_charger_t const *p_instance,
						 int16_t *temperature))
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	int16_t temperature;

	npmx_error_t err_code = func(charger_instance, &temperature);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d *C.", temperature);
	} else {
		shell_error(shell, "Error: unable to read die temperature threshold.");
	}

	return 0;
}

static int cmd_die_temp_resume_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_die_temp_set(shell, argc, argv, npmx_charger_die_temp_resume_set);
}

static int cmd_die_temp_resume_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_die_temp_get(shell, argc, argv, npmx_charger_die_temp_resume_get);
}

static int cmd_die_temp_stop_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_die_temp_set(shell, argc, argv, npmx_charger_die_temp_stop_set);
}

static int cmd_die_temp_stop_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_die_temp_get(shell, argc, argv, npmx_charger_die_temp_stop_get);
}

static int cmd_die_temp_status_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	bool status;

	npmx_error_t err_code = npmx_charger_die_temp_status_get(charger_instance, &status);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d.", status);
	} else {
		shell_error(shell,
			    "Error: unable to read charger die temperature comparator status.");
	}

	return 0;
}

static int cmd_ntc_resistance_set(const struct shell *shell, size_t argc, char **argv,
				  npmx_error_t (*func)(npmx_charger_t const *p_instance,
						       uint32_t p_resistance))
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing NTC resistance value.");
		return 0;
	}

	int err = 0;
	uint32_t resistance = shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: resistance has to be an integer.");
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
		shell_error(shell, "Error: charger must be disabled to set NTC resistance value.");
		return 0;
	}

	err_code = func(charger_instance, resistance);
	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d Ohm.", resistance);
	} else {
		shell_error(shell, "Error: unable to set NTC resistance value.");
	}

	return 0;
}

static int cmd_ntc_resistance_get(const struct shell *shell, size_t argc, char **argv,
				  npmx_error_t (*func)(npmx_charger_t const *p_instance,
						       uint32_t *resistance))
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	uint32_t resistance;

	npmx_error_t err_code = func(charger_instance, &resistance);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d Ohm.", resistance);
	} else {
		shell_error(shell, "Error: unable to read NTC resistance value.");
	}

	return 0;
}

static int cmd_ntc_resistance_cold_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_resistance_set(shell, argc, argv, npmx_charger_cold_resistance_set);
}

static int cmd_ntc_resistance_cold_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_resistance_get(shell, argc, argv, npmx_charger_cold_resistance_get);
}

static int cmd_ntc_resistance_cool_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_resistance_set(shell, argc, argv, npmx_charger_cool_resistance_set);
}

static int cmd_ntc_resistance_cool_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_resistance_get(shell, argc, argv, npmx_charger_cool_resistance_get);
}

static int cmd_ntc_resistance_warm_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_resistance_set(shell, argc, argv, npmx_charger_warm_resistance_set);
}

static int cmd_ntc_resistance_warm_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_resistance_get(shell, argc, argv, npmx_charger_warm_resistance_get);
}

static int cmd_ntc_resistance_hot_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_resistance_set(shell, argc, argv, npmx_charger_hot_resistance_set);
}

static int cmd_ntc_resistance_hot_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_resistance_get(shell, argc, argv, npmx_charger_hot_resistance_get);
}

static int cmd_ntc_temperature_get(const struct shell *shell, size_t argc, char **argv,
				   npmx_error_t (*func)(npmx_charger_t const *p_instance,
							uint32_t *resistance))
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_adc_t *adc_instance = npmx_adc_get(npmx_instance, 0);

	npmx_adc_ntc_config_t ntc_config;
	npmx_error_t err_code = npmx_adc_ntc_config_get(adc_instance, &ntc_config);
	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read NTC config.");
		return 0;
	}

	uint32_t ntc_nominal_resistance;
	if (!npmx_adc_ntc_type_convert_to_ohms(ntc_config.type, &ntc_nominal_resistance)) {
		shell_error(shell, "Error: unable to convert NTC type to resistance.");
		return 0;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	uint32_t resistance;
	err_code = func(charger_instance, &resistance);

	if (check_error_code(shell, err_code)) {
		float numerator = THERMISTOR_NOMINAL_TEMPERATURE * (float)ntc_config.beta;
		float denominator = (THERMISTOR_NOMINAL_TEMPERATURE *
				     log((float)resistance / (float)ntc_nominal_resistance)) +
				    (float)ntc_config.beta;
		float temperature = round((numerator / denominator) - ABSOLUTE_ZERO_DIFFERENCE);
		shell_print(shell, "Value: %d *C.", (int)temperature);
	} else {
		shell_error(shell, "Error: unable to read NTC resistance value.");
	}

	return 0;
}

static int cmd_ntc_temperature_set(const struct shell *shell, size_t argc, char **argv,
				   npmx_error_t (*func)(npmx_charger_t const *p_instance,
							uint32_t p_resistance))
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing NTC temperature value.");
		return 0;
	}

	int err = 0;
	int32_t temperature = shell_strtol(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: NTC temperature has to be an integer.");
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
		shell_error(shell, "Error: charger must be disabled to set NTC temperature value.");
		return 0;
	}

	npmx_adc_t *adc_instance = npmx_adc_get(npmx_instance, 0);

	npmx_adc_ntc_config_t ntc_config;
	err_code = npmx_adc_ntc_config_get(adc_instance, &ntc_config);
	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read NTC config.");
		return 0;
	}

	uint32_t ntc_nominal_resistance;
	if (!npmx_adc_ntc_type_convert_to_ohms(ntc_config.type, &ntc_nominal_resistance)) {
		shell_error(shell, "Error: unable to convert NTC type to resistance.");
		return 0;
	}

	float target_temperature = ((float)temperature + ABSOLUTE_ZERO_DIFFERENCE);
	float exp_val = ((1.0f / target_temperature) - (1.0f / THERMISTOR_NOMINAL_TEMPERATURE)) *
			(float)ntc_config.beta;
	float resistance = round((float)ntc_nominal_resistance * exp(exp_val));

	err_code = func(charger_instance, (uint32_t)resistance);
	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d *C.", temperature);
	} else {
		shell_error(shell, "Error: unable to set NTC temperature value.");
	}

	return 0;
}

static int cmd_ntc_temperature_cold_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_temperature_set(shell, argc, argv, npmx_charger_cold_resistance_set);
}

static int cmd_ntc_temperature_cold_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_temperature_get(shell, argc, argv, npmx_charger_cold_resistance_get);
}

static int cmd_ntc_temperature_cool_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_temperature_set(shell, argc, argv, npmx_charger_cool_resistance_set);
}

static int cmd_ntc_temperature_cool_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_temperature_get(shell, argc, argv, npmx_charger_cool_resistance_get);
}

static int cmd_ntc_temperature_warm_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_temperature_set(shell, argc, argv, npmx_charger_warm_resistance_set);
}

static int cmd_ntc_temperature_warm_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_temperature_get(shell, argc, argv, npmx_charger_warm_resistance_get);
}

static int cmd_ntc_temperature_hot_set(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_temperature_set(shell, argc, argv, npmx_charger_hot_resistance_set);
}

static int cmd_ntc_temperature_hot_get(const struct shell *shell, size_t argc, char **argv)
{
	return cmd_ntc_temperature_get(shell, argc, argv, npmx_charger_hot_resistance_get);
}

static int cmd_charger_discharging_current_get(const struct shell *shell, size_t argc, char **argv)
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
	npmx_error_t err_code = npmx_charger_discharging_current_get(charger_instance, &current);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %u mA.", current);
	} else {
		shell_error(shell, "Error: unable to read trickle voltage value.");
	}

	return 0;
}

static int cmd_charger_discharging_current_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing current value.");
		return 0;
	}

	int err = 0;
	uint16_t discharging_current = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT16_MAX);

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
		shell_error(shell, "Error: charger must be disabled to set discharging current.");
		return 0;
	}

	err_code = npmx_charger_discharging_current_set(charger_instance, discharging_current);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to set discharging current value.");
	}

	err_code = npmx_charger_discharging_current_get(charger_instance, &discharging_current);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d mA.", discharging_current);
		shell_print(shell, "Set value may be different than requested.");
	} else {
		shell_error(shell, "Error: unable to get discharging current value.");
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

	if (buck_indx >= NPM_BUCK_COUNT) {
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
	uint8_t ldsw_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	uint8_t ldsw_set = CLAMP(shell_strtoul(argv[2], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: instance index and status have to be integers.");
		return 0;
	}

	if (ldsw_indx >= NPM_LDSW_COUNT) {
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
	uint8_t ldsw_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: instance index has to be an integer.");
		return 0;
	}

	if (ldsw_indx >= NPM_LDSW_COUNT) {
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
	uint8_t ldsw_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	npmx_ldsw_mode_t mode =
		(npmx_ldsw_mode_t)(CLAMP(shell_strtoul(argv[2], 0, &err), 0, UINT8_MAX));

	if (err != 0) {
		shell_error(shell, "Error: LDSW instance index and LDSW mode must be integers.");
		return 0;
	}

	if (ldsw_indx >= NPM_LDSW_COUNT) {
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
	uint8_t ldsw_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: LDSW instance must be an integer.");
		return 0;
	}

	if (ldsw_indx >= NPM_LDSW_COUNT) {
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
	uint8_t ldsw_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	uint32_t ldo_set = (uint32_t)shell_strtoul(argv[2], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: instance index and voltage must be integers.");
		return 0;
	}

	if (ldsw_indx >= NPM_LDSW_COUNT) {
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
	uint8_t ldsw_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: LDSW instance must be an integer.");
		return 0;
	}

	if (ldsw_indx >= NPM_LDSW_COUNT) {
		shell_error(shell, "Error: LDSW instance index is too high: no such instance.");
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = npmx_ldsw_get(npmx_instance, ldsw_indx);

	npmx_error_t err_code;
	npmx_ldsw_voltage_t voltage;

	err_code = npmx_ldsw_ldo_voltage_get(ldsw_instance, &voltage);

	if (check_error_code(shell, err_code)) {
		uint32_t voltage_mv;

		if (npmx_ldsw_voltage_convert_to_mv(voltage, &voltage_mv)) {
			shell_print(shell, "Value: %d mV.", voltage_mv);
		} else {
			shell_error(shell,
				    "Error: unable to convert LDO voltage value to millivolts.");
		}
	} else {
		shell_error(shell, "Error: unable to read LDO voltage.");
	}

	return 0;
}

static int ldsw_soft_start_config_set(const struct shell *shell, size_t argc, char **argv,
				      ldsw_soft_start_config_type_t config_type)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}
	if (argc < 2) {
		shell_error(shell, "Error: missing LDSW instance index and config value.");
		return 0;
	}
	if (argc < 3) {
		shell_error(shell, "Error: missing config value.");
		return 0;
	}

	int err = 0;
	uint8_t ldsw_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	uint32_t config_val = (uint32_t)shell_strtoul(argv[2], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: instance index and config value must be integers.");
		return 0;
	}

	if (ldsw_indx >= NPM_LDSW_COUNT) {
		shell_error(shell, "Error: LDSW instance index is too high: no such instance.");
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = npmx_ldsw_get(npmx_instance, ldsw_indx);
	npmx_ldsw_soft_start_config_t soft_start_config;
	npmx_error_t err_code = npmx_ldsw_soft_start_config_get(ldsw_instance, &soft_start_config);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read soft-start config.");
	}

	switch (config_type) {
	case LDSW_SOFT_START_TYPE_ENABLE:
		soft_start_config.enable = !!config_val;
		break;

	case LDSW_SOFT_START_TYPE_CURRENT:
		npmx_ldsw_soft_start_current_t current =
			npmx_ldsw_soft_start_current_convert(config_val);

		if (current == NPMX_LDSW_SOFT_START_CURRENT_INVALID) {
			shell_error(shell, "Error: wrong soft-start value.");
			return 0;
		}
		soft_start_config.current = current;
		break;
	}

	err_code = npmx_ldsw_soft_start_config_set(ldsw_instance, &soft_start_config);

	if (check_error_code(shell, err_code)) {
		switch (config_type) {
		case LDSW_SOFT_START_TYPE_ENABLE:
			shell_print(shell, "Success: %d.", !!config_val);
			break;
		case LDSW_SOFT_START_TYPE_CURRENT:
			shell_print(shell, "Success: %u mA.", config_val);
			break;
		}
	} else {
		shell_error(shell, "Error: unable to set soft-start value.");
	}

	return 0;
}

static int ldsw_soft_start_config_get(const struct shell *shell, size_t argc, char **argv,
				      ldsw_soft_start_config_type_t config_type)
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
	uint8_t ldsw_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: LDSW instance must be an integer.");
		return 0;
	}

	if (ldsw_indx >= NPM_LDSW_COUNT) {
		shell_error(shell, "Error: LDSW instance index is too high: no such instance.");
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = npmx_ldsw_get(npmx_instance, ldsw_indx);

	npmx_error_t err_code;
	npmx_ldsw_soft_start_config_t soft_start_config;

	err_code = npmx_ldsw_soft_start_config_get(ldsw_instance, &soft_start_config);

	if (check_error_code(shell, err_code)) {
		switch (config_type) {
		case LDSW_SOFT_START_TYPE_ENABLE:
			shell_print(shell, "Value: %d.", soft_start_config.enable);
			break;
		case LDSW_SOFT_START_TYPE_CURRENT:
			uint32_t config_val;
			if (npmx_ldsw_soft_start_current_convert_to_ma(soft_start_config.current,
								       &config_val)) {
				shell_print(shell, "Value: %u mA.", config_val);
			} else {
				shell_error(
					shell,
					"Error: unable to convert current value to milliamperes.");
			}

			break;
		}

	} else {
		shell_error(shell, "Error: unable to read soft-start config.");
	}

	return 0;
}

static int cmd_ldsw_soft_start_enable_get(const struct shell *shell, size_t argc, char **argv)
{
	return ldsw_soft_start_config_get(shell, argc, argv, LDSW_SOFT_START_TYPE_ENABLE);
}

static int cmd_ldsw_soft_start_enable_set(const struct shell *shell, size_t argc, char **argv)
{
	return ldsw_soft_start_config_set(shell, argc, argv, LDSW_SOFT_START_TYPE_ENABLE);
}

static int cmd_ldsw_soft_start_current_get(const struct shell *shell, size_t argc, char **argv)
{
	return ldsw_soft_start_config_get(shell, argc, argv, LDSW_SOFT_START_TYPE_CURRENT);
}

static int cmd_ldsw_soft_start_current_set(const struct shell *shell, size_t argc, char **argv)
{
	return ldsw_soft_start_config_set(shell, argc, argv, LDSW_SOFT_START_TYPE_CURRENT);
}

static int cmd_ldsw_active_discharge_enable_get(const struct shell *shell, size_t argc, char **argv)
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
	uint8_t ldsw_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: LDSW instance must be an integer.");
		return 0;
	}

	if (ldsw_indx >= NPM_LDSW_COUNT) {
		shell_error(shell, "Error: LDSW instance index is too high: no such instance.");
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = npmx_ldsw_get(npmx_instance, ldsw_indx);

	npmx_error_t err_code;
	bool enable;

	err_code = npmx_ldsw_active_discharge_enable_get(ldsw_instance, &enable);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d.", enable);
	} else {
		shell_error(shell, "Error: unable to read active discharge enable status.");
	}

	return 0;
}

static int cmd_ldsw_active_discharge_enable_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}
	if (argc < 2) {
		shell_error(shell, "Error: missing LDSW instance index and enable value.");
		return 0;
	}
	if (argc < 3) {
		shell_error(shell, "Error: missing enable value.");
		return 0;
	}

	int err = 0;
	uint8_t ldsw_indx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	uint8_t discharge_enable = CLAMP(shell_strtoul(argv[2], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: instance index and enable value must be integers.");
		return 0;
	}

	if (ldsw_indx >= NPM_LDSW_COUNT) {
		shell_error(shell, "Error: LDSW instance index is too high: no such instance.");
		return 0;
	}

	if (discharge_enable > 1) {
		shell_error(
			shell,
			"Error: invalid active discharge value. Accepted values: 0 = disabled, 1 = enabled.");
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = npmx_ldsw_get(npmx_instance, ldsw_indx);

	npmx_error_t err_code =
		npmx_ldsw_active_discharge_enable_set(ldsw_instance, discharge_enable == 1);

	if (err_code == NPMX_SUCCESS) {
		shell_print(shell, "Success: %d.", discharge_enable);
	} else {
		shell_error(shell, "Error: unable to set ldsw active discharge status.");
	}

	return 0;
}

static int cmd_leds_mode_get(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing LED instance index.");
		return 0;
	}

	int err = 0;
	uint8_t led_idx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: LED instance index must be an integer.");
		return 0;
	}

	if (led_idx >= NPM_LEDDRV_COUNT) {
		shell_error(shell, "Error: LED instance index is too high: no such instance.");
		return 0;
	}

	npmx_led_t *led_instance = npmx_led_get(npmx_instance, led_idx);

	npmx_led_mode_t mode;
	npmx_error_t err_code = npmx_led_mode_get(led_instance, &mode);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %d.", mode);
	} else {
		shell_error(shell, "Error: unable to read LED %d mode.", led_idx);
	}

	return 0;
}

static int cmd_leds_mode_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing LED instance index and LED mode.");
		return 0;
	}

	if (argc < 3) {
		shell_error(shell, "Error: missing LED mode.");
		return 0;
	}

	int err = 0;
	uint8_t led_idx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	int led_mode = shell_strtol(argv[2], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: instance index and mode have to be integers.");
		return 0;
	}

	if (led_idx >= NPM_LEDDRV_COUNT) {
		shell_error(shell, "Error: LED instance index is too high: no such instance.");
		return 0;
	}

	npmx_led_t *led_instance = npmx_led_get(npmx_instance, led_idx);
	npmx_led_mode_t mode;

	switch (led_mode) {
	case 0:
		mode = NPMX_LED_MODE_ERROR;
		break;
	case 1:
		mode = NPMX_LED_MODE_CHARGING;
		break;
	case 2:
		mode = NPMX_LED_MODE_HOST;
		break;
	case 3:
		mode = NPMX_LED_MODE_NOTUSED;
		break;
	default:
		shell_error(
			shell,
			"Error: LED mode can be 0-Charger error, 1-Charging, 2-HOST, 3-Not used.");
		return 0;
	}

	npmx_error_t err_code = npmx_led_mode_set(led_instance, mode);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d.", led_mode);
	} else {
		shell_error(shell, "Error: unable to set LED %d mode.", led_idx);
	}

	return 0;
}

static int cmd_leds_state_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing LED instance index and LED state.");
		return 0;
	}

	if (argc < 3) {
		shell_error(shell, "Error: missing LED state.");
		return 0;
	}

	int err = 0;
	uint8_t led_idx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);
	uint8_t led_state = CLAMP(shell_strtoul(argv[2], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: instance index and state have to be integers.");
		return 0;
	}

	if (led_idx >= NPM_LEDDRV_COUNT) {
		shell_error(shell, "Error: LED instance index is too high: no such instance.");
		return 0;
	}

	if (led_state > 1) {
		shell_error(shell, "Error: invalid LED state value (0 -> OFF, 1 -> ON).");
		return 0;
	}

	npmx_led_t *led_instance = npmx_led_get(npmx_instance, led_idx);
	npmx_error_t err_code = npmx_led_state_set(led_instance, (led_state == 1 ? true : false));

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d.", led_state);
	} else {
		shell_error(shell, "Error: unable to set LED %d state.", led_idx);
	}

	return 0;
}

static npmx_gpio_mode_t gpio_mode_convert(uint8_t mode)
{
	switch (mode) {
	case 0:
		return NPMX_GPIO_MODE_INPUT;
	case 1:
		return NPMX_GPIO_MODE_INPUT_OVERRIDE_1;
	case 2:
		return NPMX_GPIO_MODE_INPUT_OVERRIDE_0;
	case 3:
		return NPMX_GPIO_MODE_INPUT_RISING_EDGE;
	case 4:
		return NPMX_GPIO_MODE_INPUT_FALLING_EDGE;
	case 5:
		return NPMX_GPIO_MODE_OUTPUT_IRQ;
	case 6:
		return NPMX_GPIO_MODE_OUTPUT_RESET;
	case 7:
		return NPMX_GPIO_MODE_OUTPUT_PLW;
	case 8:
		return NPMX_GPIO_MODE_OUTPUT_OVERRIDE_1;
	case 9:
		return NPMX_GPIO_MODE_OUTPUT_OVERRIDE_0;
	default:
		return NPMX_GPIO_MODE_INVALID;
	}
}

static npmx_gpio_pull_t gpio_pull_convert(uint8_t pull)
{
	switch (pull) {
	case 0:
		return NPMX_GPIO_PULL_DOWN;
	case 1:
		return NPMX_GPIO_PULL_UP;
	case 2:
		return NPMX_GPIO_PULL_NONE;
	default:
		return NPMX_GPIO_PULL_INVALID;
	}
}

static npmx_error_t gpio_config_type_set(const struct shell *shell, gpio_config_type_t config_type,
					 npmx_gpio_config_t *gpio_config, uint32_t input_arg)
{
	if (config_type == GPIO_CONFIG_TYPE_MODE) {
		gpio_config->mode = gpio_mode_convert(input_arg);

		if (gpio_config->mode == NPMX_GPIO_MODE_INVALID) {
			shell_error(shell, "Error: mode value can be:");
			shell_error(shell, "       0 - Input");
			shell_error(shell, "       1 - Input logic 1");
			shell_error(shell, "       2 - Input logic 0");
			shell_error(shell, "       3 - Input rising edge event");
			shell_error(shell, "       4 - Input falling edge event");
			shell_error(shell, "       5 - Output interrupt");
			shell_error(shell, "       6 - Output reset");
			shell_error(shell, "       7 - Output power loss warning");
			shell_error(shell, "       8 - Output logic 1");
			shell_error(shell, "       9 - Output logic 0");
			return NPMX_ERROR_INVALID_PARAM;
		}
	} else if (config_type == GPIO_CONFIG_TYPE_PULL) {
		gpio_config->pull = gpio_pull_convert(input_arg);

		if (gpio_config->pull == NPMX_GPIO_PULL_INVALID) {
			shell_error(
				shell,
				"Error: pull value can be 0-pull down, 1-pull up, 2-pull disable.");
			return NPMX_ERROR_INVALID_PARAM;
		}
	} else if (config_type == GPIO_CONFIG_TYPE_DRIVE) {
		gpio_config->drive = npmx_gpio_drive_convert(input_arg);

		if (gpio_config->drive == NPMX_GPIO_DRIVE_INVALID) {
			shell_error(shell, "Error: drive current value can be 1 or 6 [mA].");
			return NPMX_ERROR_INVALID_PARAM;
		}
	} else if (config_type == GPIO_CONFIG_TYPE_OPEN_DRAIN) {
		if (input_arg <= 1) {
			gpio_config->open_drain = (input_arg != 0);
		} else {
			shell_error(shell, "Error: open drain value can be 0-off or 1-on.");
			return NPMX_ERROR_INVALID_PARAM;
		}
	} else if (config_type == GPIO_CONFIG_TYPE_DEBOUNCE) {
		if (input_arg <= 1) {
			gpio_config->debounce = (input_arg != 0);
		} else {
			shell_error(shell, "Error: debounce value can be 0-off or 1-on.");
			return NPMX_ERROR_INVALID_PARAM;
		}
	} else {
		shell_error(shell, "Error: invalid config type value.");
		return NPMX_ERROR_INVALID_PARAM;
	}

	return NPMX_SUCCESS;
}

static int gpio_config_get(const struct shell *shell, size_t argc, char **argv,
			   gpio_config_type_t config_type)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing GPIO number.");
		return 0;
	}

	int err = 0;
	uint8_t gpio_idx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: GPIO number has to be an integer.");
		return 0;
	}

	if (gpio_idx >= NPM_GPIOS_COUNT) {
		shell_error(shell, "Error: GPIO number is too high: no such instance.");
		return 0;
	}

	npmx_gpio_t *gpio_instance = npmx_gpio_get(npmx_instance, gpio_idx);

	npmx_gpio_config_t gpio_config;
	npmx_error_t err_code = npmx_gpio_config_get(gpio_instance, &gpio_config);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read GPIO config.");
		return 0;
	}

	if (config_type == GPIO_CONFIG_TYPE_MODE) {
		shell_print(shell, "Value: %d.", (int)gpio_config.mode);
	} else if (config_type == GPIO_CONFIG_TYPE_TYPE) {
		if (gpio_config.mode <= NPMX_GPIO_MODE_INPUT_FALLING_EDGE) {
			shell_print(shell, "Value: input.");
		} else {
			shell_print(shell, "Value: output.");
		}
	} else if (config_type == GPIO_CONFIG_TYPE_PULL) {
		shell_print(shell, "Value: %d.", (int)gpio_config.pull);
	} else if (config_type == GPIO_CONFIG_TYPE_DRIVE) {
		uint32_t drive_val;
		bool ok = npmx_gpio_drive_convert_to_ma(gpio_config.drive, &drive_val);
		if (ok) {
			shell_print(shell, "Value: %u.", drive_val);
		} else {
			shell_error(shell, "Error: unable to read GPIO drive current value.");
			return 0;
		}
	} else if (config_type == GPIO_CONFIG_TYPE_OPEN_DRAIN) {
		shell_print(shell, "Value: %d.", (int)gpio_config.open_drain);
	} else if (config_type == GPIO_CONFIG_TYPE_DEBOUNCE) {
		shell_print(shell, "Value: %d.", (int)gpio_config.debounce);
	} else {
		shell_error(shell, "Error: invalid config type value.");
		return 0;
	}

	return 0;
}

static int gpio_config_set(const struct shell *shell, size_t argc, char **argv,
			   gpio_config_type_t config_type)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	char config_name[14];
	if (config_type == GPIO_CONFIG_TYPE_MODE) {
		strcpy(config_name, "mode");
	} else if (config_type == GPIO_CONFIG_TYPE_PULL) {
		strcpy(config_name, "pull");
	} else if (config_type == GPIO_CONFIG_TYPE_DRIVE) {
		strcpy(config_name, "drive current");
	} else if (config_type == GPIO_CONFIG_TYPE_OPEN_DRAIN) {
		strcpy(config_name, "open drain");
	} else if (config_type == GPIO_CONFIG_TYPE_DEBOUNCE) {
		strcpy(config_name, "debounce");
	} else {
		shell_error(shell, "Error: invalid config type value.");
		return 0;
	}

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing GPIO number and %s config.", config_name);
		return 0;
	}

	if (argc < 3) {
		shell_error(shell, "Error: missing GPIO %s config.", config_name);
		return 0;
	}

	int err = 0;
	uint8_t gpio_idx = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: GPIO number has to be an integer.");
		return 0;
	}

	if (gpio_idx >= NPM_GPIOS_COUNT) {
		shell_error(shell, "Error: GPIO number is too high: no such GPIO instance.");
		return 0;
	}

	int pmic_int_pin = npmx_driver_int_pin_get(pmic_dev);
	int pmic_pof_pin = npmx_driver_pof_pin_get(pmic_dev);

	if ((pmic_int_pin != -1) && (pmic_int_pin == gpio_idx)) {
		shell_error(shell, "Error: GPIO used as interrupt.");
		return 0;
	}

	if ((pmic_pof_pin != -1) && (pmic_pof_pin == gpio_idx)) {
		shell_error(shell, "Error: GPIO used as POF.");
		return 0;
	}

	uint8_t input_arg = CLAMP(shell_strtoul(argv[2], 0, &err), 0, UINT8_MAX);

	if (err != 0) {
		shell_error(shell, "Error: GPIO %s config has to be an integer.", config_name);
		return 0;
	}

	npmx_gpio_t *gpio_instance = npmx_gpio_get(npmx_instance, gpio_idx);

	npmx_gpio_config_t gpio_config;
	npmx_error_t err_code = npmx_gpio_config_get(gpio_instance, &gpio_config);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read GPIO config.");
		return 0;
	}

	if (gpio_config_type_set(shell, config_type, &gpio_config, input_arg) != NPMX_SUCCESS) {
		return 0;
	}

	err_code = npmx_gpio_config_set(gpio_instance, &gpio_config);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to set GPIO config.");
		return 0;
	}

	shell_print(shell, "Success: %d.", input_arg);

	return 0;
}

static int cmd_gpio_mode_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_TYPE_MODE);
}

static int cmd_gpio_mode_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_TYPE_MODE);
}

static int cmd_gpio_type_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_TYPE_TYPE);
}

static int cmd_gpio_pull_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_TYPE_PULL);
}

static int cmd_gpio_pull_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_TYPE_PULL);
}

static int cmd_gpio_drive_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_TYPE_DRIVE);
}

static int cmd_gpio_drive_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_TYPE_DRIVE);
}

static int cmd_gpio_open_drain_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_TYPE_OPEN_DRAIN);
}

static int cmd_gpio_open_drain_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_TYPE_OPEN_DRAIN);
}

static int cmd_gpio_debounce_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_TYPE_DEBOUNCE);
}

static int cmd_gpio_debounce_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_TYPE_DEBOUNCE);
}

static int adc_ntc_get(const struct shell *shell, size_t argc, char **argv,
		       adc_ntc_config_param_t config_type)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);
	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_adc_t *adc_instance = npmx_adc_get(npmx_instance, 0);

	npmx_adc_ntc_config_t ntc_config;
	npmx_error_t err_code = npmx_adc_ntc_config_get(adc_instance, &ntc_config);

	if (check_error_code(shell, err_code)) {
		switch (config_type) {
		case ADC_NTC_CONFIG_PARAM_TYPE:
			uint32_t config_value;
			if (!npmx_adc_ntc_type_convert_to_ohms(ntc_config.type, &config_value)) {
				shell_error(shell,
					    "Error: unable to convert NTC type to resistance.");
				return 0;
			}
			shell_print(shell, "Value: %u Ohm.", config_value);
			break;
		case ADC_NTC_CONFIG_PARAM_BETA:
			shell_print(shell, "Value: %u.", ntc_config.beta);
			break;
		}
	} else {
		shell_error(shell, "Error: unable to read ADC NTC config value.");
	}

	return 0;
}

static int adc_ntc_set(const struct shell *shell, size_t argc, char **argv,
		       adc_ntc_config_param_t config_type)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing config value.");
		return 0;
	}

	int err = 0;
	uint32_t config_val = shell_strtoul(argv[1], 0, &err);
	if (err != 0) {
		shell_error(shell, "Error: config has to be an integer.");
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
		shell_error(shell, "Error: charger must be disabled to set ADC NTC value.");
		return 0;
	}

	npmx_adc_t *adc_instance = npmx_adc_get(npmx_instance, 0);
	npmx_adc_ntc_config_t ntc_config;
	err_code = npmx_adc_ntc_config_get(adc_instance, &ntc_config);
	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read ADC NTC config.");
		return 0;
	}

	switch (config_type) {
	case ADC_NTC_CONFIG_PARAM_TYPE:
		ntc_config.type = npmx_adc_ntc_type_convert(config_val);
		if (ntc_config.type == NPMX_ADC_NTC_TYPE_INVALID) {
			shell_error(shell, "Error: unable to convert resistance to NTC type.");
			return 0;
		}

		if (ntc_config.type == NPMX_ADC_NTC_TYPE_HI_Z) {
			err_code = npmx_charger_module_get(charger_instance, &modules_mask);
			if (!check_error_code(shell, err_code)) {
				shell_error(shell,
					    "Error: unable to get NTC limits module status.");
			}

			if ((modules_mask & NPMX_CHARGER_MODULE_NTC_LIMITS_MASK) == 0) {
				/* NTC limits module already disabled. */
				break;
			}

			err_code = npmx_charger_module_disable_set(
				charger_instance, NPMX_CHARGER_MODULE_NTC_LIMITS_MASK);
			if (check_error_code(shell, err_code)) {
				shell_info(
					shell,
					"Info: the NTC temperature limit control module has been disabled.");
				shell_info(shell,
					   "      To re-enable, change the NTC type to != 0.");
			} else {
				shell_error(
					shell,
					"Error: unable to disable the NTC temperature limit control module.");
				return 0;
			}
		} else {
			err_code = npmx_charger_module_get(charger_instance, &modules_mask);
			if (!check_error_code(shell, err_code)) {
				shell_error(shell,
					    "Error: unable to get NTC limits module status.");
			}

			if ((modules_mask & NPMX_CHARGER_MODULE_NTC_LIMITS_MASK) > 0) {
				/* NTC limits module already enabled. */
				break;
			}

			err_code = npmx_charger_module_enable_set(
				charger_instance, NPMX_CHARGER_MODULE_NTC_LIMITS_MASK);
			if (check_error_code(shell, err_code)) {
				shell_info(
					shell,
					"Info: the NTC temperature limit control module has been enabled.");
			} else {
				shell_error(
					shell,
					"Error: unable to enable the NTC temperature limit control module.");
				return 0;
			}
		}
		break;

	case ADC_NTC_CONFIG_PARAM_BETA:
		ntc_config.beta = config_val;
		break;
	}

	err_code = npmx_adc_ntc_config_set(adc_instance, &ntc_config);
	if (check_error_code(shell, err_code)) {
		switch (config_type) {
		case ADC_NTC_CONFIG_PARAM_TYPE:
			shell_print(shell, "Success: %u Ohm.", config_val);
			break;
		case ADC_NTC_CONFIG_PARAM_BETA:
			shell_print(shell, "Success: %u.", config_val);
			break;
		}
	} else {
		shell_error(shell, "Error: unable to set ADC NTC value.");
	}

	return 0;
}

static int cmd_adc_ntc_type_get(const struct shell *shell, size_t argc, char **argv)
{
	return adc_ntc_get(shell, argc, argv, ADC_NTC_CONFIG_PARAM_TYPE);
}

static int cmd_adc_ntc_type_set(const struct shell *shell, size_t argc, char **argv)
{
	return adc_ntc_set(shell, argc, argv, ADC_NTC_CONFIG_PARAM_TYPE);
}

static int cmd_adc_ntc_beta_get(const struct shell *shell, size_t argc, char **argv)
{
	return adc_ntc_get(shell, argc, argv, ADC_NTC_CONFIG_PARAM_BETA);
}

static int cmd_adc_ntc_beta_set(const struct shell *shell, size_t argc, char **argv)
{
	return adc_ntc_set(shell, argc, argv, ADC_NTC_CONFIG_PARAM_BETA);
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

	int32_t battery_voltage_millivolts;
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

static int pof_config_get(const struct shell *shell, pof_config_type_t config_type)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_pof_t *pof_instance = npmx_pof_get(npmx_instance, 0);

	npmx_pof_config_t pof_config;
	npmx_error_t err_code = npmx_pof_config_get(pof_instance, &pof_config);

	if (check_error_code(shell, err_code)) {
		switch (config_type) {
		case POF_CONFIG_TYPE_ENABLE:
			shell_print(shell, "Value: %d.", (int)pof_config.status);
			break;
		case POF_CONFIG_TYPE_POLARITY:
			shell_print(shell, "Value: %d.", (int)pof_config.polarity);
			break;
		case POF_CONFIG_TYPE_THRESHOLD:
			uint32_t mvolts;
			if (npmx_pof_threshold_convert_to_mv(pof_config.threshold, &mvolts)) {
				shell_print(shell, "Value: %d mV.", mvolts);
			} else {
				shell_error(
					shell,
					"Error: unable to convert threshold value to millivolts.");
			}
			break;
		}
	} else {
		shell_error(shell, "Error: unable to read POF config.");
	}

	return 0;
}

static int pof_config_set(const struct shell *shell, size_t argc, char **argv,
			  pof_config_type_t config_type)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		switch (config_type) {
		case POF_CONFIG_TYPE_ENABLE:
			shell_error(shell, "Error: missing enable value.");
			return 0;
		case POF_CONFIG_TYPE_POLARITY:
			shell_error(shell, "Error: missing polarity value.");
			return 0;
		case POF_CONFIG_TYPE_THRESHOLD:
			shell_error(shell, "Error: missing threshold value.");
			return 0;
		default:
			shell_error(shell, "Error: invalid config type.");
			return 0;
		}
	}

	int err = 0;
	uint32_t value = CLAMP(shell_strtoul(argv[1], 0, &err), 0, UINT32_MAX);

	if (err != 0) {
		shell_error(shell, "Error: value must be an integer.");
		return 0;
	}

	npmx_pof_t *pof_instance = npmx_pof_get(npmx_instance, 0);

	npmx_pof_config_t pof_config;
	npmx_error_t err_code = npmx_pof_config_get(pof_instance, &pof_config);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to get POF config.");
	}

	switch (config_type) {
	case POF_CONFIG_TYPE_ENABLE:
		if (value < NPMX_POF_STATUS_COUNT) {
			pof_config.status =
				(value ? NPMX_POF_STATUS_ENABLE : NPMX_POF_STATUS_DISABLE);
		} else {
			shell_error(shell, "Error: enable value can be 0-Disable, 1-Enable.");
			return 0;
		}
		break;
	case POF_CONFIG_TYPE_POLARITY:
		if (value < NPMX_POF_POLARITY_COUNT) {
			pof_config.polarity =
				(value ? NPMX_POF_POLARITY_HIGH : NPMX_POF_POLARITY_LOW);
		} else {
			shell_error(shell,
				    "Error: polarity value can be 0-Active low, 1-Active high.");
			return 0;
		}
		break;
	case POF_CONFIG_TYPE_THRESHOLD:
		pof_config.threshold = npmx_pof_threshold_convert(value);
		if (pof_config.threshold == NPMX_POF_THRESHOLD_INVALID) {
			shell_error(
				shell,
				"Error: threshold value can be between 2600mV and 3500mV, in increments of 100mV.");
			return 0;
		}
		break;
	default:
		shell_error(shell, "Error: invalid config type.");
		return 0;
	}

	err_code = npmx_pof_config_set(pof_instance, &pof_config);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d.", value);
	} else {
		shell_error(shell, "Error: unable to set POF config.");
	}

	return 0;
}

static int cmd_pof_enable_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return pof_config_get(shell, POF_CONFIG_TYPE_ENABLE);
}

static int cmd_pof_enable_set(const struct shell *shell, size_t argc, char **argv)
{
	return pof_config_set(shell, argc, argv, POF_CONFIG_TYPE_ENABLE);
}

static int cmd_pof_polarity_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return pof_config_get(shell, POF_CONFIG_TYPE_POLARITY);
}

static int cmd_pof_polarity_set(const struct shell *shell, size_t argc, char **argv)
{
	return pof_config_set(shell, argc, argv, POF_CONFIG_TYPE_POLARITY);
}

static int cmd_pof_threshold_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return pof_config_get(shell, POF_CONFIG_TYPE_THRESHOLD);
}

static int cmd_pof_threshold_set(const struct shell *shell, size_t argc, char **argv)
{
	return pof_config_set(shell, argc, argv, POF_CONFIG_TYPE_THRESHOLD);
}

static npmx_timer_mode_t timer_mode_int_convert_to_enum(uint32_t timer_mode)
{
	switch (timer_mode) {
	case 0:
		return NPMX_TIMER_MODE_BOOT_MONITOR;
	case 1:
		return NPMX_TIMER_MODE_WATCHDOG_WARNING;
	case 2:
		return NPMX_TIMER_MODE_WATCHDOG_RESET;
	case 3:
		return NPMX_TIMER_MODE_GENERAL_PURPOSE;
	case 4:
		return NPMX_TIMER_MODE_WAKEUP;
	default:
		return NPMX_TIMER_MODE_INVALID;
	}
}

static int timer_config_get(const struct shell *shell, timer_config_type_t config_type)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_timer_t *timer_instance = npmx_timer_get(npmx_instance, 0);

	npmx_timer_config_t timer_config;
	npmx_error_t err_code = npmx_timer_config_get(timer_instance, &timer_config);

	if (check_error_code(shell, err_code)) {
		if (config_type == TIMER_CONFIG_TYPE_MODE) {
			shell_print(shell, "Value: %d.", timer_config.mode);
		} else if (config_type == TIMER_CONFIG_TYPE_PRESCALER) {
			shell_print(shell, "Value: %d.", timer_config.prescaler);
		} else if (config_type == TIMER_CONFIG_TYPE_COMPARE) {
			shell_print(shell, "Value: %d.", timer_config.compare_value);
		}
	} else {
		shell_error(shell, "Error: unable to read timer configuration.");
	}

	return 0;
}

static int timer_config_set(const struct shell *shell, size_t argc, char **argv,
			    timer_config_type_t config_type)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	char config_name[10];
	if (config_type == TIMER_CONFIG_TYPE_MODE) {
		strcpy(config_name, "mode");
	} else if (config_type == TIMER_CONFIG_TYPE_PRESCALER) {
		strcpy(config_name, "prescaler");
	} else if (config_type == TIMER_CONFIG_TYPE_COMPARE) {
		strcpy(config_name, "period");
	} else {
		shell_error(shell, "Error: invalid config type value.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing timer %s value.", config_name);
		return 0;
	}

	int err = 0;
	uint32_t input_value = (uint32_t)shell_strtoul(argv[1], 0, &err);
	if (err != 0) {
		shell_error(shell, "Error: timer %s has to be an integer.", config_name);
		return 0;
	}

	npmx_timer_t *timer_instance = npmx_timer_get(npmx_instance, 0);

	npmx_timer_config_t timer_config;
	npmx_error_t err_code = npmx_timer_config_get(timer_instance, &timer_config);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read timer configuration.");
		return 0;
	}

	if (config_type == TIMER_CONFIG_TYPE_MODE) {
		timer_config.mode = timer_mode_int_convert_to_enum(input_value);

		if (timer_config.mode == NPMX_TIMER_MODE_INVALID) {
			shell_error(shell,
				    "Error: timer mode can be: 0-boot monitor, 1-watchdog warning, "
				    "2-watchdog reset, 3-general purpose or 4-wakeup.");
			return 0;
		}
	} else if (config_type == TIMER_CONFIG_TYPE_PRESCALER) {
		if (input_value < NPMX_TIMER_PRESCALER_COUNT) {
			timer_config.prescaler = (input_value ? NPMX_TIMER_PRESCALER_FAST :
								NPMX_TIMER_PRESCALER_SLOW);
		} else {
			shell_error(shell, "Error: timer prescaler value can be 0-slow or 1-fast.");
			return 0;
		}
	} else if (config_type == TIMER_CONFIG_TYPE_COMPARE) {
		if (input_value <= NPM_TIMER_COUNTER_COMPARE_VALUE_MAX) {
			timer_config.compare_value = input_value;
		} else {
			shell_error(shell, "Error: timer period value can be 0..0x%lX.",
				    NPM_TIMER_COUNTER_COMPARE_VALUE_MAX);
			return 0;
		}
	}

	err_code = npmx_timer_config_set(timer_instance, &timer_config);

	if (check_error_code(shell, err_code)) {
		if (config_type == TIMER_CONFIG_TYPE_MODE) {
			shell_print(shell, "Success: %d.", timer_config.mode);
		} else if (config_type == TIMER_CONFIG_TYPE_PRESCALER) {
			shell_print(shell, "Success: %d.", timer_config.prescaler);
		} else if (config_type == TIMER_CONFIG_TYPE_COMPARE) {
			shell_print(shell, "Success: %d.", timer_config.compare_value);
		}
	} else {
		shell_error(shell, "Error: unable to set timer configuration.");
	}

	return 0;
}

static int cmd_timer_config_mode_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return timer_config_get(shell, TIMER_CONFIG_TYPE_MODE);
}

static int cmd_timer_config_mode_set(const struct shell *shell, size_t argc, char **argv)
{
	return timer_config_set(shell, argc, argv, TIMER_CONFIG_TYPE_MODE);
}

static int cmd_timer_config_prescaler_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return timer_config_get(shell, TIMER_CONFIG_TYPE_PRESCALER);
}

static int cmd_timer_config_prescaler_set(const struct shell *shell, size_t argc, char **argv)
{
	return timer_config_set(shell, argc, argv, TIMER_CONFIG_TYPE_PRESCALER);
}

static int cmd_timer_config_period_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return timer_config_get(shell, TIMER_CONFIG_TYPE_COMPARE);
}

static int cmd_timer_config_period_set(const struct shell *shell, size_t argc, char **argv)
{
	return timer_config_set(shell, argc, argv, TIMER_CONFIG_TYPE_COMPARE);
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

static int cmd_vbusin_status_cc_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_vbusin_cc_t cc1;
	npmx_vbusin_cc_t cc2;
	npmx_vbusin_t *vbusin_instance = npmx_vbusin_get(npmx_instance, 0);
	npmx_error_t err_code = npmx_vbusin_cc_status_get(vbusin_instance, &cc1, &cc2);

	if (check_error_code(shell, err_code)) {
		if (cc1 == NPMX_VBUSIN_CC_NOT_CONNECTED) {
			shell_print(shell, "Value: %d.", cc2);
		} else {
			shell_print(shell, "Value: %d.", cc1);
		}
	} else {
		shell_error(shell, "Error: unable to read VBUS CC line status.");
	}

	return 0;
}

static int cmd_vbusin_current_limit_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_vbusin_current_t current_limit;
	npmx_vbusin_t *vbusin_instance = npmx_vbusin_get(npmx_instance, 0);
	npmx_error_t err_code = npmx_vbusin_current_limit_get(vbusin_instance, &current_limit);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read VBUS current limit.");
		return 0;
	}

	uint32_t current;
	err_code = npmx_vbusin_current_convert_to_ma(current_limit, &current);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Value: %u mA.", current);
	} else {
		shell_error(shell, "Error: unable to convert VBUS current limit value.");
	}

	return 0;
}

static int cmd_vbusin_current_limit_set(const struct shell *shell, size_t argc, char **argv)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing current limit value.");
		return 0;
	}

	int err = 0;
	uint32_t current = shell_strtoul(argv[1], 0, &err);
	npmx_vbusin_t *vbusin_instance = npmx_vbusin_get(npmx_instance, 0);

	if (err != 0) {
		shell_error(shell, "Error: current limit must be an integer.");
		return 0;
	}

	npmx_vbusin_current_t current_limit = npmx_vbusin_current_convert(current);

	if (current_limit == NPMX_VBUSIN_CURRENT_INVALID) {
		shell_error(shell, "Error: wrong current limit value.");
		return 0;
	}

	npmx_error_t err_code = npmx_vbusin_current_limit_set(vbusin_instance, current_limit);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %s mA.", argv[2]);
	} else {
		shell_error(shell, "Error: unable to set current limit.");
	}

	return 0;
}

static int ship_config_get(const struct shell *shell, ship_config_type_t config_type)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_ship_config_t config;
	npmx_ship_t *ship_instance = npmx_ship_get(npmx_instance, 0);
	npmx_error_t err_code = npmx_ship_config_get(ship_instance, &config);

	if (check_error_code(shell, err_code)) {
		uint32_t config_val = 0;

		switch (config_type) {
		case SHIP_CONFIG_TYPE_TIME:
			if (!npmx_ship_time_convert_to_ms(config.time, &config_val)) {
				shell_error(shell, "Error: unable to convert.");
				return 0;
			}
			break;

		case SHIP_CONFIG_TYPE_INV_POLARITY:
			config_val = (uint32_t)config.inverted_polarity;
			break;
		}

		shell_print(shell, "Value: %u.", config_val);
	} else {
		shell_error(shell, "Error: unable to read config.");
	}

	return 0;
}

static int ship_config_set(const struct shell *shell, size_t argc, char **argv,
			   ship_config_type_t config_type)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing config value.");
		return 0;
	}

	int err = 0;
	uint32_t config_val = shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: config value must be an integer.");
		return 0;
	}

	npmx_ship_config_t config;
	npmx_ship_t *ship_instance = npmx_ship_get(npmx_instance, 0);

	npmx_error_t err_code = npmx_ship_config_get(ship_instance, &config);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read config.");
		return 0;
	}

	switch (config_type) {
	case SHIP_CONFIG_TYPE_TIME:
		npmx_ship_time_t time = npmx_ship_time_convert(config_val);
		if (time == NPMX_SHIP_TIME_INVALID) {
			shell_error(shell, "Error: wrong time value.");
			return 0;
		}
		config.time = time;
		break;

	case SHIP_CONFIG_TYPE_INV_POLARITY:
		config.inverted_polarity = !!config_val;
		break;
	}

	err_code = npmx_ship_config_set(ship_instance, &config);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %u.", config_val);
	} else {
		shell_error(shell, "Error: unable to set config.");
	}

	return 0;
}

static int cmd_ship_config_time_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return ship_config_get(shell, SHIP_CONFIG_TYPE_TIME);
}

static int cmd_ship_config_time_set(const struct shell *shell, size_t argc, char **argv)
{
	return ship_config_set(shell, argc, argv, SHIP_CONFIG_TYPE_TIME);
}

static int cmd_ship_config_inv_polarity_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return ship_config_get(shell, SHIP_CONFIG_TYPE_INV_POLARITY);
}

static int cmd_ship_config_inv_polarity_set(const struct shell *shell, size_t argc, char **argv)
{
	return ship_config_set(shell, argc, argv, SHIP_CONFIG_TYPE_INV_POLARITY);
}

static int ship_reset_config_get(const struct shell *shell, ship_reset_config_type_t config_type)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_ship_reset_config_t reset_config;
	npmx_ship_t *ship_instance = npmx_ship_get(npmx_instance, 0);
	npmx_error_t err_code = npmx_ship_reset_config_get(ship_instance, &reset_config);

	if (check_error_code(shell, err_code)) {
		bool config_val = false;

		switch (config_type) {
		case SHIP_RESET_CONFIG_TYPE_LONG_PRESS:
			config_val = reset_config.long_press;
			break;

		case SHIP_RESET_CONFIG_TYPE_TWO_BUTTONS:
			config_val = reset_config.two_buttons;
			break;
		}

		shell_print(shell, "Value: %u.", config_val);
	} else {
		shell_error(shell, "Error: unable to read reset config.");
	}

	return 0;
}

static int ship_reset_config_set(const struct shell *shell, size_t argc, char **argv,
				 ship_reset_config_type_t config_type)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	if (argc < 2) {
		shell_error(shell, "Error: missing config value.");
		return 0;
	}

	int err = 0;
	uint32_t config_val = shell_strtoul(argv[1], 0, &err);

	if (err != 0) {
		shell_error(shell, "Error: config value must be an integer.");
		return 0;
	}

	npmx_ship_reset_config_t reset_config;
	npmx_ship_t *ship_instance = npmx_ship_get(npmx_instance, 0);

	npmx_error_t err_code = npmx_ship_reset_config_get(ship_instance, &reset_config);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to read reset config.");
		return 0;
	}

	bool val = !!config_val;

	switch (config_type) {
	case SHIP_RESET_CONFIG_TYPE_LONG_PRESS:
		reset_config.long_press = val;
		break;
	case SHIP_RESET_CONFIG_TYPE_TWO_BUTTONS:
		reset_config.two_buttons = val;
		break;
	}

	err_code = npmx_ship_reset_config_set(ship_instance, &reset_config);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: %d.", val);
	} else {
		shell_error(shell, "Error: unable to set reset config.");
	}

	return 0;
}

static int cmd_ship_reset_long_press_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return ship_reset_config_get(shell, SHIP_RESET_CONFIG_TYPE_LONG_PRESS);
}

static int cmd_ship_reset_long_press_set(const struct shell *shell, size_t argc, char **argv)
{
	return ship_reset_config_set(shell, argc, argv, SHIP_RESET_CONFIG_TYPE_LONG_PRESS);
}

static int cmd_ship_reset_two_buttons_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return ship_reset_config_get(shell, SHIP_RESET_CONFIG_TYPE_TWO_BUTTONS);
}

static int cmd_ship_reset_two_buttons_set(const struct shell *shell, size_t argc, char **argv)
{
	return ship_reset_config_set(shell, argc, argv, SHIP_RESET_CONFIG_TYPE_TWO_BUTTONS);
}

static int ship_mode_set(const struct shell *shell, npmx_ship_task_t ship_task)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_ship_t *ship_instance = npmx_ship_get(npmx_instance, 0);

	npmx_error_t err_code = npmx_ship_task_trigger(ship_instance, ship_task);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: 1.");
	} else {
		shell_error(shell, "Error: unable to set mode.");
	}

	return 0;
}

static int cmd_ship_mode_ship_set(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return ship_mode_set(shell, NPMX_SHIP_TASK_SHIPMODE);
}

static int cmd_ship_mode_hibernate_set(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return ship_mode_set(shell, NPMX_SHIP_TASK_HIBERNATE);
}

static int cmd_reset(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);

	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return 0;
	}

	npmx_error_t err_code = npmx_core_task_trigger(npmx_instance, NPMX_CORE_TASK_RESET);

	if (check_error_code(shell, err_code)) {
		shell_print(shell, "Success: restarting.");
	} else {
		shell_error(shell, "Error: unable to restart device.");
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

/* Creating subcommands (level 4 command) array for command "charger termination_voltage warm". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_voltage_warm,
			       SHELL_CMD(get, NULL, "Get charger warm termination voltage",
					 cmd_charger_termination_voltage_warm_get),
			       SHELL_CMD(set, NULL, "Set charger warm termination voltage",
					 cmd_charger_termination_voltage_warm_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger termination_voltage". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_voltage,
			       SHELL_CMD(normal, &sub_charger_termination_voltage_normal,
					 "Charger normal termination voltage", NULL),
			       SHELL_CMD(warm, &sub_charger_termination_voltage_warm,
					 "Charger warm termination voltage", NULL),
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

/* Creating subcommands (level 4 command) array for command "charger module ntc". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_module_ntc,
			       SHELL_CMD(set, NULL, "Enable or disable NTC",
					 cmd_charger_module_ntc_set),
			       SHELL_CMD(get, NULL, "Get NTC status", cmd_charger_module_ntc_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger module full_cool". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_module_full_cool,
			       SHELL_CMD(set, NULL, "Enable or disable full charge",
					 cmd_charger_module_full_cool_set),
			       SHELL_CMD(get, NULL, "Get full charge status",
					 cmd_charger_module_full_cool_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger module". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_charger_module, SHELL_CMD(charger, &sub_charger_module_charger, "Charger module", NULL),
	SHELL_CMD(recharge, &sub_charger_module_recharge, "Recharge module", NULL),
	SHELL_CMD(ntc, &sub_charger_module_ntc, "NTC module", NULL),
	SHELL_CMD(full_cool, &sub_charger_module_full_cool, "Full charge in cool temp module",
		  NULL),
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

/* Creating subcommands (level 4 command) array for command "charger die_temp resume". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_die_temp_resume,
			       SHELL_CMD(get, NULL,
					 "Get die temperature threshold for resuming charging",
					 cmd_die_temp_resume_get),
			       SHELL_CMD(set, NULL,
					 "Set die temperature threshold for resuming charging",
					 cmd_die_temp_resume_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger die_temp stop". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_die_temp_stop,
			       SHELL_CMD(get, NULL,
					 "Get die temperature threshold for stop charging",
					 cmd_die_temp_stop_get),
			       SHELL_CMD(set, NULL,
					 "Set die temperature threshold for stop charging",
					 cmd_die_temp_stop_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger die_temp status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_die_temp_status,
			       SHELL_CMD(get, NULL, "Get die temperature comparator status",
					 cmd_die_temp_status_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger die_temp". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_die_temp,
			       SHELL_CMD(resume, &sub_die_temp_resume,
					 "Die temperature threshold where charging resumes", NULL),
			       SHELL_CMD(stop, &sub_die_temp_stop,
					 "Die temperature threshold where charging stops", NULL),
			       SHELL_CMD(status, &sub_die_temp_status,
					 "Die temperature comparator status", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_resistance cold". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ntc_resistance_cold,
			       SHELL_CMD(get, NULL, "Get NTC resistance value at 0*C",
					 cmd_ntc_resistance_cold_get),
			       SHELL_CMD(set, NULL, "Set NTC resistance value at 0*C",
					 cmd_ntc_resistance_cold_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_resistance cool". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ntc_resistance_cool,
			       SHELL_CMD(get, NULL, "Get NTC resistance value at 10*C",
					 cmd_ntc_resistance_cool_get),
			       SHELL_CMD(set, NULL, "Set NTC resistance value at 10*C",
					 cmd_ntc_resistance_cool_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_resistance warm". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ntc_resistance_warm,
			       SHELL_CMD(get, NULL, "Get NTC resistance value at 45*C",
					 cmd_ntc_resistance_warm_get),
			       SHELL_CMD(set, NULL, "Set NTC resistance value at 45*C",
					 cmd_ntc_resistance_warm_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_resistance hot". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ntc_resistance_hot,
			       SHELL_CMD(get, NULL, "Get NTC resistance value at 60*C",
					 cmd_ntc_resistance_hot_get),
			       SHELL_CMD(set, NULL, "Set NTC resistance value at 60*C",
					 cmd_ntc_resistance_hot_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger ntc_resistance". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_ntc_resistance,
	SHELL_CMD(cold, &sub_ntc_resistance_cold, "NTC resistance value at 0*C", NULL),
	SHELL_CMD(cool, &sub_ntc_resistance_cool, "NTC resistance value at 10*C", NULL),
	SHELL_CMD(warm, &sub_ntc_resistance_warm, "NTC resistance value at 45*C", NULL),
	SHELL_CMD(hot, &sub_ntc_resistance_hot, "NTC resistance value at 60*C", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_temperature cold". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ntc_temperature_cold,
			       SHELL_CMD(get, NULL, "Get NTC temperature at cold",
					 cmd_ntc_temperature_cold_get),
			       SHELL_CMD(set, NULL, "Set NTC temperature at cold",
					 cmd_ntc_temperature_cold_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_temperature cool". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ntc_temperature_cool,
			       SHELL_CMD(get, NULL, "Get NTC temperature at cool",
					 cmd_ntc_temperature_cool_get),
			       SHELL_CMD(set, NULL, "Set NTC temperature at cool",
					 cmd_ntc_temperature_cool_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_temperature warm". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ntc_temperature_warm,
			       SHELL_CMD(get, NULL, "Get NTC temperature at warm",
					 cmd_ntc_temperature_warm_get),
			       SHELL_CMD(set, NULL, "Set NTC temperature at warm",
					 cmd_ntc_temperature_warm_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_temperature hot". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ntc_temperature_hot,
			       SHELL_CMD(get, NULL, "Get NTC temperature at hot",
					 cmd_ntc_temperature_hot_get),
			       SHELL_CMD(set, NULL, "Set NTC temperature at hot",
					 cmd_ntc_temperature_hot_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger ntc_temperature". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_ntc_temperature,
	SHELL_CMD(cold, &sub_ntc_temperature_cold, "NTC temperature cold", NULL),
	SHELL_CMD(cool, &sub_ntc_temperature_cool, "NTC temperature cool", NULL),
	SHELL_CMD(warm, &sub_ntc_temperature_warm, "NTC temperature warm", NULL),
	SHELL_CMD(hot, &sub_ntc_temperature_hot, "NTC temperature hot", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger discharging_current". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_discharging_current,
			       SHELL_CMD(get, NULL, "Get discharging current",
					 cmd_charger_discharging_current_get),
			       SHELL_CMD(set, NULL, "Set discharging current",
					 cmd_charger_discharging_current_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "charger". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_charger,
	SHELL_CMD(termination_voltage, &sub_charger_termination_voltage,
		  "Charger termination voltage", NULL),
	SHELL_CMD(termination_current, &sub_charger_termination_current,
		  "Charger termination current", NULL),
	SHELL_CMD(charger_current, &sub_charger_charging_current, "Charger current", NULL),
	SHELL_CMD(status, &sub_charger_status, "Charger status", NULL),
	SHELL_CMD(module, &sub_charger_module, "Charger module", NULL),
	SHELL_CMD(trickle, &sub_charger_trickle, "Charger trickle voltage", NULL),
	SHELL_CMD(die_temp, &sub_die_temp, "Charger die temperature", NULL),
	SHELL_CMD(ntc_resistance, &sub_ntc_resistance, "Battery NTC resistance values calibration",
		  NULL),
	SHELL_CMD(ntc_temperature, &sub_ntc_temperature, "Battery NTC temperature values", NULL),
	SHELL_CMD(discharging_current, &sub_charger_discharging_current,
		  "Maximum discharging current", NULL),
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

/* Creating subcommands (level 4 command) array for command "ldsw soft_start enable". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_soft_start_enable,
			       SHELL_CMD(get, NULL, "Get soft-start enable status",
					 cmd_ldsw_soft_start_enable_get),
			       SHELL_CMD(set, NULL, "Set soft-start enable status",
					 cmd_ldsw_soft_start_enable_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ldsw soft_start current". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_soft_start_current,
			       SHELL_CMD(get, NULL, "Get soft-start current status",
					 cmd_ldsw_soft_start_current_get),
			       SHELL_CMD(set, NULL, "Set soft-start current status",
					 cmd_ldsw_soft_start_current_set),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "ldsw soft_start". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_soft_start,
			       SHELL_CMD(enable, &sub_ldsw_soft_start_enable, "Soft-start enable",
					 NULL),
			       SHELL_CMD(current, &sub_ldsw_soft_start_current,
					 "Soft-start current", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ldsw active_discharge enable". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_active_discharge_enable,
			       SHELL_CMD(get, NULL, "Get active discharge enable status",
					 cmd_ldsw_active_discharge_enable_get),
			       SHELL_CMD(set, NULL, "Set active discharge enable status",
					 cmd_ldsw_active_discharge_enable_set),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "ldsw active_discharge". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_active_discharge,
			       SHELL_CMD(enable, &sub_ldsw_active_discharge_enable,
					 "Active discharge enable", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "ldsw". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw,
			       SHELL_CMD(set, NULL, "Enable or disable LDSW", cmd_ldsw_set),
			       SHELL_CMD(get, NULL, "Check if LDSW is enabled", cmd_ldsw_get),
			       SHELL_CMD(mode, &sub_ldsw_mode, "LDSW mode", NULL),
			       SHELL_CMD(ldo_voltage, &sub_ldsw_ldo_voltage, "LDO voltage", NULL),
			       SHELL_CMD(soft_start, &sub_ldsw_soft_start, "Soft-start", NULL),
			       SHELL_CMD(active_discharge, &sub_ldsw_active_discharge,
					 "Active discharge", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "leds mode". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_leds_mode,
			       SHELL_CMD(get, NULL, "Get LEDs mode", cmd_leds_mode_get),
			       SHELL_CMD(set, NULL, "Set LEDs mode", cmd_leds_mode_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "leds state". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_leds_state,
			       SHELL_CMD(set, NULL, "Set LEDs state", cmd_leds_state_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "leds". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_leds, SHELL_CMD(mode, &sub_leds_mode, "LEDs mode", NULL),
			       SHELL_CMD(state, &sub_leds_state, "LEDs state", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio mode". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_gpio_mode, SHELL_CMD(get, NULL, "Get GPIO mode configuration", cmd_gpio_mode_get),
	SHELL_CMD(set, NULL, "Set GPIO mode configuration", cmd_gpio_mode_set),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio type". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_type,
			       SHELL_CMD(get, NULL, "Check GPIO type", cmd_gpio_type_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio pull". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_pull,
			       SHELL_CMD(get, NULL, "Get pull configuration", cmd_gpio_pull_get),
			       SHELL_CMD(set, NULL, "Set pull configuration", cmd_gpio_pull_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio drive". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_gpio_drive, SHELL_CMD(get, NULL, "Get drive current configuration", cmd_gpio_drive_get),
	SHELL_CMD(set, NULL, "Set drive current configuration", cmd_gpio_drive_set),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio open_drain". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_open_drain,
			       SHELL_CMD(get, NULL, "Check if open drain is enabled",
					 cmd_gpio_open_drain_get),
			       SHELL_CMD(set, NULL, "Enable or disable open drain",
					 cmd_gpio_open_drain_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio debounce". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_debounce,
			       SHELL_CMD(get, NULL, "Check if debounce is enabled",
					 cmd_gpio_debounce_get),
			       SHELL_CMD(set, NULL, "Enable or disable debounce",
					 cmd_gpio_debounce_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "gpio". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_gpio, SHELL_CMD(mode, &sub_gpio_mode, "GPIO mode configuration", NULL),
	SHELL_CMD(type, &sub_gpio_type, "GPIO type", NULL),
	SHELL_CMD(pull, &sub_gpio_pull, "Pull type configuration", NULL),
	SHELL_CMD(drive, &sub_gpio_drive, "Drive current configuration", NULL),
	SHELL_CMD(open_drain, &sub_gpio_open_drain, "Open drain configuration", NULL),
	SHELL_CMD(debounce, &sub_gpio_debounce, "Debounce configuration", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "adc ntc type". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_ntc_type,
			       SHELL_CMD(get, NULL, "Get ADC NTC type", cmd_adc_ntc_type_get),
			       SHELL_CMD(set, NULL, "Set ADC NTC type", cmd_adc_ntc_type_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "adc ntc beta". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_ntc_beta,
			       SHELL_CMD(get, NULL, "Get ADC NTC beta value", cmd_adc_ntc_beta_get),
			       SHELL_CMD(set, NULL, "Set ADC NTC beta value", cmd_adc_ntc_beta_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "adc ntc". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_ntc,
			       SHELL_CMD(type, &sub_adc_ntc_type, "ADC NTC type", NULL),
			       SHELL_CMD(beta, &sub_adc_ntc_beta, "ADC NTC beta", NULL),
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

/* Creating subcommands (level 3 command) array for command "pof enable". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_pof_enable, SHELL_CMD(get, NULL, "Check if POF feature is enabled", cmd_pof_enable_get),
	SHELL_CMD(set, NULL, "Enable or disable POF", cmd_pof_enable_set), SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "pof polarity". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_pof_polarity, SHELL_CMD(get, NULL, "Get POF warning polarity", cmd_pof_polarity_get),
	SHELL_CMD(set, NULL, "Set POF warning polarity", cmd_pof_polarity_set),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "pof threshold". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_pof_threshold,
			       SHELL_CMD(get, NULL, "Get Vsys comparator threshold",
					 cmd_pof_threshold_get),
			       SHELL_CMD(set, NULL, "Set Vsys comparator threshold",
					 cmd_pof_threshold_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "pof". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_pof, SHELL_CMD(enable, &sub_pof_enable, "Enable power failure feature", NULL),
	SHELL_CMD(polarity, &sub_pof_polarity, "Power failure warning polarity", NULL),
	SHELL_CMD(threshold, &sub_pof_threshold, "Vsys comparator threshold select", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "timer config mode". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_timer_config_mode,
			       SHELL_CMD(get, NULL, "Get timer mode value",
					 cmd_timer_config_mode_get),
			       SHELL_CMD(set, NULL, "Set timer mode value",
					 cmd_timer_config_mode_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "timer config prescaler". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_timer_config_prescaler,
			       SHELL_CMD(get, NULL, "Get timer prescaler value",
					 cmd_timer_config_prescaler_get),
			       SHELL_CMD(set, NULL, "Set timer prescaler value",
					 cmd_timer_config_prescaler_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "timer config period". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_timer_config_period,
			       SHELL_CMD(get, NULL, "Get timer period value",
					 cmd_timer_config_period_get),
			       SHELL_CMD(set, NULL, "Set timer period value",
					 cmd_timer_config_period_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "timer config". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_timer_config, SHELL_CMD(mode, &sub_timer_config_mode, "Timer mode selection", NULL),
	SHELL_CMD(prescaler, &sub_timer_config_prescaler, "Timer prescaler selection", NULL),
	SHELL_CMD(period, &sub_timer_config_period, "Timer period value", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "timer". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_timer,
			       SHELL_CMD(config, &sub_timer_config, "Timer configuration", NULL),
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

/* Creating subcommands (level 4 command) array for command "vbusin status cc". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vbusin_status_cc,
			       SHELL_CMD(get, NULL, "Get VBUS CC status", cmd_vbusin_status_cc_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "vbusin status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vbusin_status,
			       SHELL_CMD(connected, &sub_vbusin_status_connected, "VBUS connected",
					 NULL),
			       SHELL_CMD(cc, &sub_vbusin_status_cc, "VBUS CC", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "vbusin current_limit". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vbusin_current_limit,
			       SHELL_CMD(get, NULL, "VBUS current limit get",
					 cmd_vbusin_current_limit_get),
			       SHELL_CMD(set, NULL, "VBUS current limit set",
					 cmd_vbusin_current_limit_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "vbusin". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_vbusin, SHELL_CMD(status, &sub_vbusin_status, "Status", NULL),
			       SHELL_CMD(current_limit, &sub_vbusin_current_limit, "Current limit",
					 NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ship config time". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_config_time,
			       SHELL_CMD(get, NULL, "Get ship exit time", cmd_ship_config_time_get),
			       SHELL_CMD(set, NULL, "Set ship exit time", cmd_ship_config_time_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ship config inv_polarity". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_config_inv_polarity,
			       SHELL_CMD(get, NULL, "Get inverted polarity status",
					 cmd_ship_config_inv_polarity_get),
			       SHELL_CMD(set, NULL, "Set inverted polarity status",
					 cmd_ship_config_inv_polarity_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "ship config". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_config,
			       SHELL_CMD(time, &sub_ship_config_time, "Time", NULL),
			       SHELL_CMD(inv_polarity, &sub_ship_config_inv_polarity,
					 "Button invert polarity", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ship reset long_press". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_reset_long_press,
			       SHELL_CMD(get, NULL, "Get long press status",
					 cmd_ship_reset_long_press_get),
			       SHELL_CMD(set, NULL, "Set long press status",
					 cmd_ship_reset_long_press_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "ship reset two_buttons". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_reset_two_buttons,
			       SHELL_CMD(get, NULL, "Get two buttons status",
					 cmd_ship_reset_two_buttons_get),
			       SHELL_CMD(set, NULL, "Set two buttons status",
					 cmd_ship_reset_two_buttons_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "ship reset". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_ship_reset, SHELL_CMD(long_press, &sub_ship_reset_long_press, "Long press", NULL),
	SHELL_CMD(two_buttons, &sub_ship_reset_two_buttons, "Two buttons", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "ship mode". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship_mode,
			       SHELL_CMD(ship, NULL, "Enter ship mode", cmd_ship_mode_ship_set),
			       SHELL_CMD(hibernate, NULL, "Enter hibernate mode",
					 cmd_ship_mode_hibernate_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "ship". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ship, SHELL_CMD(config, &sub_ship_config, "Ship config", NULL),
			       SHELL_CMD(reset, &sub_ship_reset, "Reset button config", NULL),
			       SHELL_CMD(mode, &sub_ship_mode, "Set ship mode", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 1 command) array for command "npmx". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_npmx, SHELL_CMD(charger, &sub_charger, "Charger", NULL),
	SHELL_CMD(buck, &sub_buck, "Buck", NULL), SHELL_CMD(ldsw, &sub_ldsw, "LDSW", NULL),
	SHELL_CMD(leds, &sub_leds, "LEDs", NULL), SHELL_CMD(gpio, &sub_gpio, "GPIOs", NULL),
	SHELL_CMD(adc, &sub_adc, "ADC", NULL), SHELL_CMD(pof, &sub_pof, "POF", NULL),
	SHELL_CMD(timer, &sub_timer, "Timer", NULL),
	SHELL_CMD(errlog, &sub_errlog, "Reset errors logs", NULL),
	SHELL_CMD(vbusin, &sub_vbusin, "VBUSIN", NULL), SHELL_CMD(ship, &sub_ship, "SHIP", NULL),
	SHELL_CMD(reset, NULL, "Restart device", cmd_reset), SHELL_SUBCMD_SET_END);

/* Creating root (level 0) command "npmx" without a handler. */
SHELL_CMD_REGISTER(npmx, &sub_npmx, "npmx", NULL);
