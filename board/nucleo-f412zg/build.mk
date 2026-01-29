# Copyright 2020 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

BASEBOARD:=nucleo-f412zg

board-y=board.o

# Enable on device tests
test-list-y=\
       boringssl_crypto \
       crc \
       printf \
       queue \
       rsa3 \
       rtc \
       sha256 \
       sha256_unrolled \
       stdlib \
       stm32f_rtc \
