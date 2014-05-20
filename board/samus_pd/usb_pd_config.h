/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery board configuration */

#ifndef __USB_PD_CONFIG_H
#define __USB_PD_CONFIG_H

#include "dma.h"

/* Timer selection for baseband PD communication */
#define TIM_CLOCK_PD_TX_C0 14
#define TIM_CLOCK_PD_RX_C0 1
#define TIM_CLOCK_PD_TX_C1 17
#define TIM_CLOCK_PD_RX_C1 3

#define TIM_CLOCK_PD_TX(p) (p ? TIM_CLOCK_PD_TX_C1 : TIM_CLOCK_PD_TX_C0)
#define TIM_CLOCK_PD_RX(p) (p ? TIM_CLOCK_PD_RX_C1 : TIM_CLOCK_PD_RX_C0)

static timer_ctlr_t * const pd_timers_tx[PD_PORT_COUNT] = {
	(void *)STM32_TIM_BASE(TIM_CLOCK_PD_TX_C0),
	(void *)STM32_TIM_BASE(TIM_CLOCK_PD_TX_C1)
};
static timer_ctlr_t * const pd_timers_rx[PD_PORT_COUNT] = {
	(void *)STM32_TIM_BASE(TIM_CLOCK_PD_RX_C0),
	(void *)STM32_TIM_BASE(TIM_CLOCK_PD_RX_C1)
};

/* use the hardware accelerator for CRC */
#define CONFIG_HW_CRC

/* TX uses SPI1 on PB3-5 for port C0, SPI2 on PB 13-15 for port C1 */
#define SPI_REGS(p) (p ? STM32_SPI2_REGS : STM32_SPI1_REGS)
static inline void spi_enable_clock(int port)
{
	if (port == 0)
		STM32_RCC_APB2ENR |= STM32_RCC_PB2_SPI1;
	else
		STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2;
}

/* DMA for transmit uses DMA CH3 for C0 and DMA_CH7 for C1 */
#define DMAC_SPI_TX(p) (p ? STM32_DMAC_CH7 : STM32_DMAC_CH3)
static const struct dma_option dma_tx_option[PD_PORT_COUNT] = {
	{DMAC_SPI_TX(0), (void *)&SPI_REGS(0)->dr,
			STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT},
	{DMAC_SPI_TX(1), (void *)&SPI_REGS(1)->dr,
			STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT},
};

/* RX uses COMP1 and TIM1 CH1 on port C0 and COMP2 and TIM3_CH1 for port C1*/
#define CMP1OUTSEL STM32_COMP_CMP1OUTSEL_TIM1_IC1
#define CMP2OUTSEL STM32_COMP_CMP2OUTSEL_TIM3_IC1
#define TIM_CCR_IDX(p) (p ? 1 : 1) /* Timer channel */
#define TIM_CCR_CS  1
#define EXTI_COMP_MASK(p) (p ? (1<<22) : (1 << 21))
#define IRQ_COMP STM32_IRQ_COMP
/* triggers packet detection on comparator falling edge */
#define EXTI_XTSR STM32_EXTI_FTSR

/* DMA for receive uses DMA_CH2 for C0 and DMA_CH6 for C1 */
#define DMAC_TIM_RX(p) (p ? STM32_DMAC_CH6 : STM32_DMAC_CH2)
static const struct dma_option dma_tim_option[PD_PORT_COUNT] = {
	{DMAC_TIM_RX(0),
		(void *)&STM32_TIM_CCRx(TIM_CLOCK_PD_RX_C0, TIM_CCR_IDX(0)),
		STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_16_BIT},
	{DMAC_TIM_RX(1),
		(void *)&STM32_TIM_CCRx(TIM_CLOCK_PD_RX_C1, TIM_CCR_IDX(1)),
		STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_16_BIT},
};

/* the pins used for communication need to be hi-speed */
static inline void pd_set_pins_speed(int port)
{
	if (port == 0) {
		/* 40 MHz pin speed on SPI PB3/4/5 */
		STM32_GPIO_OSPEEDR(GPIO_B) |= 0x00000FC0;
		/* 40 MHz pin speed on TIM14_CH1 (PB1) */
		STM32_GPIO_OSPEEDR(GPIO_B) |= 0x0000000C;
		/* 40 MHz pin speed on CC1 and CC2, PA0 and PA4 */
		STM32_GPIO_OSPEEDR(GPIO_A) |= 0x303;
	} else {
		/* 40 MHz pin speed on SPI PB13/14/15 */
		STM32_GPIO_OSPEEDR(GPIO_B) |= 0xFC000000;
		/* 40 MHz pin speed on TIM17_CH1 (PE1) */
		STM32_GPIO_OSPEEDR(GPIO_E) |= 0x0000000C;
		/* 40 MHz pin speed on CC1 and CC2, PA2 and PA5 */
		STM32_GPIO_OSPEEDR(GPIO_A) |= 0xc30;
	}
}

/* Drive the CC line from the TX block */
static inline void pd_tx_enable(int port, int polarity)
{
	if (port == 0) {
		/* set the low level reference */
		gpio_set_level(polarity ? GPIO_USB_C0_CC2_TX_EN :
					GPIO_USB_C0_CC1_TX_EN, 1);

		/* put SPI function on TX pin */
		if (polarity) /* PE14 is SPI1 MISO */
			gpio_set_alternate_function(GPIO_E, 0x4000, 1);
		else /* PB4 is SPI1 MISO */
			gpio_set_alternate_function(GPIO_B, 0x0010, 0);
	} else {
		/* set the low level reference */
		gpio_set_level(polarity ? GPIO_USB_C1_CC2_TX_EN :
					GPIO_USB_C1_CC1_TX_EN, 1);

		/* put SPI function on TX pin */
		if (polarity) /* PD3 is SPI2 MISO */
			gpio_set_alternate_function(GPIO_D, 0x0008, 1);
		else /* PB14 is SPI2 MISO */
			gpio_set_alternate_function(GPIO_B, 0x4000, 0);
	}
}

