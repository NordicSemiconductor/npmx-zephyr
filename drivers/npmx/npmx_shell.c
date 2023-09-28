/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <npmx.h>
#include <npmx_driver.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>

/** @brief Max supported number of shell arguments. */
#define SHELL_ARG_MAX_COUNT 3U

/** @brief NTC thermistor configuration parameter. */
typedef enum {
	ADC_NTC_CONFIG_PARAM_TYPE, /* Battery NTC type. */
	ADC_NTC_CONFIG_PARAM_BETA /* Battery NTC beta value. */
} adc_ntc_config_param_t;

/** @brief GPIO configuration parameter. */
typedef enum {
	GPIO_CONFIG_PARAM_DEBOUNCE, /* GPIO debounce. */
	GPIO_CONFIG_PARAM_DRIVE, /* GPIO drive. */
	GPIO_CONFIG_PARAM_MODE, /* GPIO mode. */
	GPIO_CONFIG_PARAM_OPEN_DRAIN, /* GPIO open drain. */
	GPIO_CONFIG_PARAM_PULL, /* GPIO pull. */
	GPIO_CONFIG_PARAM_TYPE, /* Helper config to check if GPIO is output or input. */
} gpio_config_param_t;

/** @brief Load switch soft-start configuration parameter. */
typedef enum {
	LDSW_SOFT_START_PARAM_ENABLE, /* Soft-start enable. */
	LDSW_SOFT_START_PARAM_CURRENT /* Soft-start current. */
} ldsw_soft_start_config_param_t;

/** @brief POF config parameter. */
typedef enum {
	POF_CONFIG_PARAM_POLARITY, /* POF polarity. */
	POF_CONFIG_PARAM_STATUS, /* Enable POF. */
	POF_CONFIG_PARAM_THRESHOLD, /* POF threshold value. */
} pof_config_param_t;

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

/** @brief Supported shell argument types. */
typedef enum {
	SHELL_ARG_TYPE_INT32_VALUE, /* Shell int32_t type used for values. */
	SHELL_ARG_TYPE_UINT32_VALUE, /* Shell uint32_t type used for values. */
	SHELL_ARG_TYPE_BOOL_VALUE, /* Shell bool type used for values. */
	SHELL_ARG_TYPE_UINT32_INDEX, /* Shell uint32_t type used for peripherals index. */
} shell_arg_type_t;

/** @brief Shell argument result value. */
typedef union {
	int32_t ivalue; /* int32_t result. */
	uint32_t uvalue; /* uint32_t result. */
	bool bvalue; /* bool result. */
} shell_arg_result_t;

/** @brief Shell argument structure. */
typedef struct {
	const shell_arg_type_t type; /* Shell argument type. */
	const char *name; /* Shell argument name. */
	shell_arg_result_t result; /* Shell argument result. */
} shell_arg_t;

/** @brief Shell arguments infomation structure. */
typedef struct {
	const int expected_args; /* Expected arguments count. */
	shell_arg_t arg[SHELL_ARG_MAX_COUNT]; /* Shell arguments. */
} args_info_t;

/** @brief Supported unit types. */
typedef enum {
	UNIT_TYPE_MILLIAMPERE, /* Unit type mA. */
	UNIT_TYPE_MILLIVOLT, /* Unit type mV. */
	UNIT_TYPE_CELSIUS, /* Unit type *C. */
	UNIT_TYPE_OHM, /* Unit type ohms. */
	UNIT_TYPE_PCT, /* Unit type %. */
	UNIT_TYPE_NONE, /* No unit type. */
} unit_type_t;

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

static void print_value(const struct shell *shell, int value, unit_type_t unit_type)
{
	shell_print(shell, "Value: %d%s.", value, unit_str_get(unit_type));
}

static void print_success(const struct shell *shell, int value, unit_type_t unit_type)
{
	shell_print(shell, "Success: %d%s.", value, unit_str_get(unit_type));
}

static void print_hint_error(const struct shell *shell, int32_t index, const char *str)
{
	shell_error(shell, "%d-%s", index, str);
}

static void print_set_error(const struct shell *shell, const char *str)
{
	shell_error(shell, "Error: unable to set %s.", str);
}

static void print_get_error(const struct shell *shell, const char *str)
{
	shell_error(shell, "Error: unable to get %s.", str);
}

static void print_convert_error(const struct shell *shell, const char *src, const char *dst)
{
	shell_error(shell, "Error: unable to convert %s to %s.", src, dst);
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

static bool arguments_check(const struct shell *shell, size_t argc, char **argv,
			    args_info_t *args_info)
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

static npmx_instance_t *npmx_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_driver_instance_get(pmic_dev);
	if (npmx_instance == NULL) {
		shell_error(shell, "Error: shell is not initialized.");
		return NULL;
	}

	return npmx_instance;
}

static bool check_instance_index(const struct shell *shell, const char *instance_name,
				 uint32_t index, uint32_t max_index)
{
	if (index >= max_index) {
		shell_error(shell, "Error: %s instance index is too high: no such instance.",
			    instance_name);
		return false;
	}

	return true;
}

static npmx_adc_t *adc_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);

	return npmx_instance ? npmx_adc_get(npmx_instance, 0) : NULL;
}

static npmx_charger_t *charger_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);

	return npmx_instance ? npmx_charger_get(npmx_instance, 0) : NULL;
}

static npmx_buck_t *buck_instance_get(const struct shell *shell, uint32_t index)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);
	bool status = check_instance_index(shell, "buck", index, NPM_BUCK_COUNT);

	return (npmx_instance && status) ? npmx_buck_get(npmx_instance, (uint8_t)index) : NULL;
}

static bool charger_disabled_check(const struct shell *shell, npmx_charger_t *charger_instance,
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

static npmx_gpio_t *gpio_instance_get(const struct shell *shell, uint32_t index)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);
	bool status = check_instance_index(shell, "GPIO", index, NPM_GPIOS_COUNT);

	return (npmx_instance && status) ? npmx_gpio_get(npmx_instance, (uint8_t)index) : NULL;
}

static npmx_ldsw_t *ldsw_instance_get(const struct shell *shell, uint32_t index)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);
	bool status = check_instance_index(shell, "LDSW", index, NPM_LDSW_COUNT);

	return (npmx_instance && status) ? npmx_ldsw_get(npmx_instance, (uint8_t)index) : NULL;
}

static npmx_led_t *led_instance_get(const struct shell *shell, uint32_t index)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);
	bool status = check_instance_index(shell, "LED", index, NPM_LEDDRV_COUNT);

	return (npmx_instance && status) ? npmx_led_get(npmx_instance, (uint8_t)index) : NULL;
}

static npmx_pof_t *pof_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);

	return npmx_instance ? npmx_pof_get(npmx_instance, 0) : NULL;
}

