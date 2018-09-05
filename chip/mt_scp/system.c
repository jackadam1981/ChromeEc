/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* System : hardware specific implementation */

#include "console.h"
#include "cpu.h"
#include "flash.h"
#include "hooks.h"
#include "host_command.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "util.h"
#include "version.h"
#include "watchdog.h"

/*
 * SCP_GPR[0] b15-b0  - scratchpad
 * SCP_GPR[0] b31-b16 - saved_flags
 */

int system_set_scratchpad(uint32_t value)
{
	/* Check if value fits in 16 bits */
	if (value & 0xffff0000)
		return EC_ERROR_INVAL;

	SCP_GPR[0] = (SCP_GPR[0] & 0xffff0000) | value;

	return EC_SUCCESS;
}

uint32_t system_get_scratchpad(void)
{
	return SCP_GPR[0] & 0xffff;
}

const char *system_get_chip_vendor(void)
{
	return "mtk";
}

const char *system_get_chip_name(void)
{
	/* Support only SCP_A for now */
	return "scp_a";
}

const char *system_get_chip_revision(void)
{
	return "";
}

void chip_pre_init(void)
{
}

static void scp_enable_tcm(void)
{
	/* Enable tightly coupled memory (TCM) */
	SCP_CLK_L1_SRAM_PD = 0;
	SCP_CLK_TCM_TAIL_SRAM_PD = 0;
	/* SCP CM4 mod */
	CM4_MODIFICATION = 3;
	CM4_DCM_FEATURE = 3;
}

static void scp_enable_pirq(void)
{
	/* Enable peripheral to SCP IRQ */
	SCP_INTC_IRQ_ENABLE = 0xFFFFFFF5;
	SCP_INTC_IRQ_ENABLE_MSB = 0xFFFFFFFF;
}

static void scp_enable_clock(void)
{
	/* Enable clock gate */
	SCP_CLK_GATE |= CG_DMA_CH3 | CG_DMA_CH2 | CG_DMA_CH1 | CG_DMA_CH0 |
			CG_I2C_M | CG_MAD_M;
}

static void scp_memmap_init(void)
{
	/*
	 * SCP addr    :  AP addr
	 * 0xA0000000     0x10000000
	 * 0xB0000000     0x20000000
	 * 0xC0000000     0x30000000
	 * 0x20000000     0x40000000
	 * 0x30000000     0x50000000
	 * 0x60000000     0x60000000
	 * 0x70000000     0x70000000
	 * 0x80000000     0x80000000
	 * 0xF0000000     0x90000000
	 *
	 * Default config, LARGE DRAM not active:
	 *   REG32(0xA0001F00) & 0x2000 != 0
	 */
	SCP_REMAP_CFG1 = 0x07060504;
	SCP_REMAP_CFG2 = 0x02010008;
	SCP_REMAP_CFG3 = 0x10000A03;
}

void system_pre_init(void)
{
	/* SRAM */
	scp_enable_tcm();
	/* Clock */
	scp_enable_clock();
	/* Peripheral IRQ */
	scp_enable_pirq();
	/* Init dram mapping */
	scp_memmap_init();
	/* Init inter processor communication */
	/* scp_ipi_init(); */
}

void system_reset(int flags)
{
	uint32_t save_flags = 0;

	/* Disable interrupts to avoid task swaps during reboot */
	interrupt_disable();

	/* Save current reset reasons if necessary */
	if (flags & SYSTEM_RESET_PRESERVE_FLAGS)
		save_flags = system_get_reset_flags() | RESET_FLAG_PRESERVED;

	if (flags & SYSTEM_RESET_LEAVE_AP_OFF)
		save_flags |= RESET_FLAG_AP_OFF;

	/* Remember that the software asked us to hard reboot */
	if (flags & SYSTEM_RESET_HARD)
		save_flags |= RESET_FLAG_HARD;

	/* Reset flags are 32-bits, but save only 16 bits. */
	ASSERT(!(save_flags >> 16));
	SCP_GPR[0] = (save_flags << 16) | (SCP_GPR[0] & 0xffff);

	if (flags & SYSTEM_RESET_HARD) {
		/* Disable watchdog */
		SCP_WDT_CFG = 10;
		/* Enable watchdog */
		SCP_WDT_CFG = SCP_WDT_ENABLE | 10;
		/* Reload to trigger watchdog restart */
		SCP_WDT_RELOAD = SCP_WDT_RELOAD_VALUE;
	} else {
		if (flags & SYSTEM_RESET_WAIT_EXT) {
			int i;

			/* Wait 10 seconds for external reset */
			for (i = 0; i < 1000; i++) {
				watchdog_reload();
				udelay(10000);
			}
		}
		/* SCB AIRCR reset */
		CPU_NVIC_APINT = 0x05fa0004;
	}
	/* Spin wait for chip to reboot */
	while (1)
		;
}

void system_hibernate(uint32_t seconds, uint32_t microseconds)
{
#ifdef CONFIG_HOSTCMD_PD
	host_command_pd_request_hibernate();
	msleep(100);
#endif /* CONFIG_HOSTCMD_PD */
	cflush();

	if (board_hibernate)
		board_hibernate();

	cprints(CC_SYSTEM, "hibernate not supported, so rebooting");
	cflush();
	system_reset(SYSTEM_RESET_HARD);
}

static void check_reset_cause(void)
{
	uint32_t flags = 0;
	uint32_t raw_reset_cause = SCP_GPR[1];

	/* Set state to power-on */
	SCP_PWRON_STATE = PWRON_DEFAULT;

	if ((raw_reset_cause & 0xffff0000) == PWRON_DEFAULT) {
		/* Reboot */
		if (raw_reset_cause & PWRON_WATCHDOG)
			flags |= RESET_FLAG_WATCHDOG;
		else if (raw_reset_cause & PWRON_RESET)
			flags |= RESET_FLAG_POWER_ON;
		else
			flags |= RESET_FLAG_OTHER;
	} else {
		/* Power lost restart */
		flags |= RESET_FLAG_POWER_ON;
	}
	system_set_reset_flags(SCP_GPR[0] >> 16);
	SCP_GPR[0] &= 0xffff;
}

int system_is_reboot_warm(void)
{
	const uint32_t cold_flags =
		RESET_FLAG_RESET_PIN |
		RESET_FLAG_POWER_ON  |
		RESET_FLAG_WATCHDOG  |
		RESET_FLAG_HARD      |
		RESET_FLAG_SOFT      |
		RESET_FLAG_HIBERNATE;

	check_reset_cause();

	return !(system_get_reset_flags() & cold_flags);
}

int system_get_bbram(enum system_bbram_idx idx, uint8_t *value)
{
	return EC_ERROR_INVAL;
}
