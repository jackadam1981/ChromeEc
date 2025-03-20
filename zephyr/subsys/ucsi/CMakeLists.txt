# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_library_sources_ifdef(CONFIG_UCSI_PPM_EC_MODE_ENTRY ppm_am_policy_handler.c)
zephyr_library_sources(ppm_common.c)
zephyr_library_sources_ifdef(CONFIG_UCSI_PPM_CONSOLE ppm_console.c ppm_utils.c)
zephyr_library_sources(ppm_driver.c)
zephyr_library_sources(ucsi_host_cmd.c)

if(DEFINED CONFIG_UCSI_PPM_EC_MODE_ENTRY OR DEFINED CONFIG_UCSI_PPM_CONSOLE)
  zephyr_library_sources(ppm_utils.c)
endif()
