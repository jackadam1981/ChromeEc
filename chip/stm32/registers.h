/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Register map for the STM32 family of chips
 *
 * This header file should only contain register definitions and
 * functionality that are common to all STM32 chips.
 * Any chip/family specific macros must be placed in their family
 * specific registers file, which is conditionally included at the
 * end of this file.
 * Include this file directly for all STM32 register definitions.
 *
 * ### History and Reasoning ###
 * In a time before chip family register file separation,
 * long long ago, there lived a single file called `registers.h`,
 * which housed register definitions for all STM32 chip family and variants.
 * This poor file was 3000 lines of register macros and C definitions,
 * swiss-cheesed by nested preprocessor conditional logic.
 * Adding a single new chip variant required splitting multiple,
 * already nested, conditional sections throughout the file.
 * Readability was on the difficult side and refactoring was dangerous.
 *
 * The number of STM32 variants had outgrown the single registers file model.
 * The minor gains of sharing a set of registers between a subset of chip
 * variants no longer outweighed the complexity of the following operations:
 * - Adding a new chip variant or variant feature
 * - Determining if a register was properly setup for a variant or if it
 *   was simply not unset
 *
 * To strike a balance between shared registers and chip specific registers,
 * the registers.h file remains a place for common definitions, but family
 * specific definitions were moved to their own files.
 * These family specific files contain a much reduced level of preprocessor
 * logic for variant specific registers.
 *
 * See https://crrev.com/c/1674679 to witness the separation steps.
 */

#ifndef __CROS_EC_REGISTERS_H
#define __CROS_EC_REGISTERS_H

#include "common.h"
#include "compile_time_macros.h"


#ifndef __ASSEMBLER__

/* Register definitions */

/* --- USART --- */
#define STM32_USART_BASE(n)           CONCAT3(STM32_USART, n, _BASE)
#define STM32_USART_REG(base, offset) REG32((base) + (offset))

#define STM32_IRQ_USART(n)         CONCAT2(STM32_IRQ_USART, n)

/* --- TIMERS --- */
#define STM32_TIM_BASE(n)          CONCAT3(STM32_TIM, n, _BASE)

#define STM32_TIM_REG(n, offset) \
		REG16(STM32_TIM_BASE(n) + (offset))
#define STM32_TIM_REG32(n, offset) \
		REG32(STM32_TIM_BASE(n) + (offset))

#define STM32_TIM_CR1(n)           STM32_TIM_REG(n, 0x00)
#define STM32_TIM_CR1_CEN		BIT(0)
#define STM32_TIM_CR2(n)           STM32_TIM_REG(n, 0x04)
#define STM32_TIM_SMCR(n)          STM32_TIM_REG(n, 0x08)
#define STM32_TIM_DIER(n)          STM32_TIM_REG(n, 0x0C)
#define STM32_TIM_SR(n)            STM32_TIM_REG(n, 0x10)
#define STM32_TIM_EGR(n)           STM32_TIM_REG(n, 0x14)
#define STM32_TIM_EGR_UG		BIT(0)
#define STM32_TIM_CCMR1(n)         STM32_TIM_REG(n, 0x18)
#define STM32_TIM_CCMR1_OC1PE		BIT(2)
/* Use in place of TIM_CCMR1_OC1M_0 through 2 from STM documentation. */
#define STM32_TIM_CCMR1_OC1M(n)    (((n) & 0x7) << 4)
#define STM32_TIM_CCMR1_OC1M_MASK              STM32_TIM_CCMR1_OC1M(~0)
#define STM32_TIM_CCMR1_OC1M_FROZEN            STM32_TIM_CCMR1_OC1M(0x0)
#define STM32_TIM_CCMR1_OC1M_ACTIVE_ON_MATCH   STM32_TIM_CCMR1_OC1M(0x1)
#define STM32_TIM_CCMR1_OC1M_INACTIVE_ON_MATCH STM32_TIM_CCMR1_OC1M(0x2)
#define STM32_TIM_CCMR1_OC1M_TOGGLE            STM32_TIM_CCMR1_OC1M(0x3)
#define STM32_TIM_CCMR1_OC1M_FORCE_INACTIVE    STM32_TIM_CCMR1_OC1M(0x4)
#define STM32_TIM_CCMR1_OC1M_FORCE_ACTIVE      STM32_TIM_CCMR1_OC1M(0x5)
#define STM32_TIM_CCMR1_OC1M_PWM_MODE_1        STM32_TIM_CCMR1_OC1M(0x6)
#define STM32_TIM_CCMR1_OC1M_PWM_MODE_2        STM32_TIM_CCMR1_OC1M(0x7)
#define STM32_TIM_CCMR2(n)         STM32_TIM_REG(n, 0x1C)
#define STM32_TIM_CCER(n)          STM32_TIM_REG(n, 0x20)
#define STM32_TIM_CCER_CC1E		BIT(0)
#define STM32_TIM_CCER_CC1P		BIT(1)
#define STM32_TIM_CCER_CC1NE		BIT(2)
#define STM32_TIM_CCER_CC1NP		BIT(3)
#define STM32_TIM_CNT(n)           STM32_TIM_REG(n, 0x24)
#define STM32_TIM_PSC(n)           STM32_TIM_REG(n, 0x28)
#define STM32_TIM_ARR(n)           STM32_TIM_REG(n, 0x2C)
#define STM32_TIM_RCR(n)           STM32_TIM_REG(n, 0x30)
#define STM32_TIM_CCR1(n)          STM32_TIM_REG(n, 0x34)
#define STM32_TIM_CCR2(n)          STM32_TIM_REG(n, 0x38)
#define STM32_TIM_CCR3(n)          STM32_TIM_REG(n, 0x3C)
#define STM32_TIM_CCR4(n)          STM32_TIM_REG(n, 0x40)
#define STM32_TIM_BDTR(n)          STM32_TIM_REG(n, 0x44)
#define STM32_TIM_BDTR_MOE		BIT(15)
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
	unsigned cr1;
	unsigned cr2;
	unsigned smcr;
	unsigned dier;

	unsigned sr;
	unsigned egr;
	unsigned ccmr1;
	unsigned ccmr2;

	unsigned ccer;
	unsigned cnt;
	unsigned psc;
	unsigned arr;

	unsigned ccr[5]; /* ccr[0] = reserved30 */

	unsigned bdtr;
	unsigned dcr;
	unsigned dmar;

	unsigned or;
};
/* Must be volatile, or compiler optimizes out repeated accesses */
typedef volatile struct timer_ctlr timer_ctlr_t;

/* --- Low power timers --- */
#define STM32_LPTIM_BASE(n)          CONCAT3(STM32_LPTIM, n, _BASE)

#define STM32_LPTIM_REG(n, offset)   REG32(STM32_LPTIM_BASE(n) + (offset))

#define STM32_LPTIM_ISR(n)           STM32_LPTIM_REG(n, 0x00)
#define STM32_LPTIM_ICR(n)           STM32_LPTIM_REG(n, 0x04)
#define STM32_LPTIM_IER(n)           STM32_LPTIM_REG(n, 0x08)
#define STM32_LPTIM_INT_DOWN		BIT(6)
#define STM32_LPTIM_INT_UP		BIT(5)
#define STM32_LPTIM_INT_ARROK		BIT(4)
#define STM32_LPTIM_INT_CMPOK		BIT(3)
#define STM32_LPTIM_INT_EXTTRIG		BIT(2)
#define STM32_LPTIM_INT_ARRM		BIT(1)
#define STM32_LPTIM_INT_CMPM		BIT(0)
#define STM32_LPTIM_CFGR(n)          STM32_LPTIM_REG(n, 0x0C)
#define STM32_LPTIM_CR(n)            STM32_LPTIM_REG(n, 0x10)
#define STM32_LPTIM_CR_RSTARE		BIT(4)
#define STM32_LPTIM_CR_COUNTRST		BIT(3)
#define STM32_LPTIM_CR_CNTSTRT		BIT(2)
#define STM32_LPTIM_CR_SNGSTRT		BIT(1)
#define STM32_LPTIM_CR_ENABLE		BIT(0)
#define STM32_LPTIM_CMP(n)           STM32_LPTIM_REG(n, 0x14)
#define STM32_LPTIM_ARR(n)           STM32_LPTIM_REG(n, 0x18)
#define STM32_LPTIM_CNT(n)           STM32_LPTIM_REG(n, 0x1C)
#define STM32_LPTIM_CFGR2(n)         STM32_LPTIM_REG(n, 0x24)

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


/* --- I2C --- */
#define STM32_I2C1_PORT             0
#define STM32_I2C2_PORT             1
#define STM32_I2C3_PORT             2
#define STM32_FMPI2C4_PORT          3

#define stm32_i2c_reg(port, offset) \
	((uint16_t *)((STM32_I2C1_BASE + ((port) * 0x400)) + (offset)))
/* --- Power / Reset / Clocks --- */
#define STM32_PWR_CR                REG32(STM32_PWR_BASE + 0x00)
#define STM32_PWR_CR_LPSDSR		(1 << 0)
#define STM32_PWR_CR_FLPS		(1 << 9)
#define STM32_PWR_CR_SVOS5		(1 << 14)
#define STM32_PWR_CR_SVOS4		(2 << 14)
#define STM32_PWR_CR_SVOS3		(3 << 14)
#define STM32_PWR_CR_SVOS_MASK		(3 << 14)

<<<<<<< HEAD   (6f5daa driver/opt3100: Set min/max frequency that match the driver)
#if defined(CHIP_FAMILY_STM32L4)
#define STM32_PWR_CR2               REG32(STM32_PWR_BASE + 0x04)
#define STM32_PWR_CSR               REG32(STM32_PWR_BASE + 0x10)
#else
#define STM32_PWR_CSR               REG32(STM32_PWR_BASE + 0x04)
#ifdef CHIP_FAMILY_STM32H7
#define STM32_PWR_CR2               REG32(STM32_PWR_BASE + 0x08)
#define STM32_PWR_CR3               REG32(STM32_PWR_BASE + 0x0C)
#define STM32_PWR_CR3_BYPASS        (1 <<  0)
#define STM32_PWR_CR3_LDOEN         (1 <<  1)
#define STM32_PWR_CR3_SCUEN         (1 <<  2)
#define STM32_PWR_CR3_VBE           (1 <<  8)
#define STM32_PWR_CR3_VBRS          (1 <<  9)
#define STM32_PWR_CR3_USB33DEN      (1 << 24)
#define STM32_PWR_CR3_USBREGEN      (1 << 25)
#define STM32_PWR_CR3_USB33RDY      (1 << 26)
#define STM32_PWR_CPUCR             REG32(STM32_PWR_BASE + 0x10)
#define STM32_PWR_CPUCR_PDDS_D1     (1 << 0)
#define STM32_PWR_CPUCR_PDDS_D2     (1 << 1)
#define STM32_PWR_CPUCR_PDDS_D3     (1 << 2)
#define STM32_PWR_CPUCR_STOPF       (1 << 5)
#define STM32_PWR_CPUCR_SBF         (1 << 6)
#define STM32_PWR_CPUCR_SBF_D1      (1 << 7)
#define STM32_PWR_CPUCR_SBF_D2      (1 << 8)
#define STM32_PWR_CPUCR_CSSF        (1 << 9)
#define STM32_PWR_CPUCR_RUN_D3      (1 << 11)
#define STM32_PWR_D3CR              REG32(STM32_PWR_BASE + 0x18)
#define STM32_PWR_D3CR_VOS1         (3 << 14)
#define STM32_PWR_D3CR_VOS2         (2 << 14)
#define STM32_PWR_D3CR_VOS3         (1 << 14)
#define STM32_PWR_D3CR_VOSMASK      (3 << 14)
#define STM32_PWR_D3CR_VOSRDY       (1 << 13)
#define STM32_PWR_WKUPCR            REG32(STM32_PWR_BASE + 0x20)
#define STM32_PWR_WKUPFR            REG32(STM32_PWR_BASE + 0x24)
#define STM32_PWR_WKUPEPR           REG32(STM32_PWR_BASE + 0x28)
#endif /* CHIP_FAMILY_STM32H7 */
#endif
#if defined(CHIP_FAMILY_STM32F0) || defined(CHIP_FAMILY_STM32F3)
#define STM32_PWR_CSR_EWUP1         (1 << 8)
#define STM32_PWR_CSR_EWUP2         (1 << 9)
#define STM32_PWR_CSR_EWUP3         (1 << 10)
#define STM32_PWR_CSR_EWUP4         (1 << 11) /* STM32F0xx only */
#define STM32_PWR_CSR_EWUP5         (1 << 12) /* STM32F0xx only */
#define STM32_PWR_CSR_EWUP6         (1 << 13) /* STM32F0xx only */
#define STM32_PWR_CSR_EWUP7         (1 << 14) /* STM32F0xx only */
#define STM32_PWR_CSR_EWUP8         (1 << 15) /* STM32F0xx only */
#endif

#if defined(CHIP_FAMILY_STM32L)
#define STM32_RCC_CR                REG32(STM32_RCC_BASE + 0x00)
#define STM32_RCC_CR_HSION		(1 << 0)
#define STM32_RCC_CR_HSIRDY		(1 << 1)
#define STM32_RCC_CR_MSION		(1 << 8)
#define STM32_RCC_CR_MSIRDY		(1 << 9)
#define STM32_RCC_CR_PLLON		(1 << 24)
#define STM32_RCC_CR_PLLRDY		(1 << 25)
#define STM32_RCC_ICSCR             REG32(STM32_RCC_BASE + 0x04)
#define STM32_RCC_ICSCR_MSIRANGE(n)	((n) << 13)
#define STM32_RCC_ICSCR_MSIRANGE_1MHZ	STM32_RCC_ICSCR_MSIRANGE(4)
#define STM32_RCC_ICSCR_MSIRANGE_2MHZ	STM32_RCC_ICSCR_MSIRANGE(5)
#define STM32_RCC_ICSCR_MSIRANGE_MASK	STM32_RCC_ICSCR_MSIRANGE(7)
#define STM32_RCC_CFGR              REG32(STM32_RCC_BASE + 0x08)
#define STM32_RCC_CFGR_SW_MSI		(0 << 0)
#define STM32_RCC_CFGR_SW_HSI		(1 << 0)
#define STM32_RCC_CFGR_SW_HSE		(2 << 0)
#define STM32_RCC_CFGR_SW_PLL		(3 << 0)
#define STM32_RCC_CFGR_SW_MASK		(3 << 0)
#define STM32_RCC_CFGR_SWS_MSI		(0 << 2)
#define STM32_RCC_CFGR_SWS_HSI		(1 << 2)
#define STM32_RCC_CFGR_SWS_HSE		(2 << 2)
#define STM32_RCC_CFGR_SWS_PLL		(3 << 2)
#define STM32_RCC_CFGR_SWS_MASK		(3 << 2)
#define STM32_RCC_CIR               REG32(STM32_RCC_BASE + 0x0C)
#define STM32_RCC_AHBRSTR           REG32(STM32_RCC_BASE + 0x10)
#define STM32_RCC_APB2RSTR          REG32(STM32_RCC_BASE + 0x14)
#define STM32_RCC_APB1RSTR          REG32(STM32_RCC_BASE + 0x18)
#define STM32_RCC_AHBENR            REG32(STM32_RCC_BASE + 0x1C)
#define STM32_RCC_APB2ENR           REG32(STM32_RCC_BASE + 0x20)
#define STM32_RCC_SYSCFGEN		(1 << 0)

