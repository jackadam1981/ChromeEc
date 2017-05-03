#!/bin/bash
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

rm -rf build

DEST=build/tigertool
DATE=$(date +"%Y%m%d")

mkdir -p ${DEST}
cp ../usb_serial/console.py ${DEST}
cp ../../../../../chroot/usr/bin/dfu-util ${DEST}
cp flash_dfu.sh ${DEST}
cp tigertool.py ${DEST}

cp -r ecusb ${DEST}
cp -r ../../../../../chroot/usr/lib64/python2.7/site-packages/usb ${DEST}
find ${DEST} -name "*.py[co]" | xargs rm
cp -r ../usb_serial ${DEST}

(cd build; tar -cvzf tigertool_${DATE}.tgz tigertool)
