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