#define STM32_RCC_APB1ENR           REG32(STM32_RCC_BASE + 0x24)
#define STM32_RCC_PWREN                 (1 << 28)

#define STM32_RCC_AHBLPENR          REG32(STM32_RCC_BASE + 0x28)
#define STM32_RCC_APB2LPENR         REG32(STM32_RCC_BASE + 0x2C)
#define STM32_RCC_APB1LPENR         REG32(STM32_RCC_BASE + 0x30)
#define STM32_RCC_CSR               REG32(STM32_RCC_BASE + 0x34)

#define STM32_RCC_HB_DMA1		(1 << 24)
#define STM32_RCC_PB2_TIM9		(1 << 2)
#define STM32_RCC_PB2_TIM10		(1 << 3)
#define STM32_RCC_PB2_TIM11		(1 << 4)
#define STM32_RCC_PB1_USB		(1 << 23)

#define STM32_SYSCFG_MEMRMP         REG32(STM32_SYSCFG_BASE + 0x00)
#define STM32_SYSCFG_PMC            REG32(STM32_SYSCFG_BASE + 0x04)
#define STM32_SYSCFG_EXTICR(n)      REG32(STM32_SYSCFG_BASE + 8 + 4 * (n))

#elif defined(CHIP_FAMILY_STM32L4)
#define STM32_RCC_CR			REG32(STM32_RCC_BASE + 0x00)
#define STM32_RCC_CR_MSION		(1 << 0)
#define STM32_RCC_CR_MSIRDY		(1 << 1)
#define STM32_RCC_CR_HSION		(1 << 8)
#define STM32_RCC_CR_HSIRDY		(1 << 10)
#define STM32_RCC_CR_HSEON		(1 << 16)
#define STM32_RCC_CR_HSERDY		(1 << 17)
#define STM32_RCC_CR_PLLON		(1 << 24)
#define STM32_RCC_CR_PLLRDY		(1 << 25)

#define STM32_RCC_ICSCR			REG32(STM32_RCC_BASE + 0x04)
#define STM32_RCC_ICSCR_MSIRANGE(n)	((n) << 13)
#define STM32_RCC_ICSCR_MSIRANGE_1MHZ	STM32_RCC_ICSCR_MSIRANGE(4)
#define STM32_RCC_ICSCR_MSIRANGE_2MHZ	STM32_RCC_ICSCR_MSIRANGE(5)
#define STM32_RCC_ICSCR_MSIRANGE_MASK	STM32_RCC_ICSCR_MSIRANGE(7)

#define STM32_RCC_CFGR			REG32(STM32_RCC_BASE + 0x08)
#define STM32_RCC_CFGR_SW_MSI		(0 << 0)
#define STM32_RCC_CFGR_SW_HSI		(1 << 0)
#define STM32_RCC_CFGR_SW_HSE		(2 << 0)
#define STM32_RCC_CFGR_SW_PLL		(3 << 0)
#define STM32_RCC_CFGR_SW_MASK		(3 << 0)
#define STM32_RCC_CFGR_SWS_MSI		(0 << 2)
#define STM32_RCC_CFGR_SWS_HSI		(1 << 2)
#define STM32_RCC_CFGR_SWS_HSE		(2 << 2)
#define STM32_RCC_CFGR_SWS_PLL		(3 << 2)
#define STM32_RCC_CFGR_SWS_MASK		(3 << 2)

#define STM32_RCC_PLLCFGR		REG32(STM32_RCC_BASE + 0x0C)
#define STM32_RCC_PLLCFGR_PLLSRC_SHIFT	(0)
#define STM32_RCC_PLLCFGR_PLLSRC_NONE	(0 << STM32_RCC_PLLCFGR_PLLSRC_SHIFT)
#define STM32_RCC_PLLCFGR_PLLSRC_MSI	(1 << STM32_RCC_PLLCFGR_PLLSRC_SHIFT)
#define STM32_RCC_PLLCFGR_PLLSRC_HSI	(2 << STM32_RCC_PLLCFGR_PLLSRC_SHIFT)
#define STM32_RCC_PLLCFGR_PLLSRC_HSE	(3 << STM32_RCC_PLLCFGR_PLLSRC_SHIFT)
#define STM32_RCC_PLLCFGR_PLLSRC_MASK	(3 << STM32_RCC_PLLCFGR_PLLSRC_SHIFT)
#define STM32_RCC_PLLCFGR_PLLM_SHIFT	(4)
#define STM32_RCC_PLLCFGR_PLLM_MASK	(0x7 << STM32_RCC_PLLCFGR_PLLM_SHIFT)
#define STM32_RCC_PLLCFGR_PLLN_SHIFT	(8)
#define STM32_RCC_PLLCFGR_PLLN_MASK	(0x7f << STM32_RCC_PLLCFGR_PLLN_SHIFT)
#define STM32_RCC_PLLCFGR_PLLREN_SHIFT	(24)
#define STM32_RCC_PLLCFGR_PLLREN_MASK	(1 << STM32_RCC_PLLCFGR_PLLREN_SHIFT)
#define STM32_RCC_PLLCFGR_PLLR_SHIFT	(25)
#define STM32_RCC_PLLCFGR_PLLR_MASK	(3 << STM32_RCC_PLLCFGR_PLLR_SHIFT)

#define STM32_RCC_AHB1RSTR              REG32(STM32_RCC_BASE + 0x28)
#define STM32_RCC_AHB2RSTR              REG32(STM32_RCC_BASE + 0x2C)
#define STM32_RCC_AHB3RSTR              REG32(STM32_RCC_BASE + 0x30)
#define STM32_RCC_APB1RSTR1             REG32(STM32_RCC_BASE + 0x38)
#define STM32_RCC_APB1RSTR2             REG32(STM32_RCC_BASE + 0x3C)
#define STM32_RCC_APB2RSTR              REG32(STM32_RCC_BASE + 0x40)

#define STM32_RCC_AHB1ENR		REG32(STM32_RCC_BASE + 0x48)
#define STM32_RCC_AHB1ENR_DMA1EN	(1 << 0)
#define STM32_RCC_AHB1ENR_DMA2EN	(1 << 1)

#define STM32_RCC_AHB2ENR		REG32(STM32_RCC_BASE + 0x4C)
#define STM32_RCC_AHB2ENR_GPIOMASK	(0xff << 0)
#define STM32_RCC_AHB2ENR_RNGEN		(1 << 18)

#define STM32_RCC_APB1ENR		REG32(STM32_RCC_BASE + 0x58)
#define STM32_RCC_PWREN                 (1 << 28)

#define STM32_RCC_APB1ENR2		REG32(STM32_RCC_BASE + 0x5C)
#define STM32_RCC_APB1ENR2_LPUART1EN	(1 << 0)

#define STM32_RCC_APB2ENR		REG32(STM32_RCC_BASE + 0x60)
#define STM32_RCC_SYSCFGEN		(1 << 0)

#define STM32_RCC_CCIPR			REG32(STM32_RCC_BASE + 0x88)
#define STM32_RCC_CCIPR_USART1SEL_SHIFT (0)
#define STM32_RCC_CCIPR_USART1SEL_MASK  (3 << STM32_RCC_CCIPR_USART1SEL_SHIFT)
#define STM32_RCC_CCIPR_USART2SEL_SHIFT (2)
#define STM32_RCC_CCIPR_USART2SEL_MASK  (3 << STM32_RCC_CCIPR_USART2SEL_SHIFT)
#define STM32_RCC_CCIPR_USART3SEL_SHIFT (4)
#define STM32_RCC_CCIPR_USART3SEL_MASK  (3 << STM32_RCC_CCIPR_USART3SEL_SHIFT)
#define STM32_RCC_CCIPR_UART4SEL_SHIFT (6)
#define STM32_RCC_CCIPR_UART4SEL_MASK  (3 << STM32_RCC_CCIPR_UART4SEL_SHIFT)
#define STM32_RCC_CCIPR_UART5SEL_SHIFT (8)
#define STM32_RCC_CCIPR_UART5SEL_MASK  (3 << STM32_RCC_CCIPR_UART5SEL_SHIFT)
#define STM32_RCC_CCIPR_LPUART1SEL_SHIFT (10)
#define STM32_RCC_CCIPR_LPUART1SEL_MASK  (3 << STM32_RCC_CCIPR_LPUART1SEL_SHIFT)
#define STM32_RCC_CCIPR_I2C1SEL_SHIFT (12)
#define STM32_RCC_CCIPR_I2C1SEL_MASK  (3 << STM32_RCC_CCIPR_I2C1SEL_SHIFT)
#define STM32_RCC_CCIPR_I2C2SEL_SHIFT (14)
#define STM32_RCC_CCIPR_I2C2SEL_MASK  (3 << STM32_RCC_CCIPR_I2C2SEL_SHIFT)
#define STM32_RCC_CCIPR_I2C3SEL_SHIFT (16)
#define STM32_RCC_CCIPR_I2C3SEL_MASK  (3 << STM32_RCC_CCIPR_I2C3SEL_SHIFT)
#define STM32_RCC_CCIPR_LPTIM1SEL_SHIFT (18)
#define STM32_RCC_CCIPR_LPTIM1SEL_MASK  (3 << STM32_RCC_CCIPR_LPTIM1SEL_SHIFT)
#define STM32_RCC_CCIPR_LPTIM2SEL_SHIFT (20)
#define STM32_RCC_CCIPR_LPTIM2SEL_MASK  (3 << STM32_RCC_CCIPR_LPTIM2SEL_SHIFT)
#define STM32_RCC_CCIPR_SAI1SEL_SHIFT (22)
#define STM32_RCC_CCIPR_SAI1SEL_MASK  (3 << STM32_RCC_CCIPR_SAI1SEL_SHIFT)
#define STM32_RCC_CCIPR_SAI2SEL_SHIFT (24)
#define STM32_RCC_CCIPR_SAI2SEL_MASK  (3 << STM32_RCC_CCIPR_SAI2SEL_SHIFT)
#define STM32_RCC_CCIPR_CLK48SEL_SHIFT (26)
#define STM32_RCC_CCIPR_CLK48SEL_MASK  (3 << STM32_RCC_CCIPR_CLK48SEL_SHIFT)
#define STM32_RCC_CCIPR_ADCSEL_SHIFT (28)
#define STM32_RCC_CCIPR_ADCSEL_MASK  (3 << STM32_RCC_CCIPR_ADCSEL_SHIFT)
#define STM32_RCC_CCIPR_SWPMI1SEL_SHIFT (30)
#define STM32_RCC_CCIPR_SWPMI1SEL_MASK  (1 << STM32_RCC_CCIPR_SWPMI1SEL_SHIFT)
#define STM32_RCC_CCIPR_DFSDM1SEL_SHIFT (31)
#define STM32_RCC_CCIPR_DFSDM1SEL_MASK  (1 << STM32_RCC_CCIPR_DFSDM1SEL_SHIFT)

/* Possible clock sources for each peripheral */
#define STM32_RCC_CCIPR_UART_PCLK 	0
#define STM32_RCC_CCIPR_UART_SYSCLK	1
#define STM32_RCC_CCIPR_UART_HSI16	2
#define STM32_RCC_CCIPR_UART_LSE	3

#define STM32_RCC_CCIPR_I2C_PCLK	0
#define STM32_RCC_CCIPR_I2C_SYSCLK	1
#define STM32_RCC_CCIPR_I2C_HSI16	2

#define STM32_RCC_CCIPR_LPTIM_PCLK	0
#define STM32_RCC_CCIPR_LPTIM_LSI	1
#define STM32_RCC_CCIPR_LPTIM_HSI16	2
#define STM32_RCC_CCIPR_LPTIM_LSE	3

#define STM32_RCC_CCIPR_SAI_PLLSAI1CLK	0
#define STM32_RCC_CCIPR_SAI_PLLSAI2CLK	1
#define STM32_RCC_CCIPR_SAI_PLLSAI3CLK	2
#define STM32_RCC_CCIPR_SAI_EXTCLK		3

#define STM32_RCC_CCIPR_CLK48_NONE			0
#define STM32_RCC_CCIPR_CLK48_PLL48M2CLK	1
#define STM32_RCC_CCIPR_CLK48_PLL48M1CLK	2
#define STM32_RCC_CCIPR_CLK48_MSI			3

#define STM32_RCC_CCIPR_ADC_NONE		0
#define STM32_RCC_CCIPR_ADC_PLLADC1CLK	1
#define STM32_RCC_CCIPR_ADC_PLLADC2CLK	2
#define STM32_RCC_CCIPR_ADC_SYSCLK	3

#define STM32_RCC_CCIPR_SWPMI_PCLK	0
#define STM32_RCC_CCIPR_SWPMI_HSI16	1

#define STM32_RCC_CCIPR_DFSDM_PCLK		0
#define STM32_RCC_CCIPR_DFSDM_SYSCLK	1

#define STM32_RCC_BDCR			REG32(STM32_RCC_BASE + 0x90)

#define STM32_RCC_CSR			REG32(STM32_RCC_BASE + 0x94)

#define STM32_RCC_CRRCR			REG32(STM32_RCC_BASE + 0x98)

#define STM32_RCC_CRRCR_HSI48ON         (1<<0)
#define STM32_RCC_CRRCR_HSI48RDY        (1<<1)
#define STM32_RCC_CRRCR_HSI48CAL_MASK   (0x1ff<<7)

#define STM32_RCC_PB2_TIM1		(1 << 11)
#define STM32_RCC_PB2_TIM8		(1 << 13)

#define STM32_SYSCFG_EXTICR(n)		REG32(STM32_SYSCFG_BASE + 8 + 4 * (n))

