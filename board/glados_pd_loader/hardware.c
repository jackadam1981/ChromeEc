/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Hardware initialization and common functions */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "cpu.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

static void adc_init(void)
{
	/* Enable ADC clock */
	STM32_RCC_APB2ENR |= (1 << 9);

	/* Only do the calibration if the ADC is off  */
	if (!(STM32_ADC_CR & 1)) {
		/* ADC calibration */
		STM32_ADC_CR = STM32_ADC_CR_ADCAL; /* set ADCAL = 1, ADC off */
		/* wait for the end of calibration */
		while (STM32_ADC_CR & STM32_ADC_CR_ADCAL)
			;
	}
	/* Single conversion, right aligned, 12-bit */
	STM32_ADC_CFGR1 = 1 << 12; /* (1 << 15) => AUTOOFF */;
	/* clock is ADCCLK (ADEN must be off when writing this reg) */
	STM32_ADC_CFGR2 = 0;
	/* Sampling time : 71.5 ADC clock cycles, about 5us */
	STM32_ADC_SMPR = 6;

	/*
	 * ADC enable (note: takes 4 ADC clocks between end of calibration
	 * and setting ADEN).
	 */
	STM32_ADC_CR = STM32_ADC_CR_ADEN;
	while (!(STM32_ADC_ISR & STM32_ADC_ISR_ADRDY))
		STM32_ADC_CR = STM32_ADC_CR_ADEN;
}

int adc_read_channel(enum adc_channel ch)
{
	int value;

	/* Select channel to convert */
	STM32_ADC_CHSELR = 1 << ch;
	/* Clear flags */
	STM32_ADC_ISR = 0x8e;
	/* Start conversion */
	STM32_ADC_CR |= 1 << 2; /* ADSTART */
	/* Wait for end of conversion */
	while (!(STM32_ADC_ISR & (1 << 2)))
		;
	/* read converted value */
	value = STM32_ADC_DR;

	return value;
}

static void uart_init(void)
{
	/* Enable USART1 clock */
	STM32_RCC_APB2ENR |= 0x4000;
	/* set baudrate */
	STM32_USART_BRR(UARTN_BASE) =
		DIV_ROUND_NEAREST(CPU_CLOCK, CONFIG_UART_BAUD_RATE);
	/* UART enabled, 8 Data bits, oversampling x16, no parity */
	STM32_USART_CR1(UARTN_BASE) =
		STM32_USART_CR1_UE | STM32_USART_CR1_TE | STM32_USART_CR1_RE;
	/* 1 stop bit, no fancy stuff */
	STM32_USART_CR2(UARTN_BASE) = 0x0000;
	/* DMA disabled, special modes disabled, error interrupt disabled */
	STM32_USART_CR3(UARTN_BASE) = 0x0000;
}

static void timers_init(void)
{
	/* Enable TIM2 clock */
	STM32_RCC_APB1ENR |= 0x1;
	/* TIM2 is a 32-bit free running counter with 1Mhz frequency */
	STM32_TIM_CR2(2) = 0x0000;
	STM32_TIM32_ARR(2) = 0xFFFFFFFF;
	STM32_TIM_PSC(2) = CPU_CLOCK / 1000000 - 1;
	STM32_TIM_EGR(2) = 0x0001; /* Reload the pre-scaler */
	STM32_TIM_CR1(2) = 1;
	STM32_TIM32_CNT(2) = 0x00000000;
	STM32_TIM_SR(2) = 0; /* Clear pending interrupts */
	STM32_TIM_DIER(2) = 1; /* Overflow interrupt */
	task_enable_irq(STM32_IRQ_TIM2);
}

static void irq_init(void)
{
	/* clear all pending interrupts */
	CPU_NVIC_UNPEND(0) = 0xffffffff;
	/* enable global interrupts */
	asm("cpsie i");
}

extern void runtime_init(void);
void hardware_init(void)
{
	runtime_init(); /* sets clock */
	uart_init();
	timers_init();
	watchdog_init();
	adc_init();
	irq_init();
}

