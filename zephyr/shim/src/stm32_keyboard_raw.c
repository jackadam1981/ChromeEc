/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Functions needed by keyboard scanner module for Chrome EC */

#include "common.h"
#include "ec_tasks.h"
#include "keyboard_raw.h"
#include "task.h"
#include "util.h"

#include "gpio/gpio_int.h"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <soc.h>

/* --- External Interrupts --- */
#define STM32_EXTI_BASE 0x40010400
#define STM32_EXTI_IMR REG32(STM32_EXTI_BASE + 0x00)
#define STM32_EXTI_PR REG32(STM32_EXTI_BASE + 0x14)

#define STM32_GPIO_BSRR(b) REG32((b) + 0x18)

#define STM32_GPIOA_BASE 0x48000000
#define STM32_GPIOB_BASE 0x48000400
#define STM32_GPIOC_BASE 0x48000800
#define STM32_GPIOF_BASE 0x48001400
#define GPIO_A STM32_GPIOA_BASE
#define GPIO_B STM32_GPIOB_BASE
#define GPIO_C STM32_GPIOC_BASE
#define GPIO_F STM32_GPIOF_BASE
#define KB_OUT_PORT_LIST GPIO_A, GPIO_B, GPIO_C, GPIO_F

LOG_MODULE_REGISTER(shim_cros_kb_raw, LOG_LEVEL_ERR);

/* this magic number comes from the cros_ec */
static unsigned int irq_mask = 53405;


//static const uint32_t kb_out_ports[] = { KB_OUT_PORT_LIST };

/**
 * Initialize the raw keyboard interface.
 */
void keyboard_raw_init(void)
{
	keyboard_raw_enable_interrupt(0);
}

/**
 * Finish initialization after task scheduling has started.
 */
void keyboard_raw_task_start(void)
{
	/* Enable interrupts for keyboard matrix inputs */
	gpio_enable_dt_interrupt(
		GPIO_INT_FROM_NODELABEL(int_kb_in00));
	gpio_enable_dt_interrupt(
		GPIO_INT_FROM_NODELABEL(int_kb_in01));
	gpio_enable_dt_interrupt(
		GPIO_INT_FROM_NODELABEL(int_kb_in02));
	gpio_enable_dt_interrupt(
		GPIO_INT_FROM_NODELABEL(int_kb_in03));
	gpio_enable_dt_interrupt(
		GPIO_INT_FROM_NODELABEL(int_kb_in04));
	gpio_enable_dt_interrupt(
		GPIO_INT_FROM_NODELABEL(int_kb_in05));
	gpio_enable_dt_interrupt(
		GPIO_INT_FROM_NODELABEL(int_kb_in06));
	gpio_enable_dt_interrupt(
		GPIO_INT_FROM_NODELABEL(int_kb_in07));
/*
	gpio_enable_interrupt(GPIO_KB_IN00);
	gpio_enable_interrupt(GPIO_KB_IN01);
	gpio_enable_interrupt(GPIO_KB_IN02);
	gpio_enable_interrupt(GPIO_KB_IN03);
	gpio_enable_interrupt(GPIO_KB_IN04);
	gpio_enable_interrupt(GPIO_KB_IN05);
	gpio_enable_interrupt(GPIO_KB_IN06);
	gpio_enable_interrupt(GPIO_KB_IN07);
*/
}

/**
 * Drive the specified column low.
 */
void keyboard_raw_drive_column(int out)
{
	/* TODO: */
}

/**
 * Read raw row state.
 * Bits are 1 if signal is present, 0 if not present.
 */
test_mockable int keyboard_raw_read_rows(void)
{
	/* TODO: */
	return 0;
}
/**
 * Enable or disable keyboard interrupts.
 */
void keyboard_raw_enable_interrupt(int enable)
{
	if (enable) {
		/*
		 * Assert all outputs would trigger un-wanted interrupts.
		 * Clear them before enable interrupt.
		 */
		STM32_EXTI_PR |= irq_mask;
		STM32_EXTI_IMR |= irq_mask; /* 1: unmask interrupt */
	} else {
		STM32_EXTI_IMR &= ~irq_mask; /* 0: mask interrupts */
	}
}

void keyboard_raw_gpio_interrupt(enum gpio_signal signal)
{
	printk("\n\n---KB gpio interrupt---\n\n");
	task_wake(TASK_ID_KEYSCAN);
}


// /**
//  * Enable or disable keyboard alternative function.
//  */
// void keybaord_raw_config_alt(bool enable)
// {
// 	/* TODO: */
// }
