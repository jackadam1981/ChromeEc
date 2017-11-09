/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for STM32H7 family
 */

#ifndef __CROS_EC_REGISTERS_STM32H7_H
#define __CROS_EC_REGISTERS_STM32H7_H

#include "common.h"

/* --- USART --- */
#define STM32_USART1_BASE          0x40011000
#define STM32_USART2_BASE          0x40004400
#define STM32_USART3_BASE          0x40004800
#define STM32_USART4_BASE          0x40004c00
#define STM32_USART5_BASE          0x40005000
#define STM32_USART6_BASE          0x40011400
#define STM32_USART7_BASE          0x40007800
#define STM32_USART8_BASE          0x40007C00

#define STM32_USART_BASE(n)           CONCAT3(STM32_USART, n, _BASE)
#define STM32_USART_REG(base, offset) REG32((base) + (offset))

#define STM32_USART_CR1(base)      STM32_USART_REG(base, 0x00)
#define STM32_USART_CR1_UE              (1 << 0)
#define STM32_USART_CR1_UESM            (1 << 1)
#define STM32_USART_CR1_RE              (1 << 2)
#define STM32_USART_CR1_TE              (1 << 3)
#define STM32_USART_CR1_RXNEIE          (1 << 5)
#define STM32_USART_CR1_TCIE            (1 << 6)
#define STM32_USART_CR1_TXEIE           (1 << 7)
#define STM32_USART_CR1_PS              (1 << 9)
#define STM32_USART_CR1_PCE             (1 << 10)
#define STM32_USART_CR1_OVER8           (1 << 15)
#define STM32_USART_CR2(base)      STM32_USART_REG(base, 0x04)
#define STM32_USART_CR2_SWAP            (1 << 15)
#define STM32_USART_CR3(base)      STM32_USART_REG(base, 0x08)
#define STM32_USART_CR3_EIE             (1 << 0)
#define STM32_USART_CR3_DMAR            (1 << 6)
#define STM32_USART_CR3_DMAT            (1 << 7)
#define STM32_USART_CR3_ONEBIT          (1 << 11)
#define STM32_USART_CR3_OVRDIS          (1 << 12)
#define STM32_USART_CR3_WUS_START_BIT   (2 << 20)
#define STM32_USART_CR3_WUFIE           (1 << 22)
#define STM32_USART_BRR(base)      STM32_USART_REG(base, 0x0C)
#define STM32_USART_GTPR(base)     STM32_USART_REG(base, 0x10)
#define STM32_USART_RTOR(base)     STM32_USART_REG(base, 0x14)
#define STM32_USART_RQR(base)      STM32_USART_REG(base, 0x18)
#define STM32_USART_ISR(base)      STM32_USART_REG(base, 0x1C)
#define STM32_USART_ICR(base)      STM32_USART_REG(base, 0x20)
#define STM32_USART_ICR_ORECF           (1 << 3)
#define STM32_USART_ICR_TCCF            (1 << 6)
#define STM32_USART_RDR(base)      STM32_USART_REG(base, 0x24)
#define STM32_USART_TDR(base)      STM32_USART_REG(base, 0x28)
#define STM32_USART_PRESC(base)    STM32_USART_REG(base, 0x2C)
/* register alias */
#define STM32_USART_SR(base)       STM32_USART_ISR(base)
#define STM32_USART_SR_ORE              (1 << 3)
#define STM32_USART_SR_RXNE             (1 << 5)
#define STM32_USART_SR_TC               (1 << 6)
#define STM32_USART_SR_TXE              (1 << 7)

#define STM32_IRQ_USART(n)         CONCAT2(STM32_IRQ_USART, n)

/* --- TIMERS --- */
#define STM32_TIM1_BASE            0x40010000

#define STM32_TIM2_BASE            0x40000000
#define STM32_TIM3_BASE            0x40000400
#define STM32_TIM4_BASE            0x40000800
#define STM32_TIM5_BASE            0x40000c00
#define STM32_TIM6_BASE            0x40001000
#define STM32_TIM7_BASE            0x40001400

#define STM32_TIM8_BASE            0x40010400

#define STM32_TIM12_BASE           0x40001800
#define STM32_TIM13_BASE           0x40001c00
#define STM32_TIM14_BASE           0x40002000

#define STM32_TIM15_BASE           0x40014000
#define STM32_TIM16_BASE           0x40014400
#define STM32_TIM17_BASE           0x40014800

#define STM32_TIM_BASE(n)          CONCAT3(STM32_TIM, n, _BASE)

#define STM32_TIM_REG(n, offset) \
		REG16(STM32_TIM_BASE(n) + (offset))
#define STM32_TIM_REG32(n, offset) \
		REG32(STM32_TIM_BASE(n) + (offset))

