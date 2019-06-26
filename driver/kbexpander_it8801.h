/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Raw access to keyboard GPIOs.
 *
 * The keyboard matrix is read by driving output signals on the column lines
 * and reading the row lines.
 */

#ifndef __CROS_EC_KBEXPANDER_IT8801_H
#define __CROS_EC_KBEXPANDER_IT8801_H

/* SMBus interface */
#define IT8801_I2C_ADDR 0x70

/* Keyboard Matrix Scan control (KBS) */
#define IT8801_I2C_KSOMCR   0x40
#define I2C_KSOSDIC         BIT(7)
#define I2C_KSE             BIT(6)
#define I2C_AKSOSC          BIT(5)
#define IT8801_I2C_KSIDR    0x41
#define IT8801_I2C_KSIEER   0x42
#define IT8801_I2C_KSIIER   0x43
#define IT8801_I2C_SMBCR    0xfa
#define I2C_ARE             BIT(4)
#define IT8801_I2C_GIECR    0xfb
#define I2C_GKSIIE          BIT(3)
#define IT8801_I2C_KSO18    0x0b
#define IT8801_I2C_KSO19    0x0a
#define IT8801_I2C_KSO20    0x1d
#define IT8801_I2C_KSO21    0x1c
#define I2C_FUNC2           BIT(6)

/* Setting master interrupt and port */
#define I2C_PORT 1

/* Column values for keyboard_raw_drive_column() */
enum keyboard_column_index {
	KEYBOARD_COLUMN_ALL = -2,  /* Drive all columns */
	KEYBOARD_COLUMN_NONE = -1, /* Drive no columns (tri-state all) */
	/* 0 ~ KEYBOARD_COLS_MAX-1 for the corresponding column */
};

void keyboard_raw_enable_interrupt(int enable);

void keyboard_expander_interrupt(enum gpio_signal signal);

#endif /* __CROS_EC_KBEXPANDER_IT8801_H */
