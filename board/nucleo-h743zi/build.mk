# Copyright 2019 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Board specific files build

BASEBOARD:=nucleo-h743zi

board-y=board.o

# Enable on device tests
test-list-y=\
       boringssl \
       crc \
       printf \
       queue \
       rsa3 \
       rtc \
       sha256 \
       sha256_unrolled \
       stdlib \
