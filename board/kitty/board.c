/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Kitty board-specific configuration */

#include "chipset.h"
#include "common.h"
#include "extpower.h"
#include "gpio.h"
#include "power.h"
#include "power_button.h"
#include "power.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "util.h"
#include "timer.h"

#define GPIO_INPUT_UP  (GPIO_INPUT | GPIO_PULL_UP)

/* GPIO signal list.  Must match order from enum gpio_signal. */
const struct gpio_info gpio_list[] = {
	/* Inputs with interrupt handlers are first for efficiency */
	{"POWER_BUTTON_L", GPIO_B, (1<<5),  GPIO_INT_BOTH,
	 power_button_interrupt},
	{"XPSHOLD",     GPIO_A, (1<<3),  GPIO_INT_BOTH,
	 power_signal_interrupt},
	{"SUSPEND_L",   GPIO_C, (1<<7),  GPIO_INPUT_UP | GPIO_INT_BOTH,
	 power_signal_interrupt},
	{"SPI1_NSS",    GPIO_A, (1<<4),  GPIO_INT_BOTH | GPIO_PULL_UP,
	 spi_event},
	{"AC_PRESENT",  GPIO_A, (1<<0),  GPIO_INT_BOTH, extpower_interrupt},
	/* Other inputs */
	{"LID_OPEN",    GPIO_C, (1<<13), GPIO_INPUT, NULL},
	{"WP_L",        GPIO_B, (1<<4),  GPIO_INPUT, NULL},
	/* Outputs */
	{"AP_RESET_L",  GPIO_B, (1<<3),  GPIO_ODR_HIGH, NULL},
	{"EC_INT",      GPIO_B, (1<<9),  GPIO_ODR_HIGH, NULL},
	{"ENTERING_RW", GPIO_H, (1<<0),  GPIO_OUT_LOW, NULL},
	{"LED_POWER_L", GPIO_A, (1<<2),  GPIO_OUT_HIGH, NULL},  /* PWR_LED1 */
	{"PMIC_PWRON_L", GPIO_A, (1<<12), GPIO_OUT_HIGH, NULL},
	{"PMIC_RESET",  GPIO_A, (1<<15), GPIO_OUT_LOW, NULL},
	{"PWR_LED0",    GPIO_B, (1<<10), GPIO_OUT_LOW, NULL},
	{"PMIC_THERM_L",  GPIO_A, (1<<1),  GPIO_ODR_HIGH, NULL},
	{"PMIC_WARM_RESET_L", GPIO_C, (1<<3),  GPIO_ODR_HIGH, NULL},
};
BUILD_ASSERT(ARRAY_SIZE(gpio_list) == GPIO_COUNT);

/* Pins with alternate functions */
const struct gpio_alt_func gpio_alt_funcs[] = {
	{GPIO_A, 0x00f0, GPIO_ALT_SPI,   MODULE_SPI, GPIO_DEFAULT},
	{GPIO_A, 0x0600, GPIO_ALT_USART, MODULE_UART, GPIO_DEFAULT},
};
const int gpio_alt_funcs_count = ARRAY_SIZE(gpio_alt_funcs);

/* power signal list.  Must match order of enum power_signal. */
const struct power_signal_info power_signal_list[] = {
	{GPIO_SOC1V8_XPSHOLD, 1, "XPSHOLD"},
	{GPIO_SUSPEND_L,      0, "SUSPEND#_ASSERTED"},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

/* PWM channels. Must be in the exactly same order as in enum pwm_channel. */
const struct pwm_t pwm_channels[] = {
	{STM32_TIM(2), STM32_TIM_CH(3),
	 PWM_CONFIG_ACTIVE_LOW, GPIO_LED_POWER_L},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

/*
 * kitty has no battery.
 * So implement an empty battery_wait_for_stable() function here.
 */
int battery_wait_for_stable(void)
{
	return EC_SUCCESS;
}
