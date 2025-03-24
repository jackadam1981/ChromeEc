#!/bin/bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script builds and signs an ISH main firmware
#
# The script copy ish_fw.bin from ec build directory as ish_main.bin, and
# run meu to append key and manifest, output file is ish_output.bin. Copy this
# to file system /lib/firmware/intel/ish with name specified by kernel driver.
#
# Siging key authentication is not required for Chrome SKU, the key appended
# is dummy key.
# run as ./sign_ish_main.sh <board>
# example: 
#
function die() {
        echo "$1"
        exit 1
}

echo "$1"

BOARD=$1;
MEU_BIN="./meu"

ISH_INPUT_BIN="../../build/zephyr/${BOARD}/output/ish_fw.bin"
ISH_OUTPUT_BIN="ish_output.bin"

if [ ! -f "${MEU_BIN}" ]; then
        die "MEU ${MEU_BIN} is missing"
fi
if [ ! -x "${MEU_BIN}" ]; then
        die "MEU ${MEU_BIN} must be executable"
fi

if [ ! -f "${ISH_INPUT_BIN}" ]; then
        die "File ${ISH_INPUT_BIN} not found."
fi

# Signing tool and config
MEU_XML="./meu_config.xml"
ISH_XML="./ish_partition.xml"

# Append dummy key and extensions
cp ${ISH_INPUT_BIN} ./ish_main.bin
${MEU_BIN} -f "${ISH_XML}" -o ${ISH_OUTPUT_BIN} \
        -cfg "${MEU_XML}" &>/dev/null || die "Failed to sign"
./insert0.py ${ISH_OUTPUT_BIN}

rm -f meu.log
rm -f ish_main.bin
