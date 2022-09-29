# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.20)

project(example)

add_library(example
        example.cc
        )

target_include_directories(example PUBLIC
        ${PROJECT_SOURCE_DIR})
