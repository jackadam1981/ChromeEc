/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for STM32F processor
 */

#ifndef __STM32_REGISTERS
#error "Do not #include this file directly, #include registers.h instead"
#endif

#include <stdint.h>

/* concatenation helper */
#define STM32F_CAT(prefix, n, suffix) prefix ## n ## suffix

/* Macros to access registers */
#define REG32(addr) (*(volatile uint32_t*)(addr))
#define REG16(addr) (*(volatile uint16_t*)(addr))

/* IRQ numbers */
#define STM32F_IRQ_WWDG                0
#define STM32F_IRQ_PVD                 1
#define STM32F_IRQ_TAMPER_STAMP        2
#define STM32F_IRQ_RTC_WAKEUP          3
#define STM32F_IRQ_FLASH               4
#define STM32F_IRQ_RCC                 5
#define STM32F_IRQ_EXTI0               6
#define STM32F_IRQ_EXTI1               7
#define STM32F_IRQ_EXTI2               8
#define STM32F_IRQ_EXTI3               9
#define STM32F_IRQ_EXTI4              10
#define STM32F_IRQ_DMA_CHANNEL_1      11
#define STM32F_IRQ_DMA_CHANNEL_2      12
#define STM32F_IRQ_DMA_CHANNEL_3      13
#define STM32F_IRQ_DMA_CHANNEL_4      14
#define STM32F_IRQ_DMA_CHANNEL_5      15
#define STM32F_IRQ_DMA_CHANNEL_6      16
#define STM32F_IRQ_DMA_CHANNEL_7      17
#define STM32F_IRQ_ADC_1              18
/* positions 19-22 are reserved on STM32F */
#define STM32F_IRQ_EXTI9_5            23
#define STM32F_IRQ_TIM1_BRK_TIM15     24
#define STM32F_IRQ_TIM1_UP_TIM16      25
#define STM32F_IRQ_TIM1_TRG_COM_TIM17 26
#define STM32F_IRQ_TIM1_CC            27
#define STM32F_IRQ_TIM2               28
#define STM32F_IRQ_TIM3               29
#define STM32F_IRQ_TIM4               30
#define STM32F_IRQ_I2C1_EV            31
#define STM32F_IRQ_I2C1_ER            32
#define STM32F_IRQ_I2C2_EV            33
#define STM32F_IRQ_I2C2_ER            34
#define STM32F_IRQ_SPI1               35
#define STM32F_IRQ_SPI2               36
#define STM32F_IRQ_USART1             37
#define STM32F_IRQ_USART2             38
#define STM32F_IRQ_USART3             39
#define STM32F_IRQ_EXTI15_10          40
#define STM32F_IRQ_RTC_ALARM          41
#define STM32F_IRQ_CEC                42
#define STM32F_IRQ_TIM12              43
#define STM32F_IRQ_TIM13              44
#define STM32F_IRQ_TIM14              45
/* positions 46-47 are reserved on STM32F */
#define STM32F_IRQ_FSMC               48
/* position 49 is reserved on STM32F */
#define STM32F_IRQ_TIM5               50
#define STM32F_IRQ_SPI3               51
#define STM32F_IRQ_UART4              52
#define STM32F_IRQ_UART5              53
#define STM32F_IRQ_TIM6_DAC           54
#define STM32F_IRQ_TIM7               55
#define STM32F_IRQ_DMA2_CHANNEL1      56
#define STM32F_IRQ_DMA2_CHANNEL2      57
#define STM32F_IRQ_DMA2_CHANNEL3      58
#define STM32F_IRQ_DMA2_CHANNEL4_5    59
#define STM32F_IRQ_DMA2_CHANNEL5      60	/* if MISC_REMAP bits are set */

/* --- USART --- */
#define STM32F_USART1_BASE          0x40013800
#define STM32F_USART2_BASE          0x40004400
#define STM32F_USART3_BASE          0x40004800

#define STM32F_USART_BASE(n)        STM32F_CAT(STM32F_USART, n, _BASE)

#define STM32F_USART_REG(n, offset) \
		REG16(STM32F_CAT(STM32F_USART, n, _BASE) + (offset))

