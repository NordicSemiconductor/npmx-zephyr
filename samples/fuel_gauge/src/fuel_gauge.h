/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __FUEL_GAUGE_H__
#define __FUEL_GAUGE_H__

#include <npmx_driver.h>

/**
 * @brief Function for initializing the fuel gauge module.
 *
 * @param[in] p_pm Pointer to the instance of PMIC device.
 *
 * @retval 0     On success.
 * @retval other Errno codes.
 */
int fuel_gauge_init(npmx_instance_t *const p_pm);

/**
 * @brief Function for periodically updating the fuel gauge module with battery voltage, current and temperature.
 *
 * @param[in] p_pm Pointer to the instance of PMIC device.
 *
 * @retval 0     On success.
 * @retval other Errno codes.
 */
int fuel_gauge_update(npmx_instance_t *const p_pm);

#endif /* __FUEL_GAUGE_H__ */