#define STM32_TIM_CR1(n)           STM32_TIM_REG(n, 0x00)
#define STM32_TIM_CR2(n)           STM32_TIM_REG(n, 0x04)
#define STM32_TIM_SMCR(n)          STM32_TIM_REG(n, 0x08)
#define STM32_TIM_DIER(n)          STM32_TIM_REG(n, 0x0C)
#define STM32_TIM_SR(n)            STM32_TIM_REG(n, 0x10)
#define STM32_TIM_EGR(n)           STM32_TIM_REG(n, 0x14)
#define STM32_TIM_CCMR1(n)         STM32_TIM_REG(n, 0x18)
#define STM32_TIM_CCMR2(n)         STM32_TIM_REG(n, 0x1C)
#define STM32_TIM_CCER(n)          STM32_TIM_REG(n, 0x20)
#define STM32_TIM_CNT(n)           STM32_TIM_REG(n, 0x24)
#define STM32_TIM_PSC(n)           STM32_TIM_REG(n, 0x28)
#define STM32_TIM_ARR(n)           STM32_TIM_REG(n, 0x2C)
#define STM32_TIM_RCR(n)           STM32_TIM_REG(n, 0x30)
#define STM32_TIM_CCR1(n)          STM32_TIM_REG(n, 0x34)
#define STM32_TIM_CCR2(n)          STM32_TIM_REG(n, 0x38)
#define STM32_TIM_CCR3(n)          STM32_TIM_REG(n, 0x3C)
#define STM32_TIM_CCR4(n)          STM32_TIM_REG(n, 0x40)
#define STM32_TIM_BDTR(n)          STM32_TIM_REG(n, 0x44)
#define STM32_TIM_DCR(n)           STM32_TIM_REG(n, 0x48)
#define STM32_TIM_DMAR(n)          STM32_TIM_REG(n, 0x4C)
#define STM32_TIM_OR(n)            STM32_TIM_REG(n, 0x50)

#define STM32_TIM_CCRx(n, x)       STM32_TIM_REG(n, 0x34 + ((x) - 1) * 4)

#define STM32_TIM32_CNT(n)         STM32_TIM_REG32(n, 0x24)
#define STM32_TIM32_ARR(n)         STM32_TIM_REG32(n, 0x2C)
#define STM32_TIM32_CCR1(n)        STM32_TIM_REG32(n, 0x34)
#define STM32_TIM32_CCR2(n)        STM32_TIM_REG32(n, 0x38)
#define STM32_TIM32_CCR3(n)        STM32_TIM_REG32(n, 0x3C)
#define STM32_TIM32_CCR4(n)        STM32_TIM_REG32(n, 0x40)
/* Timer registers as struct */
struct timer_ctlr {
	uint32_t cr1;
	uint32_t cr2;
	uint32_t smcr;
	uint32_t dier;

	uint32_t sr;
	uint32_t egr;
	uint32_t ccmr1;
	uint32_t ccmr2;

	uint32_t ccer;
	uint32_t cnt;
	uint32_t psc;
	uint32_t arr;

	uint32_t ccr[5]; /* ccr[0] = reserved30 */

	uint32_t bdtr;
	uint32_t dcr;
	uint32_t dmar;

	uint32_t or;
};
typedef volatile struct timer_ctlr timer_ctlr_t;

/* --- GPIO --- */

#define GPIO_A                       STM32_GPIOA_BASE
#define GPIO_B                       STM32_GPIOB_BASE
#define GPIO_C                       STM32_GPIOC_BASE
#define GPIO_D                       STM32_GPIOD_BASE
#define GPIO_E                       STM32_GPIOE_BASE
#define GPIO_F                       STM32_GPIOF_BASE
#define GPIO_G                       STM32_GPIOG_BASE
#define GPIO_H                       STM32_GPIOH_BASE
#define GPIO_I                       STM32_GPIOI_BASE
#define GPIO_J                       STM32_GPIOJ_BASE
#define GPIO_K                       STM32_GPIOK_BASE

#define DUMMY_GPIO_BANK GPIO_A

#define STM32_GPIOA_BASE            0x58020000
#define STM32_GPIOB_BASE            0x58020400
#define STM32_GPIOC_BASE            0x58020800
#define STM32_GPIOD_BASE            0x58020C00
#define STM32_GPIOE_BASE            0x58021000
#define STM32_GPIOF_BASE            0x58021400
#define STM32_GPIOG_BASE            0x58021800
#define STM32_GPIOH_BASE            0x58021C00
#define STM32_GPIOI_BASE            0x58022000
#define STM32_GPIOJ_BASE            0x58022400
#define STM32_GPIOK_BASE            0x58022800

#define STM32_GPIO_MODER(b)         REG32((b) + 0x00)
#define STM32_GPIO_OTYPER(b)        REG16((b) + 0x04)
#define STM32_GPIO_OSPEEDR(b)       REG32((b) + 0x08)
#define STM32_GPIO_PUPDR(b)         REG32((b) + 0x0C)
#define STM32_GPIO_IDR(b)           REG16((b) + 0x10)
#define STM32_GPIO_ODR(b)           REG16((b) + 0x14)
#define STM32_GPIO_BSRR(b)          REG32((b) + 0x18)
#define STM32_GPIO_LCKR(b)          REG32((b) + 0x1C)
#define STM32_GPIO_AFRL(b)          REG32((b) + 0x20)
#define STM32_GPIO_AFRH(b)          REG32((b) + 0x24)

#define GPIO_ALT_SYS                 0x0
#define GPIO_ALT_TIM2                0x1
#define GPIO_ALT_TIM3_4              0x2
#define GPIO_ALT_TIM9_11             0x3
#define GPIO_ALT_I2C                 0x4
#define GPIO_ALT_USART1              0x4
#define GPIO_ALT_SPI                 0x5
#define GPIO_ALT_SPI3                0x6
#define GPIO_ALT_USART               0x7
#define GPIO_ALT_I2C_23              0x9
#define GPIO_ALT_USB                 0xA
#define GPIO_ALT_LCD                 0xB
#define GPIO_ALT_RI                  0xE
#define GPIO_ALT_EVENTOUT            0xF

/* --- External Interrupts --- */
#define STM32_EXTI_BASE             0x58000000