#elif defined(CHIP_FAMILY_STM32F0) || defined(CHIP_FAMILY_STM32F3)
#define STM32_CRS_CR                REG32(STM32_CRS_BASE + 0x00) /* STM32F0XX */
#define STM32_CRS_CR_SYNCOKIE           (1 << 0)
#define STM32_CRS_CR_SYNCWARNIE         (1 << 1)
#define STM32_CRS_CR_ERRIE              (1 << 2)
#define STM32_CRS_CR_ESYNCIE            (1 << 3)
#define STM32_CRS_CR_CEN                (1 << 5)
#define STM32_CRS_CR_AUTOTRIMEN         (1 << 6)
#define STM32_CRS_CR_SWSYNC             (1 << 7)
#define STM32_CRS_CR_TRIM(n)		(((n) & 0x3f) << 8)

#define STM32_CRS_CFGR              REG32(STM32_CRS_BASE + 0x04) /* STM32F0XX */
#define STM32_CRS_CFGR_RELOAD(n)	(((n) & 0xffff) << 0)
#define STM32_CRS_CFGR_FELIM(n)		(((n) & 0xff) << 16)
#define STM32_CRS_CFGR_SYNCDIV(n)	(((n) & 7) << 24)
#define STM32_CRS_CFGR_SYNCSRC(n)	(((n) & 3) << 28)
#define STM32_CRS_CFGR_SYNCPOL		(1 << 31)

#define STM32_CRS_ISR               REG32(STM32_CRS_BASE + 0x08) /* STM32F0XX */
#define STM32_CRS_ISR_SYNCOKF		(1 << 0)
#define STM32_CRS_ISR_SYNCWARNF		(1 << 1)
#define STM32_CRS_ISR_ERRF		(1 << 2)
#define STM32_CRS_ISR_ESYNCF		(1 << 3)
#define STM32_CRS_ISR_SYNCERR		(1 << 8)
#define STM32_CRS_ISR_SYNCMISS		(1 << 9)
#define STM32_CRS_ISR_TRIMOVF		(1 << 10)
#define STM32_CRS_ISR_FEDIR		(1 << 15)
#define STM32_CRS_ISR_FECAP		(0xffff << 16)

#define STM32_CRS_ICR               REG32(STM32_CRS_BASE + 0x0c) /* STM32F0XX */
#define STM32_CRS_ICR_SYNCOKC		(1 << 0)
#define STM32_CRS_ICR_SYNCWARINC	(1 << 1)
#define STM32_CRS_ICR_ERRC		(1 << 2)
#define STM32_CRS_ICR_ESYNCC		(1 << 3)

#define STM32_RCC_CR                REG32(STM32_RCC_BASE + 0x00)
#define STM32_RCC_CFGR              REG32(STM32_RCC_BASE + 0x04)
#define STM32_RCC_CIR               REG32(STM32_RCC_BASE + 0x08)
#define STM32_RCC_APB2RSTR          REG32(STM32_RCC_BASE + 0x0c)
#define STM32_RCC_APB1RSTR          REG32(STM32_RCC_BASE + 0x10)
#define STM32_RCC_AHBENR            REG32(STM32_RCC_BASE + 0x14)
#define STM32_RCC_APB2ENR           REG32(STM32_RCC_BASE + 0x18)
#define STM32_RCC_APB2ENR_TIM16EN   (1 << 17)
#define STM32_RCC_APB2ENR_TIM17EN   (1 << 18)
#define STM32_RCC_DBGMCUEN          (1 << 22)
#define STM32_RCC_SYSCFGEN          (1 << 0)

#define STM32_RCC_APB1ENR           REG32(STM32_RCC_BASE + 0x1c)
#define STM32_RCC_PWREN                 (1 << 28)

#define STM32_RCC_BDCR              REG32(STM32_RCC_BASE + 0x20)
#define STM32_RCC_CSR               REG32(STM32_RCC_BASE + 0x24)
/* STM32F373 */
#define STM32_RCC_CFGR2             REG32(STM32_RCC_BASE + 0x2c)
/* STM32F0XX and STM32F373 */
#define STM32_RCC_CFGR3             REG32(STM32_RCC_BASE + 0x30)
#define STM32_RCC_CR2               REG32(STM32_RCC_BASE + 0x34) /* STM32F0XX */

#define STM32_RCC_HB_DMA1		(1 << 0)
/* STM32F373 */
#define STM32_RCC_HB_DMA2		(1 << 1)
#define STM32_RCC_PB2_TIM1		(1 << 11) /* Except STM32F373 */
#define STM32_RCC_PB2_TIM15		(1 << 16) /* STM32F0XX and STM32F373 */
#define STM32_RCC_PB2_TIM16		(1 << 17) /* STM32F0XX and STM32F373 */
#define STM32_RCC_PB2_TIM17		(1 << 18) /* STM32F0XX and STM32F373 */
#define STM32_RCC_PB2_TIM19		(1 << 19) /* STM32F373 */
#define STM32_RCC_PB2_PMAD		(1 << 11) /* STM32TS */
#define STM32_RCC_PB2_PMSE		(1 << 13) /* STM32TS */
#define STM32_RCC_PB1_TIM12		(1 << 6)  /* STM32F373 */
#define STM32_RCC_PB1_TIM13		(1 << 7)  /* STM32F373 */
#define STM32_RCC_PB1_TIM14		(1 << 8)  /* STM32F0XX and STM32F373 */
#define STM32_RCC_PB1_TIM18		(1 << 9)  /* STM32F373 */
#define STM32_RCC_PB1_USB		(1 << 23)
#define STM32_RCC_PB1_CRS		(1 << 27)

#define STM32_SYSCFG_CFGR1          REG32(STM32_SYSCFG_BASE + 0x00)
#define STM32_SYSCFG_EXTICR(n)      REG32(STM32_SYSCFG_BASE + 8 + 4 * (n))
#define STM32_SYSCFG_CFGR2          REG32(STM32_SYSCFG_BASE + 0x18)

#elif defined(CHIP_FAMILY_STM32F4)
#define STM32_RCC_CR                    REG32(STM32_RCC_BASE + 0x00)
#define STM32_RCC_CR_HSION		(1 << 0)
#define STM32_RCC_CR_HSIRDY		(1 << 1)
#define STM32_RCC_CR_HSEON		(1 << 16)
#define STM32_RCC_CR_HSERDY		(1 << 17)
#define STM32_RCC_CR_PLLON		(1 << 24)
#define STM32_RCC_CR_PLLRDY		(1 << 25)

#if defined(CHIP_VARIANT_STM32F446)
/* Required or recommended clocks for stm32f446 */
#define STM32F4_PLL_REQ 2000000
#define STM32F4_RTC_REQ 1000000
#define STM32F4_IO_CLOCK  42000000
#define STM32F4_USB_REQ 48000000
#define STM32F4_VCO_CLOCK 336000000
#define STM32F4_HSI_CLOCK 16000000
#define STM32F4_LSI_CLOCK 32000
#define STM32F4_TIMER_CLOCK STM32F4_IO_CLOCK
#define STM32F4_PLLP_DIV 4
#define STM32F4_AHB_PRE 0x8
#define STM32F4_APB1_PRE 0x0
#define STM32F4_APB2_PRE 0x0
#define STM32_FLASH_ACR_LATENCY     (1 << 0)

#elif defined(CHIP_VARIANT_STM32F412)
/* Required or recommended clocks for stm32f412 */
#define STM32F4_PLL_REQ 2000000
#define STM32F4_RTC_REQ 1000000
#define STM32F4_IO_CLOCK  48000000
#define STM32F4_USB_REQ 48000000
#define STM32F4_VCO_CLOCK 384000000
#define STM32F4_HSI_CLOCK 16000000
#define STM32F4_LSI_CLOCK 32000
#define STM32F4_TIMER_CLOCK (STM32F4_IO_CLOCK * 2)
#define STM32F4_PLLP_DIV 4
#define STM32F4_AHB_PRE 0x0
#define STM32F4_APB1_PRE 0x4
#define STM32F4_APB2_PRE 0x4
#define STM32_FLASH_ACR_LATENCY     (3 << 0)

#elif defined(CHIP_VARIANT_STM32F411)
/* Required or recommended clocks for stm32f411 */
#define STM32F4_PLL_REQ 2000000
#define STM32F4_RTC_REQ 1000000
#define STM32F4_IO_CLOCK  48000000
#define STM32F4_USB_REQ 48000000
#define STM32F4_VCO_CLOCK 384000000
#define STM32F4_HSI_CLOCK 16000000
#define STM32F4_LSI_CLOCK 32000
#define STM32F4_TIMER_CLOCK STM32F4_IO_CLOCK
#define STM32F4_PLLP_DIV 4
#define STM32F4_AHB_PRE 0x8
#define STM32F4_APB1_PRE 0x0
#define STM32F4_APB2_PRE 0x0
#define STM32_FLASH_ACR_LATENCY     (1 << 0)

#elif defined(CHIP_VARIANT_STM32F76X)
/* Required or recommended clocks for stm32f767/769 */
#define STM32F4_PLL_REQ 2000000
#define STM32F4_RTC_REQ 1000000
#define STM32F4_IO_CLOCK 45000000
#define STM32F4_USB_REQ 45000000 /* not compatible with USB, will use PLLSAI */
#define STM32F4_VCO_CLOCK 360000000
#define STM32F4_HSI_CLOCK 16000000
#define STM32F4_LSI_CLOCK 32000
#define STM32F4_TIMER_CLOCK (STM32F4_IO_CLOCK * 2)
#define STM32F4_PLLP_DIV 2   /* sys = VCO/2  = 180 Mhz */
#define STM32F4_AHB_PRE 0x0  /* AHB = sysclk = 180 Mhz */
#define STM32F4_APB1_PRE 0x5 /* APB1 = AHB /4 = 45 Mhz */
#define STM32F4_APB2_PRE 0x5 /* APB2 = AHB /4 = 45 Mhz */
#define STM32_FLASH_ACR_LATENCY     (5 << 0)

#else
#error "No valid clocks defined"
#endif

#define STM32_RCC_PLLCFGR               REG32(STM32_RCC_BASE + 0x04)
/* PLL Division factor */
#define  PLLCFGR_PLLM_OFF		0
#define  PLLCFGR_PLLM(val)		(((val) & 0x1f) << PLLCFGR_PLLM_OFF)
/* PLL Multiplication factor */
#define  PLLCFGR_PLLN_OFF		6
#define  PLLCFGR_PLLN(val)		(((val) & 0x1ff) << PLLCFGR_PLLN_OFF)
/* Main CPU Clock */
#define  PLLCFGR_PLLP_OFF		16
#define  PLLCFGR_PLLP(val)		(((val) & 0x3) << PLLCFGR_PLLP_OFF)

#define  PLLCFGR_PLLSRC_HSI		(0 << 22)
#define  PLLCFGR_PLLSRC_HSE		(1 << 22)
/* USB OTG FS: Must equal 48MHz */
#define  PLLCFGR_PLLQ_OFF		24
#define  PLLCFGR_PLLQ(val)		(((val) & 0xf) << PLLCFGR_PLLQ_OFF)
/* SYSTEM */
#define  PLLCFGR_PLLR_OFF		28
#define  PLLCFGR_PLLR(val)		(((val) & 0x7) << PLLCFGR_PLLR_OFF)

#define STM32_RCC_CFGR                  REG32(STM32_RCC_BASE + 0x08)
#define STM32_RCC_CFGR_SW_HSI		(0 << 0)
#define STM32_RCC_CFGR_SW_HSE		(1 << 0)
#define STM32_RCC_CFGR_SW_PLL		(2 << 0)
#define STM32_RCC_CFGR_SW_PLL_R		(3 << 0)
#define STM32_RCC_CFGR_SW_MASK		(3 << 0)
#define STM32_RCC_CFGR_SWS_HSI		(0 << 2)
#define STM32_RCC_CFGR_SWS_HSE		(1 << 2)
#define STM32_RCC_CFGR_SWS_PLL		(2 << 2)
#define STM32_RCC_CFGR_SWS_PLL_R	(3 << 2)
#define STM32_RCC_CFGR_SWS_MASK		(3 << 2)
/* AHB Prescalar: nonlinear values, look up in RM0390 */
#define  CFGR_HPRE_OFF			4
#define  CFGR_HPRE(val)			(((val) & 0xf) << CFGR_HPRE_OFF)
/* APB1 Low Speed Prescalar < 45MHz */
#define  CFGR_PPRE1_OFF			10
#define  CFGR_PPRE1(val)		(((val) & 0x7) << CFGR_PPRE1_OFF)
/* APB2 High Speed Prescalar < 90MHz */
#define  CFGR_PPRE2_OFF			13
#define  CFGR_PPRE2(val)		(((val) & 0x7) << CFGR_PPRE2_OFF)
/* RTC CLock: Must equal 1MHz */
#define  CFGR_RTCPRE_OFF		16
#define  CFGR_RTCPRE(val)		(((val) & 0x1f) << CFGR_RTCPRE_OFF)

#define STM32_RCC_CIR                   REG32(STM32_RCC_BASE + 0x0C)
#define STM32_RCC_AHB1RSTR              REG32(STM32_RCC_BASE + 0x10)
#define  RCC_AHB1RSTR_OTGHSRST		(1 << 29)

#define STM32_RCC_AHB2RSTR              REG32(STM32_RCC_BASE + 0x14)
#define STM32_RCC_AHB3RSTR              REG32(STM32_RCC_BASE + 0x18)

#define STM32_RCC_APB1RSTR              REG32(STM32_RCC_BASE + 0x20)
#define STM32_RCC_APB2RSTR              REG32(STM32_RCC_BASE + 0x24)

#define STM32_RCC_AHB1ENR               REG32(STM32_RCC_BASE + 0x30)
#define STM32_RCC_AHB1ENR_GPIOMASK	(0xff << 0)
#define STM32_RCC_AHB1ENR_BKPSRAMEN	(1 << 18)
#define STM32_RCC_AHB1ENR_DMA1EN	(1 << 21)
#define STM32_RCC_AHB1ENR_DMA2EN	(1 << 22)
/* TODO(nsanders): normalize naming.*/
#define STM32_RCC_HB1_DMA1		(1 << 21)
#define STM32_RCC_HB1_DMA2		(1 << 22)
#define STM32_RCC_AHB1ENR_OTGHSEN	(1 << 29)
#define STM32_RCC_AHB1ENR_OTGHSULPIEN	(1 << 30)

#define STM32_RCC_AHB2ENR               REG32(STM32_RCC_BASE + 0x34)
#define STM32_RCC_AHB2ENR_OTGFSEN	(1 << 7)
#define STM32_RCC_AHB3ENR               REG32(STM32_RCC_BASE + 0x38)

