/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Daisy board-specific configuration */

#include "board.h"
#include "common.h"
#include "gpio.h"
#include "registers.h"
#include "spi.h"
#include "util.h"

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
	/* Required to configure external IRQ lines (SYSCFG_EXTICRn) */
	STM32L_RCC_APB2ENR |= (1<<12) | (1 << 0);

	/* Select Alternate function for USART1 on pins PA9/PA10 */
	gpio_set_alternate_function(GPIO_A, (1<<9) | (1<<10), GPIO_ALT_USART);

	/* I2C2 SCL/SDA on pins PB10/PB11 */
	STM32L_GPIO_PUPDR_OFF(GPIO_B) &= ~(0xF << (2*10)); /* no pullup/down */
	STM32L_GPIO_OTYPER_OFF(GPIO_B) |= (0x3 << 10); /* open-drain */
	gpio_set_alternate_function(GPIO_B, (1<<10) | (1<<11), GPIO_ALT_I2C);

	/* EC_INT is open-drain, asserted by setting to 1 */
	STM32L_GPIO_OTYPER_OFF(GPIO_B) |= (1<<9); 
	STM32L_GPIO_PUPDR_OFF(GPIO_B) &= ~(0x3 << (2*9));
	STM32L_GPIO_MODER_OFF(GPIO_B) &= ~(0x3 << (2*9));
	STM32L_GPIO_MODER_OFF(GPIO_B) |= 0x1 << (2*9);

#if 0
	/* SPI1 on pins PA15, PA12, PA11, PB3 (push-pull, no pullup/down, 10MHz) */
	STM32L_GPIO_PUPDR_OFF(GPIO_A) &= ~(((1 << 15) * 2) |
	                                  ((1 << 12) * 2) |
	                                  ((1 << 11) * 2));
	STM32L_GPIO_OTYPER_OFF(GPIO_A) &= ~((1 << 15) |
	                                    (1 << 12) |
	                                    (1 << 11));
	gpio_set_alternate_function(GPIO_A, (1<<15) | (1<<12) | (1<<11), GPIO_ALT_SPI);

	STM32L_GPIO_PUPDR_OFF(GPIO_B) &= ~((1 << 3) * 2);
	STM32L_GPIO_OTYPER_OFF(GPIO_B) &= ~0x0008;
	STM32L_GPIO_OSPEEDR_OFF(GPIO_B) &= ~0x000000c0;
	STM32L_GPIO_OSPEEDR_OFF(GPIO_B) |= 0x00000080;
	gpio_set_alternate_function(GPIO_B, (1<<3), GPIO_ALT_SPI);
#endif
	/* SPI1 on pins PA4-7 (push-pull, no pullup/down, 10MHz) */
	STM32L_GPIO_PUPDR_OFF(GPIO_A) &= ~(((1 << 7) * 2) |
	                                  ((1 << 6) * 2) |
	                                  ((1 << 5) * 2) |
	                                  ((1 << 4) * 2));
	STM32L_GPIO_OTYPER_OFF(GPIO_A) &= ~((1 << 7) |
	                                    (1 << 6) |
	                                    (1 << 5) |
	                                    (1 << 4));
	gpio_set_alternate_function(GPIO_A, (1<<7) |
	                                    (1<<6) |
	                                    (1<<5) |
	                                    (1<<4), GPIO_ALT_SPI);

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

/* globally visible keyboard packet */
int kb_packet_len = KB_COLS + 1;
uint8_t kb_packet[KB_COLS + 1];

/*
 * Daisy keyboard summary:
 * 1. KEYSCAN task woken up via GPIO external interrupt when a key is pressed.
 * 2. The task scans the keyboard matrix for changes. If key state has
 *    changed, the board-specific kb_send() function is called.
 * 3. For Daisy, the EC is connected via I2C and acts as a slave, so the AP
 *    must initiate all transactions. EC_INT is driven low to interrupt AP when
 *    new data becomes available.
 * 4. When the AP is interrupted, it initiates two i2c transactions:
 *    1. 1-byte write: AP writes 0x01 to make EC send keyboard state.
 *    2. 14-byte read: AP reads 1 keyboard packet (13 byte keyboard state +
 *       1-byte checksum).
 */
void kb_send(uint8_t kb_state[], int len)
{
	int i;

	for (i = 0; i < KB_COLS; i++)
		kb_packet[i] = kb_state[i];

	kb_packet[kb_packet_len - 1] = rolling8_csum(kb_state, len);

	/* interrupt host by toggling EC_INT */
	gpio_set_level(GPIO_EC_INT, 0);
	gpio_set_level(GPIO_EC_INT, 1);

	spi_write(STM32L_SPI1_PORT, &kb_packet[0], kb_packet_len);
}
