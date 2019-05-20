/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "ina2xx.h"
#include "lcd.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#include "usb_pd.h"

#include "registers.h"

/* Console output macros */
#define CPUTS(outstr) cputs(10, outstr)
#define CPRINTS(format, args...) cprints(10, format, ## args)

void demo_joy_sel(void)
{
	gpio_set_level(GPIO_LD4, 1);
	gpio_set_level(GPIO_LD5, 1);
	gpio_set_level(GPIO_LD7, 1);
	gpio_set_level(GPIO_LD6, 1);
}

void demo_joy_left(void)
{
	gpio_set_level(GPIO_LD4, 1);
	gpio_set_level(GPIO_LD5, 0);
	gpio_set_level(GPIO_LD7, 0);
	gpio_set_level(GPIO_LD6, 0);
}

void demo_joy_down(void)
{
	gpio_set_level(GPIO_LD4, 0);
	gpio_set_level(GPIO_LD5, 1);
	gpio_set_level(GPIO_LD7, 0);
	gpio_set_level(GPIO_LD6, 0);
}

void demo_joy_right(void)
{
	gpio_set_level(GPIO_LD4, 0);
	gpio_set_level(GPIO_LD5, 0);
	gpio_set_level(GPIO_LD7, 1);
	gpio_set_level(GPIO_LD6, 0);
}

void demo_joy_up(void)
{
	gpio_set_level(GPIO_LD4, 0);
	gpio_set_level(GPIO_LD5, 0);
	gpio_set_level(GPIO_LD7, 0);
	gpio_set_level(GPIO_LD6, 1);
}

#define ALPHA "zyxwvutsrqponmlkjihgfedcba9876543210123456789"\
		"abcdefghijklmnopqrstuvwxyz"
static char *itoa(int value, char *result, int base)
{
	char *ptr;
	char *ptr1;
	char tmp_char;
	int tmp_value;

	/* check that the base if valid */
	if (base < 2 || base > 36) {
		*result = '\0';
		return result;
	}

	ptr = result;
	ptr1 = result;

	do {
		tmp_value = value;
		value /= base;
		*ptr++ = ALPHA[35 + (tmp_value - value * base)];
	} while (value);

	/* Apply negative sign */
	if (tmp_value < 0)
		*ptr++ = '-';

	*ptr-- = '\0';
	while (ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr-- = *ptr1;
		*ptr1++ = tmp_char;
	}

	return result;
}

void demo_task(void)
{
	uint32_t v = 0xabcd;
	char snum[20];

	STM32_RCC_APBENR1 |= STM32_RCC_UCPD1EN;

	itoa(v, snum, 16);

	lcd_prints(29, 10, " EC ON");
	lcd_prints(24, 25, "STM32 GO");
	lcd_prints(1,  40, snum);

	while (1)
		task_wait_event(-1);
}