static bool check_pin_configuration_correctness(const struct shell *shell, int8_t gpio_idx)
{
	int8_t pmic_int_pin = (int8_t)npmx_driver_int_pin_get(pmic_dev);
	int8_t pmic_pof_pin = (int8_t)npmx_driver_pof_pin_get(pmic_dev);

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

static int cmd_adc_meas_vbat_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_adc_t *adc_instance = adc_instance_get(shell);
	if (adc_instance == NULL) {
		return 0;
	}

	npmx_error_t err_code = npmx_adc_task_trigger(adc_instance, NPMX_ADC_TASK_SINGLE_SHOT_VBAT);
	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to trigger the measurement.");
		return 0;
	}

	int32_t voltage_mv;
	bool meas_get = false;
	while (!meas_get) {
		err_code = npmx_adc_meas_check(adc_instance, NPMX_ADC_MEAS_VBAT, &meas_get);
		if (!check_error_code(shell, err_code)) {
			print_get_error(shell, "measurement status");
			return 0;
		}
	}

	err_code = npmx_adc_meas_get(adc_instance, NPMX_ADC_MEAS_VBAT, &voltage_mv);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "measurement");
		return 0;
	}

	print_value(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

static int adc_ntc_set(const struct shell *shell, size_t argc, char **argv,
		       adc_ntc_config_param_t config_type)
{
	char *config_name;
	switch (config_type) {
	case ADC_NTC_CONFIG_PARAM_TYPE:
		config_name = "NTC type";
		break;
	case ADC_NTC_CONFIG_PARAM_BETA:
		config_name = "NTC beta";
		break;
	}

	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE, config_name },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	uint32_t config_value = args_info.arg[0].result.uvalue;
	if ((config_type == ADC_NTC_CONFIG_PARAM_BETA) && (config_value == 0)) {
		shell_error(shell, "Error: beta cannot be equal to zero.");
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "NTC config")) {
		return 0;
	}

	npmx_adc_t *adc_instance = adc_instance_get(shell);
	if (adc_instance == NULL) {
		return 0;
	}

	npmx_adc_ntc_config_t ntc_config;
	npmx_error_t err_code = npmx_adc_ntc_config_get(adc_instance, &ntc_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "NTC config");
		return 0;
	}

	bool recalculate_temps =
		(config_type == ADC_NTC_CONFIG_PARAM_BETA &&
		 ntc_config.type != NPMX_ADC_NTC_TYPE_HI_Z && ntc_config.beta != 0);
	int16_t temp_cool, temp_cold, temp_warm, temp_hot;
	if (recalculate_temps) {
		err_code = npmx_charger_cold_temperature_get(charger_instance, &temp_cold);
		if (!check_error_code(shell, err_code)) {
			print_get_error(shell, "previous NTC cold treshold");
			return 0;
		}

		err_code = npmx_charger_cool_temperature_get(charger_instance, &temp_cool);
		if (!check_error_code(shell, err_code)) {
			print_get_error(shell, "previous NTC cool treshold");
			return 0;
		}

		err_code = npmx_charger_warm_temperature_get(charger_instance, &temp_warm);
		if (!check_error_code(shell, err_code)) {
			print_get_error(shell, "previous NTC warm treshold");
			return 0;
		}

		err_code = npmx_charger_hot_temperature_get(charger_instance, &temp_hot);
		if (!check_error_code(shell, err_code)) {
			print_get_error(shell, "previous NTC hot treshold");
			return 0;
		}
	}

	switch (config_type) {
	case ADC_NTC_CONFIG_PARAM_TYPE:
		ntc_config.type = npmx_adc_ntc_type_convert(config_value);
		if (ntc_config.type == NPMX_ADC_NTC_TYPE_INVALID) {
			print_convert_error(shell, "resistance", "NTC type");
			return 0;
		}

		uint32_t modules_mask;
		if (ntc_config.type == NPMX_ADC_NTC_TYPE_HI_Z) {
			err_code = npmx_charger_module_get(charger_instance, &modules_mask);
			if (!check_error_code(shell, err_code)) {
				print_get_error(shell, "NTC limits module status");
				return 0;
			}

			if ((modules_mask & NPMX_CHARGER_MODULE_NTC_LIMITS_MASK) == 0) {
				/* NTC limits module is already disabled. */
				break;
			}

			err_code = npmx_charger_module_disable_set(
				charger_instance, NPMX_CHARGER_MODULE_NTC_LIMITS_MASK);
			if (!check_error_code(shell, err_code)) {
				shell_error(shell,
					    "Error: unable to disable the NTC limits module.");
				return 0;
			}

			shell_info(shell, "Info: the NTC limits module has been disabled.");
			shell_info(shell, "      To re-enable, change the NTC type to != 0.");
		} else {
			err_code = npmx_charger_module_get(charger_instance, &modules_mask);
			if (!check_error_code(shell, err_code)) {
				print_get_error(shell, "NTC limits module status");
				return 0;
			}

			if ((modules_mask & NPMX_CHARGER_MODULE_NTC_LIMITS_MASK) > 0) {
				/* NTC limits module is already enabled. */
				break;
			}

			err_code = npmx_charger_module_enable_set(
				charger_instance, NPMX_CHARGER_MODULE_NTC_LIMITS_MASK);
			if (!check_error_code(shell, err_code)) {
				shell_error(shell,
					    "Error: unable to enable the NTC limits module.");
				return 0;
			}

			shell_info(shell, "Info: the NTC limits module has been enabled.");
		}
		break;

	case ADC_NTC_CONFIG_PARAM_BETA:
		ntc_config.beta = config_value;
		break;
	}

	err_code = npmx_adc_ntc_config_set(adc_instance, &ntc_config);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "NTC config");
		return 0;
	}

	switch (config_type) {
	case ADC_NTC_CONFIG_PARAM_TYPE:
		print_success(shell, config_value, UNIT_TYPE_OHM);
		break;
	case ADC_NTC_CONFIG_PARAM_BETA:
		print_success(shell, config_value, UNIT_TYPE_NONE);

		if (!recalculate_temps) {
			break;
		}

		/* Recalculate threshold temperatures. */
		err_code = npmx_charger_cold_temperature_set(charger_instance, temp_cold);
		if (!check_error_code(shell, err_code)) {
			print_set_error(shell, "new NTC cold treshold");
			break;
		}

		err_code = npmx_charger_cool_temperature_set(charger_instance, temp_cool);
		if (!check_error_code(shell, err_code)) {
			print_set_error(shell, "new NTC cool treshold");
			break;
		}

		err_code = npmx_charger_warm_temperature_set(charger_instance, temp_warm);
		if (!check_error_code(shell, err_code)) {
			print_set_error(shell, "new NTC warm treshold");
			break;
		}

		err_code = npmx_charger_hot_temperature_set(charger_instance, temp_hot);
		if (!check_error_code(shell, err_code)) {
			print_set_error(shell, "new NTC hot treshold");
			break;
		}
		shell_info(shell, "Info: NTC thresholds recalculated successfully.");

		break;
	}

	return 0;
}

static int adc_ntc_get(const struct shell *shell, adc_ntc_config_param_t config_type)
{
	npmx_adc_t *adc_instance = adc_instance_get(shell);
	if (adc_instance == NULL) {
		return 0;
	}

	npmx_adc_ntc_config_t ntc_config;
	npmx_error_t err_code = npmx_adc_ntc_config_get(adc_instance, &ntc_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "NTC config");
		return 0;
	}

	switch (config_type) {
	case ADC_NTC_CONFIG_PARAM_TYPE:
		uint32_t config_value;
		if (!npmx_adc_ntc_type_convert_to_ohms(ntc_config.type, &config_value)) {
			print_convert_error(shell, "NTC type", "resistance");
			return 0;
		}
		print_value(shell, config_value, UNIT_TYPE_OHM);
		break;
	case ADC_NTC_CONFIG_PARAM_BETA:
		print_value(shell, ntc_config.beta, UNIT_TYPE_NONE);
		break;
	}

	return 0;
}

static int cmd_adc_ntc_beta_set(const struct shell *shell, size_t argc, char **argv)
{
	return adc_ntc_set(shell, argc, argv, ADC_NTC_CONFIG_PARAM_BETA);
}

static int cmd_adc_ntc_beta_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return adc_ntc_get(shell, ADC_NTC_CONFIG_PARAM_BETA);
}

static int cmd_adc_ntc_type_set(const struct shell *shell, size_t argc, char **argv)
{
	return adc_ntc_set(shell, argc, argv, ADC_NTC_CONFIG_PARAM_TYPE);
}

static int cmd_adc_ntc_type_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return adc_ntc_get(shell, ADC_NTC_CONFIG_PARAM_TYPE);
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

	bool active_discharge = args_info.arg[0].result.bvalue;
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