#define STM32_EXTI_RTSR1            REG32(STM32_EXTI_BASE + 0x00)
#define STM32_EXTI_FTSR1            REG32(STM32_EXTI_BASE + 0x04)
#define STM32_EXTI_SWIER1           REG32(STM32_EXTI_BASE + 0x08)
#define STM32_EXTI_D3PMR1           REG32(STM32_EXTI_BASE + 0x0C)
#define STM32_EXTI_D3PCR1L          REG32(STM32_EXTI_BASE + 0x10)
#define STM32_EXTI_D3PCR1H          REG32(STM32_EXTI_BASE + 0x14)
#define STM32_EXTI_RTSR2            REG32(STM32_EXTI_BASE + 0x20)
#define STM32_EXTI_FTSR2            REG32(STM32_EXTI_BASE + 0x24)
#define STM32_EXTI_SWIER2           REG32(STM32_EXTI_BASE + 0x28)
#define STM32_EXTI_D3PMR2           REG32(STM32_EXTI_BASE + 0x2C)
#define STM32_EXTI_D3PCR2L          REG32(STM32_EXTI_BASE + 0x30)
#define STM32_EXTI_D3PCR2H          REG32(STM32_EXTI_BASE + 0x34)
#define STM32_EXTI_RTSR3            REG32(STM32_EXTI_BASE + 0x40)
#define STM32_EXTI_FTSR3            REG32(STM32_EXTI_BASE + 0x44)
#define STM32_EXTI_SWIER3           REG32(STM32_EXTI_BASE + 0x48)
#define STM32_EXTI_D3PMR3           REG32(STM32_EXTI_BASE + 0x4C)
#define STM32_EXTI_D3PCR3L          REG32(STM32_EXTI_BASE + 0x50)
#define STM32_EXTI_D3PCR3H          REG32(STM32_EXTI_BASE + 0x54)
#define STM32_EXTI_CPUIMR1          REG32(STM32_EXTI_BASE + 0x80)
#define STM32_EXTI_CPUIER1          REG32(STM32_EXTI_BASE + 0x84)
#define STM32_EXTI_CPUPR1           REG32(STM32_EXTI_BASE + 0x88)
#define STM32_EXTI_CPUIMR2          REG32(STM32_EXTI_BASE + 0x90)
#define STM32_EXTI_CPUIER2          REG32(STM32_EXTI_BASE + 0x94)
#define STM32_EXTI_CPUPR2           REG32(STM32_EXTI_BASE + 0x98)
#define STM32_EXTI_CPUIMR3          REG32(STM32_EXTI_BASE + 0xA0)
#define STM32_EXTI_CPUIER3          REG32(STM32_EXTI_BASE + 0xA4)
#define STM32_EXTI_CPUPR3           REG32(STM32_EXTI_BASE + 0xA8)
/* Aliases */
#define STM32_EXTI_IMR              STM32_EXTI_CPUIMR1
#define STM32_EXTI_EMR              STM32_EXTI_CPUIMR1
#define STM32_EXTI_RTSR             STM32_EXTI_RTSR1
#define STM32_EXTI_FTSR             STM32_EXTI_FTSR1
#define STM32_EXTI_SWIER            STM32_EXTI_SWIER1
#define STM32_EXTI_PR               STM32_EXTI_CPUPR1


/* --- Power / Reset / Clocks --- */
#define STM32_PWR_BASE              0x58024800

#define STM32_PWR_CR                REG32(STM32_PWR_BASE + 0x00)
#define STM32_PWR_CR_LPSDSR         (1 << 0)
#define STM32_PWR_CSR               REG32(STM32_PWR_BASE + 0x04)
#define STM32_PWR_CR2               REG32(STM32_PWR_BASE + 0x08)
#define STM32_PWR_CR3               REG32(STM32_PWR_BASE + 0x0C)
#define STM32_PWR_CPUCR             REG32(STM32_PWR_BASE + 0x10)
#define STM32_PWR_D3CR              REG32(STM32_PWR_BASE + 0x18)
#define STM32_PWR_WKUPCR            REG32(STM32_PWR_BASE + 0x20)
#define STM32_PWR_WKUPFR            REG32(STM32_PWR_BASE + 0x24)
#define STM32_PWR_WKUPEPR           REG32(STM32_PWR_BASE + 0x28)


#define STM32_RCC_BASE              0x58024400

#define STM32_RCC_CR                REG32(STM32_RCC_BASE + 0x000)

#define STM32_RCC_BDCR              REG32(STM32_RCC_BASE + 0x070)
#define STM32_RCC_CSR               REG32(STM32_RCC_BASE + 0x074)

#define STM32_RCC_APB2RST           REG32(STM32_RCC_BASE + 0x098)

#define STM32_RCC_RSR               REG32(STM32_RCC_BASE + 0x0D0)

