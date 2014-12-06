/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Functions needed by keyboard scanner module for Chrome EC */

#include "common.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"
#include "clock.h"
#include "registers.h"
#include "task.h"

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

#if !(SUPPORT_JTAG) /* mark this for JEN0 debugging */
    /* pull-up KBSIN 0-7 internally */
	NUCMX_KBSINPU = 0xFF;
#endif

	/* Disable automatic scan mode */
    CLEAR_BIT(NUCMX_KBSCTL, NUCMX_KBSMODE);

    /* Disable automatic interrupt enable */
    CLEAR_BIT(NUCMX_KBSCTL, NUCMX_KBSIEN);

    /* Disable increment enable */
    CLEAR_BIT(NUCMX_KBSCTL, NUCMX_KBSINC);

    /* Set KBSOUT to zero to detect key-press */
    NUCMX_KBSOUT0 = 0x00;
    NUCMX_KBSOUT1 = 0x00;

	/*
	 * Enable interrupts for the inputs.  The top-level interrupt is still
	 * masked off, so this won't trigger interrupts yet.
	 */
#if !(SUPPORT_JTAG) /* mark this for JEN0 debugging */
	/* Clear pending input sources used by scanner */
	NUCMX_WKPCL(MIWU_TABLE_WKKEY,MIWU_GROUP_WKKEY) = 0xFF;

	/* Enable Wake-up Button */
	NUCMX_WKEN(MIWU_TABLE_WKKEY, MIWU_GROUP_WKKEY) = 0xFF;

	/* Select high to low transition (falling edge) */
	NUCMX_WKEDG(MIWU_TABLE_WKKEY,MIWU_GROUP_WKKEY) =  0xFF;
#endif

#if !(SUPPORT_JTAG) /* mark this for JEN0 debugging */
	keyboard_raw_enable_interrupt(1);
#endif
}

/**
 * Finish initialization after task scheduling has started.
 */
void keyboard_raw_task_start(void)
{
#if !(SUPPORT_JTAG) /* mark this for JEN0 debugging */
	/* Enable MIWU to trigger KBS interrupt */
	task_enable_irq(NUCMX_IRQ_KSI_WKINTC_1);
#endif
}

/**
 * Drive the specified column low.
 */
test_mockable void keyboard_raw_drive_column(int col)
{
	/*
     *	Nuvoton Keyboard Scan IP supports 18x8 Matrix
     *	It also support automatic scan functionality
     */
	uint32_t mask;

	if (col == KEYBOARD_COLUMN_NONE)
		mask = KB_COL_MASK;						/* Drive all lines to high */
	else if (col == KEYBOARD_COLUMN_ALL)
		mask = 0;								/* Set KBSOUT to zero to detect key-press */
	else
		mask = ((~(1 << col)) & KB_COL_MASK);	/* Drive one line for detection */

	/* Set KBSOUT */
	NUCMX_KBSOUT0 = (mask & 0xFFFF);
	NUCMX_KBSOUT1 = ((mask >> 16) & 0x03);
}

/**
 * Read raw row state.
 * Bits are 1 if signal is present, 0 if not present.
 */
test_mockable int keyboard_raw_read_rows(void)
{
	/* Bits are active-low, so invert returned levels */
	return ((~NUCMX_KBSIN) & KB_ROW_MASK);
}

/**
 * Enable or disable keyboard interrupts.
 */
void keyboard_raw_enable_interrupt(int enable)
{
#if !(SUPPORT_JTAG) /* mark this for JEN0 debugging */
	if (enable) {
		/*
		 * Enabling interrupts will clear any pending interrupt bits.  To avoid missing
		 * any interrupts that occur between the end of scanning and then, you should
		 * call keyboard_raw_read_rows() after this.  If it returns non-zero, disable
		 * interrupts and go back to polling mode instead of waiting for an interrupt.
		 */
		task_enable_irq(NUCMX_IRQ_KSI_WKINTC_1);
	} else {
		task_disable_irq(NUCMX_IRQ_KSI_WKINTC_1);
	}
#endif
}

/*
 * Interrupt handler for the entire GPIO bank of keyboard rows.
 */
void keyboard_raw_interrupt(void)
{
	/* Clear pending input sources used by scanner */
	NUCMX_WKPCL(MIWU_TABLE_WKKEY,MIWU_GROUP_WKKEY) = 0xFF;

	/* Wake the scan task */
	task_wake(TASK_ID_KEYSCAN);
}
DECLARE_IRQ(NUCMX_IRQ_KSI_WKINTC_1, keyboard_raw_interrupt, 3);