#define STM32F_USART_SR(n)          STM32F_USART_REG(n, 0x00)
#define STM32F_USART_DR(n)          STM32F_USART_REG(n, 0x04)
#define STM32F_USART_BRR(n)         STM32F_USART_REG(n, 0x08)
#define STM32F_USART_CR1(n)         STM32F_USART_REG(n, 0x0C)
#define STM32F_USART_CR2(n)         STM32F_USART_REG(n, 0x10)
#define STM32F_USART_CR3(n)         STM32F_USART_REG(n, 0x14)
#define STM32F_USART_GTPR(n)        STM32F_USART_REG(n, 0x18)

#define STM32F_IRQ_USART(n)         STM32F_CAT(STM32F_IRQ_USART, n, )

#if defined(STM32F_HIGH_DENSITY)
/* --- UART --- */
#define STM32F_UART4_BASE           0x40004c00
#define STM32F_UART5_BASE           0x40005000
#endif

/* --- TIMERS --- */
#define STM32F_TIM1_BASE            0x40012c00
#define STM32F_TIM2_BASE            0x40000000
#define STM32F_TIM3_BASE            0x40000400
#define STM32F_TIM4_BASE            0x40000800
#define STM32F_TIM5_BASE            0x40000c00
#define STM32F_TIM6_BASE            0x40001000
#define STM32F_TIM7_BASE            0x40001400
#if defined(STM32F_HIGH_DENSITY)
#define STM32F_TIM12_BASE           0x40001800
#define STM32F_TIM13_BASE           0x40001c00
#define STM32F_TIM14_BASE           0x40002000
#endif
#define STM32F_TIM15_BASE           0x40014000
#define STM32F_TIM16_BASE           0x40014400
#define STM32F_TIM17_BASE           0x40014800

#define STM32F_TIM_REG(n, offset) \
		REG16(STM32F_CAT(STM32F_TIM, n, _BASE) + (offset))

#define STM32F_TIM_CR1(n)           STM32F_TIM_REG(n, 0x00)
#define STM32F_TIM_CR2(n)           STM32F_TIM_REG(n, 0x04)
#define STM32F_TIM_SMCR(n)          STM32F_TIM_REG(n, 0x08)
#define STM32F_TIM_DIER(n)          STM32F_TIM_REG(n, 0x0C)
#define STM32F_TIM_SR(n)            STM32F_TIM_REG(n, 0x10)
#define STM32F_TIM_EGR(n)           STM32F_TIM_REG(n, 0x14)
#define STM32F_TIM_CCMR1(n)         STM32F_TIM_REG(n, 0x18)
#define STM32F_TIM_CCMR2(n)         STM32F_TIM_REG(n, 0x1C)
#define STM32F_TIM_CCER(n)          STM32F_TIM_REG(n, 0x20)
#define STM32F_TIM_CNT(n)           STM32F_TIM_REG(n, 0x24)
#define STM32F_TIM_PSC(n)           STM32F_TIM_REG(n, 0x28)
#define STM32F_TIM_ARR(n)           STM32F_TIM_REG(n, 0x2C)
#define STM32F_TIM_CCR1(n)          STM32F_TIM_REG(n, 0x34)
#define STM32F_TIM_CCR2(n)          STM32F_TIM_REG(n, 0x38)
#define STM32F_TIM_CCR3(n)          STM32F_TIM_REG(n, 0x3C)
#define STM32F_TIM_CCR4(n)          STM32F_TIM_REG(n, 0x40)
#define STM32F_TIM_DCR(n)           STM32F_TIM_REG(n, 0x48)
#define STM32F_TIM_DMAR(n)          STM32F_TIM_REG(n, 0x4C)

/* --- GPIO --- */
#define STM32F_GPIOA_BASE            0x40010800
#define STM32F_GPIOB_BASE            0x40010c00
#define STM32F_GPIOC_BASE            0x40011000
#define STM32F_GPIOD_BASE            0x40011400
#define STM32F_GPIOE_BASE            0x40011800
#if defined(STM32F_HIGH_DENSITY)
#define STM32F_GPIOF_BASE            0x4001c000
#define STM32F_GPIOG_BASE            0x40012000
#endif

