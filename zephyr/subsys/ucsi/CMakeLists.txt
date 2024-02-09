# SPDX-License-Identifier: Apache-2.0

zephyr_library_include_directories(${PLATFORM_EC}/extra/um_ppm)

zephyr_library_sources(ppm_driver.c)
zephyr_library_sources(ucsi_host_cmd.c)
zephyr_library_sources(platform_zephyr.c)
zephyr_library_sources(${PLATFORM_EC}/extra/um_ppm/ppm_common.c)
