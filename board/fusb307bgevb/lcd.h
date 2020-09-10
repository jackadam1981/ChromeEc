/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * LCD driver for I2C LCD 2004.
 */

#ifndef __CROS_EC_LCD_H
#define __CROS_EC_LCD_H

#include "common.h"

/* commands */
#define LCD_CLEARDISPLAY	BIT(0)
#define LCD_RETURNHOME		BIT(1)
#define LCD_ENTRYMODESET	BIT(2)
#define LCD_DISPLAYCONTROL	BIT(3)
#define LCD_CURSORSHIFT		BIT(4)
#define LCD_FUNCTIONSET		BIT(5)
#define LCD_SETCGRAMADDR	BIT(6)
#define LCD_SETDDRAMADDR	BIT(7)

/* flags for display entry mode */
#define LCD_ENTRYRIGHT	0x00
#define LCD_ENTRYLEFT	0x02
#define LCD_ENTRYSHIFTINCREMENT	0x01
#define LCD_ENTRYSHIFTDECREMENT	0x00

/* flags for display on/off control */
#define LCD_DISPLAYON	0x04
#define LCD_DISPLAYOFF	0x00
#define LCD_CURSORON	0x02
#define LCD_CURSOROFF	0x00
#define LCD_BLINKON	0x01
#define LCD_BLINKOFF	0x00

/* flags for display/cursor shift */
#define LCD_DISPLAYMOVE	0x08
#define LCD_CURSORMOVE	0x00
#define LCD_MOVERIGHT	0x04
#define LCD_MOVELEFT	0x00

/* flags for function set */
#define LCD_8BITMODE	0x10
#define LCD_4BITMODE	0x00
#define LCD_2LINE	0x08
#define LCD_1LINE	0x00
#define LCD_5x10DOTS	0x04
#define LCD_5x8DOTS	0x00

/* flags for backlight control */
#define LCD_BACKLIGHT	0x08
#define LCD_NOBACKLIGHT	0x00

#define LCD_En	BIT(2) /* Enable bit */
#define LCD_Rw	BIT(1) /* Read/Write bit */
#define LCD_Rs	BIT(0) /* Register select bit */

void lcd_init(uint8_t cols, uint8_t rows, uint8_t dotsize);
void lcd_setCursor(uint8_t col, uint8_t row);
void lcd_setChar(char data);
void lcd_printString(char *str);
void lcd_clear(void);
void lcd_home(void);
void lcd_enable_display(void);
void lcd_disable_display(void);
void lcd_enable_backlight(void);
void lcd_disable_backlight(void);

#endif /*__CROS_EC_LCD_H */
