# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_sources(app PRIVATE src/usbc_dp_mode.c)
target_sources(app PRIVATE src/usbc_dp_mode_svdm_ver_20.c)
target_sources(app PRIVATE src/usbc_dp_mode_svdm_ver_21.c)