#define STM32_RCC_AHB3ENR           REG32(STM32_RCC_BASE + 0x0D4)
#define STM32_RCC_AHB1ENR           REG32(STM32_RCC_BASE + 0x0D8)
#define STM32_RCC_AHB2ENR           REG32(STM32_RCC_BASE + 0x0DC)
#define STM32_RCC_AHB4ENR           REG32(STM32_RCC_BASE + 0x0E0)
#define STM32_RCC_AHB4ENR_GPIOMASK  0x3ff
#define STM32_RCC_APB3ENR           REG32(STM32_RCC_BASE + 0x0E4)
#define STM32_RCC_APB1LENR          REG32(STM32_RCC_BASE + 0x0E8)
#define STM32_RCC_APB1HENR          REG32(STM32_RCC_BASE + 0x0EC)
#define STM32_RCC_APB2ENR           REG32(STM32_RCC_BASE + 0x0F0)
#define STM32_RCC_APB4ENR           REG32(STM32_RCC_BASE + 0x0F4)
#define STM32_RCC_SYSCFGEN          (1 << 1)
#define STM32_RCC_AHB3LPENR         REG32(STM32_RCC_BASE + 0x0FC)
#define STM32_RCC_AHB1LPENR         REG32(STM32_RCC_BASE + 0x100)
#define STM32_RCC_AHB2LPENR         REG32(STM32_RCC_BASE + 0x104)
#define STM32_RCC_AHB4LPENR         REG32(STM32_RCC_BASE + 0x108)
#define STM32_RCC_APB3LPENR         REG32(STM32_RCC_BASE + 0x10C)
#define STM32_RCC_APB1LLPENR        REG32(STM32_RCC_BASE + 0x110)
#define STM32_RCC_APB1HLPENR        REG32(STM32_RCC_BASE + 0x114)
#define STM32_RCC_APB2LPENR         REG32(STM32_RCC_BASE + 0x118)
#define STM32_RCC_APB4LPENR         REG32(STM32_RCC_BASE + 0x11C)
/* Aliases */
#define STM32_RCC_APB1ENR           STM32_RCC_APB1LENR
/* Peripheral bits for APB1LPENR/APB2ENR regs */
#define STM32_RCC_PB1_TIM2              (1 << 0)
#define STM32_RCC_PB1_TIM3              (1 << 1)
#define STM32_RCC_PB1_TIM4              (1 << 2)
#define STM32_RCC_PB1_TIM5              (1 << 3)
#define STM32_RCC_PB1_TIM6              (1 << 4)
#define STM32_RCC_PB1_TIM7              (1 << 5)
#define STM32_RCC_PB1_TIM12             (1 << 6)
#define STM32_RCC_PB1_TIM13             (1 << 7)
#define STM32_RCC_PB1_TIM14             (1 << 8)
#define STM32_RCC_PB1_SPI2              (1 << 14)
#define STM32_RCC_PB1_SPI3              (1 << 15)
#define STM32_RCC_PB1_USART2            (1 << 17)
#define STM32_RCC_PB1_USART3            (1 << 18)
#define STM32_RCC_PB1_USART4            (1 << 19)
#define STM32_RCC_PB1_USART5            (1 << 20)

#define STM32_RCC_PB2_TIM1              (1 << 0)
#define STM32_RCC_PB2_TIM2              (1 << 1)
#define STM32_RCC_PB2_USART1            (1 << 4)
#define STM32_RCC_PB2_SPI1              (1 << 12)
#define STM32_RCC_PB2_SPI4              (1 << 13)
#define STM32_RCC_PB2_TIM15             (1 << 16)
#define STM32_RCC_PB2_TIM16             (1 << 17)
#define STM32_RCC_PB2_TIM17             (1 << 18)

/* Peripheral bits for AHB1/2/3/4ENR regs */
#define STM32_RCC_HB1_DMA1		(1 << 0)
#define STM32_RCC_HB1_DMA2		(1 << 1)
#define STM32_RCC_HB3_MDMA		(1 << 0)
#define STM32_RCC_HB4_BDMA		(1 << 21)

/* RTC domain control register */
#define STM32_RCC_BDCR_BDRST            (1 << 16)
#define STM32_RCC_BDCR_RTCEN            (1 << 15)
#define STM32_RCC_BDCR_LSERDY           (1 << 1)
#define STM32_RCC_BDCR_LSEON            (1 << 0)
#define  BDCR_RTCSEL_MASK               ((0x3) << 8)
#define  BDCR_RTCSEL(source)            (((source) << 8) & BDCR_RTCSEL_MASK)
#define  BDCR_SRC_LSE                   0x1
#define  BDCR_SRC_LSI                   0x2
#define  BDCR_SRC_HSE                   0x3


#define STM32_SYSCFG_BASE           0x58000400

#define STM32_SYSCFG_PMCR           REG32(STM32_SYSCFG_BASE + 0x04)
#define STM32_SYSCFG_EXTICR(n)      REG32(STM32_SYSCFG_BASE + 8 + 4 * (n))

/* --- Debug --- */

#define STM32_DBGMCU_BASE           0x5C001000

#define STM32_DBGMCU_IDCODE         REG32(STM32_DBGMCU_BASE + 0x00)
#define STM32_DBGMCU_CR             REG32(STM32_DBGMCU_BASE + 0x04)
#define STM32_DBGMCU_APB3FZ         REG32(STM32_DBGMCU_BASE + 0x34)
#define STM32_DBGMCU_APB1LFZ        REG32(STM32_DBGMCU_BASE + 0x3C)
#define STM32_DBGMCU_APB1HFZ        REG32(STM32_DBGMCU_BASE + 0x44)
#define STM32_DBGMCU_APB2FZ         REG32(STM32_DBGMCU_BASE + 0x4C)
#define STM32_DBGMCU_APB4FZ         REG32(STM32_DBGMCU_BASE + 0x54)
/* Alias */
#define STM32_DBGMCU_APB1FZ         STM32_DBGMCU_APB1LFZ

/* --- Flash --- */

#define STM32_FLASH_REGS_BASE       0x52002000

#define STM32_FLASH_ACR             REG32(STM32_FLASH_REGS_BASE + 0x00)
#define STM32_FLASH_ACR_LATENCY_SHIFT (0)
#define STM32_FLASH_ACR_LATENCY_MASK  (7 << STM32_FLASH_ACR_LATENCY_SHIFT)
#define STM32_FLASH_ACR_WRHIGHFREQ_85MHZ  (0 << 4)
#define STM32_FLASH_ACR_WRHIGHFREQ_185MHZ (1 << 4)
#define STM32_FLASH_ACR_WRHIGHFREQ_285MHZ (2 << 4)
#define STM32_FLASH_ACR_WRHIGHFREQ_385MHZ (3 << 4)

