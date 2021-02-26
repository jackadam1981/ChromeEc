/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/cros_system.h>
#include <logging/log.h>

#include "watchdog.h"
#include "system.h"

LOG_MODULE_REGISTER(shim_npcx_system, LOG_LEVEL_ERR);

void interrupt_disable_all(void)
{
	__asm__("li t0, 0x800");
	__asm__("csrc mie, t0");
}

void chip_save_reset_flags(uint32_t flags)
{
	BRAM_RESET_FLAGS0 = flags >> 24;
	BRAM_RESET_FLAGS1 = (flags >> 16) & 0xff;
	BRAM_RESET_FLAGS2 = (flags >> 8) & 0xff;
	BRAM_RESET_FLAGS3 = flags & 0xff;
}

uint32_t chip_read_reset_flags(void)
{
	uint32_t flags = 0;

	flags |= BRAM_RESET_FLAGS0 << 24;
	flags |= BRAM_RESET_FLAGS1 << 16;
	flags |= BRAM_RESET_FLAGS2 << 8;
	flags |= BRAM_RESET_FLAGS3;

	return flags;
}

void system_reset(int flags)
{

	uint32_t save_flags;

	/* Disable interrupts to avoid task swaps during reboot */
	interrupt_disable_all();

	/* Handle saving common reset flags. */
	system_encode_save_flags(flags, &save_flags);

	/* Store flags to battery backed RAM. */
	chip_save_reset_flags(save_flags);

	/* If WAIT_EXT is set, then allow 10 seconds for external reset */
	if (flags & SYSTEM_RESET_WAIT_EXT) {
		int i;

		/* Wait 10 seconds for external reset */
		for (i = 0; i < 1000; i++) {
			watchdog_reload();
			udelay(10000);
		}
	}

	/*
	 * Writing invalid key to watchdog module triggers a soft or hardware
	 * reset. It depends on the setting of bit0 at ETWDUARTCR register.
	 */
	ETWCFG |= 0x20;
	EWDKEYR = 0x00;

	/* Spin and wait for reboot; should never return */
	while (1)
		;
}

static int chip_system_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	return 0;
}
SYS_INIT(chip_system_init, PRE_KERNEL_1, 50);

