/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "chip/stm32/registers.h"
#include "console.h"
#include "gpio.h"
#include "ec_commands.h"
#include "usb_pd_tcpm.h"

/* USB Power delivery board configuration */

#ifndef __CROS_EC_USB_PD_CONFIG_H
#define __CROS_EC_USB_PD_CONFIG_H

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)


/* NOTES: Servo V4 and glados equivalents:
 *	Glados		Servo V4
 *	C0		CHG
 *	C1		DUT
 *
 */
#define CHG 0
#define DUT 1

/* Timer selection for baseband PD communication */
#define TIM_CLOCK_PD_TX_CHG 16
#define TIM_CLOCK_PD_RX_CHG 1
#define TIM_CLOCK_PD_TX_DUT 15
#define TIM_CLOCK_PD_RX_DUT 3

/* Timer channel */
#define TIM_TX_CCR_CHG 1
#define TIM_RX_CCR_CHG 1
#define TIM_TX_CCR_DUT 2
#define TIM_RX_CCR_DUT 1

#define TIM_CLOCK_PD_TX(p) ((p) ? TIM_CLOCK_PD_TX_DUT : TIM_CLOCK_PD_TX_CHG)
#define TIM_CLOCK_PD_RX(p) ((p) ? TIM_CLOCK_PD_RX_DUT : TIM_CLOCK_PD_RX_CHG)

/* RX timer capture/compare register */
#define TIM_CCR_CHG (&STM32_TIM_CCRx(TIM_CLOCK_PD_RX_CHG, TIM_RX_CCR_CHG))
#define TIM_CCR_DUT (&STM32_TIM_CCRx(TIM_CLOCK_PD_RX_DUT, TIM_RX_CCR_DUT))
#define TIM_RX_CCR_REG(p) ((p) ? TIM_CCR_DUT : TIM_CCR_CHG)

/* TX and RX timer register */
#define TIM_REG_TX_CHG (STM32_TIM_BASE(TIM_CLOCK_PD_TX_CHG))
#define TIM_REG_RX_CHG (STM32_TIM_BASE(TIM_CLOCK_PD_RX_CHG))
#define TIM_REG_TX_DUT (STM32_TIM_BASE(TIM_CLOCK_PD_TX_DUT))
#define TIM_REG_RX_DUT (STM32_TIM_BASE(TIM_CLOCK_PD_RX_DUT))
#define TIM_REG_TX(p) ((p) ? TIM_REG_TX_DUT : TIM_REG_TX_CHG)
#define TIM_REG_RX(p) ((p) ? TIM_REG_RX_DUT : TIM_REG_RX_CHG)

/* use the hardware accelerator for CRC */
#define CONFIG_HW_CRC

/* TX uses SPI1 on PB3-4 for CHG port, SPI2 on PB 13-14 for DUT port */
#define SPI_REGS(p) ((p) ? STM32_SPI2_REGS : STM32_SPI1_REGS)
void spi_enable_clock(int port);

/* DMA for transmit uses DMA CH3 for CHG and DMA_CH7 for DUT */
#define DMAC_SPI_TX(p) ((p) ? STM32_DMAC_CH7 : STM32_DMAC_CH3)

/* RX uses COMP1 and TIM1_CH1 on port CHG and COMP2 and TIM3_CH1 for port DUT*/
/* DUT RX use CMP1, TIM3_CH1, DMA_CH6 */
#define CMP1OUTSEL STM32_COMP_CMP1OUTSEL_TIM3_IC1
/* CHG RX use CMP2, TIM1_CH1, DMA_CH2 */
#define CMP2OUTSEL STM32_COMP_CMP2OUTSEL_TIM1_IC1

#define TIM_TX_CCR_IDX(p) ((p) ? TIM_TX_CCR_DUT : TIM_TX_CCR_CHG)
#define TIM_RX_CCR_IDX(p) ((p) ? TIM_RX_CCR_DUT : TIM_RX_CCR_CHG)
#define TIM_CCR_CS  1

/*
 * EXTI line 21 is connected to the CMP1 output,
 * EXTI line 22 is connected to the CMP2 output,
 * CHG uses CMP2, and DUT uses CMP1.
 */
#define EXTI_COMP_MASK(p) ((p) ? (1<<21) : (1 << 22))

#define IRQ_COMP STM32_IRQ_COMP
/* triggers packet detection on comparator falling edge */
#define EXTI_XTSR STM32_EXTI_FTSR

/* DMA for receive uses DMA_CH2 for CHG and DMA_CH6 for DUT */
#define DMAC_TIM_RX(p) ((p) ? STM32_DMAC_CH6 : STM32_DMAC_CH2)

/* the pins used for communication need to be hi-speed */
void pd_set_pins_speed(int port);

/* Reset SPI peripheral used for TX */
void pd_tx_spi_reset(int port);

/* Drive the CC line from the TX block */
void pd_tx_enable(int port, int polarity);

/* Put the TX driver in Hi-Z state */
void pd_tx_disable(int port, int polarity);

/* we know the plug polarity, do the right configuration */
void pd_select_polarity(int port, int polarity);

/* Initialize pins used for TX and put them in Hi-Z */
void pd_tx_init(void);

void pd_set_host_mode(int port, int enable);

/**
 * Initialize various GPIOs and interfaces to safe state at start of pd_task.
 *
 * These include:
 *   VBUS, charge path based on power role.
 *   Physical layer CC transmit.
 *
 * @param port        USB-C port number
 * @param power_role  Power role of device
 */
void pd_config_init(int port, uint8_t power_role);

int pd_adc_read(int port, int cc);

#endif /* __CROS_EC_USB_PD_CONFIG_H */

