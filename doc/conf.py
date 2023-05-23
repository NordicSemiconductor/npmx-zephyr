#!/usr/bin/env python3
#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: BSD-3-Clause
#

# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import os
from pathlib import Path
import sys

# Paths ------------------------------------------------------------------------

NPMX_ZEPHYR_BASE = Path(__file__).absolute().parents[1]

ZEPHYR_BASE = Path(__file__).absolute().parents[2] / "zephyr"

# General configuration --------------------------------------------------------

# The root document.
root_doc = 'index'

project = 'npmx-zephyr'
copyright = '2023, Nordic Semiconductor'
author = 'Nordic Semiconductor'
version = release = "0.6.0"

sys.path.insert(0, str(NPMX_ZEPHYR_BASE / "doc" / "_extensions"))
sys.path.insert(0, str(ZEPHYR_BASE / "doc" / "_extensions"))

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.

extensions = [
    "table_from_rows",
    "zephyr.external_content",
]

rst_epilog = """
.. include:: /links.txt
.. include:: /shortcuts.txt
"""

# Options for HTML output ------------------------------------------------------

html_theme = "sphinx_ncs_theme"
html_static_path = [str(NPMX_ZEPHYR_BASE / "doc" / "_static")]
html_last_updated_fmt = "%b %d, %Y"
html_show_sourcelink = True
html_show_sphinx = False

# Options for external_content -------------------------------------------------

external_content_contents = [
    (NPMX_ZEPHYR_BASE / "doc" , "*"),
    (NPMX_ZEPHYR_BASE, "samples/**/*.rst"),
]

# Options for table_from_rows --------------------------------------------------

table_from_rows_base_dir = NPMX_ZEPHYR_BASE
table_from_sample_yaml_board_reference = "/includes/sample_board_rows.txt"

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ["venv"]
