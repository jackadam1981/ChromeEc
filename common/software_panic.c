/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "software_panic.h"
#include <util.h>

#ifdef CONFIG_SOFTWARE_PANIC
const char * const panic_sw_reasons[] = {
	"PANIC_SW_DIV_ZERO",
	"PANIC_SW_STACK_OVERFLOW",
	"PANIC_SW_PD_CRASH",
	"PANIC_SW_ASSERT",
	"PANIC_SW_WATCHDOG",
	"PANIC_SW_RNG",
	"PANIC_SW_PMIC_FAULT",
	NULL,
};
#endif
