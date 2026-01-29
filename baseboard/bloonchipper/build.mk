# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

# the IC is STmicro STM32F412
CHIP := stm32
CHIP_FAMILY := stm32f4
CHIP_VARIANT := stm32f412

# Don't forget that the board build.mk is included more than once to allow
# conditional variables to be realized. This means that we need to redefine all
# variable or the "+=" lines will compound.
baseboard-rw = base_board_rw.o
baseboard-y = base_board.o
baseboard-y += fpsensor_detect.o
baseboard-rw += fpsensor_detect_rw.o

# Do not build rsa test because this board uses RSA exponent 3 and the rsa test
# will fail on device.
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
-include ../ec-private/board/hatch_fp/build.mk