/* Put the TX driver in Hi-Z state */
static inline void pd_tx_disable(int port, int polarity)
{
	if (port == 0) {
		/* output low on SPI TX to disable the FET */
		if (polarity) /* PE14 is SPI1 MISO */
			STM32_GPIO_MODER(GPIO_E) = (STM32_GPIO_MODER(GPIO_E)
						   & ~(3 << (2*14)))
						   |  (1 << (2*14));
		else /* PB4 is SPI1 MISO */
			STM32_GPIO_MODER(GPIO_B) = (STM32_GPIO_MODER(GPIO_B)
						   & ~(3 << (2*4)))
						   |  (1 << (2*4));

		/* put the low level reference in Hi-Z */
		gpio_set_level(polarity ? GPIO_USB_C0_CC2_TX_EN :
					GPIO_USB_C0_CC1_TX_EN, 0);
	} else {
		/* output low on SPI TX to disable the FET */
		if (polarity) /* PD3 is SPI2 MISO */
			STM32_GPIO_MODER(GPIO_E) = (STM32_GPIO_MODER(GPIO_E)
						   & ~(3 << (2*3)))
						   |  (1 << (2*3));
		else /* PB14 is SPI2 MISO */
			STM32_GPIO_MODER(GPIO_B) = (STM32_GPIO_MODER(GPIO_B)
						   & ~(3 << (2*14)))
						   |  (1 << (2*14));

		/* put the low level reference in Hi-Z */
		gpio_set_level(polarity ? GPIO_USB_C1_CC2_TX_EN :
					GPIO_USB_C1_CC1_TX_EN, 0);
	}
}

/* we know the plug polarity, do the right configuration */
static inline void pd_select_polarity(int port, int polarity)
{
	if (port == 0) {
		/* use the right comparator non inverted input for COMP1 */
		STM32_COMP_CSR = (STM32_COMP_CSR & ~STM32_COMP_CMP1INSEL_MASK)
			| STM32_COMP_CMP1EN
			| (polarity ? STM32_COMP_CMP1INSEL_INM4
					: STM32_COMP_CMP1INSEL_INM6);
	} else {
		/* use the right comparator non inverted input for COMP2 */
		STM32_COMP_CSR = (STM32_COMP_CSR & ~STM32_COMP_CMP2INSEL_MASK)
			| STM32_COMP_CMP2EN
			| (polarity ? STM32_COMP_CMP2INSEL_INM5
					: STM32_COMP_CMP2INSEL_INM6);
	}
}

/* Initialize pins used for TX and put them in Hi-Z */
static inline void pd_tx_init(void)
{
	gpio_config_module(MODULE_USB_PD, 1);
}

static inline void pd_set_host_mode(int port, int enable)
{
	if (port == 0) {
		if (enable) {
			/* We never charging in power source mode */
			gpio_set_level(GPIO_USB_C0_CHARGE_EN_L, 1);
			/* High-Z is used for host mode. */
			gpio_set_level(GPIO_USB_C0_CC1_ODL, 1);
			gpio_set_level(GPIO_USB_C0_CC2_ODL, 1);
		} else {
			/* Kill VBUS power supply */
			gpio_set_level(GPIO_USB_C0_5V_EN, 0);
			/* Pull low for device mode. */
			gpio_set_level(GPIO_USB_C0_CC1_ODL, 0);
			gpio_set_level(GPIO_USB_C0_CC2_ODL, 0);
			/* Enable the charging path*/
			gpio_set_level(GPIO_USB_C0_CHARGE_EN_L, 0);
		}
	} else {
		if (enable) {
			/* We never charging in power source mode */
			gpio_set_level(GPIO_USB_C1_CHARGE_EN_L, 1);
			/* High-Z is used for host mode. */
			gpio_set_level(GPIO_USB_C1_CC1_ODL, 1);
			gpio_set_level(GPIO_USB_C1_CC2_ODL, 1);
		} else {
			/* Kill VBUS power supply */
			gpio_set_level(GPIO_USB_C1_5V_EN, 0);
			/* Pull low for device mode. */
			gpio_set_level(GPIO_USB_C1_CC1_ODL, 0);
			gpio_set_level(GPIO_USB_C1_CC2_ODL, 0);
			/* Enable the charging path*/
			gpio_set_level(GPIO_USB_C1_CHARGE_EN_L, 0);
		}
	}
}

static inline int pd_adc_read(int port, int cc)
{
	if (port == 0) {
		if (cc == 0)
			return adc_read_channel(ADC_C0_CC1_PD);
		else
			return adc_read_channel(ADC_C0_CC2_PD);
	} else {
		if (cc == 0)
			return adc_read_channel(ADC_C1_CC1_PD);
		else
			return adc_read_channel(ADC_C1_CC2_PD);
	}
}

static inline int pd_snk_is_vbus_provided(int port)
{
	if (port == 0)
		return gpio_get_level(GPIO_USB_C0_VBUS_WAKE);
	else
		return gpio_get_level(GPIO_USB_C1_VBUS_WAKE);
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
