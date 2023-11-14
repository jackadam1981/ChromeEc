# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS "${ZEPHYR_BASE}")

set(CRYPTOC ../../../../../third_party/cryptoc/)

zephyr_library_sources(
		"src/fakes.c"
		"src/one_wire.c"
)

zephyr_library_sources(
		"../../../../common/curve25519.c"
		"../../../../common/curve25519-generic.c"
)

zephyr_library_sources(
#		"${CRYPTOC}/hmac.c"
#		"${CRYPTOC}/md5.c"
#		"${CRYPTOC}/p256.c"
#		"${CRYPTOC}/p256_ec.c"
#		"${CRYPTOC}/p256_ecdsa.c"
#		"${CRYPTOC}/p256_prng.c"
#		"${CRYPTOC}/sha.c"
#		"${CRYPTOC}/sha224.c"
#		"${CRYPTOC}/sha256.c"
		"${CRYPTOC}/util.c"
)

zephyr_include_directories(
		"include"
		"${CRYPTOC}/include"
)

if(DEFINED CONFIG_BOARD_ROACH)
	project(roach)
endif()
