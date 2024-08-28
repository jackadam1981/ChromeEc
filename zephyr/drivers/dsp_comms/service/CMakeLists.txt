# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_library(cros.dsp.service.transport_state_machine STATIC
  HEADERS
    include/cros/dsp/service/transport_state_machine.hh
  PUBLIC_INCLUDES
    include
  PUBLIC_DEPS
    pw_status
  SOURCES
    transport_state_machine.cc
)

pw_add_test(cros.dsp.service.transport_state_machine_test
  SOURCES
    test_transport_state_machine.cc
  PRIVATE_DEPS
    cros.dsp.service.transport_state_machine
    pw_unit_test.light
    pw_unit_test.logging_event_handler
    zephyr_interface
    cros_ec_interface
    pw_function
)