static int buck_gpio_set(const struct shell *shell, size_t argc, char **argv,
			 npmx_error_t (*gpio_config_set)(npmx_buck_t const *,
							 npmx_buck_gpio_config_t const *))
{
	args_info_t args_info = { .expected_args = 3,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "buck" },
					  [1] = { SHELL_ARG_TYPE_INT32_VALUE, "GPIO number" },
					  [2] = { SHELL_ARG_TYPE_BOOL_VALUE,
						  "GPIO inversion status" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_buck_t *buck_instance = buck_instance_get(shell, args_info.arg[0].result.uvalue);
	if (buck_instance == NULL) {
		return 0;
	}

	int gpio_index = args_info.arg[1].result.ivalue;
	npmx_buck_gpio_config_t gpio_config = {
		.inverted = args_info.arg[2].result.bvalue,
	};

	static const npmx_buck_gpio_t gpios[] = {
		NPMX_BUCK_GPIO_0, NPMX_BUCK_GPIO_1, NPMX_BUCK_GPIO_2,
		NPMX_BUCK_GPIO_3, NPMX_BUCK_GPIO_4,
	};

	if (!check_pin_configuration_correctness(shell, gpio_index)) {
		return 0;
	}

	if (gpio_index == -1) {
		gpio_config.gpio = NPMX_BUCK_GPIO_NC;
	} else {
		if ((gpio_index >= 0) && (gpio_index < ARRAY_SIZE(gpios))) {
			gpio_config.gpio = gpios[gpio_index];
		} else {
			shell_error(shell, "Error: wrong GPIO index.");
			return 0;
		}
	}

	npmx_error_t err_code = gpio_config_set(buck_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "GPIO config");
		return 0;
	}

	print_success(shell, gpio_index, UNIT_TYPE_NONE);
	return 0;
}

static int buck_gpio_get(const struct shell *shell, size_t argc, char **argv,
			 npmx_error_t (*gpio_config_get)(npmx_buck_t const *,
							 npmx_buck_gpio_config_t *))
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

	int gpio_index = ((gpio_config.gpio == NPMX_BUCK_GPIO_NC) ||
			  (gpio_config.gpio == NPMX_BUCK_GPIO_NC1)) ?
				 -1 :
				 ((int)gpio_config.gpio - 1);

	shell_print(shell, "Value: %d %d.", gpio_index, gpio_config.inverted);
	return 0;
}

static int cmd_buck_gpio_on_off_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_set(shell, argc, argv, npmx_buck_enable_gpio_config_set);
}

static int cmd_buck_gpio_on_off_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_get(shell, argc, argv, npmx_buck_enable_gpio_config_get);
}

static int cmd_buck_gpio_pwm_force_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_set(shell, argc, argv, npmx_buck_forced_pwm_gpio_config_set);
}

static int cmd_buck_gpio_pwm_force_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_get(shell, argc, argv, npmx_buck_forced_pwm_gpio_config_get);
}

static int cmd_buck_gpio_retention_set(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_set(shell, argc, argv, npmx_buck_retention_gpio_config_set);
}

static int cmd_buck_gpio_retention_get(const struct shell *shell, size_t argc, char **argv)
{
	return buck_gpio_get(shell, argc, argv, npmx_buck_retention_gpio_config_get);
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

static int cmd_charger_charging_current_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE, "charging current" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "charging current")) {
		return 0;
	}

	uint16_t charging_current_ma = CLAMP(args_info.arg[0].result.uvalue, 0, UINT16_MAX);
	npmx_error_t err_code =
		npmx_charger_charging_current_set(charger_instance, charging_current_ma);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "charging current");
		return 0;
	}

	print_success(shell, charging_current_ma, UNIT_TYPE_MILLIAMPERE);
	return 0;
}

static int cmd_charger_charging_current_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	uint16_t charging_current_ma;
	npmx_error_t err_code =
		npmx_charger_charging_current_get(charger_instance, &charging_current_ma);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "charging current");
		return 0;
	}

	print_value(shell, charging_current_ma, UNIT_TYPE_MILLIAMPERE);
	return 0;
}

static int charger_die_temp_set(const struct shell *shell, size_t argc, char **argv,
				npmx_error_t (*func)(npmx_charger_t const *p_instance,
						     int16_t temperature))
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_INT32_VALUE,
						  "die temperature threshold" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "die temperature threshold")) {
		return 0;
	}

	int16_t temperature = CLAMP(args_info.arg[0].result.ivalue, 0, INT16_MAX);
	if (temperature < NPM_BCHARGER_DIE_TEMPERATURE_MIN_VAL ||
	    temperature > NPM_BCHARGER_DIE_TEMPERATURE_MAX_VAL) {
		shell_error(shell, "Error: value out of range.");
		return 0;
	}

	npmx_error_t err_code = func(charger_instance, temperature);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "die temperature threshold");
		return 0;
	}

	print_success(shell, temperature, UNIT_TYPE_CELSIUS);
	return 0;
}

static int charger_die_temp_get(const struct shell *shell,
				npmx_error_t (*func)(npmx_charger_t const *p_instance,
						     int16_t *temperature))
{
	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	int16_t temperature;
	npmx_error_t err_code = func(charger_instance, &temperature);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "die temperature threshold");
		return 0;
	}

	print_value(shell, temperature, UNIT_TYPE_CELSIUS);
	return 0;
}

static int cmd_charger_die_temp_resume_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_die_temp_set(shell, argc, argv, npmx_charger_die_temp_resume_set);
}

static int cmd_charger_die_temp_resume_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_die_temp_get(shell, npmx_charger_die_temp_resume_get);
}

static int cmd_charger_die_temp_status_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	bool status;
	npmx_error_t err_code = npmx_charger_die_temp_status_get(charger_instance, &status);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "charger die temperature comparator status");
		return 0;
	}

	print_value(shell, status, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_charger_die_temp_stop_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_die_temp_set(shell, argc, argv, npmx_charger_die_temp_stop_set);
}

static int cmd_charger_die_temp_stop_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_die_temp_get(shell, npmx_charger_die_temp_stop_get);
}

static int cmd_charger_discharging_current_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE,
						  "discharging current" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "discharging current")) {
		return 0;
	}

	uint16_t discharging_current_ma = CLAMP(args_info.arg[0].result.uvalue, 0, UINT16_MAX);
	npmx_error_t err_code =
		npmx_charger_discharging_current_set(charger_instance, discharging_current_ma);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "discharging current");
		return 0;
	}

	err_code = npmx_charger_discharging_current_get(charger_instance, &discharging_current_ma);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "discharging current");
		return 0;
	}

	print_success(shell, discharging_current_ma, UNIT_TYPE_MILLIAMPERE);
	shell_print(shell, "Set value may be different than requested.");
	return 0;
}

static int cmd_charger_discharging_current_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	uint16_t charging_current_ma;
	npmx_error_t err_code =
		npmx_charger_discharging_current_get(charger_instance, &charging_current_ma);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "discharging current");
		return 0;
	}

	print_value(shell, charging_current_ma, UNIT_TYPE_MILLIAMPERE);
	return 0;
}

static int charger_module_set(const struct shell *shell, size_t argc, char **argv,
			      npmx_charger_module_mask_t mask)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_BOOL_VALUE, "status" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	bool charger_set = args_info.arg[0].result.bvalue;

	npmx_error_t (*change_status_function)(const npmx_charger_t *p_instance,
					       uint32_t module_mask) =
		charger_set ? npmx_charger_module_enable_set : npmx_charger_module_disable_set;

	npmx_error_t err_code = change_status_function(charger_instance, mask);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "charging module status");
		return 0;
	}

	print_success(shell, charger_set, UNIT_TYPE_NONE);
	return 0;
}

static int charger_module_get(const struct shell *shell, npmx_charger_module_mask_t mask)
{
	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	uint32_t module_mask;
	npmx_error_t err_code = npmx_charger_module_get(charger_instance, &module_mask);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "charging module status");
		return 0;
	}

	print_value(shell, !!(module_mask & (uint32_t)mask), UNIT_TYPE_NONE);
	return 0;
}

static int cmd_charger_module_charger_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_module_set(shell, argc, argv, NPMX_CHARGER_MODULE_CHARGER_MASK);
}

static int cmd_charger_module_charger_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_module_get(shell, NPMX_CHARGER_MODULE_CHARGER_MASK);
}

