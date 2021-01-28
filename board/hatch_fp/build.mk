# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

BASEBOARD:=bloonchipper

# Enable on device tests
test-list-y=\
       aes \
       compile_time_macros \
       crc \
       flash_physical \
       flash_write_protect \
       fpsensor \
       mpu \
       mutex \
       pingpong \
       rollback \
       rollback_entropy \
       rsa3 \
       rtc \
       scratchpad \
       sha256 \
       sha256_unrolled \
       stm32f_rtc \
       utils \