/* --- Real-Time Clock --- */

#define STM32_RTC_BASE              0x58004000

#define STM32_RTC_TR                REG32(STM32_RTC_BASE + 0x00)
#define STM32_RTC_DR                REG32(STM32_RTC_BASE + 0x04)
#define STM32_RTC_CR                REG32(STM32_RTC_BASE + 0x08)
#define STM32_RTC_CR_BYPSHAD        (1 << 5)
#define STM32_RTC_CR_ALRAE          (1 << 8)
#define STM32_RTC_CR_ALRAIE         (1 << 12)
#define STM32_RTC_ISR               REG32(STM32_RTC_BASE + 0x0C)
#define STM32_RTC_ISR_ALRAWF        (1 << 0)
#define STM32_RTC_ISR_RSF           (1 << 5)
#define STM32_RTC_ISR_INITF         (1 << 6)
#define STM32_RTC_ISR_INIT          (1 << 7)
#define STM32_RTC_ISR_ALRAF         (1 << 8)
#define STM32_RTC_PRER              REG32(STM32_RTC_BASE + 0x10)
#define STM32_RTC_PRER_A_MASK       (0x7f << 16)
#define STM32_RTC_PRER_S_MASK       (0x7fff << 0)
#define STM32_RTC_WUTR              REG32(STM32_RTC_BASE + 0x14)
#define STM32_RTC_CALIBR            REG32(STM32_RTC_BASE + 0x18)
#define STM32_RTC_ALRMAR            REG32(STM32_RTC_BASE + 0x1C)
#define STM32_RTC_ALRMBR            REG32(STM32_RTC_BASE + 0x20)
#define STM32_RTC_WPR               REG32(STM32_RTC_BASE + 0x24)
#define STM32_RTC_SSR               REG32(STM32_RTC_BASE + 0x28)
#define STM32_RTC_TSTR              REG32(STM32_RTC_BASE + 0x30)
#define STM32_RTC_TSDR              REG32(STM32_RTC_BASE + 0x34)
#define STM32_RTC_TAFCR             REG32(STM32_RTC_BASE + 0x40)
#define STM32_RTC_ALRMASSR          REG32(STM32_RTC_BASE + 0x44)
#define STM32_RTC_BACKUP(n)         REG32(STM32_RTC_BASE + 0x50 + 4 * (n))

#define STM32_BKP_DATA(n)           STM32_RTC_BACKUP(n)
#define STM32_BKP_ENTRIES           32

/* --- SPI --- */
#define STM32_SPI1_BASE             0x40013000
#define STM32_SPI2_BASE             0x40003800
#define STM32_SPI3_BASE             0x40003c00 /* STM32F373, L4, F7, H7 */
#define STM32_SPI4_BASE             0x40013400 /* STM32F373, L4, F7, H7 */
#define STM32_SPI5_BASE             0x40015000 /* STM32H7 */

/* The SPI controller registers */
struct stm32_spi_regs {
	uint32_t cr1;
	uint32_t cr2;
	uint32_t cfg1;
	uint32_t cfg2;
	uint32_t ier;
	uint32_t sr;
	uint32_t ifcr;
	uint32_t _pad0;
	uint32_t txdr;
	uint32_t _pad1[3];
	uint32_t rxdr;
	uint32_t _pad2[3];
	uint32_t crcpoly;
	uint32_t rxcrcr;
	uint32_t txcrcr;
};
/* Must be volatile, or compiler optimizes out repeated accesses */
typedef volatile struct stm32_spi_regs stm32_spi_regs_t;

#define STM32_SPI1_REGS ((stm32_spi_regs_t *)STM32_SPI1_BASE)
#define STM32_SPI2_REGS ((stm32_spi_regs_t *)STM32_SPI2_BASE)
#define STM32_SPI3_REGS ((stm32_spi_regs_t *)STM32_SPI3_BASE)
#define STM32_SPI4_REGS ((stm32_spi_regs_t *)STM32_SPI4_BASE)

#define STM32_SPI_CR1_SPE		(1 << 0)
#define STM32_SPI_CR1_CSTART		(1 << 9)
#define STM32_SPI_CR1_SSI		(1 << 12)
#define STM32_SPI_CFG1_RXDMAEN		(1 << 14)
#define STM32_SPI_CFG1_TXDMAEN		(1 << 15)
#define STM32_SPI_CFG1_DATASIZE(n)	(((n) - 1) << 0)
#define STM32_SPI_CFG2_SSM		(1 << 26)
#define STM32_SPI_CFG2_MSTR		(1 << 22)

#define STM32_SPI_SR_RXNE		(1 << 0)
#define STM32_SPI_SR_FRLVL		(3 << 13)
#define STM32_SPI_SR_TXC		(1 << 12)

/* --- DMA --- */

#define STM32_DMA1_BASE             0x40020000
#define STM32_DMA2_BASE             0x40020400
#define STM32_DMAMUX1_BASE          0x40020800
#define STM32_MDMA_BASE             0x52000000
#define STM32_DMA2D_BASE            0x52001000
#define STM32_BDMA_BASE             0x58025400
#define STM32_DMAMUX2_BASE          0x58025800

/* DMAMUX1/2 registers */
#define DMAMUX1 0
#define DMAMUX2 1
#define STM32_DMAMUX_BASE(n)        ((n) ? STM32_DMAMUX2_BASE \
					 : STM32_DMAMUX1_BASE)