#define GPIO_A                       STM32F_GPIOA_BASE
#define GPIO_B                       STM32F_GPIOB_BASE
#define GPIO_C                       STM32F_GPIOC_BASE
#define GPIO_D                       STM32F_GPIOD_BASE
#define GPIO_E                       STM32F_GPIOE_BASE
#define GPIO_F                       STM32F_GPIOF_BASE
#define GPIO_G                       STM32F_GPIOG_BASE

#define STM32F_GPIO_REG32(l, offset) \
		REG32(STM32F_CAT(STM32F_GPIO, l, _BASE) + (offset))
#define STM32F_GPIO_REG16(l, offset) \
		REG16(STM32F_CAT(STM32F_GPIO, l, _BASE) + (offset))

#define STM32F_GPIO_CRL(l)           STM32F_GPIO_REG32(l, 0x00)
#define STM32F_GPIO_CRH(l)           STM32F_GPIO_REG32(l, 0x04)
#define STM32F_GPIO_IDR(l)           STM32F_GPIO_REG16(l, 0x08)
#define STM32F_GPIO_ODR(l)           STM32F_GPIO_REG16(l, 0x0c)
#define STM32F_GPIO_BSRR(l)          STM32F_GPIO_REG32(l, 0x10)
#define STM32F_GPIO_BRR(l)           STM32F_GPIO_REG16(l, 0x14)
#define STM32F_GPIO_LCKR(l)          STM32F_GPIO_REG16(l, 0x18)

#define STM32F_GPIO_CRL_OFF(b)       REG32((b) + 0x00)
#define STM32F_GPIO_CRH_OFF(b)       REG32((b) + 0x04)
#define STM32F_GPIO_IDR_OFF(b)       REG16((b) + 0x08)
#define STM32F_GPIO_ODR_OFF(b)       REG16((b) + 0x0c)
#define STM32F_GPIO_BSRR_OFF(b)      REG32((b) + 0x10)
#define STM32F_GPIO_BRR_OFF(b)       REG32((b) + 0x14)
#define STM32F_GPIO_LCKR_OFF(b)      REG32((b) + 0x18)

#define STM32F_GPIO_AFIO_EVCR_OFF(b) REG16((b) + 0x00)
#define STM32F_GPIO_AFIO_MAPR_OFF(b) REG32((b) + 0x04)
#define STM32F_GPIO_AFIO_EXTICR1(b)  REG16((b) + 0x08)
#define STM32F_GPIO_AFIO_EXTICR2(b)  REG16((b) + 0x0c)
#define STM32F_GPIO_AFIO_EXTICR3(b)  REG16((b) + 0x10)
#define STM32F_GPIO_AFIO_EXTICR4(b)  REG16((b) + 0x14)
#define STM32F_GPIO_AFIO_MAPR2(b)    REG16((b) + 0x1c)

/* FIXME: alternate function mapping on stm32f is... different. */
#if 0
#define GPIO_ALT_SYS                 0x0
#define GPIO_ALT_TIM2                0x1
#define GPIO_ALT_TIM3_4              0x2
#define GPIO_ALT_TIM9_11             0x3
#define GPIO_ALT_I2C                 0x4
#define GPIO_ALT_SPI                 0x5
#define GPIO_ALT_USART               0x7
#define GPIO_ALT_USB                 0xA
#define GPIO_ALT_LCD                 0xB
#define GPIO_ALT_RI                  0xE
#define GPIO_ALT_EVENTOUT            0xF
#endif

/* --- I2C --- */
#define STM32F_I2C1_BASE             0x40005400
#define STM32F_I2C2_BASE             0x40005800

#define STM32F_I2C_REG(n, offset) \
		REG16(STM32F_CAT(STM32F_I2C, n, _BASE) + (offset))