#define STM32_RCC_APB1ENR               REG32(STM32_RCC_BASE + 0x40)
#define STM32_RCC_PWREN                 (1 << 28)
#define STM32_RCC_I2C1EN                (1 << 21)
#define STM32_RCC_I2C2EN                (1 << 22)
#define STM32_RCC_I2C3EN                (1 << 23)
#define STM32_RCC_FMPI2C4EN             (1 << 24)

#define STM32_RCC_APB2ENR               REG32(STM32_RCC_BASE + 0x44)

#define STM32_RCC_PB2_USART6            (1 << 5)
#define STM32_RCC_SYSCFGEN		(1 << 14)

#define STM32_RCC_AHB1LPENR             REG32(STM32_RCC_BASE + 0x50)
#define STM32_RCC_AHB2LPENR             REG32(STM32_RCC_BASE + 0x54)
#define STM32_RCC_AHB3LPENR             REG32(STM32_RCC_BASE + 0x58)
#define STM32_RCC_APB1LPENR             REG32(STM32_RCC_BASE + 0x60)
#define STM32_RCC_APB2LPENR             REG32(STM32_RCC_BASE + 0x64)

#define STM32_RCC_BDCR                  REG32(STM32_RCC_BASE + 0x70)
#define STM32_RCC_CSR                   REG32(STM32_RCC_BASE + 0x74)
#define STM32_RCC_CSR_LSION		(1 << 0)
#define STM32_RCC_CSR_LSIRDY		(1 << 1)

#define STM32_RCC_HB_DMA1		(1 << 24)
#define STM32_RCC_PB2_TIM9		(1 << 2)
#define STM32_RCC_PB2_TIM10		(1 << 3)
#define STM32_RCC_PB2_TIM11		(1 << 4)
#define STM32_RCC_PB1_USB		(1 << 23)

#define STM32_RCC_DCKCFGR2              REG32(STM32_RCC_BASE + 0x94)
#define  DCKCFGR2_FMPI2C1SEL(val)       (((val) & 0x3) << 22)
#define  DCKCFGR2_FMPI2C1SEL_MASK       (0x3 << 22)
#define  FMPI2C1SEL_APB                 0x0

#define STM32_SYSCFG_MEMRMP             REG32(STM32_SYSCFG_BASE + 0x00)
#define STM32_SYSCFG_PMC                REG32(STM32_SYSCFG_BASE + 0x04)
#define STM32_SYSCFG_EXTICR(n)          REG32(STM32_SYSCFG_BASE + 8 + 4 * (n))
#define STM32_SYSCFG_CMPCR              REG32(STM32_SYSCFG_BASE + 0x20)
#define STM32_SYSCFG_CFGR               REG32(STM32_SYSCFG_BASE + 0x2C)

#elif defined(CHIP_FAMILY_STM32H7)
#define STM32_RCC_CR                REG32(STM32_RCC_BASE + 0x000)
#define STM32_RCC_ICSCR             REG32(STM32_RCC_BASE + 0x004)
#define STM32_RCC_CRRCR             REG32(STM32_RCC_BASE + 0x008)
#define STM32_RCC_CFGR              REG32(STM32_RCC_BASE + 0x010)
#define STM32_RCC_D1CFGR            REG32(STM32_RCC_BASE + 0x018)
#define STM32_RCC_D2CFGR            REG32(STM32_RCC_BASE + 0x01C)
#define STM32_RCC_D3CFGR            REG32(STM32_RCC_BASE + 0x020)
#define STM32_RCC_PLLCKSELR         REG32(STM32_RCC_BASE + 0x028)
#define STM32_RCC_PLLCFGR           REG32(STM32_RCC_BASE + 0x02C)
#define STM32_RCC_PLL1DIVR          REG32(STM32_RCC_BASE + 0x030)
#define STM32_RCC_PLL1FRACR         REG32(STM32_RCC_BASE + 0x034)
#define STM32_RCC_PLL2DIVR          REG32(STM32_RCC_BASE + 0x038)
#define STM32_RCC_PLL2FRACR         REG32(STM32_RCC_BASE + 0x03C)
#define STM32_RCC_PLL3DIVR          REG32(STM32_RCC_BASE + 0x040)
#define STM32_RCC_PLL3FRACR         REG32(STM32_RCC_BASE + 0x044)
#define STM32_RCC_D1CCIPR           REG32(STM32_RCC_BASE + 0x04C)
#define STM32_RCC_D2CCIP1R          REG32(STM32_RCC_BASE + 0x050)
#define STM32_RCC_D2CCIP2R          REG32(STM32_RCC_BASE + 0x054)
#define STM32_RCC_D3CCIPR           REG32(STM32_RCC_BASE + 0x058)
#define STM32_RCC_CIER              REG32(STM32_RCC_BASE + 0x060)
#define STM32_RCC_CIFR              REG32(STM32_RCC_BASE + 0x064)
#define STM32_RCC_CICR              REG32(STM32_RCC_BASE + 0x068)
#define STM32_RCC_BDCR              REG32(STM32_RCC_BASE + 0x070)
#define STM32_RCC_CSR               REG32(STM32_RCC_BASE + 0x074)

#define STM32_RCC_APB2RSTR          REG32(STM32_RCC_BASE + 0x098)

#define STM32_RCC_RSR               REG32(STM32_RCC_BASE + 0x0D0)
#define STM32_RCC_AHB3ENR           REG32(STM32_RCC_BASE + 0x0D4)
#define STM32_RCC_AHB1ENR           REG32(STM32_RCC_BASE + 0x0D8)
#define STM32_RCC_AHB2ENR           REG32(STM32_RCC_BASE + 0x0DC)
#define STM32_RCC_AHB2ENR_RNGEN     (1 << 6)
#define STM32_RCC_AHB2ENR_HASHEN    (1 << 5)
#define STM32_RCC_AHB2ENR_CRYPTEN   (1 << 4)
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

#define STM32_RCC_CR_HSION                     (1 << 0)
#define STM32_RCC_CR_HSIRDY                    (1 << 2)
#define STM32_RCC_CR_CSION                     (1 << 7)
#define STM32_RCC_CR_CSIRDY                    (1 << 8)
#define STM32_RCC_CR_HSI48ON                   (1 << 12)
#define STM32_RCC_CR_HSI48RDY                  (1 << 13)
#define STM32_RCC_CR_PLL1ON                    (1 << 24)
#define STM32_RCC_CR_PLL1RDY                   (1 << 25)
#define STM32_RCC_CR_PLL2ON                    (1 << 26)
#define STM32_RCC_CR_PLL2RDY                   (1 << 27)
#define STM32_RCC_CR_PLL3ON                    (1 << 28)
#define STM32_RCC_CR_PLL3RDY                   (1 << 29)
#define STM32_RCC_CFGR_SW_HSI                  (0 << 0)
#define STM32_RCC_CFGR_SW_CSI                  (1 << 0)
#define STM32_RCC_CFGR_SW_HSE                  (2 << 0)
#define STM32_RCC_CFGR_SW_PLL1                 (3 << 0)
#define STM32_RCC_CFGR_SW_MASK                 (3 << 0)
#define STM32_RCC_CFGR_SWS_HSI                 (0 << 3)
#define STM32_RCC_CFGR_SWS_CSI                 (1 << 3)
#define STM32_RCC_CFGR_SWS_HSE                 (2 << 3)
#define STM32_RCC_CFGR_SWS_PLL1                (3 << 3)
#define STM32_RCC_CFGR_SWS_MASK                (3 << 3)
#define STM32_RCC_D1CFGR_HPRE_DIV1             (0 << 0)
#define STM32_RCC_D1CFGR_HPRE_DIV2             (8 << 0)
#define STM32_RCC_D1CFGR_HPRE_DIV4             (9 << 0)
#define STM32_RCC_D1CFGR_HPRE_DIV8            (10 << 0)
#define STM32_RCC_D1CFGR_HPRE_DIV16           (11 << 0)
#define STM32_RCC_D1CFGR_D1PPRE_DIV1           (0 << 4)
#define STM32_RCC_D1CFGR_D1PPRE_DIV2           (4 << 4)
#define STM32_RCC_D1CFGR_D1PPRE_DIV4           (5 << 4)
#define STM32_RCC_D1CFGR_D1PPRE_DIV8           (6 << 4)
#define STM32_RCC_D1CFGR_D1PPRE_DIV16          (7 << 4)
#define STM32_RCC_D1CFGR_D1CPRE_DIV1           (0 << 8)
#define STM32_RCC_D1CFGR_D1CPRE_DIV2           (8 << 8)
#define STM32_RCC_D1CFGR_D1CPRE_DIV4           (9 << 8)
#define STM32_RCC_D1CFGR_D1CPRE_DIV8          (10 << 8)
#define STM32_RCC_D1CFGR_D1CPRE_DIV16         (11 << 8)
#define STM32_RCC_PLLCKSEL_PLLSRC_HSI          (0 << 0)
#define STM32_RCC_PLLCKSEL_PLLSRC_CSI          (1 << 0)
#define STM32_RCC_PLLCKSEL_PLLSRC_HSE          (2 << 0)
#define STM32_RCC_PLLCKSEL_PLLSRC_NONE         (3 << 0)
#define STM32_RCC_PLLCKSEL_PLLSRC_MASK         (3 << 0)
#define STM32_RCC_PLLCKSEL_DIVM1(m)            ((m) << 4)
#define STM32_RCC_PLLCKSEL_DIVM2(m)            ((m) << 12)
#define STM32_RCC_PLLCKSEL_DIVM3(m)            ((m) << 20)
#define STM32_RCC_PLLCFG_PLL1VCOSEL_FRACEN     (1 << 0)
#define STM32_RCC_PLLCFG_PLL1VCOSEL_WIDE       (0 << 1)
#define STM32_RCC_PLLCFG_PLL1VCOSEL_MEDIUM     (1 << 1)
#define STM32_RCC_PLLCFG_PLL1RGE_1M_2M         (0 << 2)
#define STM32_RCC_PLLCFG_PLL1RGE_2M_4M         (1 << 2)
#define STM32_RCC_PLLCFG_PLL1RGE_4M_8M         (2 << 2)
#define STM32_RCC_PLLCFG_PLL1RGE_8M_16M        (3 << 2)
#define STM32_RCC_PLLCFG_DIVP1EN               (1 << 16)
#define STM32_RCC_PLLCFG_DIVQ1EN               (1 << 17)
#define STM32_RCC_PLLCFG_DIVR1EN               (1 << 18)
#define STM32_RCC_PLLDIV_DIVN(n)               (((n) - 1) << 0)
#define STM32_RCC_PLLDIV_DIVP(p)               (((p) - 1) << 9)
#define STM32_RCC_PLLDIV_DIVQ(q)               (((q) - 1) << 16)
#define STM32_RCC_PLLDIV_DIVR(r)               (((r) - 1) << 24)
#define STM32_RCC_PLLFRAC(n)                   ((n) << 3)
#define STM32_RCC_D2CCIP1R_SPI123SEL_PLL1Q     (0 << 12)
#define STM32_RCC_D2CCIP1R_SPI123SEL_PLL2P     (1 << 12)
#define STM32_RCC_D2CCIP1R_SPI123SEL_PLL3P     (2 << 12)
#define STM32_RCC_D2CCIP1R_SPI123SEL_I2SCKIN   (3 << 12)
#define STM32_RCC_D2CCIP1R_SPI123SEL_PERCK     (4 << 12)
#define STM32_RCC_D2CCIP1R_SPI123SEL_MASK      (7 << 12)
#define STM32_RCC_D2CCIP1R_SPI45SEL_APB        (0 << 16)
#define STM32_RCC_D2CCIP1R_SPI45SEL_PLL2Q      (1 << 16)
#define STM32_RCC_D2CCIP1R_SPI45SEL_PLL3Q      (2 << 16)
#define STM32_RCC_D2CCIP1R_SPI45SEL_HSI        (3 << 16)
#define STM32_RCC_D2CCIP1R_SPI45SEL_CSI        (4 << 16)
#define STM32_RCC_D2CCIP1R_SPI45SEL_HSE        (5 << 16)
#define STM32_RCC_D2CCIP1R_SPI45SEL_MASK       (7 << 16)
#define STM32_RCC_D2CCIP2_USART234578SEL_PCLK  (0 << 0)
#define STM32_RCC_D2CCIP2_USART234578SEL_PLL2Q (1 << 0)
#define STM32_RCC_D2CCIP2_USART234578SEL_PLL3Q (2 << 0)
#define STM32_RCC_D2CCIP2_USART234578SEL_HSI   (3 << 0)
#define STM32_RCC_D2CCIP2_USART234578SEL_CSI   (4 << 0)
#define STM32_RCC_D2CCIP2_USART234578SEL_LSE   (5 << 0)
#define STM32_RCC_D2CCIP2_USART234578SEL_MASK  (7 << 0)
#define STM32_RCC_D2CCIP2_USART16SEL_PCLK      (0 << 3)
#define STM32_RCC_D2CCIP2_USART16SEL_PLL2Q     (1 << 3)
#define STM32_RCC_D2CCIP2_USART16SEL_PLL3Q     (2 << 3)
#define STM32_RCC_D2CCIP2_USART16SEL_HSI       (3 << 3)
#define STM32_RCC_D2CCIP2_USART16SEL_CSI       (4 << 3)
#define STM32_RCC_D2CCIP2_USART16SEL_LSE       (5 << 3)
#define STM32_RCC_D2CCIP2_USART16SEL_MASK      (7 << 3)
#define STM32_RCC_D2CCIP2_RNGSEL_HSI48         (0 << 8)
#define STM32_RCC_D2CCIP2_RNGSEL_PLL1Q         (1 << 8)
#define STM32_RCC_D2CCIP2_RNGSEL_LSE           (2 << 8)
#define STM32_RCC_D2CCIP2_RNGSEL_LSI           (3 << 8)
#define STM32_RCC_D2CCIP2_RNGSEL_MASK          (3 << 8)
#define STM32_RCC_D2CCIP2_LPTIM1SEL_PCLK       (0 << 28)
#define STM32_RCC_D2CCIP2_LPTIM1SEL_PLL2       (1 << 28)
#define STM32_RCC_D2CCIP2_LPTIM1SEL_PLL3       (2 << 28)
#define STM32_RCC_D2CCIP2_LPTIM1SEL_LSE        (3 << 28)
#define STM32_RCC_D2CCIP2_LPTIM1SEL_LSI        (4 << 28)
#define STM32_RCC_D2CCIP2_LPTIM1SEL_PER        (5 << 28)
#define STM32_RCC_D2CCIP2_LPTIM1SEL_MASK       (7 << 28)
#define STM32_RCC_CSR_LSION                    (1 << 0)
#define STM32_RCC_CSR_LSIRDY                   (1 << 1)

