#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: BSD-3-Clause
#

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pmic_buck_pins)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