#define STM32_DMAMUX_REG32(n, off)  REG32(STM32_DMAMUX_BASE(n) + (off))
#define STM2_DMAMUX_CxCR(n, x)      STM32_DMAMUX_REG32(n, 4 * (x))
#define STM2_DMAMUX_CSR(n)          STM32_DMAMUX_REG32(n, 0x80)
#define STM2_DMAMUX_CFR(n)          STM32_DMAMUX_REG32(n, 0x84)
#define STM2_DMAMUX_RGxCR(n, x)     STM32_DMAMUX_REG32(n, 0x100 + 4 * (x))
#define STM2_DMAMUX_RGSR(n)         STM32_DMAMUX_REG32(n, 0x140)
#define STM2_DMAMUX_RGCFR(n)        STM32_DMAMUX_REG32(n, 0x144)

/*
 * Use the DMA through the DMA1/DMA2 controllers which are connected to all
 * peripherals (BDMA is connected only to a subset).
 * The requests are routed through DMAMUX1
 */
/* Always use stm32_dma_regs_t so volatile keyword is included! */
typedef volatile struct stm32_dma_regs stm32_dma_regs_t;

#define STM32_DMA1_REGS ((stm32_dma_regs_t *)STM32_DMA1_BASE)
#define STM32_DMA2_REGS ((stm32_dma_regs_t *)STM32_DMA2_BASE)

#define STM32_DMA_REGS(channel) \
	((channel) < STM32_DMAS_COUNT ? STM32_DMA1_REGS : STM32_DMA2_REGS)

/*
 * Available DMA streams, numbered from 0.
 *
 * Named channel to respect older interface, but a stream can serve
 * any channels, as long as they are in the same DMA controller.
 *
 * Stream 0 - 7 are managed by controller DMA1, 8 - 15 DMA2.
 */
enum dma_channel {
	/* Channel numbers */
	STM32_DMA1_STREAM0 = 0,
	STM32_DMA1_STREAM1 = 1,
	STM32_DMA1_STREAM2 = 2,
	STM32_DMA1_STREAM3 = 3,
	STM32_DMA1_STREAM4 = 4,
	STM32_DMA1_STREAM5 = 5,
	STM32_DMA1_STREAM6 = 6,
	STM32_DMA1_STREAM7 = 7,
	STM32_DMAS_COUNT = 8,
	STM32_DMA2_STREAM0 = 8,
	STM32_DMA2_STREAM1 = 9,
	STM32_DMA2_STREAM2 = 10,
	STM32_DMA2_STREAM3 = 11,
	STM32_DMA2_STREAM4 = 12,
	STM32_DMA2_STREAM5 = 13,
	STM32_DMA2_STREAM6 = 14,
	STM32_DMA2_STREAM7 = 15,

	/* Legacy naming for uart.c */
	STM32_DMAC_USART1_TX = STM32_DMA2_STREAM7,
	STM32_DMAC_USART1_RX = STM32_DMA2_STREAM5,

	/* Legacy naming for spi_master.c */
	STM32_DMAC_SPI1_TX = STM32_DMA2_STREAM3,
	STM32_DMAC_SPI1_RX = STM32_DMA2_STREAM0,
	STM32_DMAC_SPI2_TX = STM32_DMA1_STREAM4,
	STM32_DMAC_SPI2_RX = STM32_DMA1_STREAM3,
	STM32_DMAC_SPI3_TX = STM32_DMA1_STREAM7,
	STM32_DMAC_SPI3_RX = STM32_DMA1_STREAM0,
	STM32_DMAC_SPI4_TX = STM32_DMA2_STREAM1,
	STM32_DMAC_SPI4_RX = STM32_DMA2_STREAM4,
};
#define STM32_DMAS_TOTAL_COUNT 16

