/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "ec2i_chip.h"
#include "espi.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "intc.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "lpc.h"
#include "power.h"
#include "power_button.h"
#include "registers.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

int charge_prevent_power_on(int power_button_pressed)
{
	return 0;
}

int charge_want_shutdown(void)
{
	return 0;
}

#include "gpio_list.h"

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_RSMRST_L_PGOOD,    1, "RSMRST_N_PWRGD"},
	{GPIO_PCH_SLP_S0_L,      1, "SLP_S0_DEASSERTED"},
	{VW_SLP_S3_L,            1, "SLP_S3_DEASSERTED"},
	{VW_SLP_S4_L,            1, "SLP_S4_DEASSERTED"},
	{GPIO_PCH_SLP_SUS_L,     1, "SLP_SUS_DEASSERTED"},
	{GPIO_PMIC_DPWROK,       1, "PMIC_DPWROK"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

/* PNPCFG settings */
const struct ec2i_t pnpcfg_settings[] = {
	/* Select logical device 06h(keyboard) */
	{HOST_INDEX_LDN, LDN_KBC_KEYBOARD},
	/* Set IRQ=01h for logical device */
	{HOST_INDEX_IRQNUMX, 0x01},
	/* Enable logical device */
	{HOST_INDEX_LDA, 0x01},

	/* Select logical device 05h(mouse) */
	{HOST_INDEX_LDN, LDN_KBC_MOUSE},
	/* Set IRQ=0Ch for logical device */
	{HOST_INDEX_IRQNUMX, 0x0C},
	/* Enable logical device */
	{HOST_INDEX_LDA, 0x01},

	/* Select logical device 11h(PM1 ACPI) */
	{HOST_INDEX_LDN, LDN_PMC1},
	/* Set IRQ=00h for logical device */
	{HOST_INDEX_IRQNUMX, 0x00},
	/* Enable logical device */
	{HOST_INDEX_LDA, 0x01},

	/* Select logical device 12h(PM2) */
	{HOST_INDEX_LDN, LDN_PMC2},
	/* I/O Port Base Address 200h/204h */
	{HOST_INDEX_IOBAD0_MSB, 0x02},
	{HOST_INDEX_IOBAD0_LSB, 0x00},
	{HOST_INDEX_IOBAD1_MSB, 0x02},
	{HOST_INDEX_IOBAD1_LSB, 0x04},
	/* Set IRQ=00h for logical device */
	{HOST_INDEX_IRQNUMX, 0x00},
	/* Enable logical device */
	{HOST_INDEX_LDA, 0x01},

	/* Select logical device 0Fh(SMFI) */
	{HOST_INDEX_LDN, LDN_SMFI},
	/* H2RAM LPC I/O cycle Dxxx */
	{HOST_INDEX_DSLDC6, 0x00},
	/* Enable H2RAM LPC I/O cycle */
	{HOST_INDEX_DSLDC7, 0x01},
	/* Enable logical device */
	{HOST_INDEX_LDA, 0x01},

	/* Select logical device 17h(PM3) */
	{HOST_INDEX_LDN, LDN_PMC3},
	/* I/O Port Base Address 80h */
	{HOST_INDEX_IOBAD0_MSB, 0x00},
	{HOST_INDEX_IOBAD0_LSB, 0x80},
	{HOST_INDEX_IOBAD1_MSB, 0x00},
	{HOST_INDEX_IOBAD1_LSB, 0x00},
	/* Set IRQ=00h for logical device */
	{HOST_INDEX_IRQNUMX, 0x00},
	/* Enable logical device */
	{HOST_INDEX_LDA, 0x01},
	/* Select logical device 10h(RTCT) */
	{HOST_INDEX_LDN, LDN_RTCT},
	/* P80L Begin Index */
	{HOST_INDEX_DSLDC4, P80L_P80LB},
	/* P80L End Index */
	{HOST_INDEX_DSLDC5, P80L_P80LE},
	/* P80L Current Index */
	{HOST_INDEX_DSLDC6, P80L_P80LC},
#ifdef CONFIG_UART_HOST
	/* Select logical device 2h(UART2) */
	{HOST_INDEX_LDN, LDN_UART2},
	/*
	 * I/O port base address is 2F8h.
	 * Host can use LPC I/O port 0x2F8 ~ 0x2FF to access UART2.
	 * See specification 7.24.4 for more detial.
	 */
	{HOST_INDEX_IOBAD0_MSB, 0x02},
	{HOST_INDEX_IOBAD0_LSB, 0xF8},
	/* IRQ number is 3 */
	{HOST_INDEX_IRQNUMX, 0x03},
	/*
	 * Interrupt Request Type Select
	 * bit1, 0: IRQ request is buffered and applied to SERIRQ.
	 *       1: IRQ request is inverted before being applied to SERIRQ.
	 * bit0, 0: Edge triggered mode.
	 *       1: Level triggered mode.
	 */
	{HOST_INDEX_IRQTP, 0x02},
	/* Enable logical device */
	{HOST_INDEX_LDA, 0x01},
#endif
};
BUILD_ASSERT(ARRAY_SIZE(pnpcfg_settings) == EC2I_SETTING_COUNT);

/* Wake-up pins for hibernate */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_POWER_BUTTON_L, GPIO_LID_OPEN
};
const int hibernate_wake_pins_used = ARRAY_SIZE(hibernate_wake_pins);

/* Initialize board. */
static void board_init(void)
{
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

/* Keyboard scan setting */
struct keyboard_scan_config keyscan_config = {
	.output_settle_us = 35,
	.debounce_down_us = 5 * MSEC,
	.debounce_up_us = 40 * MSEC,
	.scan_period_us = 3 * MSEC,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = 100 * MSEC,
	.actual_key_mask = {
		0x14, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfa, 0xca  /* full set */
	},
};

/*
 * I2C channels (A, B, and C) are using the same timing registers (00h~07h)
 * at default.
 * In order to set frequency independently for each channels,
 * We use timing registers 09h~0Bh, and the supported frequency will be:
 * 50KHz, 100KHz, 400KHz, or 1MHz.
 * I2C channels (D, E and F) can be set different frequency on different ports.
 * The I2C(D/E/F) frequency depend on the frequency of SMBus Module and
 * the individual prescale register.
 * The frequency of SMBus module is 24MHz on default.
 * The allowed range of I2C(D/E/F) frequency is as following setting.
 * SMBus Module Freq = PLL_CLOCK / ((IT83XX_ECPM_SCDCR2 & 0x0F) + 1)
 * (SMBus Module Freq / 510) <=  I2C Freq <= (SMBus Module Freq / 8)
 * Channel D has multi-function and can be used as UART interface.
 * Channel F is reserved for EC debug.
 */
/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"i2c-cha", IT83XX_I2C_CH_A, 100, GPIO_I2C_A_SCL, GPIO_I2C_A_SDA},
	{"i2c-chb", IT83XX_I2C_CH_B, 100, GPIO_I2C_B_SCL, GPIO_I2C_B_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);
