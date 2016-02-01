/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Functions needed by keyboard scanner module for Chrome EC */

#include "chip/npcx/keyboard_raw.h"

#include "common.h"
#include "compile_time_macros.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"
#include "clock.h"
#include "gpio.h"
#include "registers.h"
#include "task.h"
#include "util.h"

#define OFFSET_MASK(size, position) (((1 << (size)) - 1) << (position))

#define KSI_MASK OFFSET_MASK(KEYBOARD_ROWS, 0)
#define KSO_MASK OFFSET_MASK(KEYBOARD_COLS, CONFIG_KEYBOARD_KSO_BASE)

/*
 * Make sure that the row and column masks are actually subsets of the masks
 * for the total available rows and columns.
 */
BUILD_ASSERT((KSI_MASK & KB_ROW_MASK) == KSI_MASK);
BUILD_ASSERT((KSO_MASK & KB_COL_MASK) == KSO_MASK);

#if defined(CONFIG_KEYBOARD_COL2_INVERTED)
/*
 * When column 2 is inverted, Nuvoton EC KBS outputs only support
 * open-drain. So we should change this pin to GPIO
 */
#define KSO_VALUE (1 << 2)
#else
#define KSO_VALUE 0
#endif

/*
 * Make sure that there are not bits set in KSO_VALUE outside of the valid
 * positions defined by KSO_MASK.
 */
BUILD_ASSERT((KSO_MASK & KSO_VALUE) == KSO_VALUE);

#define SET_MASKED(group, mask, value)			\
	NPCX_DEVALT(group) = ((NPCX_DEVALT(group)	\
			       & ((uint8_t)~(mask)))	\
			      | ((uint8_t)(value)))

static void config_pins(void)
{
	SET_MASKED(ALT_GROUP_7, KSI_MASK,       0);
	SET_MASKED(ALT_GROUP_8, KSO_MASK >>  0, KSO_VALUE >>  0);
	SET_MASKED(ALT_GROUP_9, KSO_MASK >>  8, KSO_VALUE >>  8);
	SET_MASKED(ALT_GROUP_A, KSO_MASK >> 16, KSO_VALUE >> 16);
}

static void switch_to_gpio(uint8_t alt_group, uint8_t alt_mask)
{
	uint8_t mask = 1;
	int     i;

	NPCX_DEVALT(alt_group) |= alt_mask;

	for (i = 0; i < 8; ++i) {
		if (alt_mask & mask) {
			uint8_t gpio_port;
			uint8_t gpio_mask;

			if (gpio_get_gpio_from_alt(alt_group,
						   mask,
						   &gpio_port,
						   &gpio_mask) == EC_SUCCESS)
				gpio_set_flags_by_mask(gpio_port,
						       gpio_mask,
						       GPIO_INPUT |
						       GPIO_PULL_UP);
			else
				ASSERT(0);
		}

		mask = mask << 1;
	}
}

void keyboard_hibernate(void)
{
	/*
	 * Set all KBSOUTs to GPIOs and switch their mode to input and pull-up.
	 * Otherwise pressing the keyboard matrix might cause some current
	 * leakage during hibernating.
	 */
	switch_to_gpio(ALT_GROUP_8, (uint8_t)(KSO_MASK >>  0));
	switch_to_gpio(ALT_GROUP_9, (uint8_t)(KSO_MASK >>  8));
	switch_to_gpio(ALT_GROUP_A, (uint8_t)(KSO_MASK >> 16));
}

/**
 * Initialize the raw keyboard interface.
 */
