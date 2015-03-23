/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* oak board configuration */

#include "battery.h"
#include "charger.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "keyboard_raw.h"
#include "lid_switch.h"
#include "power.h"
#include "power_button.h"
// #include "pwm.h"
// #include "pwm_chip.h"
#include "registers.h"
#include "spi.h"
#include "switch.h"
#include "task.h"
#include "timer.h"
#include "usbc_port.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ## args)

#define GPIO_KB_INPUT  (GPIO_INPUT | GPIO_PULL_UP | GPIO_INT_BOTH)
#define GPIO_KB_OUTPUT GPIO_ODR_HIGH

static void pd_mcu_interrupt(enum gpio_signal signal)
{
	/* Exchange status with PD MCU. */
	host_command_pd_send_status(PD_CHARGE_NO_CHANGE);
	/* Get USB-C port status, configure mux and charging source */
	task_wake(TASK_ID_USBCPORT);
}

#include "gpio_list.h"

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_SOC_POWER_GOOD_L, 0, "POWER_GOOD#"},	/* Active low */
	{GPIO_SUSPEND_L, 0, "SUSPEND#_ASSERTED"},	/* Active low */
};

BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

#ifdef CONFIG_I2C
/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"battery", I2C_PORT_BATTERY, 100, GPIO_I2C0_SCL, GPIO_I2C0_SDA},
	{"pd", I2C_PORT_PD_MCU, 100, GPIO_I2C1_SCL, GPIO_I2C1_SDA}
};

const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);
#endif

static int discharging_on_ac;

/**
 * Discharge battery when on AC power for factory test.
 */
int board_discharge_on_ac(int enable)
{
	int rv = charger_discharge_on_ac(enable);

	if (rv == EC_SUCCESS)
		discharging_on_ac = enable;

	return rv;
}

/**
 * Check if we are discharging while connected to AC
 */
int board_is_discharging_on_ac(void)
{
	return discharging_on_ac;
}

/**
 * Reset PD MCU
 */
void board_reset_pd_mcu(void)
{
	gpio_set_level(GPIO_USB_PD_RST_L, 0);
	usleep(100);
	gpio_set_level(GPIO_USB_PD_RST_L, 1);
}

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;

	/* Remap USART DMA to match the USART driver */
	/*
	 * the DMA mapping is :
	 *  Chan 2 : TIM1_CH1
	 *  Chan 3 : SPI1_TX
	 *  Chan 4 : USART1_TX
	 *  Chan 5 : USART1_RX
	 */
#ifdef CONFIG_UART_RX_DMA
	STM32_SYSCFG_CFGR1 |= (1 << 9); /* Remap USART1 RX/TX DMA */
#endif

#ifdef CONFIG_UART_TX_DMA
	STM32_SYSCFG_CFGR1 |= (1 << 10);
#endif
}

void __board_i2c_set_timeout(int port, uint32_t timeout)
{
}

void i2c_set_timeout(int port, uint32_t timeout)
		__attribute__((weak, alias("__board_i2c_set_timeout")));
