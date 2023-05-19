/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ZEPHYR_DRIVERS_NPMX_NPMX_GLUE_H__
#define ZEPHYR_DRIVERS_NPMX_NPMX_GLUE_H__

#include <npmx.h>
#include <zephyr/sys/__assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup npmx_glue npmx_glue.h
 * @{
 * @ingroup npmx
 *
 * @brief This file contains functions and macros' declarations that should be implemented
 *        according to the needs of the host environment into which @em npmx is integrated.
 */

/**
 * @brief Macro for placing a runtime assertion.
 *
 * @param expression Expression to be evaluated.
 */
#define NPMX_ASSERT(expression) __ASSERT_NO_MSG(expression)

/**
 * @brief Macro for placing a compile time assertion.
 *
 * @param expression Expression to be evaluated.
 */
#define NPMX_STATIC_ASSERT(expression) BUILD_ASSERT(expression)

/**
 * @brief Function for writing I2C.
 *
 * @param[in] p_inst           Pointer to the backend instance.
 * @param[in] register_address Register address in npmx device to be modified.
 * @param[in] p_data           Pointer to data to be written.
 * @param[in] num_of_bytes     Number of bytes of data to be written.
 *
 * @retval NPMX_SUCCESS  Writing I2C was successful.
 * @retval NPMX_ERROR_IO General input/output error.
 */
npmx_error_t npmx_backend_i2c_write(npmx_backend_instance_t const *p_inst,
				    uint16_t register_address, uint8_t *p_data,
				    uint32_t num_of_bytes);
/**
 * @brief Function for reading I2C.
 *
 * @param[in] p_inst           Pointer to the backend instance.
 * @param[in] register_address Register address in npmx device to be read.
 * @param[in] p_data           Pointer to buffer for read data.
 * @param[in] num_of_bytes     Number of bytes to be read.
 *
 * @retval NPMX_SUCCESS  Reading I2C was successful.
 * @retval NPMX_ERROR_IO General input/output error.
 */
npmx_error_t npmx_backend_i2c_read(npmx_backend_instance_t const *p_inst, uint16_t register_address,
				   uint8_t *p_data, uint32_t num_of_bytes);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_NPMX_NPMX_GLUE_H__ */
