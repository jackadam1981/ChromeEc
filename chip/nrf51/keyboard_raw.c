/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Raw keyboard I/O layer for nRF51
 *
 * To make this code portable, we rely heavily on looping over the keyboard
 * input and output entries in the board's gpio_list[].
 * Each set of inputs(ksi pins) or output(kso pins) must be listed in
 * consecutive, increasing order so that scan loops can interate beginning
 * beginning at KB_IN00 or KB_OUT00 for however many GPIOs are utilized
 * (KEYBOARD_KSI_PINS or KEYBOARD_KSO_PINS).
 */

#include "gpio.h"
#include "keyboard_config.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"
#include "registers.h"
#include "task.h"
#include "util.h"

/* Mask of output pins for driving. */
static unsigned int kso_mask;

void keyboard_raw_init(void)
{
	int i;

	/* Initialize kso_mask */
	kso_mask = 0;
	for (i = 0; i < KEYBOARD_KSO_COUNT; i++)
		kso_mask |= gpio_list[GPIO_KB_OUT00 + i].mask;

	/* Ensure interrupts are disabled */
	keyboard_raw_enable_interrupt(0);
}

void keyboard_raw_task_start(void)
{
	/*
	 * Enable the interrupt for keyboard matrix inputs.
	 * One is enough, since they are shared.
	 */
	gpio_enable_interrupt(GPIO_KB_IN00);
}

test_mockable void keyboard_raw_pins(int kso)
{
	/* tri-state all first */
	NRF51_GPIO0_OUTSET = kso_mask;

	/* drive low for specified pin(s) */
	if (kso == KEYBOARD_KSO_COUNT_ALL)
		NRF51_GPIO0_OUTCLR = kso_mask;
	else if (kso != KEYBOARD_KSO_COUNT_NONE)
		NRF51_GPIO0_OUTCLR = gpio_list[GPIO_KB_OUT00 + kso].mask;
}

test_mockable int keyboard_raw_read(void)
{
	int i;
	int state = 0;

	for (i = 0; i < KEYBOARD_KSI_COUNT; i++) {
		if (NRF51_GPIO0_IN & gpio_list[GPIO_KB_IN00 + i].mask)
			state |= 1 << i;
	}

	/* Invert it so 0=not pressed, 1=pressed */
	return state ^ 0xff;
}

void keyboard_raw_enable_interrupt(int enable)
{
	if (enable) {
		/*
		 * Clear the PORT event before enabling the interrupt.
		 */
		NRF51_GPIOTE_PORT = 0;
		NRF51_GPIOTE_INTENSET = 1 << NRF51_GPIOTE_PORT_BIT;
	} else {
		NRF51_GPIOTE_INTENCLR = 1 << NRF51_GPIOTE_PORT_BIT;
	}
}

void keyboard_raw_gpio_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_KEYSCAN);
}