void keyboard_raw_init(void)
{
	/* Enable clock for KBS peripheral */
	clock_enable_peripheral(CGC_OFFSET_KBS, CGC_KBS_MASK,
			CGC_MODE_RUN | CGC_MODE_SLEEP);

	/* Ensure top-level interrupt is disabled */
	keyboard_raw_enable_interrupt(0);

	/* pull-up KBSIN 0-7 internally */
	NPCX_KBSINPU = 0xFF;

	/* Disable automatic scan mode */
	CLEAR_BIT(NPCX_KBSCTL, NPCX_KBSMODE);

	/* Disable automatic interrupt enable */
	CLEAR_BIT(NPCX_KBSCTL, NPCX_KBSIEN);

	/* Disable increment enable */
	CLEAR_BIT(NPCX_KBSCTL, NPCX_KBSINC);

	/* Set KBSOUT to zero to detect key-press */
	NPCX_KBSOUT0 = 0x00;
	NPCX_KBSOUT1 = 0x00;

	config_pins();

	/*
	 * Enable interrupts for the inputs.  The top-level interrupt is still
	 * masked off, so this won't trigger interrupts yet.
	 */

	/* Clear pending input sources used by scanner */
	NPCX_WKPCL(MIWU_TABLE_WKKEY, MIWU_GROUP_WKKEY) = 0xFF;

	/* Enable Wake-up Button */
	NPCX_WKEN(MIWU_TABLE_WKKEY, MIWU_GROUP_WKKEY) = 0xFF;

	/* Select high to low transition (falling edge) */
	NPCX_WKEDG(MIWU_TABLE_WKKEY, MIWU_GROUP_WKKEY) =  0xFF;

	/* Enable interrupt of WK KBS */
	keyboard_raw_enable_interrupt(1);
}

/**
 * Finish initialization after task scheduling has started.
 */
void keyboard_raw_task_start(void)
{
	/* Enable MIWU to trigger KBS interrupt */
	task_enable_irq(NPCX_IRQ_KSI_WKINTC_1);
}

/**
 * Drive the specified column low.
 */
test_mockable void keyboard_raw_drive_column(int col)
{
	/*
	 * Nuvoton Keyboard Scan IP supports 18x8 Matrix
	 * It also support automatic scan functionality
	 */
	uint32_t mask, col_out;

	/* Add support for CONFIG_KEYBOARD_KSO_BASE shifting */
	col_out = col + CONFIG_KEYBOARD_KSO_BASE;

	/* Drive all lines to high */
	if (col == KEYBOARD_COLUMN_NONE) {
		mask = KB_COL_MASK;
#ifdef CONFIG_KEYBOARD_COL2_INVERTED
		gpio_set_level(GPIO_KBD_KSO2, 0);
#endif
	}
	/* Set KBSOUT to zero to detect key-press */
	else if (col == KEYBOARD_COLUMN_ALL) {
		mask = 0;
#ifdef CONFIG_KEYBOARD_COL2_INVERTED
		gpio_set_level(GPIO_KBD_KSO2, 1);
#endif
	}
	/* Drive one line for detection */
	else {
#ifdef CONFIG_KEYBOARD_COL2_INVERTED
		if (col == 2)
			gpio_set_level(GPIO_KBD_KSO2, 1);
		else
			gpio_set_level(GPIO_KBD_KSO2, 0);
#endif
		mask = ((~(1 << col_out)) & KB_COL_MASK);
	}

	/* Set KBSOUT */
	NPCX_KBSOUT0 = (mask & 0xFFFF);
	NPCX_KBSOUT1 = ((mask >> 16) & 0x03);
}

/**
 * Read raw row state.
 * Bits are 1 if signal is present, 0 if not present.
 */
test_mockable int keyboard_raw_read_rows(void)
{
	/* Bits are active-low, so invert returned levels */
	return (~NPCX_KBSIN) & KB_ROW_MASK;
}

/**
 * Enable or disable keyboard interrupts.
 */
void keyboard_raw_enable_interrupt(int enable)
{
	if (enable)
		task_enable_irq(NPCX_IRQ_KSI_WKINTC_1);
	else
		task_disable_irq(NPCX_IRQ_KSI_WKINTC_1);
}

/*
 * Interrupt handler for the entire GPIO bank of keyboard rows.
 */
void keyboard_raw_interrupt(void)
{
	/* Clear pending input sources used by scanner */
	NPCX_WKPCL(MIWU_TABLE_WKKEY, MIWU_GROUP_WKKEY) = 0xFF;

	/* Wake the scan task */
	task_wake(TASK_ID_KEYSCAN);
}
DECLARE_IRQ(NPCX_IRQ_KSI_WKINTC_1, keyboard_raw_interrupt, 3);
