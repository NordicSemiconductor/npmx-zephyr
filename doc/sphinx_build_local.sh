#!/usr/bin/env bash
#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: BSD-3-Clause
#

set -e

python3 -m venv venv
source ./venv/bin/activate

pip install wheel
pip install -r $(pwd)/requirements-doc.txt

sphinx-build -M html . build
