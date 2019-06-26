/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "driver/kbexpander_it8801.h"
#include "gpio.h"
#include "hooks.h"
#include "irq_chip.h"
#include "i2c.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"
#include "registers.h"
#include "task.h"

#define CPRINTF(format, args...) cprintf(CC_KEYSCAN, format, ## args)

static int hook_init_done;

/*
 * Initialize the raw keyboard interface.
 */
void keyboard_raw_init(void)
{
	/* TODO(b:): The expander keyboard communicated with
	 * EC is through I2C, but the I2C is not initialized
	 * during initialization of the keyboard raw. So the
	 * hook task should be initialized first.
	 */
}

/*
 * Finish initialization after task scheduling has started.
 */
void keyboard_raw_task_start(void)
{
	gpio_clear_pending_interrupt(GPIO_SMB_INT);
	gpio_enable_interrupt(GPIO_SMB_INT);
}

/*
 * Drive the specified column low.
 */
test_mockable void keyboard_raw_drive_column(int col)
{
	int kso_val = 0;

	if (hook_init_done == 1) {
		/* Tri-state all outputs */
		if (col == KEYBOARD_COLUMN_NONE)
			/* KSO[21:18,12:11,6:0] pull high */
			kso_val = IT8801_REG_MASK_KSOSDIC |
				IT8801_REG_MASK_AKSOSC;
		/* Assert all outputs */
		else if (col == KEYBOARD_COLUMN_ALL)
			/* KSO[21:18,12:11,6:0] output low */
			kso_val = IT8801_REG_MASK_AKSOSC;
		/* Selected KSO output low, all others KSO pull high. */
		else {
			if (col <= 6 || col > 10)
				/* KSO[12:11,6:0] */
				kso_val = col;
			else if (col > 6 && col < 11)
				/* KSO[21:18] */
				kso_val = col + 11;
		}

		i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR,
			IT8801_REG_KSOMCR, kso_val);

		if (IS_ENABLED(CONFIG_KEYBOARD_COL2_INVERTED))
			/* KSO[2] is inverted. */
			i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR,
				IT8801_REG_KSOMCR, kso_val ^= 0x02);
	}
}

/*
 * Read raw row state.
 * Bits are 1 if signal is present, 0 if not present.
 */
test_mockable int keyboard_raw_read_rows(void)
{
	int data = 0;
	int ksieer = 0;

	if (hook_init_done == 1) {
		i2c_read8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR,
			IT8801_REG_KSIDR, &data);

		/* This register needs to write clear after reading data */
		i2c_read8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR,
			IT8801_REG_KSIEER, &ksieer);
		i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR,
			IT8801_REG_KSIEER, ksieer);

		/* Bits are active-low, so invert returned levels */
		return data ^ 0xff;
	} else
		return data;
}

/*
 * Enable or disable keyboard matrix scan interrupts.
 */
void keyboard_raw_enable_interrupt(int enable)
{
	if (enable) {
		gpio_clear_pending_interrupt(GPIO_SMB_INT);
		gpio_enable_interrupt(GPIO_SMB_INT);
	} else {
		gpio_disable_interrupt(GPIO_SMB_INT);
	}
}

/*
 * Interrupt handler for keyboard matrix scan interrupt.
 */
void keyboard_expander_interrupt(enum gpio_signal signal)
{
	gpio_clear_pending_interrupt(GPIO_SMB_INT);

	/* Wake the scan task */
	task_wake(TASK_ID_KEYSCAN);
}

/*
 * Initialize the raw keyboard interface.
 */
static void hook_keyboard_raw_init(void)
{
	hook_init_done = 1;

	/* Ensure top-level interrupt is disabled */
	keyboard_raw_enable_interrupt(0);

	/* Keyboard scan in edge event register */
	i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR, IT8801_REG_KSIEER,
		0xff);
	/* Keyboard scan in interrupt enable register */
	i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR, IT8801_REG_KSIIER,
		0xff);
	/* Gather KSI interrupt enable */
	i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR, IT8801_REG_GIECR,
		IT8801_REG_MASK_GKSIIE);
	/* Alert response enable */
	i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR, IT8801_REG_SMBCR,
		IT8801_REG_MASK_ARE);

	if (IS_ENABLED(CONFIG_KEYBOARD_COL2_INVERTED))
		/* KSO[2] is high, others are low. */
		i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR,
			IT8801_REG_KSOMCR,	IT8801_REG_MASK_KSOSDIC | 0x02);
	else
		/* KSO[21:18,12:11,6:0] pins low. */
		i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR,
			IT8801_REG_KSOMCR,	IT8801_REG_MASK_AKSOSC);

	/* GPIO alternate function switching(KSO[21:18]) */
	i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR, IT8801_REG_KSO18,
		IT8801_REG_MASK_FUNC2);
	i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR, IT8801_REG_KSO19,
		IT8801_REG_MASK_FUNC2);
	i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR, IT8801_REG_KSO20,
		IT8801_REG_MASK_FUNC2);
	i2c_write8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR, IT8801_REG_KSO21,
		IT8801_REG_MASK_FUNC2);

	/*
	 * After hook task is initialized, and then do
	 * the keyboard initialization.
	 */
	keyboard_scan_init();

	gpio_clear_pending_interrupt(GPIO_SMB_INT);
}
DECLARE_HOOK(HOOK_INIT, hook_keyboard_raw_init, HOOK_PRIO_INIT_I2C + 1);

static void dump_register(int reg)
{
	int rv;
	int data;

		CPRINTF("[%Xh] = ", reg);
		rv = i2c_read8(I2C_PORT_KB_EXPANDER, IT8801_REG_ADDR,
			reg, &data);
		if (!rv)
			CPRINTF("0x%02x\n", data);
		else
			CPRINTF("ERR (%d)\n", rv);
}

static int it8801_dump(int argc, char **argv)
{
	dump_register(IT8801_REG_KSIIER);
	dump_register(IT8801_REG_KSIEER);
	dump_register(IT8801_REG_KSIDR);
	dump_register(IT8801_REG_KSOMCR);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(it8801_dump, it8801_dump, "",
			"Dumps IT8801 registers");
