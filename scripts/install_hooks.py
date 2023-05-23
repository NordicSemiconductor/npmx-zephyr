#!/usr/bin/env python3

# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: BSD-3-Clause

import os

def main():
    os.system("pip install -r requirements-format.txt")
    os.system("pre-commit install")
    os.system("pre-commit run --all-files")

if __name__ == "__main__":
    main()
