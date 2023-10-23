#
# Copyright (c) 2022-2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: BSD-3-Clause
#

zephyr_library()

# One of NPMX devices have to be selected in Kconfig
if(CONFIG_NPMX_DEVICE_NPM1300)
    zephyr_compile_definitions(NPM1300)
else()
    return()
endif()

zephyr_compile_definitions_ifdef(CONFIG_NPMX_LOG_LEVEL_DBG NPMX_DEBUG)

set(SRC_DIR ${ZEPHYR_NPMX_MODULE_DIR}/drivers/src)
set(INC_DIR ${ZEPHYR_NPMX_MODULE_DIR}/drivers/include)

# Get backend files
zephyr_include_directories("${ZEPHYR_NPMX_MODULE_DIR}/backends")
zephyr_library_sources("${ZEPHYR_NPMX_MODULE_DIR}/backends/npmx_backend.c")

# Get glue and config files
zephyr_include_directories(.)

# Get drivers files
zephyr_include_directories(${INC_DIR})
zephyr_library_sources(${SRC_DIR}/npmx_adc.c)
zephyr_library_sources(${SRC_DIR}/npmx_buck.c)
zephyr_library_sources(${SRC_DIR}/npmx_charger.c)
zephyr_library_sources(${SRC_DIR}/npmx_common.c)
zephyr_library_sources(${SRC_DIR}/npmx_core.c)
zephyr_library_sources(${SRC_DIR}/npmx_errlog.c)
zephyr_library_sources(${SRC_DIR}/npmx_gpio.c)
zephyr_library_sources(${SRC_DIR}/npmx_ldsw.c)
zephyr_library_sources(${SRC_DIR}/npmx_led.c)
zephyr_library_sources(${SRC_DIR}/npmx_pof.c)
zephyr_library_sources(${SRC_DIR}/npmx_ship.c)
zephyr_library_sources(${SRC_DIR}/npmx_timer.c)
zephyr_library_sources(${SRC_DIR}/npmx_vbusin.c)

zephyr_library_sources(npmx_driver.c)

if(CONFIG_NPMX_SHELL)
    zephyr_include_directories(shell)

    zephyr_library_sources(npmx_shell.c)
    zephyr_library_sources(shell/npmx_shell_common.c)
endif()

# Get npmx include file
zephyr_include_directories(${ZEPHYR_NPMX_MODULE_DIR})
