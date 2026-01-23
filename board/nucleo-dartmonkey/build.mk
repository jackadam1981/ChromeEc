# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

BASEBOARD:=nucleo-h743zi

board-y=board.o
board-y+=fpsensor_detect.o

# Enable on device tests
test-list-y=\
       abort \
       boringssl_crypto \
       crc \
       debug \
       exception \
       flash_physical \
       flash_write_protect \
       fpsensor_crypto \
       fpsensor_hw \
       libc_printf \
       mpu \
       null_pointer \
       rng_benchmark \
       rollback \
       rollback_entropy \
       rsa3 \
       rtc \
       scratchpad \
       stdlib \
       timer_dos \
       utils \
       utils_str \
       watchdog \
