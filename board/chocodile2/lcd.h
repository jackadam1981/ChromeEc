/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_LCD_H_
#define __CROS_EC_LCD_H_

#define LCD_PIXEL_WIDTH         128
#define LCD_PIXEL_HEIGHT        64
#define LCD_COLOR_BLACK         0x00
#define LCD_COLOR_WHITE         0xff

void lcd_init(void);
void lcd_update(void);
void lcd_clear(void);
void lcd_write_pixel(uint16_t x, uint16_t y, uint16_t c);
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t c);
void lcd_draw_char(uint16_t ox, uint16_t oy, char ch);
void lcd_prints(uint16_t x, uint16_t y, char const *str);

#endif /* __CROS_EC_LCD_H_ */
