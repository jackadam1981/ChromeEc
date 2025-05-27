# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

zephyr_library_sources_ifdef(CONFIG_FP_FPC_BEP_ALGORITHM fpc_bep_bio_alg.c)
zephyr_library_sources_ifdef(CONFIG_FP_FPC_LIBFP_ALGORITHM fpc_libfp_bio_alg.c)
zephyr_library_sources_ifdef(CONFIG_FP_FT9865_ALGORITHM ft9865_bio_alg.c)

find_library(FOCAL_ALGO_LIB zb_locker_library ${CMAKE_CURRENT_SOURCE_DIR} NO_DEFALUT_PATH)

target_link_libraries(app PUBLIC  ${FOCAL_ALGO_LIB})			
target_link_libraries(app PRIVATE m)
