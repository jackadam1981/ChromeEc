#!/bin/bash
#
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

python3 MakePgmHdr.py
sleep 1
dut-control cold_reset:on fw_up:on
sleep 1
dut-control cold_reset:off
sleep 1
dut-control uart1_baudrate:9600
sleep 1
dut-control uart1_baudrate
python3 UART_CR_V2.py $1
