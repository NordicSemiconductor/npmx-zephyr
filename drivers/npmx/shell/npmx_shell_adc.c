/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <npmx_shell_common.h>
#include <npmx_driver.h>
#include <math.h>

/** @brief The difference in centigrade scale between 0*C to absolute zero temperature. */
#define ABSOLUTE_ZERO_DIFFERENCE 273.15f

/**
 * @brief All thermistors values are defined in temperature 25*C.
 *        For calculations Kelvin scale is required.
 */
#define THERMISTOR_NOMINAL_TEMPERATURE (25.0f + ABSOLUTE_ZERO_DIFFERENCE)

static const struct device *pmic_dev = DEVICE_DT_GET(DT_NODELABEL(npm_0));

/** @brief NTC thermistor configuration parameter. */
typedef enum {
	ADC_NTC_CONFIG_PARAM_TYPE, /* Battery NTC type. */
	ADC_NTC_CONFIG_PARAM_BETA /* Battery NTC beta value. */
} adc_ntc_config_param_t;

/** @brief Data needed to recalculate NTC temperatures. */
typedef struct {
	npmx_charger_t *charger_instance; /* Charger instance. */
	npmx_adc_ntc_config_t *ntc_config_old; /* Previous NTC configuration. */
	npmx_adc_ntc_config_t *ntc_config_new; /* New NTC configuration. */
	npmx_error_t (*get_resistance_func)(
		npmx_charger_t const *p_instance,
		uint32_t *resistance); /* Pointer to function getting NTC resistance for temperature treshold. */
	npmx_error_t (*set_resistance_func)(
		npmx_charger_t const *p_instance,
		uint32_t resistance); /* Pointer to function setting NTC resistance for temperature treshold. */
} temp_recalculate_data_t;

static bool ntc_resistance_to_temperature(const struct shell *shell,
					  npmx_adc_ntc_config_t const *ntc_config,
					  uint32_t resistance, int16_t *temperature)
{
	uint32_t ntc_nominal_resistance;
	if (!npmx_adc_ntc_type_convert_to_ohms(ntc_config->type, &ntc_nominal_resistance)) {
		shell_error(shell, "Error: unable to convert NTC type to resistance.");
		return false;
	}

	float numerator = THERMISTOR_NOMINAL_TEMPERATURE * (float)ntc_config->beta;
	float denominator = (THERMISTOR_NOMINAL_TEMPERATURE *
			     log((float)resistance / (float)ntc_nominal_resistance)) +
			    (float)ntc_config->beta;
	float temperature_float = round((numerator / denominator) - ABSOLUTE_ZERO_DIFFERENCE);

	*temperature = (int16_t)temperature_float;

	return true;
}

static bool ntc_temperature_to_resistance(const struct shell *shell,
					  npmx_adc_ntc_config_t const *ntc_config,
					  int16_t temperature, uint32_t *resistance)
{
	uint32_t ntc_nominal_resistance;
	if (!npmx_adc_ntc_type_convert_to_ohms(ntc_config->type, &ntc_nominal_resistance)) {
		shell_error(shell, "Error: unable to convert NTC type to resistance.");
		return false;
	}

	float target_temperature = ((float)temperature + ABSOLUTE_ZERO_DIFFERENCE);
	float exp_val = ((1.0f / target_temperature) - (1.0f / THERMISTOR_NOMINAL_TEMPERATURE)) *
			(float)ntc_config->beta;
	float resistance_float = round((float)ntc_nominal_resistance * exp(exp_val));

	*resistance = (uint32_t)resistance_float;

	return true;
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

static bool adc_ntc_temperatures_recalculate(const struct shell *shell,
					     temp_recalculate_data_t const *data)
{
	int16_t temperature;
	uint32_t resistance;

	npmx_error_t err_code = data->get_resistance_func(data->charger_instance, &resistance);
	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to get old resistance value.");
		return false;
	}
	if (!ntc_resistance_to_temperature(shell, data->ntc_config_old, resistance, &temperature)) {
		shell_error(shell, "Error: unable convert old resistance to temperature.");
		return false;
	}
	if (!ntc_temperature_to_resistance(shell, data->ntc_config_new, temperature, &resistance)) {
		shell_error(shell, "Error: unable convert temperature to new resistance value.");
		return false;
	}
	err_code = data->set_resistance_func(data->charger_instance, resistance);

	if (!check_error_code(shell, err_code)) {
		shell_error(shell, "Error: unable to set new resistance value.");
		return false;
	}

	return true;
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

	if (config_type == ADC_NTC_CONFIG_PARAM_BETA && config_val == 0) {
		shell_error(shell, "Error: beta cannot be equal to zero.");
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

	npmx_adc_ntc_config_t ntc_config_old;
	memcpy(&ntc_config_old, &ntc_config, sizeof(npmx_adc_ntc_config_t));

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
		return 0;
	}

	if (config_type == ADC_NTC_CONFIG_PARAM_BETA && ntc_config.type != NPMX_ADC_NTC_TYPE_HI_Z &&
	    ntc_config_old.type != NPMX_ADC_NTC_TYPE_HI_Z && ntc_config_old.beta != 0) {
		temp_recalculate_data_t recalculate_data = {
			.charger_instance = charger_instance,
			.ntc_config_old = &ntc_config_old,
			.ntc_config_new = &ntc_config,
			.get_resistance_func = npmx_charger_cold_resistance_get,
			.set_resistance_func = npmx_charger_cold_resistance_set
		};
		if (!adc_ntc_temperatures_recalculate(shell, &recalculate_data)) {
			shell_error(
				shell,
				"Error: unable to recalculate NTC threshold for cold temperature.");
			return 0;
		}

		recalculate_data.get_resistance_func = npmx_charger_cool_resistance_get;
		recalculate_data.set_resistance_func = npmx_charger_cool_resistance_set;
		if (!adc_ntc_temperatures_recalculate(shell, &recalculate_data)) {
			shell_error(
				shell,
				"Error: unable to recalculate NTC threshold for cool temperature.");
			return 0;
		}

		recalculate_data.get_resistance_func = npmx_charger_warm_resistance_get;
		recalculate_data.set_resistance_func = npmx_charger_warm_resistance_set;
		if (!adc_ntc_temperatures_recalculate(shell, &recalculate_data)) {
			shell_error(
				shell,
				"Error: unable to recalculate NTC threshold for warm temperature.");
			return 0;
		}

		recalculate_data.get_resistance_func = npmx_charger_hot_resistance_get;
		recalculate_data.set_resistance_func = npmx_charger_hot_resistance_set;
		if (!adc_ntc_temperatures_recalculate(shell, &recalculate_data)) {
			shell_error(
				shell,
				"Error: unable to recalculate threshold NTC for hot temperature.");
			return 0;
		}
		shell_print(shell, "Info: NTC thresholds recalculated successfully.");
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

void dynamic_cmd_adc(size_t index, struct shell_static_entry *entry)
{
	if (index < 2) {
		entry->syntax = sub_adc.entry[index].syntax;
		entry->handler = sub_adc.entry[index].handler;
		entry->subcmd = sub_adc.entry[index].subcmd;
		entry->help = sub_adc.entry[index].help;
	} else {
		entry->syntax = NULL;
	}
}
