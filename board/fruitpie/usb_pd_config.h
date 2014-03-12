/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery board configuration */

#ifndef __USB_PD_CONFIG_H
#define __USB_PD_CONFIG_H

/* Timer selection for baseband PD communication */
#define TIM_CLOCK_PD_TX 17
#define TIM_CLOCK_PD_RX 1

/* use the hardware accelerator for CRC */
#define CONFIG_HW_CRC

/* TX is using SPI2 on PB12-14 */
#define SPI_REGS STM32_SPI2_REGS
#define DMAC_SPI_TX STM32_DMAC_CH7

static inline void spi_enable_clock(void)
{
	STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2;
	STM32_SYSCFG_CFGR1 |= 1 << 24; /* Remap SPI2 DMA */
}

/* RX is using COMP1 triggering TIM1 CH1 */
#define DMAC_TIM_RX STM32_DMAC_CH2
#define TIM_CCR_IDX 1
#define EXTI_COMP 21

/* Clock divider for RX edges timings (2.4Mhz counter from 48Mhz clock) */
#define RX_CLOCK_DIV (20 - 1)

/* the pins used for communication need to be hi-speed */
static inline void pd_set_pins_speed(void)
{
	/* 40 MHz pin speed on SPI PB12/13/14 */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0x7f000000;
	/* 40 MHz pin speed on TIM17_CH1 (PB9) */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0x000C0000;
}

#endif /* __USB_PD_CONFIG_H */
