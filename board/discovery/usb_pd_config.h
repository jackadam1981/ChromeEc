/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery board configuration */

#ifndef __USB_PD_CONFIG_H
#define __USB_PD_CONFIG_H

#include "registers.h"

/* Timer selection for baseband PD communication */
#define TIM_CLOCK_PD_TX 10
#define TIM_CLOCK_PD_RX 2

/* The hardware accelerator for CRC is not usable, use software */
#define CONFIG_SW_CRC

/* TX using SPI1 on PA4-7 */
#define SPI_REGS STM32_SPI1_REGS
#define DMAC_SPI_TX STM32_DMAC_SPI1_TX

static inline void spi_enable_clock(void)
{
	STM32_RCC_APB2ENR |= STM32_RCC_PB2_SPI1;
}

/* RX is using COMP2 triggering TIM2 CH4 */
#define DMAC_TIM_RX STM32_DMAC_CH7
#define TIM_CCR_IDX 4
#define TIM_CCR_CS  1
#define EXTI_COMP 22
#define IRQ_COMP STM32_IRQ_COMP
/* triggers packet detection on comparator falling edge */
#define EXTI_xSR STM32_EXTI_FTSR

/* Clock divider for RX edges timings (2.4Mhz counter from 32Mhz clock) */
#define RX_CLOCK_DIV (13 - 1)

/* the pins used for communication need to be hi-speed */
static inline void pd_set_pins_speed(void)
{
	/* 40 MHz pin speed on SPI PA4/5/6/7 */
	STM32_GPIO_OSPEEDR(GPIO_A) |= 0xff00;
	/* 40 MHz pin speed on TIM10_CH1 (PB12) */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0x03000000;
}

/* Drive the CC line from the TX block */
static inline void pd_tx_enable(void)
{
	/* set the low level reference */
	gpio_set_level(GPIO_PD_TX_EN, 0);
	/* put SPI function on TX pin */
	gpio_config_module(MODULE_USB_PD, 1);
}

/* Put the TX driver in Hi-Z state */
static inline void pd_tx_disable(void)
{
	/* put SPI TX in Hi-Z */
	gpio_config_module(MODULE_USB_PD, 0);
	/* put the low level reference in Hi-Z */
	gpio_set_level(GPIO_PD_TX_EN, 1);
}

/* Initialize pins used for TX and put them in Hi-Z */
static inline void pd_tx_init(void)
{
	gpio_config_module(MODULE_USB_PD, 0);
}

/* 3.0A DFP : no-connect voltage is 2.45V */
#define PD_SRC_VNC 2450 /* mV */

/* UFP-side : threshold for DFP connection detection */
#define PD_SNK_VA   200 /* mV */

/* we are a dev board, wait for the user to tell us what we should do */
#define PD_DEFAULT_STATE PD_STATE_DISABLED

/* delay necessary for the voltage transition on the power supply */
#define PD_POWER_SUPPLY_TRANSITION_DELAY 50000 /* us */

#endif /* __USB_PD_CONFIG_H */
