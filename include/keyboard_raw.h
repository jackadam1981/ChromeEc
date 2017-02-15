/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Raw access to keyboard GPIOs.
 *
 * The keyboard matrix is read by driving output signals on the column lines
 * (KSO pins) and reading the row lines(KSI pins).
 */

#ifndef __CROS_EC_KEYBOARD_RAW_H
#define __CROS_EC_KEYBOARD_RAW_H

#include "common.h"
#include "gpio.h"

/* Column values for keyboard_raw_drive_pins() */
enum keyboard_kso_index {
	KEYBOARD_KSO_COUNT_ALL = -2,  /* Drive all columns(KSO pins) */
	KEYBOARD_KSO_COUNT_NONE = -1, /* Drive no columns (tri-state all) */
	/* 0 ~ KEYBOARD_KSO_PINS-1 for the corresponding column(KSO pin) */
};

/**
 * Initialize the raw keyboard interface.
 *
 * Must be called before any other functions in this interface.
 */
void keyboard_raw_init(void);

/**
 * Finish intitialization after task scheduling has started.
 *
 * Call from the keyboard scan task.
 */
void keyboard_raw_task_start(void);

/**
 * Assert the specified kso pin low.
 *
 * Other columns(kso pins) are tristated. See enum keyboard_kso_index
 * for special values for <kso>.
 */
void keyboard_raw_drive_pins(int kso);

/**
 * Read raw row state(ksi pin state).
 *
 * Bits are 1 if signal is present, 0 if not present.
 */
int keyboard_raw_read(void);

/**
 * Enable or disable keyboard interrupts.
 *
 * Enabling interrupts will clear any pending interrupt bits.  To avoid missing
 * any interrupts that occur between the end of scanning and then, you should
 * call keyboard_raw_read_ksi_pins() after this.
 * If it returns non-zero, disable
 * interrupts and go back to polling mode instead of waiting for an interrupt.
 */
void keyboard_raw_enable_interrupt(int enable);

#ifdef HAS_TASK_KEYSCAN

/**
 * GPIO interrupt for raw keyboard input
 */
void keyboard_raw_gpio_interrupt(enum gpio_signal signal);

#else
static inline void keyboard_raw_gpio_interrupt(enum gpio_signal signal) { }
#endif /* !HAS_TASK_KEYSCAN */

/**
 * Run keyboard factory test scanning.
 *
 * @return non-zero if keyboard pins are shorted.
 */
int keyboard_factory_test_scan(void);

#endif  /* __CROS_EC_KEYBOARD_RAW_H */
