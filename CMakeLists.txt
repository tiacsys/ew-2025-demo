# Copyright (c) 2021-2023 TiaC Systems
# Copyright (c) 2019-2021 Li-Pro.Net
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# Standard Bridle application boilerplate, includes Zephyr.
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project("stepper demo" VERSION 0.1)

# Add your source file to the "app" target. This must come after
# the boilerplate code, which defines the target.
target_sources(app PRIVATE src/main.c)
