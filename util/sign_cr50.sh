#!/bin/bash -e
#
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This ONLY works for Cr50 images, after running 'make BOARD=cr50'
#
# TODO:
#   * Add args to specify input RW elf files
#   * Add args to specify input RO elf/hex files
#   * Add args to specify output directory (and format?)
#   * Integrate with Makefiles
#   * Probably a bunch of other things too. I'll get to it eventually.

# Right now, this is just quick-and-dirty, and used only
sudo $HOME/bin/codesigner \
    --key=util/signer/cr50_rom0-dev-blsign.pem.pub \
    --input=build/cr50/RW/ec.RW.elf \
    --format=bin \
    --output=build/cr50/RW/ec.RW.flat.signed \
    -x util/signer/fuses.xml \
    -j util/signer/ec_RW-manifest-kevin_evt_1.json \
  && sudo chown $USER build/cr50/RW/ec.RW.flat.signed \
  && mv build/cr50/RW/ec.RW.flat.signed build/cr50/RW/ec.RW.flat

sudo $HOME/bin/codesigner \
    --key=util/signer/cr50_rom0-dev-blsign.pem.pub \
    --input=build/cr50/RW/ec.RW_B.elf \
    --format=bin \
    --output=build/cr50/RW/ec.RW_B.flat.signed \
    -x util/signer/fuses.xml \
    -j util/signer/ec_RW-manifest-kevin_evt_1.json \
  && sudo chown $USER build/cr50/RW/ec.RW_B.flat.signed \
  && mv build/cr50/RW/ec.RW_B.flat.signed build/cr50/RW/ec.RW_B.flat

arm-none-eabi-objcopy -I binary -O ihex \
    --change-addresses 0x44000 \
    build/cr50/RW/ec.RW.flat \
    build/cr50/RW/ec.RW.hex

arm-none-eabi-objcopy -I binary -O ihex \
    --change-addresses 0x84000 \
    build/cr50/RW/ec.RW_B.flat \
    build/cr50/RW/ec.RW_B.hex

cat build/cr50/RO/ec.RO.hex \
    build/cr50/RW/ec.RW.hex \
    build/cr50/RW/ec.RW_B.hex \
    | grep -v ':00000001FF' > build/cr50/ec.hex

arm-none-eabi-objcopy -I ihex -O binary \
    --gap-fill=0xff --pad-to=0xc0000 \
    build/cr50/ec.hex build/cr50/ec.bin

echo "Done"
ls -l build/cr50/ec.hex build/cr50/ec.bin
