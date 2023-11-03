/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "debug.h"
#include "flash-f.h"

__override void debugger_disable_on_boot(void)
{
	/*
	 * This logic must not have any dependencies on sub-systems that would
	 * require initialization, like gpio.
	 */
	if (is_flash_rdp_enabled()) {
		debugger_disable();
	}
}