static int cmd_charger_module_full_cool_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_module_set(shell, argc, argv, NPMX_CHARGER_MODULE_FULL_COOL_MASK);
}

static int cmd_charger_module_full_cool_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_module_get(shell, NPMX_CHARGER_MODULE_FULL_COOL_MASK);
}

static int cmd_charger_module_ntc_limits_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_module_set(shell, argc, argv, NPMX_CHARGER_MODULE_NTC_LIMITS_MASK);
}

static int cmd_charger_module_ntc_limits_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_module_get(shell, NPMX_CHARGER_MODULE_NTC_LIMITS_MASK);
}

static int cmd_charger_module_recharge_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_module_set(shell, argc, argv, NPMX_CHARGER_MODULE_RECHARGE_MASK);
}

static int cmd_charger_module_recharge_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_module_get(shell, NPMX_CHARGER_MODULE_RECHARGE_MASK);
}

static int charger_ntc_resistance_set(const struct shell *shell, size_t argc, char **argv,
				      npmx_error_t (*func)(npmx_charger_t const *p_instance,
							   uint32_t p_resistance))
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE, "NTC resistance" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "NTC resistance")) {
		return 0;
	}

	uint32_t resistance = args_info.arg[0].result.uvalue;
	npmx_error_t err_code = func(charger_instance, resistance);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "NTC resistance");
		return 0;
	}

	print_success(shell, resistance, UNIT_TYPE_OHM);
	return 0;
}

static int charger_ntc_resistance_get(const struct shell *shell,
				      npmx_error_t (*func)(npmx_charger_t const *p_instance,
							   uint32_t *resistance))
{
	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	uint32_t resistance;
	npmx_error_t err_code = func(charger_instance, &resistance);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "NTC resistance");
		return 0;
	}

	print_value(shell, resistance, UNIT_TYPE_OHM);
	return 0;
}

static int cmd_charger_ntc_resistance_cold_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_resistance_set(shell, argc, argv, npmx_charger_cold_resistance_set);
}

static int cmd_charger_ntc_resistance_cold_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_resistance_get(shell, npmx_charger_cold_resistance_get);
}

static int cmd_charger_ntc_resistance_cool_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_resistance_set(shell, argc, argv, npmx_charger_cool_resistance_set);
}

static int cmd_charger_ntc_resistance_cool_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_resistance_get(shell, npmx_charger_cool_resistance_get);
}

static int cmd_charger_ntc_resistance_warm_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_resistance_set(shell, argc, argv, npmx_charger_warm_resistance_set);
}

static int cmd_charger_ntc_resistance_warm_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_resistance_get(shell, npmx_charger_warm_resistance_get);
}

static int cmd_charger_ntc_resistance_hot_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_resistance_set(shell, argc, argv, npmx_charger_hot_resistance_set);
}

static int cmd_charger_ntc_resistance_hot_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_resistance_get(shell, npmx_charger_hot_resistance_get);
}

static int charger_ntc_temperature_set(const struct shell *shell, size_t argc, char **argv,
				       npmx_error_t (*func)(npmx_charger_t const *p_instance,
							    int16_t temperature))
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_INT32_VALUE, "NTC temperature" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "NTC temperature")) {
		return 0;
	}

	int32_t temperature = args_info.arg[0].result.ivalue;
	if ((temperature < -20) || (temperature > 60)) {
		shell_error(
			shell,
			"Error: The NTC temperature must be in the range between -20*C and 60*C.");
		return 0;
	}

	npmx_error_t err_code = func(charger_instance, temperature);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "NTC temperature");
		return 0;
	}

	print_success(shell, temperature, UNIT_TYPE_CELSIUS);
	return 0;
}

static int charger_ntc_temperature_get(const struct shell *shell,
				       npmx_error_t (*func)(npmx_charger_t const *p_instance,
							    int16_t *p_temperature))
{
	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	int16_t temperature;
	npmx_error_t err_code = func(charger_instance, &temperature);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "NTC temperature");
		return 0;
	}

	print_value(shell, temperature, UNIT_TYPE_CELSIUS);
	return 0;
}

static int cmd_charger_ntc_temperature_cold_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_temperature_set(shell, argc, argv, npmx_charger_cold_temperature_set);
}

static int cmd_charger_ntc_temperature_cold_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_temperature_get(shell, npmx_charger_cold_temperature_get);
}

static int cmd_charger_ntc_temperature_cool_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_temperature_set(shell, argc, argv, npmx_charger_cool_temperature_set);
}

static int cmd_charger_ntc_temperature_cool_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_temperature_get(shell, npmx_charger_cool_temperature_get);
}

static int cmd_charger_ntc_temperature_warm_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_temperature_set(shell, argc, argv, npmx_charger_warm_temperature_set);
}

static int cmd_charger_ntc_temperature_warm_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_temperature_get(shell, npmx_charger_warm_temperature_get);
}

static int cmd_charger_ntc_temperature_hot_set(const struct shell *shell, size_t argc, char **argv)
{
	return charger_ntc_temperature_set(shell, argc, argv, npmx_charger_hot_temperature_set);
}

static int cmd_charger_ntc_temperature_hot_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_ntc_temperature_get(shell, npmx_charger_hot_temperature_get);
}

static int cmd_charger_status_all_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	uint8_t status_mask;
	npmx_error_t err_code = npmx_charger_status_get(charger_instance, &status_mask);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "charger status");
		return 0;
	}

	print_value(shell, status_mask, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_charger_status_charging_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	uint8_t status_mask;
	uint8_t charging_mask = NPMX_CHARGER_STATUS_TRICKLE_CHARGE_MASK |
				NPMX_CHARGER_STATUS_CONSTANT_CURRENT_MASK |
				NPMX_CHARGER_STATUS_CONSTANT_VOLTAGE_MASK;
	npmx_error_t err_code = npmx_charger_status_get(charger_instance, &status_mask);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "charger status");
		return 0;
	}

	print_value(shell, !!(status_mask & charging_mask), UNIT_TYPE_NONE);
	return 0;
}

static int cmd_charger_termination_current_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE,
						  "termination current" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "termination current")) {
		return 0;
	}

	uint32_t current_pct = args_info.arg[0].result.uvalue;
	npmx_charger_iterm_t charger_iterm = npmx_charger_iterm_convert(current_pct);
	if (charger_iterm == NPMX_CHARGER_ITERM_INVALID) {
		print_convert_error(shell, "pct", "termination current");
		return 0;
	}

	npmx_error_t err_code =
		npmx_charger_termination_current_set(charger_instance, charger_iterm);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "termination current");
		return 0;
	}

	print_success(shell, current_pct, UNIT_TYPE_PCT);
	return 0;
}

static int cmd_charger_termination_current_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	npmx_charger_iterm_t charger_iterm;
	npmx_error_t err_code =
		npmx_charger_termination_current_get(charger_instance, &charger_iterm);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "termination current");
		return 0;
	}

	uint32_t current_pct;
	if (!npmx_charger_iterm_convert_to_pct(charger_iterm, &current_pct)) {
		print_convert_error(shell, "termination current", "pct");
		return 0;
	}

	print_value(shell, current_pct, UNIT_TYPE_PCT);
	return 0;
}

