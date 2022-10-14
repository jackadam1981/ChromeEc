#!/bin/bash
#
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Usage 1: Enables BIST RF charging.
#   $ wlcutil.sh

ECTOOL=/usr/sbin/ectool

PCHG_PORT=0
ECTOOL_PCHG=${ECTOOL} pchg ${PCHG_PORT}

I2C_PORT=9
I2C_ADDR=0x28
ECTOOL_I2CXFER=${ECTOOL} i2cxfer ${I2C_PORT} ${I2C_ADDR}

read_message_payload() {
  ${ECTOOL_I2CXFER} $1
}

read_message_header() {
  ${ECTOOL_I2CXFER} 2
}

send_reset_to_normal() {
  ${ECTOOL_I2CXFER} 0 0x00 0x01 0x00
}

send_bist_rf_on() {
  ${ECTOOL_I2CXFER} 0 0x06 0x01 0x01
}

main() {
  # Enable pass-through mode.
  ${ECTOOL_PCHG} passthru on

  # Power-cycle the chip
  ${ECTOOL_PCHG} reset
  # 'Reset port 0 complete.'
  
  # Expect IRQ.
  
  # Read out event.
  read_message_header
  # 'Read bytes: 0x80 0x2' (EVT_RESET, payload_size=2)
  read_message_payload 2
  # 'Read bytes: 0x1 00' (mode=DOWNLOAD, reason=INTENDED)
  
  # Reset to normal mode.
  send_reset_to_normal
  read_message_header
  # 'Read bytes: 0x40 0x1' (RES_RESET, payload_size=1)
  read_message_payload 1
  # 'Read bytes: 00' (=SUCCESS)
  
  # Expect IRQ.
  
  # Read out event.
  read_message_header
  # Read bytes: 0x80 0x3 (EVT_RESET, size=3)
  read_message_payload 3
  # Read bytes: 00 0x11 0x4 (mode=NORMAL, fwver=0x1104)
  
  # Send BIST RF on
  send_bist_rf_on
  read_message_header
  # Read bytes: 0x46 0x1 (RES_BIST, size=1)
  read_message_payload 1
  # Read bytes: 0x0 (=SUCCESS)
}