#define STM32F_I2C_CR1(n)            STM32F_I2C_REG(n, 0x00)
#define STM32F_I2C_CR2(n)            STM32F_I2C_REG(n, 0x04)
#define STM32F_I2C_OAR1(n)           STM32F_I2C_REG(n, 0x08)
#define STM32F_I2C_OAR2(n)           STM32F_I2C_REG(n, 0x0C)
#define STM32F_I2C_DR(n)             STM32F_I2C_REG(n, 0x10)
#define STM32F_I2C_SR1(n)            STM32F_I2C_REG(n, 0x14)
#define STM32F_I2C_SR2(n)            STM32F_I2C_REG(n, 0x18)
#define STM32F_I2C_CCR(n)            STM32F_I2C_REG(n, 0x1C)
#define STM32F_I2C_TRISE(n)          STM32F_I2C_REG(n, 0x20)

/* --- Power / Reset / Clocks --- */
#define STM32F_PWR_BASE              0x40007000

#define STM32F_PWR_CR                REG32(STM32F_PWR_BASE + 0x00)
#define STM32F_PWR_CSR               REG32(STM32F_PWR_BASE + 0x04)

#define STM32F_RCC_BASE              0x40021000

#define STM32F_RCC_CR                REG32(STM32F_RCC_BASE + 0x00)
#define STM32F_RCC_CFGR              REG32(STM32F_RCC_BASE + 0x04)
#define STM32F_RCC_CIR               REG32(STM32F_RCC_BASE + 0x08)
#define STM32F_RCC_APB2RSTR          REG32(STM32F_RCC_BASE + 0x0c)
#define STM32F_RCC_APB1RSTR          REG32(STM32F_RCC_BASE + 0x10)
#define STM32F_RCC_AHBENR            REG32(STM32F_RCC_BASE + 0x14)
#define STM32F_RCC_APB2ENR           REG32(STM32F_RCC_BASE + 0x18)
#define STM32F_RCC_APB1ENR           REG32(STM32F_RCC_BASE + 0x1c)
#define STM32F_RCC_BDCR              REG32(STM32F_RCC_BASE + 0x20)
#define STM32F_RCC_CSR               REG32(STM32F_RCC_BASE + 0x24)
#define STM32F_RCC_CFGR2             REG32(STM32F_RCC_BASE + 0x2c)

/* --- Watchdogs --- */

#define STM32F_WWDG_BASE             0x40002C00

#define STM32F_WWDG_CR               REG32(STM32F_WWDG_BASE + 0x00)
#define STM32F_WWDG_CFR              REG32(STM32F_WWDG_BASE + 0x04)
#define STM32F_WWDG_SR               REG32(STM32F_WWDG_BASE + 0x08)

#define STM32F_IWDG_BASE             0x40003000

#define STM32F_IWDG_KR               REG32(STM32F_IWDG_BASE + 0x00)
#define STM32F_IWDG_PR               REG32(STM32F_IWDG_BASE + 0x04)
#define STM32F_IWDG_RLR              REG32(STM32F_IWDG_BASE + 0x08)
#define STM32F_IWDG_SR               REG32(STM32F_IWDG_BASE + 0x0C)

/* --- Real-Time Clock --- */

#define STM32F_RTC_BASE              0x40002800

#define STM32F_RTC_CRH               REG32(STM32F_RTC_BASE + 0x00)
#define STM32F_RTC_CRL               REG32(STM32F_RTC_BASE + 0x04)
#define STM32F_RTC_PRLH              REG32(STM32F_RTC_BASE + 0x08)
#define STM32F_RTC_PRLL              REG16(STM32F_RTC_BASE + 0x0c)
#define STM32F_RTC_DIVH              REG16(STM32F_RTC_BASE + 0x10)
#define STM32F_RTC_DIVL              REG16(STM32F_RTC_BASE + 0x14)
#define STM32F_RTC_CNTH              REG16(STM32F_RTC_BASE + 0x18)
#define STM32F_RTC_CNTL              REG16(STM32F_RTC_BASE + 0x1c)
#define STM32F_RTC_ALRH              REG16(STM32F_RTC_BASE + 0x20)
#define STM32F_RTC_ALRL              REG16(STM32F_RTC_BASE + 0x24)

