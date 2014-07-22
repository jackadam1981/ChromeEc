# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

export BOARD := discovery-stm32f072

BUILD_DIR := ../../build/$(BOARD)

#
# Build just the firmware for this board
#
.PHONY: build

build:
	$(MAKE) -C ../.. -j

#
# Clean just this boards build and temporary files
#
.PHONY: clean

clean:
	rm -rf $(BUILD_DIR) *~

#
# Simple flashing command to reflash the discovery board using openocd
#
.PHONY: flash

flash:
	sudo openocd -f board/stm32f0discovery.cfg \
		-c "gdb_port 0" \
		-c "tcl_port 0" \
		-c "telnet_port 0" \
		-c "init" \
		-c "reset init" \
		-c "flash write_image erase $(BUILD_DIR)/ec.bin 0x08000000" \
		-c "reset halt" \
		-c "resume" \
		-c "shutdown"
