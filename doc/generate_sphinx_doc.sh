#!/usr/bin/env bash
#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: BSD-3-Clause
#

rm -rf build
sphinx-build -M html . build -w warnings_sphinx_npmx_zephyr.txt
