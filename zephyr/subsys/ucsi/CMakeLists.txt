# SPDX-License-Identifier: Apache-2.0

zephyr_library_include_directories(../../../extra/um_ppm)

zephyr_library_sources_ifdef(CONFIG_UCSI_PPM
	ucsi_host_cmd.c)