#define STM32_SYSCFG_PMCR           REG32(STM32_SYSCFG_BASE + 0x04)
#define STM32_SYSCFG_EXTICR(n)      REG32(STM32_SYSCFG_BASE + 8 + 4 * (n))

/* Peripheral bits for APB1ENR regs */
#define STM32_RCC_PB1_LPTIM1            (1 << 9)

/* Peripheral bits for APB2ENR regs */
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

#else
#error Unsupported chip variant
#endif

=======
>>>>>>> BRANCH (40d09f ectool: motionsense: add commands for fast/manual offset com)
/* RTC domain control register */
#define STM32_RCC_BDCR_BDRST		BIT(16)
#define STM32_RCC_BDCR_RTCEN		BIT(15)
#define STM32_RCC_BDCR_LSERDY		BIT(1)
#define STM32_RCC_BDCR_LSEON		BIT(0)
#define  BDCR_RTCSEL_MASK		((0x3) << 8)
#define  BDCR_RTCSEL(source)		(((source) << 8) & BDCR_RTCSEL_MASK)
#define  BDCR_SRC_LSE			0x1
#define  BDCR_SRC_LSI			0x2
#define  BDCR_SRC_HSE			0x3
/* Peripheral bits for RCC_APB/AHB and DBGMCU regs */
#define STM32_RCC_PB1_TIM2		BIT(0)
#define STM32_RCC_PB1_TIM3		BIT(1)
#define STM32_RCC_PB1_TIM4		BIT(2)
#define STM32_RCC_PB1_TIM5		BIT(3)
#define STM32_RCC_PB1_TIM6		BIT(4)
#define STM32_RCC_PB1_TIM7		BIT(5)
#define STM32_RCC_PB1_TIM12             BIT(6) /* STM32H7 */
#define STM32_RCC_PB1_TIM13             BIT(7) /* STM32H7 */
#define STM32_RCC_PB1_TIM14             BIT(8) /* STM32H7 */
#define STM32_RCC_PB1_RTC		BIT(10) /* DBGMCU only */
#define STM32_RCC_PB1_WWDG		BIT(11)
#define STM32_RCC_PB1_IWDG		BIT(12) /* DBGMCU only */
#define STM32_RCC_PB1_SPI2		BIT(14)
#define STM32_RCC_PB1_SPI3		BIT(15)
#define STM32_RCC_PB1_USART2		BIT(17)
#define STM32_RCC_PB1_USART3		BIT(18)
#define STM32_RCC_PB1_USART4		BIT(19)
#define STM32_RCC_PB1_USART5		BIT(20)
#define STM32_RCC_PB1_PWREN		BIT(28)
#define STM32_RCC_PB2_SPI1		BIT(12)
/* Reset causes definitions */

/* --- Watchdogs --- */

#define STM32_WWDG_CR               REG32(STM32_WWDG_BASE + 0x00)
#define STM32_WWDG_CFR              REG32(STM32_WWDG_BASE + 0x04)
#define STM32_WWDG_SR               REG32(STM32_WWDG_BASE + 0x08)

#define STM32_WWDG_TB_8             (3 << 7)
#define STM32_WWDG_EWI              BIT(9)

#define STM32_IWDG_KR               REG32(STM32_IWDG_BASE + 0x00)
#define STM32_IWDG_KR_UNLOCK		0x5555
#define STM32_IWDG_KR_RELOAD		0xaaaa
#define STM32_IWDG_KR_START		0xcccc
#define STM32_IWDG_PR               REG32(STM32_IWDG_BASE + 0x04)
#define STM32_IWDG_RLR              REG32(STM32_IWDG_BASE + 0x08)
#define STM32_IWDG_RLR_MAX		0x0fff
#define STM32_IWDG_SR               REG32(STM32_IWDG_BASE + 0x0C)
#define STM32_IWDG_SR_WVU		BIT(2)
#define STM32_IWDG_SR_RVU		BIT(1)
#define STM32_IWDG_SR_PVU		BIT(0)
#define STM32_IWDG_WINR             REG32(STM32_IWDG_BASE + 0x10)

/* --- Real-Time Clock --- */
/* --- Debug --- */
#define STM32_DBGMCU_IDCODE         REG32(STM32_DBGMCU_BASE + 0x00)
#define STM32_DBGMCU_CR             REG32(STM32_DBGMCU_BASE + 0x04)
<<<<<<< HEAD   (6f5daa driver/opt3100: Set min/max frequency that match the driver)
#ifndef CHIP_FAMILY_STM32H7
#define STM32_DBGMCU_APB1FZ         REG32(STM32_DBGMCU_BASE + 0x08)
#define STM32_DBGMCU_APB2FZ         REG32(STM32_DBGMCU_BASE + 0x0C)
#else  /* CHIP_FAMILY_STM32H7 */
#define STM32_DBGMCU_APB3FZ         REG32(STM32_DBGMCU_BASE + 0x34)
#define STM32_DBGMCU_APB1LFZ        REG32(STM32_DBGMCU_BASE + 0x3C)
#define STM32_DBGMCU_APB1HFZ        REG32(STM32_DBGMCU_BASE + 0x44)
#define STM32_DBGMCU_APB2FZ         REG32(STM32_DBGMCU_BASE + 0x4C)
#define STM32_DBGMCU_APB4FZ         REG32(STM32_DBGMCU_BASE + 0x54)
/* Alias */
#define STM32_DBGMCU_APB1FZ         STM32_DBGMCU_APB1LFZ
#endif /* CHIP_FAMILY_STM32H7 */

/* --- Flash --- */

#if defined(CHIP_FAMILY_STM32L)
#define STM32_FLASH_ACR             REG32(STM32_FLASH_REGS_BASE + 0x00)
#define STM32_FLASH_ACR_LATENCY		(1 << 0)
#define STM32_FLASH_ACR_PRFTEN		(1 << 1)
#define STM32_FLASH_ACR_ACC64		(1 << 2)
#define STM32_FLASH_PECR            REG32(STM32_FLASH_REGS_BASE + 0x04)
#define STM32_FLASH_PECR_PE_LOCK	(1 << 0)
#define STM32_FLASH_PECR_PRG_LOCK	(1 << 1)
#define STM32_FLASH_PECR_OPT_LOCK	(1 << 2)
#define STM32_FLASH_PECR_PROG		(1 << 3)
#define STM32_FLASH_PECR_ERASE		(1 << 9)
#define STM32_FLASH_PECR_FPRG		(1 << 10)
#define STM32_FLASH_PECR_OBL_LAUNCH	(1 << 18)
#define STM32_FLASH_PDKEYR          REG32(STM32_FLASH_REGS_BASE + 0x08)
#define STM32_FLASH_PEKEYR          REG32(STM32_FLASH_REGS_BASE + 0x0c)
#define STM32_FLASH_PEKEYR_KEY1		0x89ABCDEF
#define STM32_FLASH_PEKEYR_KEY2		0x02030405
#define STM32_FLASH_PRGKEYR         REG32(STM32_FLASH_REGS_BASE + 0x10)
#define STM32_FLASH_PRGKEYR_KEY1	0x8C9DAEBF
#define STM32_FLASH_PRGKEYR_KEY2	0x13141516
#define STM32_FLASH_OPTKEYR         REG32(STM32_FLASH_REGS_BASE + 0x14)
#define STM32_FLASH_OPTKEYR_KEY1	0xFBEAD9C8
#define STM32_FLASH_OPTKEYR_KEY2	0x24252627
#define STM32_FLASH_SR              REG32(STM32_FLASH_REGS_BASE + 0x18)
#define STM32_FLASH_OBR             REG32(STM32_FLASH_REGS_BASE + 0x1c)
#define STM32_FLASH_WRPR            REG32(STM32_FLASH_REGS_BASE + 0x20)

#define STM32_OPTB_RDP                  0x00
#define STM32_OPTB_USER                 0x04
#define STM32_OPTB_WRP1L                0x08
#define STM32_OPTB_WRP1H                0x0c
#define STM32_OPTB_WRP2L                0x10
#define STM32_OPTB_WRP2H                0x14
#define STM32_OPTB_WRP3L                0x18
#define STM32_OPTB_WRP3H                0x1c

#elif defined(CHIP_FAMILY_STM32F0) || defined(CHIP_FAMILY_STM32F3)
#define STM32_FLASH_ACR             REG32(STM32_FLASH_REGS_BASE + 0x00)
#define STM32_FLASH_ACR_LATENCY_SHIFT (0)
#define STM32_FLASH_ACR_LATENCY_MASK  (7 << STM32_FLASH_ACR_LATENCY_SHIFT)
#define STM32_FLASH_ACR_LATENCY     (1 << 0)
#define STM32_FLASH_ACR_PRFTEN      (1 << 4)
#define STM32_FLASH_KEYR            REG32(STM32_FLASH_REGS_BASE + 0x04)
#define  FLASH_KEYR_KEY1                0x45670123
#define  FLASH_KEYR_KEY2                0xCDEF89AB

#define STM32_FLASH_OPTKEYR         REG32(STM32_FLASH_REGS_BASE + 0x08)
#define  FLASH_OPTKEYR_KEY1             FLASH_KEYR_KEY1
#define  FLASH_OPTKEYR_KEY2             FLASH_KEYR_KEY2
#define STM32_FLASH_SR              REG32(STM32_FLASH_REGS_BASE + 0x0c)
#define  FLASH_SR_BUSY                  (1 << 0)
#define  FLASH_SR_PGERR                 (1 << 2)
#define  FLASH_SR_WRPRTERR              (1 << 4)
#define  FLASH_SR_ALL_ERR \
	(FLASH_SR_PGERR | FLASH_SR_WRPRTERR)
#define  FLASH_SR_EOP                   (1 << 5)
#define STM32_FLASH_CR              REG32(STM32_FLASH_REGS_BASE + 0x10)
#define  FLASH_CR_PG                    (1 << 0)
#define  FLASH_CR_PER                   (1 << 1)
#define  FLASH_CR_OPTPG                 (1 << 4)
#define  FLASH_CR_OPTER                 (1 << 5)
#define  FLASH_CR_STRT                  (1 << 6)
#define  FLASH_CR_LOCK                  (1 << 7)
#define  FLASH_CR_OPTWRE                (1 << 9)
#define  FLASH_CR_OBL_LAUNCH            (1 << 13)
#define STM32_FLASH_OPT_LOCKED      (!(STM32_FLASH_CR & FLASH_CR_OPTWRE))
#define STM32_FLASH_AR              REG32(STM32_FLASH_REGS_BASE + 0x14)
#define STM32_FLASH_OBR             REG32(STM32_FLASH_REGS_BASE + 0x1c)
#define STM32_FLASH_OBR_RDP_MASK        (3 << 1)
#define STM32_FLASH_WRPR            REG32(STM32_FLASH_REGS_BASE + 0x20)

#define STM32_OPTB_RDP_OFF              0x00
#define STM32_OPTB_USER_OFF             0x02
#define STM32_OPTB_WRP_OFF(n)           (0x08 + (n&3) * 2)
#define STM32_OPTB_WRP01                0x08
#define STM32_OPTB_WRP23                0x0c

#define STM32_OPTB_COMPL_SHIFT      8

#elif defined(CHIP_FAMILY_STM32L4)
#define STM32_FLASH_ACR             REG32(STM32_FLASH_REGS_BASE + 0x00)
#define STM32_FLASH_ACR_LATENCY_SHIFT (0)
#define STM32_FLASH_ACR_LATENCY_MASK  (7 << STM32_FLASH_ACR_LATENCY_SHIFT)
#define STM32_FLASH_ACR_PRFTEN      (1 << 8)
#define STM32_FLASH_ACR_ICEN        (1 << 9)
#define STM32_FLASH_ACR_DCEN        (1 << 10)
#define STM32_FLASH_ACR_ICRST       (1 << 11)
#define STM32_FLASH_ACR_DCRST       (1 << 12)
#define STM32_FLASH_PDKEYR          REG32(STM32_FLASH_REGS_BASE + 0x04)
#define STM32_FLASH_KEYR            REG32(STM32_FLASH_REGS_BASE + 0x08)
#define  FLASH_KEYR_KEY1            0x45670123
#define  FLASH_KEYR_KEY2            0xCDEF89AB
#define STM32_FLASH_OPTKEYR         REG32(STM32_FLASH_REGS_BASE + 0x0c)
#define  FLASH_OPTKEYR_KEY1         0x08192A3B
#define  FLASH_OPTKEYR_KEY2         0x4C5D6E7F
#define STM32_FLASH_SR              REG32(STM32_FLASH_REGS_BASE + 0x10)
#define  FLASH_SR_BUSY              (1 << 16)
#define  FLASH_SR_ERR_MASK          (0xc3fa)
#define STM32_FLASH_CR              REG32(STM32_FLASH_REGS_BASE + 0x14)
#define  FLASH_CR_PG                (1 << 0)
#define  FLASH_CR_PER               (1 << 1)
#define  FLASH_CR_STRT              (1 << 16)
#define  FLASH_CR_OPTSTRT           (1 << 17)
#define  FLASH_CR_OBL_LAUNCH        (1 << 27)
#define  FLASH_CR_OPTLOCK           (1 << 30)
#define  FLASH_CR_LOCK              (1 << 31)
#define  FLASH_CR_PNB(sec)          (((sec) & 0xff) << 3)
#define  FLASH_CR_PNB_MASK          FLASH_CR_PNB(0xff)
#define STM32_FLASH_ECCR            REG32(STM32_FLASH_REGS_BASE + 0x18)
#define STM32_FLASH_OPTR            REG32(STM32_FLASH_REGS_BASE + 0x20)
#define STM32_FLASH_PCROP1SR        REG32(STM32_FLASH_REGS_BASE + 0x24)
#define STM32_FLASH_PCROP1ER        REG32(STM32_FLASH_REGS_BASE + 0x28)
#define STM32_FLASH_WRP1AR          REG32(STM32_FLASH_REGS_BASE + 0x2C)
#define STM32_FLASH_WRP1BR          REG32(STM32_FLASH_REGS_BASE + 0x30)
#define  FLASH_WRP_START(val)       ((val) & 0xff)
#define  FLASH_WRP_END(val)         (((val) >> 16) & 0xff)
#define  FLASH_WRP_RANGE(strt, end) (((end) << 16) | (strt))
#define  FLASH_WRP_RANGE_DISABLED   FLASH_WRP_RANGE(0xFF, 0x00)
#define  FLASH_WRP_MASK             FLASH_WRP_RANGE(0xFF, 0xFF)

