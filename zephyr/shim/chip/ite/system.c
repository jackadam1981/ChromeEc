/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/cros_bbram.h>
#include <logging/log.h>

#include "watchdog.h"
#include "system.h"

#define GET_BBRAM_OFFSET(node) \
	DT_PROP(DT_PATH(named_bbram_regions, node), offset)
#define GET_BBRAM_SIZE(node) DT_PROP(DT_PATH(named_bbram_regions, node), size)

LOG_MODULE_REGISTER(shim_ite_system, LOG_LEVEL_ERR);

const struct device *bbram_dev;

void interrupt_disable(void)
{
	/* bit11: disable MEIE */
	__asm__("li t0, 0x800");
	__asm__("csrc mie, t0");
}

void chip_save_reset_flags(uint32_t flags)
{
	if (bbram_dev == NULL) {
		LOG_ERR("bbram_dev doesn't binding");
		return;
	}

	cros_bbram_write(bbram_dev, GET_BBRAM_OFFSET(saved_reset_flags),
			 GET_BBRAM_SIZE(saved_reset_flags), (uint8_t *)&flags);
}

uint32_t chip_read_reset_flags(void)
{
	uint32_t flags = 0;

	if (bbram_dev == NULL) {
		LOG_ERR("bbram_dev doesn't binding");
		return 0;
	}

	cros_bbram_read(bbram_dev, GET_BBRAM_OFFSET(saved_reset_flags),
			GET_BBRAM_SIZE(saved_reset_flags), (uint8_t *)&flags);

	return flags;
}

void system_reset(int flags)
{
	uint32_t save_flags = 0;

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

int system_set_scratchpad(uint32_t value)
{
	if (bbram_dev == NULL) {
		LOG_ERR("bbram_dev doesn't binding");
		return 0;
	}

	cros_bbram_write(bbram_dev, GET_BBRAM_OFFSET(scratchpad),
			 GET_BBRAM_SIZE(scratchpad), (uint8_t *)&value);

	return EC_SUCCESS;
}

uint32_t system_get_scratchpad(void)
{
	uint32_t value = 0;

	if (bbram_dev == NULL) {
		LOG_ERR("bbram_dev doesn't binding");
		return 0;
	}

	cros_bbram_read(bbram_dev, GET_BBRAM_OFFSET(scratchpad),
			GET_BBRAM_SIZE(scratchpad), (uint8_t *)&value);

	return value;
}

static int chip_system_init(const struct device *unused)
{
	ARG_UNUSED(unused);

	bbram_dev = device_get_binding(DT_LABEL(DT_NODELABEL(bbram)));
	return 0;
}
SYS_INIT(chip_system_init, PRE_KERNEL_1, 50);
