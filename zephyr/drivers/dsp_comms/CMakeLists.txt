# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)
include($ENV{PW_ROOT}/pw_protobuf_compiler/proto.cmake)

set(PW_TRANSPORT_STATUS_FLAGS_SIZE 1 CACHE STRING "")
configure_file(proto/transport.options.in proto/transport.options @ONLY)
file(
  COPY
    "${CMAKE_CURRENT_SOURCE_DIR}/proto/transport.proto"
  DESTINATION
    "${CMAKE_CURRENT_BINARY_DIR}/proto"
)

pw_proto_library(pw_transport.proto
  SOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/proto/transport.proto"
  INPUTS
    "${CMAKE_CURRENT_BINARY_DIR}/proto/transport.options"
  STRIP_PREFIX
    "${CMAKE_CURRENT_BINARY_DIR}"
  PREFIX
    "pw_transport"
  DEPS
    pw_third_party.nanopb.proto
)

pw_proto_library(dsp_comms.proto
  SOURCES
    proto/ec_dsp.proto
  INPUTS
    proto/ec_dsp.options
  DEPS
    pw_third_party.nanopb.proto
)


zephyr_include_directories(include)
zephyr_link_libraries(dsp_comms.proto.nanopb pw_transport.proto.nanopb)

add_subdirectory_ifdef(CONFIG_PLATFORM_EC_DSP_SERVICE service)
zephyr_library_sources_ifdef(CONFIG_PLATFORM_EC_DSP_CLIENT  dsp_client.c)
if(CONFIG_PLATFORM_EC_CBI AND CONFIG_PLATFORM_EC_DSP_CLIENT_REMOTE_CBI)
    zephyr_library_sources(
        dsp_client_remote_cbi.c
        "${PLATFORM_EC}/common/cbi_common.c"
    )
endif()
if(CONFIG_PLATFORM_EC_DSP_SERVICE_REMOTE_LID_ANGLE)
  zephyr_library_sources("${PLATFORM_EC}/common/lid_angle_common.c")
endif()

pw_add_test(cros.dsp.test
  SOURCES
    test_comms.cc
  PRIVATE_DEPS
    pw_unit_test.light
    pw_unit_test.logging_event_handler
    dsp_comms.proto.nanopb
    zephyr_interface
    cros_ec_interface
)

# pw_proto_library(dsp_comms.service.proto
#   SOURCES
#     proto/service.proto
#   DEPS
#     pw_protobuf.common_proto
# )
# zephyr_link_libraries(dsp_comms.service.proto.pwpb)