#define STM32_OPTB_USER_RDP         REG32(STM32_OPTB_BASE + 0x00)
#define STM32_OPTB_WRP1AR           REG32(STM32_OPTB_BASE + 0x18)
#define STM32_OPTB_WRP1BR           REG32(STM32_OPTB_BASE + 0x20)

#elif defined(CHIP_FAMILY_STM32F4)
#define STM32_FLASH_ACR             REG32(STM32_FLASH_REGS_BASE + 0x00)
#define STM32_FLASH_ACR_SHIFT           0
#define STM32_FLASH_ACR_LAT_MASK        0xf
#define STM32_FLASH_ACR_PRFTEN          (1 << 8)
#define STM32_FLASH_ACR_ICEN            (1 << 9)
#define STM32_FLASH_ACR_DCEN            (1 << 10)
#define STM32_FLASH_ACR_ICRST           (1 << 11)
#define STM32_FLASH_ACR_DCRST           (1 << 12)
#define STM32_FLASH_KEYR            REG32(STM32_FLASH_REGS_BASE + 0x04)
#define  FLASH_KEYR_KEY1                0x45670123
#define  FLASH_KEYR_KEY2                0xCDEF89AB
#define STM32_FLASH_OPTKEYR         REG32(STM32_FLASH_REGS_BASE + 0x08)
#define  FLASH_OPTKEYR_KEY1             0x08192A3B
#define  FLASH_OPTKEYR_KEY2             0x4C5D6E7F
#define STM32_FLASH_SR              REG32(STM32_FLASH_REGS_BASE + 0x0c)
#define  FLASH_SR_EOP                   (1 << 0)
#define  FLASH_SR_OPERR                 (1 << 1)
#define  FLASH_SR_WRPERR                (1 << 4)
#define  FLASH_SR_PGAERR                (1 << 5)
#define  FLASH_SR_PGPERR                (1 << 6)
#define  FLASH_SR_PGSERR                (1 << 7)
#define  FLASH_SR_RDERR                 (1 << 8)
#define  FLASH_SR_ALL_ERR \
	(FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | \
	 FLASH_SR_PGPERR | FLASH_SR_PGSERR | FLASH_SR_RDERR)
#define  FLASH_SR_BUSY                   (1 << 16)
#define STM32_FLASH_CR              REG32(STM32_FLASH_REGS_BASE + 0x10)
#define  FLASH_CR_PG                    (1 << 0)
#define  FLASH_CR_PER                   (1 << 1)
#define  FLASH_CR_MER                   (1 << 2)
#define STM32_FLASH_CR_SNB_OFFSET       (3)
#define STM32_FLASH_CR_SNB(sec) \
	(((sec) & 0xf) << STM32_FLASH_CR_SNB_OFFSET)
#define STM32_FLASH_CR_SNB_MASK         (STM32_FLASH_CR_SNB(0xf))
#define STM32_FLASH_CR_PSIZE_OFFSET     (8)
#define STM32_FLASH_CR_PSIZE(size) \
	(((size) & 0x3) << STM32_FLASH_CR_PSIZE_OFFSET)
#define STM32_FLASH_CR_PSIZE_MASK       (STM32_FLASH_CR_PSIZE(0x3))
#define  FLASH_CR_STRT                  (1 << 16)
#define  FLASH_CR_LOCK                  (1 << 31)
#define STM32_FLASH_OPTCR           REG32(STM32_FLASH_REGS_BASE + 0x14)
#define  FLASH_OPTLOCK                  (1 << 0)
#define  FLASH_OPTSTRT                  (1 << 1)
#define STM32_FLASH_BOR_LEV_OFFSET      (2)
#define STM32_FLASH_RDP_MASK            (0xFF << 8)
#define STM32_FLASH_nWRP_OFFSET         (16)
#define STM32_FLASH_nWRP(_bank)         (1 << (_bank + STM32_FLASH_nWRP_OFFSET))
#define STM32_FLASH_nWRP_ALL            (0xFF << STM32_FLASH_nWRP_OFFSET)
#define STM32_FLASH_OPT_LOCKED      (STM32_FLASH_OPTCR & FLASH_OPTLOCK)

#define STM32_OPTB_RDP_USER         REG32(STM32_OPTB_BASE + 0x00)
#define STM32_OPTB_RDP_OFF              0x00
#define STM32_OPTB_USER_OFF             0x02
#define STM32_OPTB_WRP_OFF(n)       (0x08 + (n&3) * 2)
#define STM32_OPTB_WP               REG32(STM32_OPTB_BASE + 0x08)
#define STM32_OPTB_nWRP(_bank)          (1 << (_bank))
#define STM32_OPTB_nWRP_ALL             (0xFF)

#define STM32_OPTB_COMPL_SHIFT      8

#define STM32_OTP_BLOCK_NB              16
#define STM32_OTP_BLOCK_SIZE            32
#define STM32_OTP_BLOCK_DATA(_block, _offset) \
	(STM32_OTP_BASE + STM32_OTP_BLOCK_SIZE * (_block) + (_offset) * 4)
#define STM32_OTP_UNLOCK_BYTE           0x00
#define STM32_OTP_LOCK_BYTE             0xFF
#define STM32_OTP_LOCK_BASE         \
	(STM32_OTP_BASE + STM32_OTP_BLOCK_NB * STM32_OTP_BLOCK_SIZE)
#define STM32_OTP_LOCK(_block) \
	(STM32_OTP_LOCK_BASE + ((_block) / 4) * 4)
#define STM32_OPT_LOCK_MASK(_block)    ((0xFF << ((_block) % 4) * 8))

#elif defined(CHIP_FAMILY_STM32H7)
#define STM32_FLASH_REG(bank, offset)     REG32(((bank) ? 0x100 : 0) + \
					  STM32_FLASH_REGS_BASE + (offset))

#define STM32_FLASH_ACR(bank)             STM32_FLASH_REG(bank, 0x00)
#define STM32_FLASH_ACR_LATENCY_SHIFT (0)
#define STM32_FLASH_ACR_LATENCY_MASK  (7 << STM32_FLASH_ACR_LATENCY_SHIFT)
#define STM32_FLASH_ACR_WRHIGHFREQ_85MHZ  (0 << 4)
#define STM32_FLASH_ACR_WRHIGHFREQ_185MHZ (1 << 4)
#define STM32_FLASH_ACR_WRHIGHFREQ_285MHZ (2 << 4)
#define STM32_FLASH_ACR_WRHIGHFREQ_385MHZ (3 << 4)

#define STM32_FLASH_KEYR(bank)            STM32_FLASH_REG(bank, 0x04)
#define  FLASH_KEYR_KEY1                  0x45670123
#define  FLASH_KEYR_KEY2                  0xCDEF89AB
#define STM32_FLASH_OPTKEYR(bank)         STM32_FLASH_REG(bank, 0x08)
#define  FLASH_OPTKEYR_KEY1               0x08192A3B
#define  FLASH_OPTKEYR_KEY2               0x4C5D6E7F
#define STM32_FLASH_CR(bank)              STM32_FLASH_REG(bank, 0x0C)
#define  FLASH_CR_LOCK                    (1 << 0)
#define  FLASH_CR_PG                      (1 << 1)
#define  FLASH_CR_SER                     (1 << 2)
#define  FLASH_CR_BER                     (1 << 3)
#define  FLASH_CR_PSIZE_BYTE              (0 << 4)
#define  FLASH_CR_PSIZE_HWORD             (1 << 4)
#define  FLASH_CR_PSIZE_WORD              (2 << 4)
#define  FLASH_CR_PSIZE_DWORD             (3 << 4)
#define  FLASH_CR_PSIZE_MASK              (3 << 4)
#define  FLASH_CR_FW                      (1 << 6)
#define  FLASH_CR_STRT                    (1 << 7)
#define  FLASH_CR_SNB(sec)                (((sec) & 0x7) << 8)
#define  FLASH_CR_SNB_MASK                FLASH_CR_SNB(0x7)
#define STM32_FLASH_SR(bank)              STM32_FLASH_REG(bank, 0x10)
#define  FLASH_SR_BUSY                    (1 << 0)
#define  FLASH_SR_WBNE                    (1 << 1)
#define  FLASH_SR_QW                      (1 << 2)
#define  FLASH_SR_CRC_BUSY                (1 << 3)
#define  FLASH_SR_EOP                     (1 << 16)
#define  FLASH_SR_WRPERR                  (1 << 17)
#define  FLASH_SR_PGSERR                  (1 << 18)
#define  FLASH_SR_STRBERR                 (1 << 19)
#define  FLASH_SR_INCERR                  (1 << 21)
#define  FLASH_SR_OPERR                   (1 << 22)
#define  FLASH_SR_RDPERR                  (1 << 23)
#define  FLASH_SR_RDSERR                  (1 << 24)
#define  FLASH_SR_SNECCERR                (1 << 25)
#define  FLASH_SR_DBECCERR                (1 << 26)
#define  FLASH_SR_CRCEND                  (1 << 27)
#define STM32_FLASH_CCR(bank)             STM32_FLASH_REG(bank, 0x14)
#define  FLASH_CCR_ERR_MASK              (FLASH_SR_WRPERR | FLASH_SR_PGSERR \
					| FLASH_SR_STRBERR | FLASH_SR_INCERR \
					| FLASH_SR_OPERR | FLASH_SR_RDPERR \
					| FLASH_SR_RDSERR | FLASH_SR_SNECCERR \
					| FLASH_SR_DBECCERR)
#define STM32_FLASH_OPTCR(bank)           STM32_FLASH_REG(bank, 0x18)
#define  FLASH_OPTCR_OPTLOCK              (1 << 0)
#define  FLASH_OPTCR_OPTSTART             (1 << 1)
#define STM32_FLASH_OPTSR_CUR(bank)       STM32_FLASH_REG(bank, 0x1C)
#define STM32_FLASH_OPTSR_PRG(bank)       STM32_FLASH_REG(bank, 0x20)
#define  FLASH_OPTSR_BUSY                 (1 << 0)   /* only in OPTSR_CUR */
#define  FLASH_OPTSR_RDP_MASK             (0xFF << 8)
#define  FLASH_OPTSR_RDP_LEVEL_0          (0xAA << 8)
/* RDP Level 1: Anything but 0xAA/0xCC */
#define  FLASH_OPTSR_RDP_LEVEL_1          (0x00 << 8)
#define  FLASH_OPTSR_RDP_LEVEL_2          (0xCC << 8)
#define  FLASH_OPTSR_RSS1                 (1 << 26)
#define  FLASH_OPTSR_RSS2                 (1 << 27)
#define STM32_FLASH_OPTCCR(bank)          STM32_FLASH_REG(bank, 0x24)
#define STM32_FLASH_PRAR_CUR(bank)        STM32_FLASH_REG(bank, 0x28)
#define STM32_FLASH_PRAR_PRG(bank)        STM32_FLASH_REG(bank, 0x2C)
#define STM32_FLASH_SCAR_CUR(bank)        STM32_FLASH_REG(bank, 0x30)
#define STM32_FLASH_SCAR_PRG(bank)        STM32_FLASH_REG(bank, 0x34)
#define STM32_FLASH_WPSN_CUR(bank)        STM32_FLASH_REG(bank, 0x38)
#define STM32_FLASH_WPSN_PRG(bank)        STM32_FLASH_REG(bank, 0x3C)
#define STM32_FLASH_BOOT_CUR(bank)        STM32_FLASH_REG(bank, 0x40)
#define STM32_FLASH_BOOT_PRG(bank)        STM32_FLASH_REG(bank, 0x44)
#define STM32_FLASH_CRC_CR(bank)          STM32_FLASH_REG(bank, 0x50)
#define STM32_FLASH_CRC_SADDR(bank)       STM32_FLASH_REG(bank, 0x54)
#define STM32_FLASH_CRC_EADDR(bank)       STM32_FLASH_REG(bank, 0x58)
#define STM32_FLASH_CRC_DATA(bank)        STM32_FLASH_REG(bank, 0x5C)
#define STM32_FLASH_ECC_FA(bank)          STM32_FLASH_REG(bank, 0x60)

#else
#error Unsupported chip variant
#endif

/* --- External Interrupts --- */
#ifndef CHIP_FAMILY_STM32H7
#define STM32_EXTI_IMR              REG32(STM32_EXTI_BASE + 0x00)
#define STM32_EXTI_EMR              REG32(STM32_EXTI_BASE + 0x04)
#define STM32_EXTI_RTSR             REG32(STM32_EXTI_BASE + 0x08)
#define STM32_EXTI_FTSR             REG32(STM32_EXTI_BASE + 0x0c)
#define STM32_EXTI_SWIER            REG32(STM32_EXTI_BASE + 0x10)
#define STM32_EXTI_PR               REG32(STM32_EXTI_BASE + 0x14)
#else  /* CHIP_FAMILY_STM32H7 */
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
#endif /* CHIP_FAMILY_STM32H7 */

#if defined(CHIP_FAMILY_STM32F0) || defined(CHIP_FAMILY_STM32F3) || \
	defined(CHIP_FAMILY_STM32F4)
#define EXTI_RTC_ALR_EVENT (1 << 17)
#endif

/* --- ADC --- */
#if defined(CHIP_VARIANT_STM32F373) || defined(CHIP_FAMILY_STM32F4)
#define STM32_ADC_SR               REG32(STM32_ADC1_BASE + 0x00)
#define STM32_ADC_CR1              REG32(STM32_ADC1_BASE + 0x04)
#define STM32_ADC_CR2              REG32(STM32_ADC1_BASE + 0x08)
#define STM32_ADC_SMPR1            REG32(STM32_ADC1_BASE + 0x0C)
#define STM32_ADC_SMPR2            REG32(STM32_ADC1_BASE + 0x10)
#define STM32_ADC_JOFR(n)          REG32(STM32_ADC1_BASE + 0x14 + ((n)&3) * 4)
#define STM32_ADC_HTR              REG32(STM32_ADC1_BASE + 0x24)
#define STM32_ADC_LTR              REG32(STM32_ADC1_BASE + 0x28)
#define STM32_ADC_SQR(n)           REG32(STM32_ADC1_BASE + 0x28 + ((n)&3) * 4)
#define STM32_ADC_SQR1             REG32(STM32_ADC1_BASE + 0x2C)
#define STM32_ADC_SQR2             REG32(STM32_ADC1_BASE + 0x30)
#define STM32_ADC_SQR3             REG32(STM32_ADC1_BASE + 0x34)
#define STM32_ADC_JSQR             REG32(STM32_ADC1_BASE + 0x38)
#define STM32_ADC_JDR(n)           REG32(STM32_ADC1_BASE + 0x3C + ((n)&3) * 4)
#define STM32_ADC_DR               REG32(STM32_ADC1_BASE + 0x4C)
#elif defined(CHIP_FAMILY_STM32F0)
#define STM32_ADC_ISR              REG32(STM32_ADC1_BASE + 0x00)
#define STM32_ADC_ISR_ADRDY        (1 << 0)
#define STM32_ADC_IER              REG32(STM32_ADC1_BASE + 0x04)
#define STM32_ADC_IER_AWDIE        (1 << 7)
#define STM32_ADC_IER_OVRIE        (1 << 4)
#define STM32_ADC_IER_EOSEQIE      (1 << 3)
#define STM32_ADC_IER_EOCIE        (1 << 2)
#define STM32_ADC_IER_EOSMPIE      (1 << 1)
#define STM32_ADC_IER_ADRDYIE      (1 << 0)

