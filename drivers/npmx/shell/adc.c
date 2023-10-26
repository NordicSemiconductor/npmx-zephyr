/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "shell_common.h"
#include <npmx_driver.h>
#include <math.h>

/** @brief NTC thermistor configuration parameter. */
typedef enum {
	ADC_NTC_CONFIG_PARAM_TYPE, /* Battery NTC type. */
	ADC_NTC_CONFIG_PARAM_BETA /* Battery NTC beta value. */
} adc_ntc_config_param_t;

npmx_adc_t *adc_instance_get(const struct shell *shell)
{
	npmx_instance_t *npmx_instance = npmx_instance_get(shell);

	return npmx_instance ? npmx_adc_get(npmx_instance, 0) : NULL;
}

npmx_charger_t *charger_instance_get(const struct shell *shell);

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

SHELL_SUBCMD_ADD((npmx), adc, &sub_adc, "ADC", NULL, 1, 0);
