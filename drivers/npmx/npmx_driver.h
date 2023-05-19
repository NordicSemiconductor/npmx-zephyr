/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ZEPHYR_DRIVERS_NPMX_NPMX_DRIVER_H__
#define ZEPHYR_DRIVERS_NPMX_NPMX_DRIVER_H__

#include <npmx_instance.h>
#include <npmx_core.h>

#include <zephyr/device.h>

/**
 * @brief Function for getting a pointer to the npmx instance.
 *
 * @param[in] p_dev Pointer to the nPM Zephyr device.
 *
 * @return Pointer to the npmx instance.
 */
npmx_instance_t *npmx_driver_instance_get(const struct device *p_dev);

/**
 * @brief Function for configuring power fail comparator and registering callback handler.
 *
 * @param[in] dev      Pointer to nPM Zephyr device.
 * @param[in] p_config Pointer to POF configuration structure.
 * @param[in] p_cb     Pointer to POF callback handler.
 *
 * @return Error code if error occurred, or 0 if succeeded.
 */
int npmx_driver_register_pof_cb(const struct device *dev, npmx_pof_config_t const *p_config,
				void (*p_cb)(npmx_instance_t *p_pm));

/**
 * @brief Function for getting POF pin index from nPM Zephyr device.
 *
 * @param[in] dev Pointer to nPM Zephyr device.
 *
 * @return POF pin index.
 */
int npmx_driver_pof_pin_get(const struct device *dev);

/**
 * @brief Function for getting interrupt pin index from nPM Zephyr device.
 *
 * @param[in] dev Pointer to nPM Zephyr device.
 *
 * @return Interrupt pin index.
 */
int npmx_driver_int_pin_get(const struct device *dev);

/**
 * @brief Function for getting reset pin index from nPM Zephyr device.
 *
 * @param[in] dev Pointer to nPM Zephyr device.
 *
 * @return Reset pin index.
 */
int npmx_driver_reset_pin_get(const struct device *dev);

#endif /* ZEPHYR_DRIVERS_NPMX_NPMX_DRIVER_H__ */
