# Configuration file for the Sphinx documentation builder.

# -- Path setup --------------------------------------------------------------

import os
import sys
sys.path.insert(0, os.path.abspath('../../python-sessiond'))

# -- Project information -----------------------------------------------------

project = 'sessiond'
copyright = '2020, James Reed'
author = 'James Reed'

# -- General configuration ---------------------------------------------------

extensions = [
    'sphinx.ext.autodoc',
    'sphinx_markdown_builder',
]
