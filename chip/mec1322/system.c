/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* System module for Chrome EC : MEC1322 hardware specific implementation */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "host_command.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Indices for hibernate data registers (RAM backed by VBAT) */
enum hibdata_index {
	HIBDATA_INDEX_SCRATCHPAD = 0,    /* General-purpose scratchpad */
	HIBDATA_INDEX_SAVED_RESET_FLAGS  /* Saved reset flags */
};

static void check_reset_cause(void)
{
	uint32_t status = MEC1322_VBAT_STS;
	uint32_t flags = 0;

	/* Clear the reset causes now that we've read them */
	MEC1322_VBAT_STS |= status;

	if (status & (1 << 7))
		flags |= RESET_FLAG_POWER_ON;

	if (status & (1 << 5))
		flags |= RESET_FLAG_WATCHDOG;

	flags |= MEC1322_VBAT_RAM(HIBDATA_INDEX_SAVED_RESET_FLAGS);
	MEC1322_VBAT_RAM(HIBDATA_INDEX_SAVED_RESET_FLAGS) = 0;

	system_set_reset_flags(flags);
}

void system_pre_init(void)
{
	/* Enable direct NVIC */
	MEC1322_EC_INT_CTRL |= 1;

	/* Disable ARM TRACE debug port */
	MEC1322_EC_TRACE_EN &= ~1;

	/* Deassert nSIO_RESET */
	MEC1322_PCR_PWR_RST_CTL &= ~(1 << 0);

	check_reset_cause();
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

	MEC1322_VBAT_RAM(HIBDATA_INDEX_SAVED_RESET_FLAGS) = save_flags;

	/* Trigger watchdog in 1ms */
	MEC1322_WDG_LOAD = 1;
	MEC1322_WDG_CTL |= 1;

	/* Spin and wait for reboot; should never return */
	while (1)
		;
}

const char *system_get_chip_vendor(void)
{
	return "smsc";
}

const char *system_get_chip_name(void)
{
	switch (MEC1322_CHIP_DEV_ID) {
	case 0x15:
		return "mec1322";
	default:
		return "unknown";
	}
}

static char to_hex(int x)
{
	if (x >= 0 && x <= 9)
		return '0' + x;
	return 'a' + x - 10;
}

const char *system_get_chip_revision(void)
{
	static char buf[3];
	uint8_t rev = MEC1322_CHIP_DEV_REV;

	buf[0] = to_hex(rev / 16);
	buf[1] = to_hex(rev & 0xf);
	buf[2] = '\0';
	return buf;
}

int system_get_vbnvcontext(uint8_t *block)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int system_set_vbnvcontext(const uint8_t *block)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int system_set_scratchpad(uint32_t value)
{
	MEC1322_VBAT_RAM(HIBDATA_INDEX_SCRATCHPAD) = value;
	return EC_SUCCESS;
}

uint32_t system_get_scratchpad(void)
{
	return MEC1322_VBAT_RAM(HIBDATA_INDEX_SCRATCHPAD);
}

#include "gpio.h"

void unpower_gpio(void)
{
	int i, j, k;

	for (i = 0; i < 2; ++i)
		for(j = 0; j < 7; ++j)
			for (k = 0; k < 8; ++k) {
				if (i == 1 && j == 3 && k == 1)
					continue;
				if (i == 0 && j == 6 && k == 3)
					continue;
				if (i == 1 && j == 2 && k == 1)
					continue;
				if (i == 1 && j == 4 && k == 3)
					continue;
				if (i == 0 && j == 3 && k == 7)
					break;
				if (i == 1 && j == 3 && k == 7)
					break;
				if (i == 1 && j == 6 && k == 6)
					break;
				MEC1322_GPIO_CTL(10 * i + j, k) =
					(MEC1322_GPIO_CTL(10 * i + j, k) & ~0xff) | 0x48;
			}
}

void system_hibernate(uint32_t seconds, uint32_t microseconds)
{
	int i;

	MEC1322_WDG_CTL &= ~1;

	MEC1322_TMR32_CTL(0) &= ~1;
	MEC1322_TMR32_CTL(1) &= ~1;
	MEC1322_TMR16_CTL(0) &= ~1;

	MEC1322_PCR_CHIP_SLP_EN |= 0x3;
	MEC1322_PCR_EC_SLP_EN |= 0xe0700ff7;
	MEC1322_PCR_HOST_SLP_EN |= 0x5f003;
	MEC1322_PCR_SYS_SLP_CTL |= 0x5;
	MEC1322_PCR_EC_SLP_EN2 |= 0x1ffffff8;
	MEC1322_PCR_SLOW_CLK_CTL &= 0xfffffc00;
	CPU_SCB_SYSCTRL |= 0x4;

#define PREG(x) ccprintf(#x " = 0x%08x\n", x)
	PREG(MEC1322_PCR_CHIP_CLK_REQ);
	PREG(MEC1322_PCR_EC_CLK_REQ);
	PREG(MEC1322_PCR_HOST_CLK_REQ);
	PREG(MEC1322_PCR_EC_CLK_REQ2);
	cflush();

	MEC1322_UART_ACT &= ~0x1;
	MEC1322_LPC_ACT &= ~0x1;
	MEC1322_EC_ADC_VREF_PD |= 1;
	MEC1322_PCR_PROC_CLK_CTL = 48;

	/*unpower_gpio();*/
	MEC1322_EC_JTAG_EN &= ~1;
	MEC1322_VBAT_CE &= ~0x2;

	interrupt_disable();

	for (i = 8; i <= 23; ++i)
		MEC1322_INT_DISABLE(i) = 0xffffffff;
	MEC1322_INT_BLK_DIS |= 0xffff00;
	MEC1322_EC_INT_CTRL &= ~1;

	for (i = 0; i <= 92; ++i) {
		task_disable_irq(i);
		task_clear_pending_irq(i);
	}

	asm("wfi");

	gpio_set_level(GPIO_LED2, 0);
}