enum dmamux1_request {
	DMAMUX1_REQ_ADC1 = 9,
	DMAMUX1_REQ_ADC2 = 10,
	DMAMUX1_REQ_TIM1_CH1 = 11,
	DMAMUX1_REQ_TIM1_CH2 = 12,
	DMAMUX1_REQ_TIM1_CH3 = 13,
	DMAMUX1_REQ_TIM1_CH4 = 14,
	DMAMUX1_REQ_TIM1_UP = 15,
	DMAMUX1_REQ_TIM1_TRIG = 16,
	DMAMUX1_REQ_TIM1_COM = 17,
	DMAMUX1_REQ_TIM2_CH1 = 18,
	DMAMUX1_REQ_TIM2_CH2 = 19,
	DMAMUX1_REQ_TIM2_CH3 = 20,
	DMAMUX1_REQ_TIM2_CH4 = 21,
	DMAMUX1_REQ_TIM2_UP = 22,
	DMAMUX1_REQ_TIM3_CH1 = 23,
	DMAMUX1_REQ_TIM3_CH2 = 24,
	DMAMUX1_REQ_TIM3_CH3 = 25,
	DMAMUX1_REQ_TIM3_CH4 = 26,
	DMAMUX1_REQ_TIM3_UP = 27,
	DMAMUX1_REQ_TIM3_TRIG = 28,
	DMAMUX1_REQ_TIM4_CH1 = 29,
	DMAMUX1_REQ_TIM4_CH2 = 30,
	DMAMUX1_REQ_TIM4_CH3 = 31,
	DMAMUX1_REQ_TIM4_UP = 32,
	DMAMUX1_REQ_I2C1_RX = 33,
	DMAMUX1_REQ_I2C1_TX = 34,
	DMAMUX1_REQ_I2C2_RX = 35,
	DMAMUX1_REQ_I2C2_TX = 36,
	DMAMUX1_REQ_SPI1_RX = 37,
	DMAMUX1_REQ_SPI1_TX = 38,
	DMAMUX1_REQ_SPI2_RX = 39,
	DMAMUX1_REQ_SPI2_TX = 40,
	DMAMUX1_REQ_USART1_RX = 41,
	DMAMUX1_REQ_USART1_TX = 42,
	DMAMUX1_REQ_USART2_RX = 43,
	DMAMUX1_REQ_USART2_TX = 44,
	DMAMUX1_REQ_USART3_RX = 45,
	DMAMUX1_REQ_USART3_TX = 46,
	DMAMUX1_REQ_TIM8_CH1 = 47,
	DMAMUX1_REQ_TIM8_CH2 = 48,
	DMAMUX1_REQ_TIM8_CH3 = 49,
	DMAMUX1_REQ_TIM8_CH4 = 50,
	DMAMUX1_REQ_TIM8_UP = 51,
	DMAMUX1_REQ_TIM8_TRIG = 52,
	DMAMUX1_REQ_TIM8_COM = 53,
	DMAMUX1_REQ_TIM5_CH1 = 55,
	DMAMUX1_REQ_TIM5_CH2 = 56,
	DMAMUX1_REQ_TIM5_CH3 = 57,
	DMAMUX1_REQ_TIM5_CH4 = 58,
	DMAMUX1_REQ_TIM5_UP = 59,
	DMAMUX1_REQ_TIM5_TRIG = 60,
	DMAMUX1_REQ_SPI3_RX = 61,
	DMAMUX1_REQ_SPI3_TX = 62,
	DMAMUX1_REQ_UART4_RX = 63,
	DMAMUX1_REQ_UART4_TX = 64,
	DMAMUX1_REQ_USART5_RX = 65,
	DMAMUX1_REQ_UART5_TX = 66,
	DMAMUX1_REQ_DAC1 = 67,
	DMAMUX1_REQ_DAC2 = 68,
	DMAMUX1_REQ_TIM6_UP = 69,
	DMAMUX1_REQ_TIM7_UP = 70,
	DMAMUX1_REQ_USART6_RX = 71,
	DMAMUX1_REQ_USART6_TX = 72,
	DMAMUX1_REQ_I2C3_RX = 73,
	DMAMUX1_REQ_I2C3_TX = 74,
	DMAMUX1_REQ_DCMI = 75,
	DMAMUX1_REQ_CRYP_IN = 76,
	DMAMUX1_REQ_CRYP_OUT = 77,
	DMAMUX1_REQ_HASH_IN = 78,
	DMAMUX1_REQ_UART7_RX = 79,
	DMAMUX1_REQ_UART7_TX = 80,
	DMAMUX1_REQ_UART8_RX = 81,
	DMAMUX1_REQ_UART8_TX = 82,
	DMAMUX1_REQ_SPI4_RX = 83,
	DMAMUX1_REQ_SPI4_TX = 84,
	DMAMUX1_REQ_SPI5_RX = 85,
	DMAMUX1_REQ_SPI5_TX = 86,
	DMAMUX1_REQ_SAI1_A = 87,
	DMAMUX1_REQ_SAI1_B = 88,
	DMAMUX1_REQ_SAI2_A = 89,
	DMAMUX1_REQ_SAI2_B = 90,
	DMAMUX1_REQ_SWPMI_RX = 91,
	DMAMUX1_REQ_SWPMI_TX = 92,
	DMAMUX1_REQ_SPDIFRX_DT = 93,
	DMAMUX1_REQ_SPDIFRX_CS = 94,
	DMAMUX1_REQ_TIM15_CH1 = 105,
	DMAMUX1_REQ_TIM15_UP = 106,
	DMAMUX1_REQ_TIM15_TRIG = 107,
	DMAMUX1_REQ_TIM15_COM = 108,
	DMAMUX1_REQ_TIM16_CH1 = 109,
	DMAMUX1_REQ_TIM16_UP = 110,
	DMAMUX1_REQ_TIM17_CH1 = 111,
	DMAMUX1_REQ_TIM17_UP = 112,
	DMAMUX1_REQ_SAI3_A = 113,
	DMAMUX1_REQ_SAI3_B = 114,
	DMAMUX1_REQ_ADC3 = 115,
};

/* Registers for a single stream of a DMA1/DMA2 controller */
struct stm32_dma_stream {
	uint32_t	scr;		/* Control */
	uint32_t	sndtr;		/* Number of data to transfer */
	uint32_t	spar;		/* Peripheral address */
	uint32_t	sm0ar;		/* Memory address 0 */
	uint32_t	sm1ar;		/*  address 1 for double buffer */
	uint32_t	sfcr;		/* FIFO control */
};

/* Always use stm32_dma_stream_t so volatile keyword is included! */
typedef volatile struct stm32_dma_stream stm32_dma_stream_t;
/* Common code and header file must use this */
typedef stm32_dma_stream_t dma_chan_t;

/* Registers for the DMA1/DMA2 controller */
struct stm32_dma_regs {
	uint32_t	isr[2];
	uint32_t	ifcr[2];
	stm32_dma_stream_t stream[STM32_DMAS_COUNT];
};