#define STM32_ADC_CR               REG32(STM32_ADC1_BASE + 0x08)
#define STM32_ADC_CR_ADEN          (1 << 0)
#define STM32_ADC_CR_ADDIS         (1 << 1)
#define STM32_ADC_CR_ADCAL         (1 << 31)
#define STM32_ADC_CFGR1            REG32(STM32_ADC1_BASE + 0x0C)
/* Analog watchdog channel selection */
#define STM32_ADC_CFGR1_AWDCH_MASK (0x1f << 26)
#define STM32_ADC_CFGR1_AWDEN      (1 << 23)
#define STM32_ADC_CFGR1_AWDSGL     (1 << 22)
/* Selects single vs continuous */
#define STM32_ADC_CFGR1_CONT       (1 << 13)
/* Selects ADC_DR overwrite vs preserve */
#define STM32_ADC_CFGR1_OVRMOD     (1 << 12)
/* External trigger polarity selection */
#define STM32_ADC_CFGR1_EXTEN_DIS  (0 << 10)
#define STM32_ADC_CFGR1_EXTEN_RISE (1 << 10)
#define STM32_ADC_CFGR1_EXTEN_FALL (2 << 10)
#define STM32_ADC_CFGR1_EXTEN_BOTH (3 << 10)
#define STM32_ADC_CFGR1_EXTEN_MASK (3 << 10)
/* External trigger selection */
#define STM32_ADC_CFGR1_TRG0	   (0 << 6)
#define STM32_ADC_CFGR1_TRG1	   (1 << 6)
#define STM32_ADC_CFGR1_TRG2	   (2 << 6)
#define STM32_ADC_CFGR1_TRG3	   (3 << 6)
#define STM32_ADC_CFGR1_TRG4	   (4 << 6)
#define STM32_ADC_CFGR1_TRG5	   (5 << 6)
#define STM32_ADC_CFGR1_TRG6	   (6 << 6)
#define STM32_ADC_CFGR1_TRG7	   (7 << 6)
#define STM32_ADC_CFGR1_TRG_MASK   (7 << 6)
/* Selects circular vs one-shot */
#define STM32_ADC_CFGR1_DMACFG     (1 << 1)
#define STM32_ADC_CFGR1_DMAEN      (1 << 0)
#define STM32_ADC_CFGR2            REG32(STM32_ADC1_BASE + 0x10)
/* Sampling time selection - 1.5 ADC cycles min, 239.5 cycles max */
#define STM32_ADC_SMPR             REG32(STM32_ADC1_BASE + 0x14)
#define STM32_ADC_SMPR_1_5_CY      0x0
#define STM32_ADC_SMPR_7_5_CY      0x1
#define STM32_ADC_SMPR_13_5_CY     0x2
#define STM32_ADC_SMPR_28_5_CY     0x3
#define STM32_ADC_SMPR_41_5_CY     0x4
#define STM32_ADC_SMPR_55_5_CY     0x5
#define STM32_ADC_SMPR_71_5_CY     0x6
#define STM32_ADC_SMPR_239_5_CY    0x7
#define STM32_ADC_TR               REG32(STM32_ADC1_BASE + 0x20)
#define STM32_ADC_CHSELR           REG32(STM32_ADC1_BASE + 0x28)
#define STM32_ADC_DR               REG32(STM32_ADC1_BASE + 0x40)
#define STM32_ADC_CCR              REG32(STM32_ADC1_BASE + 0x308)
#elif defined(CHIP_FAMILY_STM32L)
#define STM32_ADC_SR               REG32(STM32_ADC1_BASE + 0x00)
#define STM32_ADC_CR1              REG32(STM32_ADC1_BASE + 0x04)
#define STM32_ADC_CR2              REG32(STM32_ADC1_BASE + 0x08)
#define STM32_ADC_SMPR1            REG32(STM32_ADC1_BASE + 0x0C)
#define STM32_ADC_SMPR2            REG32(STM32_ADC1_BASE + 0x10)
#define STM32_ADC_SMPR3            REG32(STM32_ADC1_BASE + 0x14)
#define STM32_ADC_JOFR1            REG32(STM32_ADC1_BASE + 0x18)
#define STM32_ADC_JOFR2            REG32(STM32_ADC1_BASE + 0x1C)
#define STM32_ADC_JOFR3            REG32(STM32_ADC1_BASE + 0x20)
#define STM32_ADC_JOFR4            REG32(STM32_ADC1_BASE + 0x24)
#define STM32_ADC_HTR              REG32(STM32_ADC1_BASE + 0x28)
#define STM32_ADC_LTR              REG32(STM32_ADC1_BASE + 0x2C)
#define STM32_ADC_SQR(n)           REG32(STM32_ADC1_BASE + 0x2C + (n) * 4)
#define STM32_ADC_SQR1             REG32(STM32_ADC1_BASE + 0x30)
#define STM32_ADC_SQR2             REG32(STM32_ADC1_BASE + 0x34)
#define STM32_ADC_SQR3             REG32(STM32_ADC1_BASE + 0x38)
#define STM32_ADC_SQR4             REG32(STM32_ADC1_BASE + 0x3C)
#define STM32_ADC_SQR5             REG32(STM32_ADC1_BASE + 0x40)
#define STM32_ADC_JSQR             REG32(STM32_ADC1_BASE + 0x44)
#define STM32_ADC_JDR1             REG32(STM32_ADC1_BASE + 0x48)
#define STM32_ADC_JDR2             REG32(STM32_ADC1_BASE + 0x4C)
#define STM32_ADC_JDR3             REG32(STM32_ADC1_BASE + 0x50)
#define STM32_ADC_JDR3             REG32(STM32_ADC1_BASE + 0x50)
#define STM32_ADC_JDR4             REG32(STM32_ADC1_BASE + 0x54)
#define STM32_ADC_DR               REG32(STM32_ADC1_BASE + 0x58)
#define STM32_ADC_SMPR0            REG32(STM32_ADC1_BASE + 0x5C)

#define STM32_ADC_CCR              REG32(STM32_ADC_BASE + 0x04)
#endif

/* --- Comparators --- */
#if defined(CHIP_FAMILY_STM32L)
#define STM32_COMP_CSR              REG32(STM32_COMP_BASE + 0x00)

#define STM32_COMP_OUTSEL_TIM2_IC4  (0 << 21)
#define STM32_COMP_OUTSEL_TIM2_OCR  (1 << 21)
#define STM32_COMP_OUTSEL_TIM3_IC4  (2 << 21)
#define STM32_COMP_OUTSEL_TIM3_OCR  (3 << 21)
#define STM32_COMP_OUTSEL_TIM4_IC4  (4 << 21)
#define STM32_COMP_OUTSEL_TIM4_OCR  (5 << 21)
#define STM32_COMP_OUTSEL_TIM10_IC1 (6 << 21)
#define STM32_COMP_OUTSEL_NONE      (7 << 21)

#define STM32_COMP_INSEL_NONE       (0 << 18)
#define STM32_COMP_INSEL_PB3        (1 << 18)
#define STM32_COMP_INSEL_VREF       (2 << 18)
#define STM32_COMP_INSEL_VREF34     (3 << 18)
#define STM32_COMP_INSEL_VREF12     (4 << 18)
#define STM32_COMP_INSEL_VREF14     (5 << 18)
#define STM32_COMP_INSEL_DAC_OUT1   (6 << 18)
#define STM32_COMP_INSEL_DAC_OUT2   (7 << 18)

#define STM32_COMP_WNDWE            (1 << 17)
#define STM32_COMP_VREFOUTEN        (1 << 16)
#define STM32_COMP_CMP2OUT          (1 << 13)
#define STM32_COMP_SPEED_FAST       (1 << 12)

#define STM32_COMP_CMP1OUT          (1 << 7)
#define STM32_COMP_CMP1EN           (1 << 4)

#define STM32_COMP_400KPD           (1 << 3)
#define STM32_COMP_10KPD            (1 << 2)
#define STM32_COMP_400KPU           (1 << 1)
#define STM32_COMP_10KPU            (1 << 0)

#elif defined(CHIP_FAMILY_STM32F0) || defined(CHIP_FAMILY_STM32F3)
#define STM32_COMP_CSR              REG32(STM32_COMP_BASE + 0x1C)

#define STM32_COMP_CMP2LOCK            (1 << 31)
#define STM32_COMP_CMP2OUT             (1 << 30)
#define STM32_COMP_CMP2HYST_HI         (3 << 28)
#define STM32_COMP_CMP2HYST_MED        (2 << 28)
#define STM32_COMP_CMP2HYST_LOW        (1 << 28)
#define STM32_COMP_CMP2HYST_NO         (0 << 28)
#define STM32_COMP_CMP2POL             (1 << 27)

#define STM32_COMP_CMP2OUTSEL_TIM3_OCR (7 << 24)
#define STM32_COMP_CMP2OUTSEL_TIM3_IC1 (6 << 24)
#define STM32_COMP_CMP2OUTSEL_TIM2_OCR (5 << 24)
#define STM32_COMP_CMP2OUTSEL_TIM2_IC4 (4 << 24)
#ifdef CHIP_VARIANT_STM32F373
#define STM32_COMP_CMP2OUTSEL_TIM4_OCR (3 << 24)
#define STM32_COMP_CMP2OUTSEL_TIM4_IC1 (2 << 24)
#define STM32_COMP_CMP2OUTSEL_TIM16_BRK (1 << 24)
#else
#define STM32_COMP_CMP2OUTSEL_TIM1_OCR (3 << 24)
#define STM32_COMP_CMP2OUTSEL_TIM1_IC1 (2 << 24)
#define STM32_COMP_CMP2OUTSEL_TIM1_BRK (1 << 24)
#endif
#define STM32_COMP_CMP2OUTSEL_NONE     (0 << 24)
#define STM32_COMP_WNDWEN              (1 << 23)

#define STM32_COMP_CMP2INSEL_MASK      (7 << 20)
#define STM32_COMP_CMP2INSEL_INM7      (6 << 20) /* STM32F373 only */
#define STM32_COMP_CMP2INSEL_INM6      (6 << 20)
#define STM32_COMP_CMP2INSEL_INM5      (5 << 20)
#define STM32_COMP_CMP2INSEL_INM4      (4 << 20)
#define STM32_COMP_CMP2INSEL_VREF      (3 << 20)
#define STM32_COMP_CMP2INSEL_VREF34    (2 << 20)
#define STM32_COMP_CMP2INSEL_VREF12    (1 << 20)
#define STM32_COMP_CMP2INSEL_VREF14    (0 << 20)

#define STM32_COMP_CMP2MODE_VLSPEED    (3 << 18)
#define STM32_COMP_CMP2MODE_LSPEED     (2 << 18)
#define STM32_COMP_CMP2MODE_MSPEED     (1 << 18)
#define STM32_COMP_CMP2MODE_HSPEED     (0 << 18)
#define STM32_COMP_CMP2EN              (1 << 16)

#define STM32_COMP_CMP1LOCK            (1 << 15)
#define STM32_COMP_CMP1OUT             (1 << 14)
#define STM32_COMP_CMP1HYST_HI         (3 << 12)
#define STM32_COMP_CMP1HYST_MED        (2 << 12)
#define STM32_COMP_CMP1HYST_LOW        (1 << 12)
#define STM32_COMP_CMP1HYST_NO         (0 << 12)
#define STM32_COMP_CMP1POL             (1 << 11)

#ifdef CHIP_VARIANT_STM32F373
#define STM32_COMP_CMP1OUTSEL_TIM5_OCR (7 << 8)
#define STM32_COMP_CMP1OUTSEL_TIM5_IC4 (6 << 8)
#define STM32_COMP_CMP1OUTSEL_TIM2_OCR (5 << 8)
#define STM32_COMP_CMP1OUTSEL_TIM2_IC4 (4 << 8)
#define STM32_COMP_CMP1OUTSEL_TIM3_OCR (3 << 8)
#define STM32_COMP_CMP1OUTSEL_TIM3_IC1 (2 << 8)
#define STM32_COMP_CMP1OUTSEL_TIM15_BRK (1 << 8)
#else
#define STM32_COMP_CMP1OUTSEL_TIM3_OCR (7 << 8)
#define STM32_COMP_CMP1OUTSEL_TIM3_IC1 (6 << 8)
#define STM32_COMP_CMP1OUTSEL_TIM2_OCR (5 << 8)
#define STM32_COMP_CMP1OUTSEL_TIM2_IC4 (4 << 8)
#define STM32_COMP_CMP1OUTSEL_TIM1_OCR (3 << 8)
#define STM32_COMP_CMP1OUTSEL_TIM1_IC1 (2 << 8)
#define STM32_COMP_CMP1OUTSEL_TIM1_BRK (1 << 8)
#endif
#define STM32_COMP_CMP1OUTSEL_NONE     (0 << 8)

#define STM32_COMP_CMP1INSEL_MASK      (7 << 4)
#define STM32_COMP_CMP1INSEL_INM7      (7 << 4) /* STM32F373 only */
#define STM32_COMP_CMP1INSEL_INM6      (6 << 4)
#define STM32_COMP_CMP1INSEL_INM5      (5 << 4)
#define STM32_COMP_CMP1INSEL_INM4      (4 << 4)
#define STM32_COMP_CMP1INSEL_VREF      (3 << 4)
#define STM32_COMP_CMP1INSEL_VREF34    (2 << 4)
#define STM32_COMP_CMP1INSEL_VREF12    (1 << 4)
#define STM32_COMP_CMP1INSEL_VREF14    (0 << 4)

