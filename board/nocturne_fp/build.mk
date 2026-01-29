# Copyright 2017 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

# the IC is STmicro STM32H743
CHIP:=stm32
CHIP_FAMILY:=stm32h7
CHIP_VARIANT:=stm32h7x3

# Don't forget that the board build.mk is included more than once to allow
# conditional variables to be realized. This means that we need to redefine all
# variable or the "+=" lines will compound.
board-y=
board-rw=ro_workarounds.o board_rw.o
board-ro=board_ro.o
board-rw+=fpsensor_detect_rw.o
board-y+=fpsensor_detect.o

# Do not build rsa test because this board uses RSA exponent 3 and the rsa test
# will fail on device.
test-list-y=\
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

# Note that this variable includes the trailing "/"
_nocturne_fp_cur_dir:=$(dir $(lastword $(MAKEFILE_LIST)))
-include $(_nocturne_fp_cur_dir)../../../ec-private/board/nocturne_fp/build.mk
