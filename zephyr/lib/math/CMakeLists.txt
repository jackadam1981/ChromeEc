# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include(${ZEPHYR_PIGWEED_MODULE_DIR}/pw_build/pigweed.cmake)

pw_add_library(math.smoothing INTERFACE
  HEADERS
    public/cros/math/exp_smoothing.h
  PUBLIC_INCLUDES
    public
)

pw_add_library(math.iir_filter STATIC
  SOURCES
    iir_filter.c
  HEADERS
    public/cros/math/iir_filter.h
  PUBLIC_INCLUDES
    public
)

pw_add_library(math.iir_decimator STATIC
  SOURCES
    iir_decimator.c
  HEADERS
    public/cros/math/iir_decimator.h
  PUBLIC_INCLUDES
    public
  PRIVATE_DEPS
    math.iir_filter
)

pw_add_test(math.smoothing_test
  SOURCES
    exp_smoothing_test.cc
  PRIVATE_DEPS
    math.smoothing
    pw_unit_test.light
    pw_unit_test.logging_event_handler
)

pw_add_test(math.iir_filter_test
  SOURCES
    iir_filter_test.cc
  PRIVATE_DEPS
    math.iir_filter
    pw_unit_test.light
    pw_unit_test.logging_event_handler
)

pw_add_test(math.iir_decimator_test
  SOURCES
    iir_decimator_test.cc
  PRIVATE_DEPS
    math.iir_decimator
    pw_unit_test.light
    pw_unit_test.logging_event_handler
)