#define STM32_COMP_CMP1MODE_VLSPEED    (3 << 2)
#define STM32_COMP_CMP1MODE_LSPEED     (2 << 2)
#define STM32_COMP_CMP1MODE_MSPEED     (1 << 2)
#define STM32_COMP_CMP1MODE_HSPEED     (0 << 2)
#define STM32_COMP_CMP1SW1             (1 << 1)
#define STM32_COMP_CMP1EN              (1 << 0)
#endif
=======
>>>>>>> BRANCH (40d09f ectool: motionsense: add commands for fast/manual offset com)
/* --- Routing interface --- */
/* STM32L1xx only */
#define STM32_RI_ICR                REG32(STM32_COMP_BASE + 0x04)
#define STM32_RI_ASCR1              REG32(STM32_COMP_BASE + 0x08)
#define STM32_RI_ASCR2              REG32(STM32_COMP_BASE + 0x0C)
#define STM32_RI_HYSCR1             REG32(STM32_COMP_BASE + 0x10)
#define STM32_RI_HYSCR2             REG32(STM32_COMP_BASE + 0x14)
#define STM32_RI_HYSCR3             REG32(STM32_COMP_BASE + 0x18)
#define STM32_RI_AMSR1              REG32(STM32_COMP_BASE + 0x1C)
#define STM32_RI_CMR1               REG32(STM32_COMP_BASE + 0x20)
#define STM32_RI_CICR1              REG32(STM32_COMP_BASE + 0x24)
#define STM32_RI_AMSR2              REG32(STM32_COMP_BASE + 0x28)
#define STM32_RI_CMR2               REG32(STM32_COMP_BASE + 0x30)
#define STM32_RI_CICR2              REG32(STM32_COMP_BASE + 0x34)
#define STM32_RI_AMSR3              REG32(STM32_COMP_BASE + 0x38)
#define STM32_RI_CMR3               REG32(STM32_COMP_BASE + 0x3C)
#define STM32_RI_CICR3              REG32(STM32_COMP_BASE + 0x40)
#define STM32_RI_AMSR4              REG32(STM32_COMP_BASE + 0x44)
#define STM32_RI_CMR4               REG32(STM32_COMP_BASE + 0x48)
#define STM32_RI_CICR4              REG32(STM32_COMP_BASE + 0x4C)
#define STM32_RI_AMSR5              REG32(STM32_COMP_BASE + 0x50)
#define STM32_RI_CMR5               REG32(STM32_COMP_BASE + 0x54)
#define STM32_RI_CICR5              REG32(STM32_COMP_BASE + 0x58)

/* --- DAC --- */
#define STM32_DAC_CR               REG32(STM32_DAC_BASE + 0x00)
#define STM32_DAC_SWTRIGR          REG32(STM32_DAC_BASE + 0x04)
#define STM32_DAC_DHR12R1          REG32(STM32_DAC_BASE + 0x08)
#define STM32_DAC_DHR12L1          REG32(STM32_DAC_BASE + 0x0C)
#define STM32_DAC_DHR8R1           REG32(STM32_DAC_BASE + 0x10)
#define STM32_DAC_DHR12R2          REG32(STM32_DAC_BASE + 0x14)
#define STM32_DAC_DHR12L2          REG32(STM32_DAC_BASE + 0x18)
#define STM32_DAC_DHR8R2           REG32(STM32_DAC_BASE + 0x1C)
#define STM32_DAC_DHR12RD          REG32(STM32_DAC_BASE + 0x20)
#define STM32_DAC_DHR12LD          REG32(STM32_DAC_BASE + 0x24)
#define STM32_DAC_DHR8RD           REG32(STM32_DAC_BASE + 0x28)
#define STM32_DAC_DOR1             REG32(STM32_DAC_BASE + 0x2C)
#define STM32_DAC_DOR2             REG32(STM32_DAC_BASE + 0x30)
#define STM32_DAC_SR               REG32(STM32_DAC_BASE + 0x34)

#define STM32_DAC_CR_DMAEN2        BIT(28)
#define STM32_DAC_CR_TSEL2_SWTRG   (7 << 19)
#define STM32_DAC_CR_TSEL2_TMR4    (5 << 19)
#define STM32_DAC_CR_TSEL2_TMR2    (4 << 19)
#define STM32_DAC_CR_TSEL2_TMR9    (3 << 19)
#define STM32_DAC_CR_TSEL2_TMR7    (2 << 19)
#define STM32_DAC_CR_TSEL2_TMR6    (0 << 19)
#define STM32_DAC_CR_TSEL2_MASK    (7 << 19)
#define STM32_DAC_CR_TEN2          BIT(18)
#define STM32_DAC_CR_BOFF2         BIT(17)
#define STM32_DAC_CR_EN2           BIT(16)
#define STM32_DAC_CR_DMAEN1        BIT(12)
#define STM32_DAC_CR_TSEL1_SWTRG   (7 << 3)
#define STM32_DAC_CR_TSEL1_TMR4    (5 << 3)
#define STM32_DAC_CR_TSEL1_TMR2    (4 << 3)
#define STM32_DAC_CR_TSEL1_TMR9    (3 << 3)
#define STM32_DAC_CR_TSEL1_TMR7    (2 << 3)
#define STM32_DAC_CR_TSEL1_TMR6    (0 << 3)
#define STM32_DAC_CR_TSEL1_MASK    (7 << 3)
#define STM32_DAC_CR_TEN1          BIT(2)
#define STM32_DAC_CR_BOFF1         BIT(1)
#define STM32_DAC_CR_EN1           BIT(0)
/* --- CRC --- */
#define STM32_CRC_DR                REG32(STM32_CRC_BASE + 0x0)
#define STM32_CRC_DR32              REG32(STM32_CRC_BASE + 0x0)
#define STM32_CRC_DR16              REG16(STM32_CRC_BASE + 0x0)
#define STM32_CRC_DR8               REG8(STM32_CRC_BASE + 0x0)

#define STM32_CRC_IDR               REG32(STM32_CRC_BASE + 0x4)
#define STM32_CRC_CR                REG32(STM32_CRC_BASE + 0x8)
#define STM32_CRC_INIT              REG32(STM32_CRC_BASE + 0x10)
#define STM32_CRC_POL               REG32(STM32_CRC_BASE + 0x14)

#define STM32_CRC_CR_RESET          BIT(0)
#define STM32_CRC_CR_POLYSIZE_32    (0 << 3)
#define STM32_CRC_CR_POLYSIZE_16    (1 << 3)
#define STM32_CRC_CR_POLYSIZE_8     (2 << 3)
#define STM32_CRC_CR_POLYSIZE_7     (3 << 3)
#define STM32_CRC_CR_REV_IN_BYTE    (1 << 5)
#define STM32_CRC_CR_REV_IN_HWORD   (2 << 5)
#define STM32_CRC_CR_REV_IN_WORD    (3 << 5)
#define STM32_CRC_CR_REV_OUT        BIT(7)

/* --- PMSE --- */
#define STM32_PMSE_ARCR             REG32(STM32_PMSE_BASE + 0x0)
#define STM32_PMSE_ACCR             REG32(STM32_PMSE_BASE + 0x4)
#define STM32_PMSE_CR               REG32(STM32_PMSE_BASE + 0x8)
#define STM32_PMSE_CRTDR            REG32(STM32_PMSE_BASE + 0x14)
#define STM32_PMSE_IER              REG32(STM32_PMSE_BASE + 0x18)
#define STM32_PMSE_SR               REG32(STM32_PMSE_BASE + 0x1c)
#define STM32_PMSE_IFCR             REG32(STM32_PMSE_BASE + 0x20)
#define STM32_PMSE_PxPMR(x)         REG32(STM32_PMSE_BASE + 0x2c + (x) * 4)
#define STM32_PMSE_PAPMR            REG32(STM32_PMSE_BASE + 0x2c)
#define STM32_PMSE_PBPMR            REG32(STM32_PMSE_BASE + 0x30)
#define STM32_PMSE_PCPMR            REG32(STM32_PMSE_BASE + 0x34)
#define STM32_PMSE_PDPMR            REG32(STM32_PMSE_BASE + 0x38)
#define STM32_PMSE_PEPMR            REG32(STM32_PMSE_BASE + 0x3c)
#define STM32_PMSE_PFPMR            REG32(STM32_PMSE_BASE + 0x40)
#define STM32_PMSE_PGPMR            REG32(STM32_PMSE_BASE + 0x44)
#define STM32_PMSE_PHPMR            REG32(STM32_PMSE_BASE + 0x48)
#define STM32_PMSE_PIPMR            REG32(STM32_PMSE_BASE + 0x4c)
#define STM32_PMSE_MRCR             REG32(STM32_PMSE_BASE + 0x100)
#define STM32_PMSE_MCCR             REG32(STM32_PMSE_BASE + 0x104)

/* --- USB --- */
#define STM32_USB_EP(n)            REG16(STM32_USB_FS_BASE + (n) * 4)

#define STM32_USB_CNTR             REG16(STM32_USB_FS_BASE + 0x40)

#define STM32_USB_CNTR_FRES	    BIT(0)
#define STM32_USB_CNTR_PDWN	    BIT(1)
#define STM32_USB_CNTR_LP_MODE	    BIT(2)
#define STM32_USB_CNTR_FSUSP	    BIT(3)
#define STM32_USB_CNTR_RESUME	    BIT(4)
#define STM32_USB_CNTR_L1RESUME	    BIT(5)
#define STM32_USB_CNTR_L1REQM	    BIT(7)
#define STM32_USB_CNTR_ESOFM	    BIT(8)
#define STM32_USB_CNTR_SOFM	    BIT(9)
#define STM32_USB_CNTR_RESETM	    BIT(10)
#define STM32_USB_CNTR_SUSPM	    BIT(11)
#define STM32_USB_CNTR_WKUPM	    BIT(12)
#define STM32_USB_CNTR_ERRM	    BIT(13)
#define STM32_USB_CNTR_PMAOVRM	    BIT(14)
#define STM32_USB_CNTR_CTRM	    BIT(15)

#define STM32_USB_ISTR             REG16(STM32_USB_FS_BASE + 0x44)

#define STM32_USB_ISTR_EP_ID_MASK   (0x000f)
#define STM32_USB_ISTR_DIR	    BIT(4)
#define STM32_USB_ISTR_L1REQ	    BIT(7)
#define STM32_USB_ISTR_ESOF	    BIT(8)
#define STM32_USB_ISTR_SOF	    BIT(9)
#define STM32_USB_ISTR_RESET	    BIT(10)
#define STM32_USB_ISTR_SUSP	    BIT(11)
#define STM32_USB_ISTR_WKUP	    BIT(12)
#define STM32_USB_ISTR_ERR	    BIT(13)
#define STM32_USB_ISTR_PMAOVR	    BIT(14)
#define STM32_USB_ISTR_CTR	    BIT(15)

#define STM32_USB_FNR              REG16(STM32_USB_FS_BASE + 0x48)

#define STM32_USB_FNR_RXDP_RXDM_SHIFT (14)
#define STM32_USB_FNR_RXDP_RXDM_MASK  (3 << STM32_USB_FNR_RXDP_RXDM_SHIFT)

#define STM32_USB_DADDR            REG16(STM32_USB_FS_BASE + 0x4C)
#define STM32_USB_BTABLE           REG16(STM32_USB_FS_BASE + 0x50)
#define STM32_USB_LPMCSR           REG16(STM32_USB_FS_BASE + 0x54)
#define STM32_USB_BCDR             REG16(STM32_USB_FS_BASE + 0x58)

#define STM32_USB_BCDR_BCDEN	    BIT(0)
#define STM32_USB_BCDR_DCDEN	    BIT(1)
#define STM32_USB_BCDR_PDEN	    BIT(2)
#define STM32_USB_BCDR_SDEN	    BIT(3)
#define STM32_USB_BCDR_DCDET	    BIT(4)
#define STM32_USB_BCDR_PDET	    BIT(5)
#define STM32_USB_BCDR_SDET	    BIT(6)
#define STM32_USB_BCDR_PS2DET	    BIT(7)

#define EP_MASK     0x0F0F
#define EP_TX_DTOG  0x0040
#define EP_TX_MASK  0x0030
#define EP_TX_VALID 0x0030
#define EP_TX_NAK   0x0020
#define EP_TX_STALL 0x0010
#define EP_TX_DISAB 0x0000
#define EP_RX_DTOG  0x4000
#define EP_RX_MASK  0x3000
#define EP_RX_VALID 0x3000
#define EP_RX_NAK   0x2000
#define EP_RX_STALL 0x1000
#define EP_RX_DISAB 0x0000

#define EP_STATUS_OUT 0x0100

#define EP_TX_RX_MASK (EP_TX_MASK | EP_RX_MASK)
#define EP_TX_RX_VALID (EP_TX_VALID | EP_RX_VALID)

#define STM32_TOGGLE_EP(n, mask, val, flags) \
	STM32_USB_EP(n) = (((STM32_USB_EP(n) & (EP_MASK | (mask))) \
			^ (val)) | (flags))

/* --- TRNG --- */
#define STM32_RNG_CR                REG32(STM32_RNG_BASE + 0x0)
#define STM32_RNG_CR_RNGEN		BIT(2)
#define STM32_RNG_CR_IE			BIT(3)
#define STM32_RNG_CR_CED		BIT(5)
#define STM32_RNG_SR                REG32(STM32_RNG_BASE + 0x4)
#define STM32_RNG_SR_DRDY		BIT(0)
#define STM32_RNG_DR                REG32(STM32_RNG_BASE + 0x8)

/* --- AXI interconnect --- */

/* STM32H7: AXI_TARGx_FN_MOD exists for masters x = 1, 2 and 7 */
#define STM32_AXI_TARG_FN_MOD(x)    REG32(STM32_GPV_BASE + 0x1108 + \
					  0x1000 * (x))
#define  WRITE_ISS_OVERRIDE         BIT(1)
#define  READ_ISS_OVERRIDE          BIT(0)

/* --- MISC --- */
#define STM32_UNIQUE_ID_ADDRESS     REG32_ADDR(STM32_UNIQUE_ID_BASE)
#define STM32_UNIQUE_ID_LENGTH      (3 * 4)

#endif /* !__ASSEMBLER__ */

#if defined(CHIP_FAMILY_STM32F0)
#include "registers-stm32f0.h"
#elif defined(CHIP_FAMILY_STM32F3)
#include "registers-stm32f3.h"
#elif defined(CHIP_FAMILY_STM32F4)
#include "registers-stm32f4.h"
#elif defined(CHIP_FAMILY_STM32F7)
#include "registers-stm32f7.h"
#elif defined(CHIP_FAMILY_STM32H7)
#include "registers-stm32h7.h"
#elif defined(CHIP_FAMILY_STM32L)
#include "registers-stm32l.h"
#elif defined(CHIP_FAMILY_STM32L4)
#include "registers-stm32l4.h"
#else
#error "Unsupported chip family"
#endif

#endif /* __CROS_EC_REGISTERS_H */
