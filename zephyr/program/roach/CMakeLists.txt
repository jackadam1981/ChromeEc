# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

zephyr_library_sources(
		"src/fakes.c"
)

if(DEFINED CONFIG_BOARD_ROACH)
	project(roach)
endif()

if (DEFINED CONFIG_CROS_EC_RW)
	add_custom_target(key_extract
		COMMAND futility create
		        "${PLATFORM_EC_PROGRAM_DIR}/roach/dev_key.pem" "key"
		COMMENT "Extracting signing keys"
		VERBATIM
	)

	add_custom_target(sign_rw ALL
		COMMAND futility sign --type rwsig --prikey key.vbprik2
		        zephyr/zephyr.bin ec.sig
		COMMENT "Signing EC RW"
		DEPENDS zephyr_final key_extract
		VERBATIM
	)
endif()