/* --- SPI --- */
#define STM32F_SPI1_BASE             0x40013000
#define STM32F_SPI2_BASE             0x40003800
#if defined(STM32F_HIGH_DENSITY)
#define STM32F_SPI3_BASE             0x40003c00
#endif

#define STM32F_SPI1_PORT             0
#define STM32F_SPI2_PORT             1

/*
 * TODO(vpalatin):
 * For whatever reason, our toolchain is substandard and generate a
 * function every time you are using this inline function.
 *
 * That's why I have not used inline stuff in the registers definition.
 */
#define STM32F_spi_addr(port, offset) \
	((port == 0) ? \
		(STM32F_SPI1_BASE + offset) : \
		(STM32F_SPI2_BASE + offset))

#define STM32F_SPI_REG16(p, l)       REG16(STM32F_spi_addr((p), l))
#define STM32F_SPI_CR1(p)            STM32F_SPI_REG16((p), 0x00)
#define STM32F_SPI_CR2(p)            STM32F_SPI_REG16((p), 0x04)
#define STM32F_SPI_SR(p)             STM32F_SPI_REG16((p), 0x08)
#define STM32F_SPI_DR(p)             STM32F_SPI_REG16((p), 0x0c)
#define STM32F_SPI_CRCPR(p)          STM32F_SPI_REG16((p), 0x10)
#define STM32F_SPI_RXCRCR(p)         STM32F_SPI_REG16((p), 0x14)
#define STM32F_SPI_TXCRCR(p)         STM32F_SPI_REG16((p), 0x18)

/* --- Debug --- */

#define STM32F_DBGMCU_BASE           0xE0042000

#define STM32F_DBGMCU_IDCODE         REG32(STM32F_DBGMCU_BASE + 0x00)
#define STM32F_DBGMCU_CR             REG32(STM32F_DBGMCU_BASE + 0x04)

/* --- Flash --- */

#define STM32F_FLASH_REGS_BASE       0x40022000

#define STM32F_FLASH_ACR             REG32(STM32F_FLASH_REGS_BASE + 0x00)
#define STM32F_FLASH_KEYR            REG32(STM32F_FLASH_REGS_BASE + 0x04)
#define STM32F_FLASH_OPTKEYR         REG32(STM32F_FLASH_REGS_BASE + 0x08)
#define STM32F_FLASH_SR              REG32(STM32F_FLASH_REGS_BASE + 0x0c)
#define STM32F_FLASH_CR              REG32(STM32F_FLASH_REGS_BASE + 0x10)
#define STM32F_FLASH_AR              REG32(STM32F_FLASH_REGS_BASE + 0x14)
#define STM32F_FLASH_OBR             REG32(STM32F_FLASH_REGS_BASE + 0x1c)
#define STM32F_FLASH_WRPR            REG32(STM32F_FLASH_REGS_BASE + 0x20)

/* --- External Interrupts --- */
#define STM32F_EXTI_BASE             0x40010400

#define STM32F_EXTI_IMR              REG32(STM32F_EXTI_BASE + 0x00)
#define STM32F_EXTI_EMR              REG32(STM32F_EXTI_BASE + 0x04)
#define STM32F_EXTI_RTSR             REG32(STM32F_EXTI_BASE + 0x08)
#define STM32F_EXTI_FTSR             REG32(STM32F_EXTI_BASE + 0x0c)
#define STM32F_EXTI_SWIER            REG32(STM32F_EXTI_BASE + 0x10)
#define STM32F_EXTI_PR               REG32(STM32F_EXTI_BASE + 0x14)

/* --- MISC --- */

#define STM32F_RI_BASE               0x40007C04
#define STM32F_ADC1_BASE             0x40012400
#define STM32F_COMP_BASE             0x40007C00
#define STM32F_CEC_BASE              0x40007800
#define STM32F_DAC_BASE              0x40007400
#define STM32F_BKP_BASE              0x40006c00
#define STM32F_CRC_BASE              0x40023000
#define STM32F_LCD_BASE              0x40002400
#define STM32F_DMA1_BASE             0x40020000
#if defined(STM32F_HIGH_DENSITY)
#define STM32F_DMA2_BASE             0x40020400
#endif
