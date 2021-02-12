/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "panic.h"

#ifndef CONFIG_PLATFORM_EC_SOFTWARE_PANIC
#error "Must define CONFIG_PLATFORM_EC_SOFTWARE_PANIC"
#endif

void software_panic(uint32_t reason, uint32_t info)
{
	__asm__("mov " STRINGIFY(SOFTWARE_PANIC_INFO_REG) ", %0\n"
		"mov " STRINGIFY(SOFTWARE_PANIC_REASON_REG) ", %1\n"
		"bl exception_panic\n"
		: : "r"(info), "r"(reason));
	__builtin_unreachable();
}

void panic_set_reason(uint32_t reason, uint32_t info, uint8_t exception)
{
	struct panic_data * const pdata = get_panic_data_write();
	uint32_t *lregs;

	lregs = pdata->cm.regs;

	/* Setup panic data structure */
	memset(pdata, 0, CONFIG_PANIC_DATA_SIZE);
	pdata->magic = PANIC_DATA_MAGIC;
	pdata->struct_size = CONFIG_PANIC_DATA_SIZE;
	pdata->struct_version = 2;
	pdata->arch = PANIC_ARCH_CORTEX_M;

	/* Log panic cause */
	lregs[1] = exception;
	lregs[3] = reason;
	lregs[4] = info;
}

void panic_get_reason(uint32_t *reason, uint32_t *info, uint8_t *exception)
{
	struct panic_data * const pdata = panic_get_data();
	uint32_t *lregs;

	if (pdata && pdata->struct_version == 2) {
		lregs = pdata->cm.regs;
		*exception = lregs[1];
		*reason = lregs[3];
		*info = lregs[4];
	} else {
		*exception = *reason = *info = 0;
	}
}