static int charger_termination_voltage_set(const struct shell *shell, size_t argc, char **argv,
					   npmx_error_t (*func)(npmx_charger_t const *p_instance,
								npmx_charger_voltage_t voltage))
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE,
						  "termination voltage" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	uint32_t voltage_mv = args_info.arg[0].result.uvalue;
	npmx_charger_voltage_t charger_voltage = npmx_charger_voltage_convert(voltage_mv);
	if (charger_voltage == NPMX_CHARGER_VOLTAGE_INVALID) {
		print_convert_error(shell, "millivolts", "termination voltage");
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "termination voltage")) {
		return 0;
	}

	npmx_error_t err_code = func(charger_instance, charger_voltage);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "termination voltage");
		return 0;
	}

	print_success(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

static int charger_termination_voltage_get(const struct shell *shell,
					   npmx_error_t (*func)(npmx_charger_t const *p_instance,
								npmx_charger_voltage_t *voltage))
{
	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	npmx_charger_voltage_t charger_voltage;
	npmx_error_t err_code = func(charger_instance, &charger_voltage);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "termination voltage");
		return 0;
	}

	uint32_t voltage_mv;
	if (!npmx_charger_voltage_convert_to_mv(charger_voltage, &voltage_mv)) {
		print_convert_error(shell, "termination voltage", "millivolts");
		return 0;
	}

	print_value(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

static int cmd_charger_termination_voltage_normal_set(const struct shell *shell, size_t argc,
						      char **argv)
{
	return charger_termination_voltage_set(shell, argc, argv,
					       npmx_charger_termination_normal_voltage_set);
}

static int cmd_charger_termination_voltage_normal_get(const struct shell *shell, size_t argc,
						      char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_termination_voltage_get(shell, npmx_charger_termination_normal_voltage_get);
}

static int cmd_charger_termination_voltage_warm_set(const struct shell *shell, size_t argc,
						    char **argv)
{
	return charger_termination_voltage_set(shell, argc, argv,
					       npmx_charger_termination_warm_voltage_set);
}

static int cmd_charger_termination_voltage_warm_get(const struct shell *shell, size_t argc,
						    char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	return charger_termination_voltage_get(shell, npmx_charger_termination_warm_voltage_get);
}

static int cmd_charger_trickle_voltage_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_VALUE, "trickle voltage" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	if (!charger_disabled_check(shell, charger_instance, "trickle voltage")) {
		return 0;
	}

	uint32_t voltage_mv = args_info.arg[0].result.uvalue;
	npmx_charger_trickle_t charger_trickle = npmx_charger_trickle_convert(voltage_mv);
	if (charger_trickle == NPMX_CHARGER_TRICKLE_INVALID) {
		print_convert_error(shell, "millivolts", "trickle voltage");
		return 0;
	}

	npmx_error_t err_code = npmx_charger_trickle_voltage_set(charger_instance, charger_trickle);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "trickle voltage");
		return 0;
	}

	print_success(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

static int cmd_charger_trickle_voltage_get(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	npmx_charger_t *charger_instance = charger_instance_get(shell);
	if (charger_instance == NULL) {
		return 0;
	}

	npmx_charger_trickle_t charger_trickle;
	npmx_error_t err_code =
		npmx_charger_trickle_voltage_get(charger_instance, &charger_trickle);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "trickle voltage");
		return 0;
	}

	uint32_t voltage_mv;
	if (!npmx_charger_trickle_convert_to_mv(charger_trickle, &voltage_mv)) {
		print_convert_error(shell, "trickle voltage", "millivolts");
		return 0;
	}

	print_value(shell, voltage_mv, UNIT_TYPE_MILLIVOLT);
	return 0;
}

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

static npmx_gpio_mode_t gpio_mode_convert(const struct shell *shell, uint32_t mode)
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
		shell_error(shell, "Error: Wrong mode:");
		print_hint_error(shell, 0, "Input");
		print_hint_error(shell, 1, "Input logic 1");
		print_hint_error(shell, 2, "Input logic 0");
		print_hint_error(shell, 3, "Input rising edge event");
		print_hint_error(shell, 4, "Input falling edge event");
		print_hint_error(shell, 5, "Output interrupt");
		print_hint_error(shell, 6, "Output reset");
		print_hint_error(shell, 7, "Output power loss warning");
		print_hint_error(shell, 8, "Output logic 1");
		print_hint_error(shell, 9, "Output logic 0");
		return NPMX_GPIO_MODE_INVALID;
	}
}

static npmx_gpio_pull_t gpio_pull_convert(const struct shell *shell, uint32_t pull)
{
	switch (pull) {
	case 0:
		return NPMX_GPIO_PULL_DOWN;
	case 1:
		return NPMX_GPIO_PULL_UP;
	case 2:
		return NPMX_GPIO_PULL_NONE;
	default:
		shell_error(shell, "Error: Wrong pull:");
		print_hint_error(shell, 0, "Pull down");
		print_hint_error(shell, 1, "Pull up");
		print_hint_error(shell, 2, "Pull disable");
		return NPMX_GPIO_PULL_INVALID;
	}
}

static int gpio_config_set(const struct shell *shell, size_t argc, char **argv,
			   gpio_config_param_t config_type)
{
	char *config_name;
	shell_arg_type_t arg_type = SHELL_ARG_TYPE_UINT32_VALUE;
	switch (config_type) {
	case GPIO_CONFIG_PARAM_DEBOUNCE:
		config_name = "debounce";
		arg_type = SHELL_ARG_TYPE_BOOL_VALUE;
		break;
	case GPIO_CONFIG_PARAM_DRIVE:
		config_name = "drive current";
		break;
	case GPIO_CONFIG_PARAM_MODE:
		config_name = "mode";
		break;
	case GPIO_CONFIG_PARAM_OPEN_DRAIN:
		config_name = "open drain";
		arg_type = SHELL_ARG_TYPE_BOOL_VALUE;
		break;
	case GPIO_CONFIG_PARAM_PULL:
		config_name = "pull";
		break;
	case GPIO_CONFIG_PARAM_TYPE:
		return 0;
	}

	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "GPIO" },
					  [1] = { arg_type, config_name },
				  } };

	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_gpio_t *gpio_instance = gpio_instance_get(shell, args_info.arg[0].result.uvalue);
	if (gpio_instance == NULL) {
		return 0;
	}

	if (!check_pin_configuration_correctness(shell, (int8_t)(args_info.arg[0].result.uvalue))) {
		return 0;
	}

	npmx_gpio_config_t gpio_config;
	npmx_error_t err_code = npmx_gpio_config_get(gpio_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "GPIO config");
		return 0;
	}

	shell_arg_result_t result = args_info.arg[1].result;
	switch (config_type) {
	case GPIO_CONFIG_PARAM_DEBOUNCE:
		gpio_config.debounce = result.bvalue;
		break;
	case GPIO_CONFIG_PARAM_DRIVE:
		gpio_config.drive = npmx_gpio_drive_convert(result.uvalue);
		if (gpio_config.drive == NPMX_GPIO_DRIVE_INVALID) {
			shell_error(shell, "Error: Wrong drive current:");
			print_hint_error(shell, 1, "1 mA");
			print_hint_error(shell, 6, "6 mA");
			return 0;
		}
		break;
	case GPIO_CONFIG_PARAM_MODE:
		gpio_config.mode = gpio_mode_convert(shell, result.uvalue);
		if (gpio_config.mode == NPMX_GPIO_MODE_INVALID) {
			return 0;
		}
		break;
	case GPIO_CONFIG_PARAM_OPEN_DRAIN:
		gpio_config.open_drain = result.bvalue;
		break;
	case GPIO_CONFIG_PARAM_PULL:
		gpio_config.pull = gpio_pull_convert(shell, result.uvalue);
		if (gpio_config.pull == NPMX_GPIO_PULL_INVALID) {
			return 0;
		}
		break;
	default:
		return 0;
	}

	err_code = npmx_gpio_config_set(gpio_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "GPIO config");
		return 0;
	}

	switch (config_type) {
	case GPIO_CONFIG_PARAM_DRIVE:
		print_success(shell, result.uvalue, UNIT_TYPE_MILLIAMPERE);
		break;
	default:
		print_success(shell, result.uvalue, UNIT_TYPE_NONE);
		break;
	}

	return 0;
}

