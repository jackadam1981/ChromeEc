# -*- makefile -*-
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Helipilot baseboard specific files build
#

CHIP := npcx
CHIP_FAMILY := npcx9
CHIP_VARIANT := npcx9mfp

baseboard-y += base_board.o
baseboard-rw += base_board_rw.o
baseboard-y += fpsensor_detect.o
baseboard-rw += fpsensor_detect_rw.o

# Do not build rsa test because this board uses RSA exponent 3 and the rsa test
# will fail on device.
# TODO(b/314131510): Fix stm32f_rtc test (or create a new one) for helipilot
test-list-y = \
       boringssl_crypto \
       crc \
       fpsensor_auth_crypto_stateful \
       fpsensor_auth_crypto_stateless \
       fpsensor_crypto \
       printf \
       queue \
       rsa3 \
       rtc \
       sha256 \
       sha256_unrolled \
       stdlib \

# This is relative to the EC root directory.
ifneq ($(BOARD_BUCCANEER),y)
-include ../ec-private/board/helipilot/build.mk
endif
