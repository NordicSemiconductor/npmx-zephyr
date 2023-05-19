#
# Copyright (c) 2023 Nordic Semiconductor
#
# SPDX-License-Identifier: BSD-3-Clause
#

# We need to cache the build type so that it is not out of sync when CONF_FILE
# is cached.
set(CONF_FILE_BUILD_TYPE ${CONF_FILE_BUILD_TYPE} CACHE INTERNAL "The build type")

zephyr_include_directories(include)

add_subdirectory(drivers)