static int gpio_config_get(const struct shell *shell, size_t argc, char **argv,
			   gpio_config_param_t config_type)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "GPIO" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_gpio_t *gpio_instance = gpio_instance_get(shell, args_info.arg[0].result.uvalue);
	if (gpio_instance == NULL) {
		return 0;
	}

	npmx_gpio_config_t gpio_config;
	npmx_error_t err_code = npmx_gpio_config_get(gpio_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "GPIO config");
		return 0;
	}

	switch (config_type) {
	case GPIO_CONFIG_PARAM_DEBOUNCE:
		print_value(shell, gpio_config.debounce, UNIT_TYPE_NONE);
		break;
	case GPIO_CONFIG_PARAM_DRIVE:
		uint32_t current_ma = 0;
		if (!npmx_gpio_drive_convert_to_ma(gpio_config.drive, &current_ma)) {
			print_convert_error(shell, "gpio drive", "milliamperes");
			return 0;
		}
		print_value(shell, current_ma, UNIT_TYPE_MILLIAMPERE);
		break;
	case GPIO_CONFIG_PARAM_MODE:
		print_value(shell, (int)gpio_config.mode, UNIT_TYPE_NONE);
		break;
	case GPIO_CONFIG_PARAM_OPEN_DRAIN:
		print_value(shell, gpio_config.open_drain, UNIT_TYPE_NONE);
		break;
	case GPIO_CONFIG_PARAM_PULL:
		print_value(shell, (int)gpio_config.pull, UNIT_TYPE_NONE);
		break;
	case GPIO_CONFIG_PARAM_TYPE:
		if (gpio_config.mode <= NPMX_GPIO_MODE_INPUT_FALLING_EDGE) {
			shell_print(shell, "Value: input.");
		} else {
			shell_print(shell, "Value: output.");
		}
		break;
	}
	return 0;
}

static int cmd_gpio_config_debounce_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_PARAM_DEBOUNCE);
}

static int cmd_gpio_config_debounce_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_PARAM_DEBOUNCE);
}

static int cmd_gpio_config_drive_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_PARAM_DRIVE);
}

static int cmd_gpio_config_drive_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_PARAM_DRIVE);
}

static int cmd_gpio_config_mode_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_PARAM_MODE);
}

static int cmd_gpio_config_mode_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_PARAM_MODE);
}

static int cmd_gpio_config_open_drain_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_PARAM_OPEN_DRAIN);
}

static int cmd_gpio_config_open_drain_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_PARAM_OPEN_DRAIN);
}

static int cmd_gpio_config_pull_set(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_set(shell, argc, argv, GPIO_CONFIG_PARAM_PULL);
}

static int cmd_gpio_config_pull_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_PARAM_PULL);
}

static int cmd_gpio_status_get(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "GPIO" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_gpio_t *gpio_instance = gpio_instance_get(shell, args_info.arg[0].result.uvalue);
	if (gpio_instance == NULL) {
		return 0;
	}

	bool gpio_status;
	npmx_error_t err_code = npmx_gpio_status_check(gpio_instance, &gpio_status);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "GPIO status");
		return 0;
	}

	print_value(shell, gpio_status, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_gpio_type_get(const struct shell *shell, size_t argc, char **argv)
{
	return gpio_config_get(shell, argc, argv, GPIO_CONFIG_PARAM_TYPE);
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

static int cmd_ldsw_gpio_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 3,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LDSW" },
					  [1] = { SHELL_ARG_TYPE_INT32_VALUE, "GPIO number" },
					  [2] = { SHELL_ARG_TYPE_BOOL_VALUE,
						  "GPIO inversion status" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_ldsw_t *ldsw_instance = ldsw_instance_get(shell, args_info.arg[0].result.uvalue);
	if (ldsw_instance == NULL) {
		return 0;
	}

	int gpio_index = args_info.arg[1].result.ivalue;
	npmx_ldsw_gpio_config_t gpio_config = {
		.gpio = ldsw_gpio_index_convert(gpio_index),
		.inverted = args_info.arg[2].result.bvalue,
	};

	if (gpio_config.gpio == NPMX_LDSW_GPIO_INVALID) {
		shell_error(shell, "Error: wrong GPIO index.");
		return 0;
	}

	if (!check_pin_configuration_correctness(shell, gpio_index)) {
		return 0;
	}

	npmx_error_t err_code = npmx_ldsw_enable_gpio_set(ldsw_instance, &gpio_config);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "GPIO config");
		return 0;
	}

	shell_print(shell, "Success: %d %d.", gpio_index, gpio_config.inverted);
	return 0;
}

static int cmd_ldsw_gpio_get(const struct shell *shell, size_t argc, char **argv)
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

	int8_t gpio_index =
		(gpio_config.gpio == NPMX_LDSW_GPIO_NC) ? -1 : ((int8_t)gpio_config.gpio - 1);

	shell_print(shell, "Value: %d %d.", gpio_index, gpio_config.inverted);
	return 0;
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

static int cmd_led_mode_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LED" },
					  [1] = { SHELL_ARG_TYPE_UINT32_VALUE, "mode" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_led_t *led_instance = led_instance_get(shell, args_info.arg[0].result.uvalue);
	if (led_instance == NULL) {
		return 0;
	}

	uint32_t mode = args_info.arg[1].result.uvalue;
	npmx_led_mode_t led_mode;
	switch (mode) {
	case 0:
		led_mode = NPMX_LED_MODE_ERROR;
		break;
	case 1:
		led_mode = NPMX_LED_MODE_CHARGING;
		break;
	case 2:
		led_mode = NPMX_LED_MODE_HOST;
		break;
	case 3:
		led_mode = NPMX_LED_MODE_NOTUSED;
		break;
	default:
		shell_error(shell, "Error: Wrong mode:");
		print_hint_error(shell, 0, "Charger error");
		print_hint_error(shell, 1, "Charging");
		print_hint_error(shell, 2, "Host");
		print_hint_error(shell, 3, "Not used");
		return 0;
	}

	npmx_error_t err_code = npmx_led_mode_set(led_instance, led_mode);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "LED mode");
		return 0;
	}

	print_success(shell, mode, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_led_mode_get(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 1,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LED" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_led_t *led_instance = led_instance_get(shell, args_info.arg[0].result.uvalue);
	if (led_instance == NULL) {
		return 0;
	}

	npmx_led_mode_t mode;
	npmx_error_t err_code = npmx_led_mode_get(led_instance, &mode);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "LED mode");
		return 0;
	}

	print_value(shell, (int)mode, UNIT_TYPE_NONE);
	return 0;
}

