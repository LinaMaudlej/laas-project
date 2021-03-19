#!/usr/bin/env python

"""
setup.py file for laas module
"""

from distutils.core import setup, Extension


laas_module = Extension('_laas',
                           sources=['laas_wrap.cxx', 'laas.cc', 'isol.cc', 'ft3.cc', 'portmask.cc'],
                           )

setup (name = 'laas',
       version = '0.1',
       author      = "The Authors",
       description = """Links as a Service Allocation for Fat Tree""",
       ext_modules = [laas_module],
       py_modules = ["laas"],
       )
