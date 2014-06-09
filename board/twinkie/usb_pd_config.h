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

/* TX is using SPI1 on PA6/PB4 */
#define SPI_REGS STM32_SPI1_REGS
#define DMAC_SPI_TX STM32_DMAC_CH3

static inline void spi_enable_clock(void)
{
        STM32_RCC_APB2ENR |= STM32_RCC_PB2_SPI1;
}

/* RX is using COMP1 triggering TIM1 CH1 */
#define DMAC_TIM_RX STM32_DMAC_CH2
#define TIM_CCR_IDX 1
#define TIM_CCR_CS  1
#define EXTI_COMP_MASK ((1 << 21) | (1 << 22))
#define IRQ_COMP STM32_IRQ_COMP
/* triggers packet detection on comparator falling edge */
#define EXTI_XTSR STM32_EXTI_FTSR

/* the pins used for communication need to be hi-speed */
static inline void pd_set_pins_speed(void)
{
	/* 40 MHz pin speed on SPI TX PB4 */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0x00000300;
	/* 40 MHz pin speed on SPI TX PA6 */
	STM32_GPIO_OSPEEDR(GPIO_A) |= 0x00003000;
	/* 40 MHz pin speed on TIM17_CH1 (PB9) */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0x000C0000;
}

/* Drive the CC line from the TX block */
static inline void pd_tx_enable(int polarity)
{
	//if (polarity) {
		gpio_set_level(GPIO_CC2_TX_EN, 1);
		/* TX_DATA on PA6 is now connected to SPI1 */
		gpio_set_alternate_function(GPIO_A, 0x0040, 0);
	//} else {
		gpio_set_level(GPIO_CC1_TX_EN, 1);
		/* TX_DATA on PB4 is now connected to SPI1 */
		gpio_set_alternate_function(GPIO_B, 0x0010, 0);
	//}
}

/* Put the TX driver in Hi-Z state */
static inline void pd_tx_disable(int polarity)
{
	/* TX_DATA on PB4 is an output low GPIO to disable the FET */
	STM32_GPIO_MODER(GPIO_B) = (STM32_GPIO_MODER(GPIO_B) & ~(3 << (2*4)))
							     |  (1 << (2*4));
	/* TX_DATA on PA6 is an output low GPIO to disable the FET */
	STM32_GPIO_MODER(GPIO_A) = (STM32_GPIO_MODER(GPIO_A) & ~(3 << (2*6)))
							     |  (1 << (2*6));
	/*
	 * Tri-state the low side after the high side
	 * to ensure we are not going above Vnc
	 */
	gpio_set_level(GPIO_CC1_TX_EN, 0);
	gpio_set_level(GPIO_CC2_TX_EN, 0);
}

/* we know the plug polarity, do the right configuration */
static inline void pd_select_polarity(int polarity)
{
	/* use the right comparator */
        STM32_COMP_CSR =
                (STM32_COMP_CSR & ~(STM32_COMP_CMP1EN | STM32_COMP_CMP2EN))
                | (polarity ? STM32_COMP_CMP2EN : STM32_COMP_CMP1EN);
}

/* Initialize pins used for clocking */
static inline void pd_tx_init(void)
{
	gpio_config_module(MODULE_USB_PD, 1);
}

static inline void pd_set_host_mode(int enable)
{
	if (enable) {
		gpio_set_level(GPIO_CC1_RD, 1);
		/* set Rp by driving high RPUSB GPIO */
		gpio_set_flags(GPIO_CC1_RPUSB, GPIO_OUT_HIGH);
	} else {
		/* put back RPUSB GPIO in the default state and set Rd */
		gpio_set_flags(GPIO_CC1_RPUSB, GPIO_ODR_HIGH);
		gpio_set_level(GPIO_CC1_RD, 0);
	}
}

static inline int pd_adc_read(int cc)
{
	if (cc == 0)
		return adc_read_channel(ADC_CH_CC1_PD);
	else
		return adc_read_channel(ADC_CH_CC2_PD);
}

static inline int pd_snk_is_vbus_provided(void)
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
