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

/** @brief NTC thermistor configuration parameter. */
typedef enum {
	ADC_NTC_CONFIG_PARAM_TYPE, /* Battery NTC type. */
	ADC_NTC_CONFIG_PARAM_BETA /* Battery NTC beta value. */
} adc_ntc_config_param_t;

static npmx_adc_t *adc_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);

	return npmx_instance ? npmx_adc_get(npmx_instance, 0) : NULL;
}

static npmx_charger_t *charger_instance_get(const struct shell *shell, const char *help,
					    bool expect_disabled)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);
	if (npmx_instance == NULL) {
		return NULL;
	}

	npmx_charger_t *charger_instance = npmx_charger_get(npmx_instance, 0);
	if (expect_disabled) {
		uint32_t modules_mask;
		npmx_error_t err_code = npmx_charger_module_get(charger_instance, &modules_mask);

		if (!check_error_code(shell, err_code)) {
			print_get_error(shell, "charger module status");
			return NULL;
		}

		if ((modules_mask & NPMX_CHARGER_MODULE_CHARGER_MASK) != 0) {
			shell_error(shell, "Error: charger must be disabled to set %s.", help);
			return NULL;
		}
	}

	return charger_instance;
}

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
		print_convert_error(shell, "NTC type", "resistance");
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
		print_convert_error(shell, "NTC type", "resistance");
		return false;
	}

	float target_temperature = ((float)temperature + ABSOLUTE_ZERO_DIFFERENCE);
	float exp_val = ((1.0f / target_temperature) - (1.0f / THERMISTOR_NOMINAL_TEMPERATURE)) *
			(float)ntc_config->beta;
	float resistance_float = round((float)ntc_nominal_resistance * exp(exp_val));

	*resistance = (uint32_t)resistance_float;

	return true;
}

static int cmd_adc_meas_take_vbat(const struct shell *shell, size_t argc, char **argv)
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

static bool adc_ntc_temperatures_recalculate(const struct shell *shell,
					     temp_recalculate_data_t const *data)
{
	int16_t temperature;
	uint32_t resistance;

	npmx_error_t err_code = data->get_resistance_func(data->charger_instance, &resistance);
	if (!check_error_code(shell, err_code)) {
		print_get_error(shell, "old resistance");
		return false;
	}
	if (!ntc_resistance_to_temperature(shell, data->ntc_config_old, resistance, &temperature)) {
		print_convert_error(shell, "old resistance", "temperature");
		return false;
	}
	if (!ntc_temperature_to_resistance(shell, data->ntc_config_new, temperature, &resistance)) {
		print_convert_error(shell, "temperature", "new resistance");
		return false;
	}
	err_code = data->set_resistance_func(data->charger_instance, resistance);

	if (!check_error_code(shell, err_code)) {
		print_set_error(shell, "new resistance");
		return false;
	}

	return true;
}

static int adc_ntc_set(const struct shell *shell, size_t argc, char **argv,
		       adc_ntc_config_param_t config_type)
{
	char *config_name;
	bool expect_disabled;
	switch (config_type) {
	case ADC_NTC_CONFIG_PARAM_TYPE:
		config_name = "NTC type";
		expect_disabled = true;
		break;
	case ADC_NTC_CONFIG_PARAM_BETA:
		config_name = "NTC beta";
		expect_disabled = false;
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

	npmx_charger_t *charger_instance =
		charger_instance_get(shell, "NTC config", expect_disabled);
	if (charger_instance == NULL) {
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

	npmx_adc_ntc_config_t ntc_config_old;
	memcpy(&ntc_config_old, &ntc_config, sizeof(npmx_adc_ntc_config_t));

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

		if ((ntc_config.type == NPMX_ADC_NTC_TYPE_HI_Z) ||
		    (ntc_config_old.type == NPMX_ADC_NTC_TYPE_HI_Z) || (ntc_config_old.beta == 0)) {
			break;
		}

		/* Recalculate threshold temperatures. */
		temp_recalculate_data_t recalculate_data = {
			.charger_instance = charger_instance,
			.ntc_config_old = &ntc_config_old,
			.ntc_config_new = &ntc_config,
			.get_resistance_func = npmx_charger_cold_resistance_get,
			.set_resistance_func = npmx_charger_cold_resistance_set
		};
		if (!adc_ntc_temperatures_recalculate(shell, &recalculate_data)) {
			print_recalculate_error(shell, "cold");
			break;
		}

		recalculate_data.get_resistance_func = npmx_charger_cool_resistance_get;
		recalculate_data.set_resistance_func = npmx_charger_cool_resistance_set;
		if (!adc_ntc_temperatures_recalculate(shell, &recalculate_data)) {
			print_recalculate_error(shell, "cool");
			break;
		}

		recalculate_data.get_resistance_func = npmx_charger_warm_resistance_get;
		recalculate_data.set_resistance_func = npmx_charger_warm_resistance_set;
		if (!adc_ntc_temperatures_recalculate(shell, &recalculate_data)) {
			print_recalculate_error(shell, "warm");
			break;
		}

		recalculate_data.get_resistance_func = npmx_charger_hot_resistance_get;
		recalculate_data.set_resistance_func = npmx_charger_hot_resistance_set;
		if (!adc_ntc_temperatures_recalculate(shell, &recalculate_data)) {
			print_recalculate_error(shell, "hot");
			break;
		}
		shell_info(shell, "Info: NTC thresholds recalculated successfully.");

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

/* Creating subcommands (level 4 command) array for command "adc take meas". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_meas_take,
			       SHELL_CMD(vbat, NULL, "Read battery voltage",
					 cmd_adc_meas_take_vbat),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 3 command) array for command "adc meas". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_meas,
			       SHELL_CMD(take, &sub_adc_meas_take, "Take ADC measurement", NULL),
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

/* Creating subcommands (level 3 command) array for command "adc ntc". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc_ntc,
			       SHELL_CMD(beta, &sub_adc_ntc_beta, "ADC NTC beta", NULL),
			       SHELL_CMD(type, &sub_adc_ntc_type, "ADC NTC type", NULL),
			       SHELL_SUBCMD_SET_END);

/* Creating subcommands (level 2 command) array for command "adc". */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_adc, SHELL_CMD(meas, &sub_adc_meas, "ADC measurement", NULL),
			       SHELL_CMD(ntc, &sub_adc_ntc, "ADC NTC value", NULL),
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
