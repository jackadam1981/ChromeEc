# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Command automation for Nordic nRF51 chip

proc flash_nrf51 {path offset} {
	reset halt;
	program $path $offset;
}

proc unprotect_nrf51 { } {
	reset halt;
	nrf51 mass_erase;
}