static int cmd_led_state_set(const struct shell *shell, size_t argc, char **argv)
{
	args_info_t args_info = { .expected_args = 2,
				  .arg = {
					  [0] = { SHELL_ARG_TYPE_UINT32_INDEX, "LED" },
					  [1] = { SHELL_ARG_TYPE_BOOL_VALUE, "state" },
				  } };
	if (!arguments_check(shell, argc, argv, &args_info)) {
		return 0;
	}

	npmx_led_t *led_instance = led_instance_get(shell, args_info.arg[0].result.uvalue);
	if (led_instance == NULL) {
		return 0;
	}

	bool led_state = args_info.arg[1].result.bvalue;
	npmx_error_t err_code = npmx_led_state_set(led_instance, led_state);
	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "LED state");
		return 0;
	}

	print_success(shell, led_state, UNIT_TYPE_NONE);
	return 0;
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

	if (npmx_vbusin_current_convert_to_ma(current_limit, &current)) {
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
		shell_print(shell, "Success: %u mA.", current);
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

/* Creating subcommands (level 3 command) array for command "adc meas". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_meas,
			       SHELL_CMD(vbat, NULL, "Get battery voltage", cmd_adc_meas_vbat_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "adc ntc beta". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_ntc_beta,
			       SHELL_CMD(set, NULL, "Set ADC NTC beta value", cmd_adc_ntc_beta_set),
			       SHELL_CMD(get, NULL, "Get ADC NTC beta value", cmd_adc_ntc_beta_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "adc ntc type". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_ntc_type,
			       SHELL_CMD(set, NULL, "Set ADC NTC type", cmd_adc_ntc_type_set),
			       SHELL_CMD(get, NULL, "Get ADC NTC type", cmd_adc_ntc_type_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "adc ntc_type". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_ntc,
			       SHELL_CMD(beta, &sub_adc_ntc_beta, "ADC NTC beta", NULL),
			       SHELL_CMD(type, &sub_adc_ntc_type, "ADC NTC type", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "adc". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc, SHELL_CMD(meas, &sub_adc_meas, "ADC measurement", NULL),
			       SHELL_CMD(ntc, &sub_adc_ntc, "ADC NTC", NULL), SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "buck active_discharge". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_active_discharge,
			       SHELL_CMD(set, NULL, "Set active discharge status",
					 cmd_buck_active_discharge_set),
			       SHELL_CMD(get, NULL, "Get active discharge status",
					 cmd_buck_active_discharge_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck gpio on_off". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_on_off,
			       SHELL_CMD(set, NULL, "Set buck GPIO on/off",
					 cmd_buck_gpio_on_off_set),
			       SHELL_CMD(get, NULL, "Get buck GPIO on/off",
					 cmd_buck_gpio_on_off_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck gpio pwm_force". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_pwm_force,
			       SHELL_CMD(set, NULL, "Set buck GPIO PWM force",
					 cmd_buck_gpio_pwm_force_set),
			       SHELL_CMD(get, NULL, "Get buck GPIO PWM force",
					 cmd_buck_gpio_pwm_force_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 4 command) array for command "buck gpio retention". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_buck_gpio_retention,
			       SHELL_CMD(set, NULL, "Set buck GPIO retention",
					 cmd_buck_gpio_retention_set),
			       SHELL_CMD(get, NULL, "Get buck GPIO retention",
					 cmd_buck_gpio_retention_get),
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

/* Creating subcommands (level 3 command) array for command "charger charging_current". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_charging_current,
			       SHELL_CMD(set, NULL, "Set charging current",
					 cmd_charger_charging_current_set),
			       SHELL_CMD(get, NULL, "Get charging current",
					 cmd_charger_charging_current_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger die_temp resume". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_die_temp_resume,
			       SHELL_CMD(set, NULL,
					 "Set die temperature threshold for resume charging",
					 cmd_charger_die_temp_resume_set),
			       SHELL_CMD(get, NULL,
					 "Get die temperature threshold for resume charging",
					 cmd_charger_die_temp_resume_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger die_temp status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_die_temp_status,
			       SHELL_CMD(get, NULL, "Get die temperature comparator status",
					 cmd_charger_die_temp_status_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger die_temp stop". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_die_temp_stop,
			       SHELL_CMD(set, NULL,
					 "Set die temperature threshold for stop charging",
					 cmd_charger_die_temp_stop_set),
			       SHELL_CMD(get, NULL,
					 "Get die temperature threshold for stop charging",
					 cmd_charger_die_temp_stop_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger die_temp". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_die_temp,
			       SHELL_CMD(resume, &sub_charger_die_temp_resume,
					 "Die temperature threshold for resume charging", NULL),
			       SHELL_CMD(status, &sub_charger_die_temp_status,
					 "Die temperature comparator status", NULL),
			       SHELL_CMD(stop, &sub_charger_die_temp_stop,
					 "Die temperature threshold for stop charging", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger discharging_current". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_discharging_current,
			       SHELL_CMD(set, NULL, "Set discharging current",
					 cmd_charger_discharging_current_set),
			       SHELL_CMD(get, NULL, "Get discharging current",
					 cmd_charger_discharging_current_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger module charger". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_module_charger,
			       SHELL_CMD(set, NULL, "Set charger status",
					 cmd_charger_module_charger_set),
			       SHELL_CMD(get, NULL, "Get charger status",
					 cmd_charger_module_charger_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger module full_cool". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_module_full_cool,
			       SHELL_CMD(set, NULL, "Set full cool status",
					 cmd_charger_module_full_cool_set),
			       SHELL_CMD(get, NULL, "Get full cool status",
					 cmd_charger_module_full_cool_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger module ntc_limits". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_module_ntc_limits,
			       SHELL_CMD(set, NULL, "Set NTC status",
					 cmd_charger_module_ntc_limits_set),
			       SHELL_CMD(get, NULL, "Get NTC status",
					 cmd_charger_module_ntc_limits_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger module recharge". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_module_recharge,
			       SHELL_CMD(set, NULL, "Set recharge status",
					 cmd_charger_module_recharge_set),
			       SHELL_CMD(get, NULL, "Get recharge status",
					 cmd_charger_module_recharge_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger module". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_charger_module, SHELL_CMD(charger, &sub_charger_module_charger, "Charger module", NULL),
	SHELL_CMD(full_cool, &sub_charger_module_full_cool, "Full charge in cool temp module",
		  NULL),
	SHELL_CMD(ntc_limits, &sub_charger_module_ntc_limits, "NTC limits module", NULL),
	SHELL_CMD(recharge, &sub_charger_module_recharge, "Recharge module", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_resistance cold". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_resistance_cold,
			       SHELL_CMD(set, NULL, "Set NTC resistance",
					 cmd_charger_ntc_resistance_cold_set),
			       SHELL_CMD(get, NULL, "Get NTC resistance",
					 cmd_charger_ntc_resistance_cold_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_resistance cool". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_resistance_cool,
			       SHELL_CMD(set, NULL, "Set NTC resistance",
					 cmd_charger_ntc_resistance_cool_set),
			       SHELL_CMD(get, NULL, "Get NTC resistance",
					 cmd_charger_ntc_resistance_cool_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_resistance warm". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_resistance_warm,
			       SHELL_CMD(set, NULL, "Set NTC resistance",
					 cmd_charger_ntc_resistance_warm_set),
			       SHELL_CMD(get, NULL, "Get NTC resistance",
					 cmd_charger_ntc_resistance_warm_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_resistance hot". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_resistance_hot,
			       SHELL_CMD(set, NULL, "Set NTC resistance",
					 cmd_charger_ntc_resistance_hot_set),
			       SHELL_CMD(get, NULL, "Get NTC resistance",
					 cmd_charger_ntc_resistance_hot_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger ntc_resistance". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_resistance,
			       SHELL_CMD(cold, &sub_charger_ntc_resistance_cold,
					 "NTC resistance when cold temperature", NULL),
			       SHELL_CMD(cool, &sub_charger_ntc_resistance_cool,
					 "NTC resistance when cool temperature", NULL),
			       SHELL_CMD(warm, &sub_charger_ntc_resistance_warm,
					 "NTC resistance when warm temperature", NULL),
			       SHELL_CMD(hot, &sub_charger_ntc_resistance_hot,
					 "NTC resistance when hot temperature", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_temperature cold". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_temperature_cold,
			       SHELL_CMD(set, NULL, "Set NTC temperature",
					 cmd_charger_ntc_temperature_cold_set),
			       SHELL_CMD(get, NULL, "Get NTC temperature",
					 cmd_charger_ntc_temperature_cold_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_temperature cool". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_temperature_cool,
			       SHELL_CMD(set, NULL, "Set NTC temperature",
					 cmd_charger_ntc_temperature_cool_set),
			       SHELL_CMD(get, NULL, "Get NTC temperature",
					 cmd_charger_ntc_temperature_cool_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_temperature warm". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_temperature_warm,
			       SHELL_CMD(set, NULL, "Set NTC temperature",
					 cmd_charger_ntc_temperature_warm_set),
			       SHELL_CMD(get, NULL, "Get NTC temperature",
					 cmd_charger_ntc_temperature_warm_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger ntc_temperature hot". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_temperature_hot,
			       SHELL_CMD(set, NULL, "Set NTC temperature at hot",
					 cmd_charger_ntc_temperature_hot_set),
			       SHELL_CMD(get, NULL, "Get NTC temperature at hot",
					 cmd_charger_ntc_temperature_hot_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger ntc_temperature". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_ntc_temperature,
			       SHELL_CMD(cold, &sub_charger_ntc_temperature_cold,
					 "NTC temperature when cold temperature", NULL),
			       SHELL_CMD(cool, &sub_charger_ntc_temperature_cool,
					 "NTC temperature when cool temperature", NULL),
			       SHELL_CMD(warm, &sub_charger_ntc_temperature_warm,
					 "NTC temperature when warm temperature", NULL),
			       SHELL_CMD(hot, &sub_charger_ntc_temperature_hot,
					 "NTC temperature when hot temperature", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger status all". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_status_all,
			       SHELL_CMD(get, NULL, "Get all status", cmd_charger_status_all_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger status charging". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_status_charging,
			       SHELL_CMD(get, NULL, "Get charging status",
					 cmd_charger_status_charging_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_status,
			       SHELL_CMD(all, &sub_charger_status_all, "All status", NULL),
			       SHELL_CMD(charging, &sub_charger_status_charging, "Charging status",
					 NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger termination_current". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_current,
			       SHELL_CMD(set, NULL, "Set charger termination current",
					 cmd_charger_termination_current_set),
			       SHELL_CMD(get, NULL, "Get charger termination current",
					 cmd_charger_termination_current_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger termination_voltage normal". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_voltage_normal,
			       SHELL_CMD(set, NULL, "Set charger normal termination voltage",
					 cmd_charger_termination_voltage_normal_set),
			       SHELL_CMD(get, NULL, "Get charger normal termination voltage",
					 cmd_charger_termination_voltage_normal_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "charger termination_voltage warm". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_voltage_warm,
			       SHELL_CMD(set, NULL, "Set charger warm termination voltage",
					 cmd_charger_termination_voltage_warm_set),
			       SHELL_CMD(get, NULL, "Get charger warm termination voltage",
					 cmd_charger_termination_voltage_warm_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger termination_voltage". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_termination_voltage,
			       SHELL_CMD(normal, &sub_charger_termination_voltage_normal,
					 "Charger termination voltage normal", NULL),
			       SHELL_CMD(warm, &sub_charger_termination_voltage_warm,
					 "Charger termination voltage warm", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "charger trickle_voltage". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_charger_trickle_voltage,
			       SHELL_CMD(set, NULL, "Set charger trickle voltage",
					 cmd_charger_trickle_voltage_set),
			       SHELL_CMD(get, NULL, "Get charger trickle voltage",
					 cmd_charger_trickle_voltage_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "charger". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_charger,
	SHELL_CMD(charging_current, &sub_charger_charging_current, "Charger current", NULL),
	SHELL_CMD(die_temp, &sub_charger_die_temp, "Charger die temperature", NULL),
	SHELL_CMD(discharging_current, &sub_charger_discharging_current,
		  "Maximum discharging current", NULL),
	SHELL_CMD(module, &sub_charger_module, "Charger module", NULL),
	SHELL_CMD(ntc_resistance, &sub_charger_ntc_resistance, "Battery NTC resistance values",
		  NULL),
	SHELL_CMD(ntc_temperature, &sub_charger_ntc_temperature, "Battery NTC temperature values",
		  NULL),
	SHELL_CMD(status, &sub_charger_status, "Charger status", NULL),
	SHELL_CMD(termination_current, &sub_charger_termination_current,
		  "Charger termination current", NULL),
	SHELL_CMD(termination_voltage, &sub_charger_termination_voltage,
		  "Charger termination voltage", NULL),
	SHELL_CMD(trickle_voltage, &sub_charger_trickle_voltage, "Charger trickle voltage", NULL),
	SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "errlog". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_errlog, SHELL_CMD(get, NULL, "Get error logs", cmd_errlog_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "gpio config debounce". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_config_debounce,
			       SHELL_CMD(set, NULL, "Set debounce status",
					 cmd_gpio_config_debounce_set),
			       SHELL_CMD(get, NULL, "Get debounce status",
					 cmd_gpio_config_debounce_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "gpio config drive". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_config_drive,
			       SHELL_CMD(set, NULL, "Set drive status", cmd_gpio_config_drive_set),
			       SHELL_CMD(get, NULL, "Get drive status", cmd_gpio_config_drive_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "gpio config mode". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_config_mode,
			       SHELL_CMD(set, NULL, "Set GPIO mode", cmd_gpio_config_mode_set),
			       SHELL_CMD(get, NULL, "Get GPIO mode", cmd_gpio_config_mode_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 4 command) array for command "gpio config open_drain". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_config_open_drain,
			       SHELL_CMD(set, NULL, "Set open drain status",
					 cmd_gpio_config_open_drain_set),
			       SHELL_CMD(get, NULL, "Get open drain status",
					 cmd_gpio_config_open_drain_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio config pull". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_config_pull,
			       SHELL_CMD(set, NULL, "Set pull status", cmd_gpio_config_pull_set),
			       SHELL_CMD(get, NULL, "Get pull status", cmd_gpio_config_pull_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio config". */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_gpio_config, SHELL_CMD(debounce, &sub_gpio_config_debounce, "Debounce config", NULL),
	SHELL_CMD(drive, &sub_gpio_config_drive, "Drive current config", NULL),
	SHELL_CMD(mode, &sub_gpio_config_mode, "GPIO mode config", NULL),
	SHELL_CMD(open_drain, &sub_gpio_config_open_drain, "Open drain config", NULL),
	SHELL_CMD(pull, &sub_gpio_config_pull, "Pull type config", NULL), SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio status". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_status,
			       SHELL_CMD(get, NULL, "Get GPIO status", cmd_gpio_status_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "gpio type". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio_type,
			       SHELL_CMD(get, NULL, "Get GPIO type", cmd_gpio_type_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "gpio". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_gpio, SHELL_CMD(config, &sub_gpio_config, "GPIO config", NULL),
			       SHELL_CMD(status, &sub_gpio_status, "GPIO status", NULL),
			       SHELL_CMD(type, &sub_gpio_type, "GPIO type", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "ldsw active_discharge". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_active_discharge,
			       SHELL_CMD(set, NULL, "Set active discharge status",
					 cmd_ldsw_active_discharge_set),
			       SHELL_CMD(get, NULL, "Get active discharge status",
					 cmd_ldsw_active_discharge_get),
			       SHELL_SUBCMD_SET_END);

/* Creating dictionary subcommands (level 3 command) array for command "ldsw gpio". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_ldsw_gpio,
			       SHELL_CMD(set, NULL, "Set LDSW GPIO", cmd_ldsw_gpio_set),
			       SHELL_CMD(get, NULL, "Get LDSW GPIO", cmd_ldsw_gpio_get),
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

/* Creating subcommands (level 3 command) array for command "led mode". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_led_mode, SHELL_CMD(set, NULL, "Set LED mode", cmd_led_mode_set),
			       SHELL_CMD(get, NULL, "Get LED mode", cmd_led_mode_get),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "led state". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_led_state,
			       SHELL_CMD(set, NULL, "Set LED status", cmd_led_state_set),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "led". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_led, SHELL_CMD(mode, &sub_led_mode, "LED mode", NULL),
			       SHELL_CMD(state, &sub_led_state, "LED state", NULL),
			       SHELL_SUBCMD_SET_END);

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
	sub_npmx, SHELL_CMD(adc, &sub_adc, "ADC", NULL), SHELL_CMD(buck, &sub_buck, "Buck", NULL),
	SHELL_CMD(charger, &sub_charger, "Charger", NULL),
	SHELL_CMD(errlog, &sub_errlog, "Reset errors logs", NULL),
	SHELL_CMD(gpio, &sub_gpio, "GPIO", NULL), SHELL_CMD(ldsw, &sub_ldsw, "LDSW", NULL),
	SHELL_CMD(led, &sub_led, "LED", NULL), SHELL_CMD(pof, &sub_pof, "POF", NULL),
	SHELL_CMD(timer, &sub_timer, "Timer", NULL), SHELL_CMD(vbusin, &sub_vbusin, "VBUSIN", NULL),
	SHELL_CMD(ship, &sub_ship, "SHIP", NULL),
	SHELL_CMD(reset, NULL, "Restart device", cmd_reset), SHELL_SUBCMD_SET_END);

/* Creating root (level 0) command "npmx" without a handler. */
SHELL_CMD_REGISTER(npmx, &sub_npmx, "npmx", NULL);
