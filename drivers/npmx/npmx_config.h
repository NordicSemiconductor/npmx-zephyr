/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ZEPHYR_DRIVERS_NPMX_NPMX_CONFIG_H__
#define ZEPHYR_DRIVERS_NPMX_NPMX_CONFIG_H__

#if defined(NPM1300_ENG_C)
#include <npmx_config_npm1300_eng_c.h>
#else
#error "Unknown device."
#endif

#endif /* ZEPHYR_DRIVERS_NPMX_NPMX_CONFIG_H__ */
