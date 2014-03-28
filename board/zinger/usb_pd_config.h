/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery board configuration */

#ifndef __USB_PD_CONFIG_H
#define __USB_PD_CONFIG_H

/* Timer selection for baseband PD communication */
#define TIM_CLOCK_PD_TX 14
#define TIM_CLOCK_PD_RX 17

/* use the hardware accelerator for CRC */
#define CONFIG_HW_CRC

/* TX is using SPI1 on PA4-6 */
#define SPI_REGS STM32_SPI1_REGS
#define DMAC_SPI_TX STM32_DMAC_CH3

static inline void spi_enable_clock(void)
{
	/* Already done in hardware_init() */
}

/* RX is directly TIM17 CH1 (PA7, not internal COMP) */
#define DMAC_TIM_RX STM32_DMAC_CH1
#define TIM_CCR_IDX 1
#define EXTI_COMP 7

/* Clock divider for RX edges timings (2.4Mhz counter from 48Mhz clock) */
#define RX_CLOCK_DIV (20 - 1)

/* the pins used for communication need to be hi-speed */
static inline void pd_set_pins_speed(void)
{
	/* Already done in hardware_init() */
}

/* Drive the CC line from the TX block */
static inline void pd_tx_enable(void)
{
	//TODO gpio_set_level(GPIO_PD_TX_EN, 1);
}

/* Put the TX driver in Hi-Z state */
static inline void pd_tx_disable(void)
{
	//TODO gpio_set_level(GPIO_PD_TX_EN, 0);
}

/* Initialize pins used for TX and put them in Hi-Z */
static inline void pd_tx_init(void)
{
	/* Already done in hardware_init() */
}

/* 3.0A DFP : no-connect voltage is 2.45V */
#define PD_SRC_VNC 2450 /* mV */

/* UFP-side : threshold for DFP connection detection */
#define PD_SNK_VA   200 /* mV */

#endif /* __USB_PD_CONFIG_H */
