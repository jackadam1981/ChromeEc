# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

zephyr_library_sources(
  dsp_service.cc
  i2c_target_pio_funcs.cc
)
zephyr_library_include_directories(include)
zephyr_library_sources_ifdef(CONFIG_I2C_TARGET_BUFFER_MODE i2c_target_buffer_funcs.cc)
zephyr_library_link_libraries(
  pw_bytes
  pw_function
  pw_polyfill
  pw_preprocessor
  pw_result
  pw_status
  pw_transport.service
)