#define STM32_DMA_CCR_EN                (1 << 0)
#define STM32_DMA_CCR_DMEIE             (1 << 1)
#define STM32_DMA_CCR_TEIE              (1 << 2)
#define STM32_DMA_CCR_HTIE              (1 << 3)
#define STM32_DMA_CCR_TCIE              (1 << 4)
#define STM32_DMA_CCR_PFCTRL            (1 << 5)
#define STM32_DMA_CCR_DIR_P2M		(0 << 6)
#define STM32_DMA_CCR_DIR_M2P		(1 << 6)
#define STM32_DMA_CCR_DIR_M2M		(2 << 6)
#define STM32_DMA_CCR_CIRC              (1 << 8)
#define STM32_DMA_CCR_PINC              (1 << 9)
#define STM32_DMA_CCR_MINC              (1 << 10)
#define STM32_DMA_CCR_PSIZE_8_BIT       (0 << 11)
#define STM32_DMA_CCR_PSIZE_16_BIT      (1 << 11)
#define STM32_DMA_CCR_PSIZE_32_BIT      (2 << 11)
#define STM32_DMA_CCR_MSIZE_8_BIT       (0 << 13)
#define STM32_DMA_CCR_MSIZE_16_BIT      (1 << 13)
#define STM32_DMA_CCR_MSIZE_32_BIT      (2 << 13)
#define STM32_DMA_CCR_PINCOS            (1 << 15)
#define STM32_DMA_CCR_PL_LOW            (0 << 16)
#define STM32_DMA_CCR_PL_MEDIUM         (1 << 16)
#define STM32_DMA_CCR_PL_HIGH           (2 << 16)
#define STM32_DMA_CCR_PL_VERY_HIGH      (3 << 16)
#define STM32_DMA_CCR_DBM               (1 << 18)
#define STM32_DMA_CCR_CT                (1 << 19)
#define STM32_DMA_CCR_PBURST(b_len)		 ((((b_len) - 4) / 4) << 21)
#define STM32_DMA_CCR_MBURST(b_len)		 ((((b_len) - 4) / 4) << 21)
#define STM32_DMA_CCR_CHANNEL(channel)		 (0)
#define STM32_DMA_CCR_CHANNEL_MASK		 (0 << 25)
#if 0
#define STM32_DMA_CCR_CHANNEL_MASK		 (0x7 << 25)
#define STM32_DMA_CCR_CHANNEL(channel)		 ((channel) << 25)
#endif
#define STM32_DMA_CCR_RSVD_MASK		(0xF0100000)

#define STM32_DMA_SFCR_DMDIS		(1 << 2)
#define STM32_DMA_SFCR_FTH(level)	(((level) - 1) << 0)

#define STM32_DMA_CH_LOCAL(channel)     ((channel) % STM32_DMAS_COUNT)
#define STM32_DMA_CH_LH(channel)        \
	((STM32_DMA_CH_LOCAL(channel) < 4) ? 0 : 1)
#define STM32_DMA_CH_OFFSET(channel)    \
	(((STM32_DMA_CH_LOCAL(channel) % 4) * 6) + \
	(((STM32_DMA_CH_LOCAL(channel) % 4) >= 2) ? 4 : 0))
#define STM32_DMA_CH_GETBITS(channel, val) \
	(((val) >> STM32_DMA_CH_OFFSET(channel)) & 0x3f)
#define STM32_DMA_GET_IFCR(channel)      \
	(STM32_DMA_CH_GETBITS(channel,   \
	STM32_DMA_REGS(channel)->ifcr[STM32_DMA_CH_LH(channel)]))
#define STM32_DMA_GET_ISR(channel)       \
	(STM32_DMA_CH_GETBITS(channel,   \
	STM32_DMA_REGS(channel)->isr[STM32_DMA_CH_LH(channel)]))

#define STM32_DMA_SET_IFCR(channel, val) \
	(STM32_DMA_REGS(channel)->ifcr[STM32_DMA_CH_LH(channel)] = \
	(STM32_DMA_REGS(channel)->ifcr[STM32_DMA_CH_LH(channel)] & \
	~(0x3f << STM32_DMA_CH_OFFSET(channel))) | \
	(((val) & 0x3f) << STM32_DMA_CH_OFFSET(channel)))
#define STM32_DMA_SET_ISR(channel, val) \
	(STM32_DMA_REGS(channel)->isr[STM32_DMA_CH_LH(channel)] = \
	(STM32_DMA_REGS(channel)->isr[STM32_DMA_CH_LH(channel)] & \
	~(0x3f << STM32_DMA_CH_OFFSET(channel))) | \
	(((val) & 0x3f) << STM32_DMA_CH_OFFSET(channel)))

#define STM32_DMA_FEIF                  (1 << 0)
#define STM32_DMA_DMEIF                 (1 << 2)
#define STM32_DMA_TEIF                  (1 << 3)
#define STM32_DMA_HTIF                  (1 << 4)
#define STM32_DMA_TCIF                  (1 << 5)
#define STM32_DMA_ALL                   0x3d

/* --- Watchdog --- */

#define STM32_IWDG_BASE             0x58004800

#define STM32_IWDG_KR               REG32(STM32_IWDG_BASE + 0x00)
#define STM32_IWDG_KR_UNLOCK            0x5555
#define STM32_IWDG_KR_RELOAD            0xaaaa
#define STM32_IWDG_KR_START             0xcccc
#define STM32_IWDG_PR               REG32(STM32_IWDG_BASE + 0x04)
#define STM32_IWDG_RLR              REG32(STM32_IWDG_BASE + 0x08)
#define STM32_IWDG_RLR_MAX              0x0fff
#define STM32_IWDG_SR               REG32(STM32_IWDG_BASE + 0x0C)
#define STM32_IWDG_WINR             REG32(STM32_IWDG_BASE + 0x10)


/* --- MISC --- */
#define STM32_UNIQUE_ID_ADDRESS     REG32_ADDR(0x1ff1e800)
#define STM32_UNIQUE_ID_LENGTH      (3 * 4)

#endif /* __CROS_EC_REGISTERS_STM32H7_H */
