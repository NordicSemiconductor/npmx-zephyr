/*
 * Copyright (c) 2022-2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ZEPHYR_DRIVERS_NPMX_NPMX_CONFIG_H__
#define ZEPHYR_DRIVERS_NPMX_NPMX_CONFIG_H__

#if defined(NPM1300)
#include <npmx_config_npm1300.h>
#elif defined(NPM1304)
#include <npmx_config_npm1304.h>
#else
#error "Unknown device."
#endif

#endif /* ZEPHYR_DRIVERS_NPMX_NPMX_CONFIG_H__ */
