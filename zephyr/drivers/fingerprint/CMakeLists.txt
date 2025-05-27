# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_library_sources_ifdef(CONFIG_FINGERPRINT_SENSOR_EGIS630	fingerprint_egis630.c)
zephyr_library_sources_ifdef(CONFIG_FINGERPRINT_SENSOR_EGIS630_PAL
			     fingerprint_egis630_pal.c)
zephyr_library_sources_ifdef(CONFIG_FINGERPRINT_SENSOR_FPC1025	fingerprint_fpc1025.c)
zephyr_library_sources_ifdef(CONFIG_FINGERPRINT_SENSOR_FPC1025_PAL
			     fingerprint_fpc1025_pal.c)
zephyr_library_sources_ifdef(CONFIG_FINGERPRINT_SENSOR_FPC1145	fingerprint_fpc1145.c)
zephyr_library_sources_ifdef(CONFIG_FINGERPRINT_SENSOR_FPC1145_PAL
			     fingerprint_fpc1145_pal.c)
zephyr_library_sources_ifdef(CONFIG_FINGERPRINT_SENSOR_SIMULATOR
			     fingerprint_sensor_sim.c)
zephyr_library_sources_ifdef(CONFIG_FINGERPRINT_SENSOR_FT9865
			     fingerprint_ft9865.c)
zephyr_library_sources_ifdef(CONFIG_FINGERPRINT_SENSOR_FT9865
			     fingerprint_ft9865_pal.c)		

find_library(FOCAL_SENSOR_LIB chips93xx ${CMAKE_CURRENT_SOURCE_DIR} NO_DEFALUT_PATH)

target_link_libraries(app PUBLIC  ${FOCAL_SENSOR_LIB})			 
