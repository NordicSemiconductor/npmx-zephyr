/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ZEPHYR_DRIVERS_SHELL_COMMON_H__
#define ZEPHYR_DRIVERS_SHELL_COMMON_H__

#include <npmx.h>
#include <npmx_adc.h>
#include <npmx_charger.h>
#include <zephyr/shell/shell.h>

/* Due to the bug in zephyr/shell/shell.h, it is required to override macro implementation. */
/* It should be removed after the next Zephyr release with the fix. */
/* https://github.com/zephyrproject-rtos/zephyr/pull/64455 */
#undef SHELL_SUBCMD_ADD

// clang-format off
#define SHELL_SUBCMD_ADD(_parent, _syntax, _subcmd, _help, _handler, _mand, _opt)                  \
	static const struct shell_static_entry Z_SHELL_SUBCMD_NAME(__DEBRACKET _parent, _syntax)   \
		__attribute__((section("." STRINGIFY(Z_SHELL_SUBCMD_NAME(                          \
			NUM_VA_ARGS_LESS_1 _parent, __DEBRACKET _parent, _syntax)))))              \
		__attribute__((used)) =                                                            \
			SHELL_EXPR_CMD_ARG(1, _syntax, _subcmd, _help, _handler, _mand, _opt)
// clang-format on

/** @brief Max supported number of shell arguments. */
#define SHELL_ARG_MAX_COUNT 3U

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

bool check_error_code(const struct shell *shell, npmx_error_t err_code);

void print_value(const struct shell *shell, int value, unit_type_t unit_type);

void print_success(const struct shell *shell, int value, unit_type_t unit_type);

void print_hint_error(const struct shell *shell, int32_t index, const char *str);

void print_set_error(const struct shell *shell, const char *str);

void print_get_error(const struct shell *shell, const char *str);

void print_convert_error(const struct shell *shell, const char *src, const char *dst);

bool arguments_check(const struct shell *shell, size_t argc, char **argv, args_info_t *args_info);

bool range_check(const struct shell *shell, int32_t value, int32_t min, int32_t max,
		 const char *name);

bool check_pin_configuration_correctness(const struct shell *shell, int8_t gpio_index);

npmx_instance_t *npmx_instance_get(const struct shell *shell);

bool check_instance_index(const struct shell *shell, const char *instance_name, uint32_t index,
			  uint32_t max_index);

#endif /* ZEPHYR_DRIVERS_SHELL_COMMON_H__ */
