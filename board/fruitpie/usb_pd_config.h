/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery board configuration */

#ifndef __USB_PD_CONFIG_H
#define __USB_PD_CONFIG_H

#include "dma.h"

/* Timer selection for baseband PD communication */
#define TIM_CLOCK_PD_TX_C0 17
#define TIM_CLOCK_PD_RX_C0 1

#define TIM_CLOCK_PD_TX(p) TIM_CLOCK_PD_TX_C0
#define TIM_CLOCK_PD_RX(p) TIM_CLOCK_PD_RX_C0

static timer_ctlr_t * const pd_timers_tx[PD_PORT_COUNT] = {
	(void *)STM32_TIM_BASE(TIM_CLOCK_PD_TX_C0),
};
static timer_ctlr_t * const pd_timers_rx[PD_PORT_COUNT] = {
	(void *)STM32_TIM_BASE(TIM_CLOCK_PD_RX_C0),
};

/* use the hardware accelerator for CRC */
#define CONFIG_HW_CRC

/* TX is using SPI2 on PB12-14 */
#define SPI_REGS(p) STM32_SPI2_REGS

static inline void spi_enable_clock(int port)
{
	STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2;
	STM32_SYSCFG_CFGR1 |= 1 << 24; /* Remap SPI2 DMA */
}

#define DMAC_SPI_TX(p) STM32_DMAC_CH7
static const struct dma_option dma_tx_option[PD_PORT_COUNT] = {
	{DMAC_SPI_TX(0), (void *)&SPI_REGS(0)->dr,
			STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT}
};

/* RX is using COMP1 triggering TIM1 CH1 */
#define TIM_CCR_IDX(p) 1
#define TIM_CCR_CS  1
#define EXTI_COMP_MASK(p) (1 << 21)
#define IRQ_COMP STM32_IRQ_COMP
/* triggers packet detection on comparator falling edge */
#define EXTI_XTSR STM32_EXTI_FTSR

#define DMAC_TIM_RX(p) STM32_DMAC_CH2
static const struct dma_option dma_tim_option[PD_PORT_COUNT] = {
	{DMAC_TIM_RX(0),
		(void *)&STM32_TIM_CCRx(TIM_CLOCK_PD_RX_C0, TIM_CCR_IDX(0)),
		STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_16_BIT}
};

/* the pins used for communication need to be hi-speed */
static inline void pd_set_pins_speed(int port)
{
	/* 40 MHz pin speed on SPI PB12/13/14 */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0x7f000000;
	/* 40 MHz pin speed on TIM17_CH1 (PB9) */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0x000C0000;
}

/* Drive the CC line from the TX block */
static inline void pd_tx_enable(int port, int polarity)
{
	gpio_set_level(GPIO_PD_TX_EN, 1);
}

/* Put the TX driver in Hi-Z state */
static inline void pd_tx_disable(int port, int polarity)
{
	gpio_set_level(GPIO_PD_TX_EN, 0);
}

/* we know the plug polarity, do the right configuration */
static inline void pd_select_polarity(int port, int polarity)
{
	/* use the right comparator non inverted input for COMP1 */
	STM32_COMP_CSR = (STM32_COMP_CSR & ~STM32_COMP_CMP1INSEL_MASK)
		| STM32_COMP_CMP1EN
		| (polarity ? STM32_COMP_CMP1INSEL_INM4
			    : STM32_COMP_CMP1INSEL_INM6);
}

/* Initialize pins used for TX and put them in Hi-Z */
static inline void pd_tx_init(void)
{
	gpio_config_module(MODULE_USB_PD, 1);
}

static inline void pd_set_host_mode(int port, int enable)
{
	gpio_set_level(GPIO_CC_HOST, enable);
}

static inline int pd_adc_read(int port, int cc)
{
	if (cc == 0)
		return adc_read_channel(ADC_CH_CC1_PD);
	else
		return adc_read_channel(ADC_CH_CC2_PD);
}

static inline int pd_snk_is_vbus_provided(int port)
{
	return 1;
}

/* Standard-current DFP : no-connect voltage is 1.55V */
#define PD_SRC_VNC 1550 /* mV */

/* UFP-side : threshold for DFP connection detection */
#define PD_SNK_VA   200 /* mV */

/* start as a sink in case we have no other power supply/battery */
#define PD_DEFAULT_STATE PD_STATE_SNK_DISCONNECTED

/* delay necessary for the voltage transition on the power supply */
#define PD_POWER_SUPPLY_TRANSITION_DELAY 50000 /* us */

#endif /* __USB_PD_CONFIG_H */
