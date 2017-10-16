#!/bin/bash
#
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
set -u

if [[ $# == 0 ]]; then
	echo "Usage: build_vpd <input ec.bin> <brd> <oem> <sku> <output ec.bin>"
	exit 0;
fi

ec_bin=$1
brd_id=$2
oem_id=$3
sku_id=$4
ec_out=$5

vpd=/tmp/vpd.bin

printf '\x00\x00\x00\x00' | xxd | xxd -r > $vpd
printf $brd_id | xxd | xxd -r >> $vpd
printf $oem_id | xxd | xxd -r >> $vpd
printf $sku_id | xxd | xxd -r >> $vpd

# Store VPD
futility load_fmap -o $ec_out $ec_bin RO_VPD:$vpd
