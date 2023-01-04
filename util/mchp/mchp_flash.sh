#!/bin/bash
#
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

dut-control uart1_baudrate:57600
sleep 1
dut-control uart1_baudrate
python3 CrisisRcvry_Utility_Final.py $1

sleep 1
dut-control cold_reset:on fw_up:off
sleep 1
dut-control cold_reset:off
