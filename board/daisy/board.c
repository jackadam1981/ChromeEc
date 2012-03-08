/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Daisy board-specific configuration */

#include "board.h"
#include "common.h"
#include "gpio.h"
#include "i2c.h"
#include "registers.h"
#include "timer.h"
#include "util.h"

/* remove uart */
#include "uart.h"

/* GPIO interrupt handlers prototypes */
void gaia_power_event(enum gpio_signal signal);

/* GPIO signal list.  Must match order from enum gpio_signal. */
const struct gpio_info gpio_list[GPIO_COUNT] = {
	/* Inputs with interrupt handlers are first for efficiency */
	{"EC_PWRON",    GPIO_A, (1<<0),  GPIO_INT_BOTH, gaia_power_event},
	{"PP1800_LDO2", GPIO_A, (1<<1),  GPIO_INT_BOTH, gaia_power_event},
	{"XPSHOLD",     GPIO_A, (1<<11), GPIO_INT_RISING, gaia_power_event},
	{"CHARGER_INT", GPIO_B, (1<<0),  GPIO_INT_RISING, NULL},
	{"LID_OPEN",    GPIO_C, (1<<13), GPIO_INT_BOTH, NULL},
	{"KB_COL00",    GPIO_C, (1<<8),  GPIO_INT_BOTH, matrix_interrupt},
	{"KB_COL01",    GPIO_C, (1<<9),  GPIO_INT_BOTH, matrix_interrupt},
	{"KB_COL02",    GPIO_C, (1<<10), GPIO_INT_BOTH, matrix_interrupt},
	{"KB_COL03",    GPIO_C, (1<<11), GPIO_INT_BOTH, matrix_interrupt},
	{"KB_COL04",    GPIO_C, (1<<12), GPIO_INT_BOTH, matrix_interrupt},
	{"KB_COL05",    GPIO_C, (1<<14), GPIO_INT_BOTH, matrix_interrupt},
	{"KB_COL06",    GPIO_C, (1<<15), GPIO_INT_BOTH, matrix_interrupt},
	{"KB_COL07",    GPIO_D, (1<<2),  GPIO_INT_BOTH, matrix_interrupt},
	/* Other inputs */
	/* Outputs */
	{"EN_PP1350",   GPIO_A, (1<<2),  GPIO_OUT_LOW, NULL},
	{"EN_PP5000",   GPIO_A, (1<<3),  GPIO_OUT_LOW, NULL},
	{"EN_PP3300",   GPIO_A, (1<<8),  GPIO_OUT_LOW, NULL},
	{"PMIC_ACOK",   GPIO_A, (1<<12), GPIO_OUT_HIGH, NULL},
	{"ENTERING_RW", GPIO_B, (1<<1),  GPIO_OUT_LOW, NULL},
	{"CHARGER_EN",  GPIO_B, (1<<2),  GPIO_OUT_LOW, NULL},
	{"EC_INT",      GPIO_B, (1<<9),  GPIO_OUT_LOW, NULL},
};

void configure_board(void)
{
	/* Enable all GPIOs clocks
	 * TODO: more fine-grained enabling for power saving
	 */
	STM32L_RCC_AHBENR |= 0x3f;

	/* Select Alternate function for USART1 on pins PA9/PA10 */
	gpio_set_alternate_function(GPIO_A, (1<<9) | (1<<10), GPIO_ALT_USART);

	/* I2C2 SCL/SDA on pins PB10/PB11 */
	STM32L_GPIO_PUPDR_OFF(GPIO_B) &= ~(0xF << (2*10)); /* no pullup/down */
	STM32L_GPIO_OTYPER_OFF(GPIO_B) |= (0x3 << 10); /* open-drain */
	gpio_set_alternate_function(GPIO_B, (1<<10) | (1<<11), GPIO_ALT_I2C);

	/* EC_INT is output, open-drain */
	STM32L_GPIO_OTYPER_OFF(GPIO_B) |= (1<<9);
	STM32L_GPIO_PUPDR_OFF(GPIO_B) &= ~(0x3 << (2*9));
	STM32L_GPIO_MODER_OFF(GPIO_B) &= ~(0x3 << (2*9));
	STM32L_GPIO_MODER_OFF(GPIO_B) |= 0x1 << (2*9);
	/* put GPIO in Hi-Z state */
	gpio_set_level(GPIO_EC_INT, 1);
}

/* add all bytes to produce checksum */
static uint8_t rolling8_csum(void *buf, int len)
{
	int i;
	uint8_t sum = 0;
	uint8_t *data = buf;

	for (i = 0; i < len; i++)
		sum += data[i];

	return sum;
}

uint8_t kb_packet[KB_COLS + 1] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                                   0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc };
int kb_packet_len = KB_COLS + 1;
void kb_send(uint8_t kb_state[], int len)
{
	int i;

	for (i = 0; i < len; i++)
		kb_packet[i] = kb_state[i];
	kb_packet[len] = rolling8_csum(kb_state, len);

	/* interrupt host */
	//uart_printf("%s: EC_INT\n", __func__);
	gpio_set_level(GPIO_EC_INT, 0);
	/* AP should now interrupt EC with I2C read command */
	usleep(10);	/* FIXME: tweak this */
	/* put EC_INT back into Hi-Z state */
	//uart_printf("%s: de-asserting EC_INT\n", __func__);
	gpio_set_level(GPIO_EC_INT, 1);
}
