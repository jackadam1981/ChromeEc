# SPDX-License-Identifier: Apache-2.0

zephyr_library_include_directories(../../../extra/um_ppm)

zephyr_library_sources_ifdef(CONFIG_UCSI_PPM
	ucsi_host_cmd.c)
zephyr_library_sources_ifdef(CONFIG_UCSI_PPM
	platform_zephyr.c)
zephyr_library_sources_ifdef(CONFIG_UCSI_PPM
	../../../extra/um_ppm/ppm_common.c)
