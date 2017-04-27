/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for LM3x processor
 */

#ifndef __CROS_EC_REGISTERS_H
#define __CROS_EC_REGISTERS_H

#include "common.h"

#define LM3_UART_CH0_BASE      0x4000c000
#define LM3_UART_CH1_BASE      0x4000d000
#define LM3_UART_CH_SEP        0x00001000
static inline int lm3_uart_addr(int ch, int offset)
{
	return offset + LM3_UART_CH0_BASE + LM3_UART_CH_SEP * ch;
}
#define LM3UARTREG(ch, offset) REG32(lm3_uart_addr(ch, offset))
#define LM3_UART_DR(ch)        LM3UARTREG(ch, 0x000)
#define LM3_UART_FR(ch)        LM3UARTREG(ch, 0x018)
#define LM3_UART_IBRD(ch)      LM3UARTREG(ch, 0x024)
#define LM3_UART_FBRD(ch)      LM3UARTREG(ch, 0x028)
#define LM3_UART_LCRH(ch)      LM3UARTREG(ch, 0x02c)
#define LM3_UART_CTL(ch)       LM3UARTREG(ch, 0x030)
#define LM3_UART_IFLS(ch)      LM3UARTREG(ch, 0x034)
#define LM3_UART_IM(ch)        LM3UARTREG(ch, 0x038)
#define LM3_UART_ICR(ch)       LM3UARTREG(ch, 0x044)
#define LM3_UART_DMACTL(ch)    LM3UARTREG(ch, 0x048)
#define LM3_UART_CC(ch)        LM3UARTREG(ch, 0xfc8)

#define LM3_SSI_BASE           0x40008000
#define LM3_SSI_CH_SEP         0x40001000
static inline int lm3_spi_addr(int ch, int offset)
{
	return offset + LM3_SSI_BASE + LM3_SSI_CH_SEP * ch;
}
#define LM3SSIREG(ch, offset)  REG32(lm3_spi_addr(ch, offset))
#define LM3_SSI_CR0(ch)        LM3SSIREG(ch, 0x000)
#define LM3_SSI_CR1(ch)        LM3SSIREG(ch, 0x004)
#define LM3_SSI_DR(ch)         LM3SSIREG(ch, 0x008)
#define LM3_SSI_SR(ch)         LM3SSIREG(ch, 0x00c)
#define LM3_SSI_SR_TFE         (1 << 0)  /* Transmit FIFO empty */
#define LM3_SSI_SR_TNF         (1 << 1)  /* Transmit FIFO not full */
#define LM3_SSI_SR_RNE         (1 << 2)  /* Receive FIFO not empty */
#define LM3_SSI_SR_RFF         (1 << 3)  /* Receive FIFO full */
#define LM3_SSI_SR_BSY         (1 << 4)  /* Busy */
#define LM3_SSI_CPSR(ch)       LM3SSIREG(ch, 0x010)
#define LM3_SSI_IM(ch)         LM3SSIREG(ch, 0x014)
#define LM3_SSI_RIS(ch)        LM3SSIREG(ch, 0x018)
#define LM3_SSI_MIS(ch)        LM3SSIREG(ch, 0x01c)
#define LM3_SSI_ICR(ch)        LM3SSIREG(ch, 0x020)
#define LM3_SSI_DMACTL(ch)     LM3SSIREG(ch, 0x024)
#define LM3_SSI_CC(ch)         LM3SSIREG(ch, 0xfc8)

#define LM3_ADC_ADCACTSS       REG32(0x40038000)
#define LM3_ADC_ADCRIS         REG32(0x40038004)
#define LM3_ADC_ADCIM          REG32(0x40038008)
#define LM3_ADC_ADCISC         REG32(0x4003800c)
#define LM3_ADC_ADCOSTAT       REG32(0x40038010)
#define LM3_ADC_ADCEMUX        REG32(0x40038014)
#define LM3_ADC_ADCUSTAT       REG32(0x40038018)
#define LM3_ADC_ADCSSPRI       REG32(0x40038020)
#define LM3_ADC_ADCSPC         REG32(0x40038024)
#define LM3_ADC_ADCPSSI        REG32(0x40038028)
#define LM3_ADC_ADCSAC         REG32(0x40038030)
#define LM3_ADC_ADCCTL         REG32(0x40038038)
#define LM3_ADC_ADCCC          REG32(0x40038fc8)
#define LM3_ADC_SS0_BASE       0x40038040
#define LM3_ADC_SS1_BASE       0x40038060
#define LM3_ADC_SS2_BASE       0x40038080
#define LM3_ADC_SS3_BASE       0x400380a0
#define LM3_ADC_SS_SEP         0x00000020

#define LM3_ADC_SS_BASE(n)             CONCAT3(LM3_ADC_SS, n, _BASE)
#define LM3_ADC_SS_REG(base, offset)   REG32((base) + (offset))

#define LM3_ADC_SSMUX(ss)      LM3_ADC_SS_REG(ss, 0x000)
#define LM3_ADC_SSCTL(ss)      LM3_ADC_SS_REG(ss, 0x004)
#define LM3_ADC_SSFIFO(ss)     LM3_ADC_SS_REG(ss, 0x008)
#define LM3_ADC_SSFSTAT(ss)    LM3_ADC_SS_REG(ss, 0x00c)
#define LM3_ADC_SSOP(ss)       LM3_ADC_SS_REG(ss, 0x010)
#define LM3_ADC_SSEMUX(ss)     LM3_ADC_SS_REG(ss, 0x018)

#define LM3_FLASH_FMA          REG32(0x400fd000)
#define LM3_FLASH_FMD          REG32(0x400fd004)
#define LM3_FLASH_FMC          REG32(0x400fd008)
#define LM3_FLASH_FCRIS        REG32(0x400fd00c)
#define LM3_FLASH_FCMISC       REG32(0x400fd014)
#define LM3_FLASH_FMC2         REG32(0x400fd020)
#define LM3_FLASH_FWBVAL       REG32(0x400fd030)
/* FWB size is 32 words = 128 bytes */
#define LM3_FLASH_FWB          ((volatile uint32_t*)0x400fd100)
#define LM3_FLASH_FSIZE        REG32(0x400fdfc0)
#define LM3_FLASH_FMPRE0       REG32(0x400fe200)
#define LM3_FLASH_FMPRE1       REG32(0x400fe204)
#define LM3_FLASH_FMPRE2       REG32(0x400fe208)
#define LM3_FLASH_FMPRE3       REG32(0x400fe20c)
#define LM3_FLASH_FMPPE        ((volatile uint32_t*)0x400fe400)
#define LM3_FLASH_FMPPE0       REG32(0x400fe400)
#define LM3_FLASH_FMPPE1       REG32(0x400fe404)
#define LM3_FLASH_FMPPE2       REG32(0x400fe408)
#define LM3_FLASH_FMPPE3       REG32(0x400fe40c)

#define LM3_SYSTEM_DID0        REG32(0x400fe000)
#define LM3_SYSTEM_DID1        REG32(0x400fe004)
#define LM3_SYSTEM_DC0         REG32(0x400fe008)
#define LM3_SYSTEM_DC1         REG32(0x400fe010)
#define LM3_SYSTEM_DC2         REG32(0x400fe014)
#define LM3_SYSTEM_DC3         REG32(0x400fe018)
#define LM3_SYSTEM_DC4         REG32(0x400fe01c)
#define LM3_SYSTEM_PBORCTL     REG32(0x400fe030)
#define LM3_SYSTEM_LDOPCTL     REG32(0x400fe034)
#define LM3_SYSTEM_SRCR0       REG32(0x400fe040)
#define LM3_SYSTEM_SRCR1       REG32(0x400fe044)
#define LM3_SYSTEM_SRCR2       REG32(0x400fe048)
#define LM3_SYSTEM_RIS         REG32(0x400fe050)
#define LM3_SYSTEM_RIS_PLLLRIS    (1 << 6)
#define LM3_SYSTEM_RIS_CLRIS      (1 << 5)
#define LM3_SYSTEM_RIS_IOFRIS     (1 << 4)
#define LM3_SYSTEM_RIS_MOFRIS     (1 << 3)
#define LM3_SYSTEM_RIS_LDORIS     (1 << 2)
#define LM3_SYSTEM_RIS_BORRIS     (1 << 1)
#define LM3_SYSTEM_RIS_PLLFRIS    (1 << 0)
#define LM3_SYSTEM_IMC         REG32(0x400fe054)
#define LM3_SYSTEM_MISC        REG32(0x400fe058)
#define LM3_SYSTEM_RESC        REG32(0x400fe05c)
#define LM3_SYSTEM_RCC         REG32(0x400fe060)
#define LM3_SYSTEM_RCC_ACG        (1 << 27)
#define LM3_SYSTEM_RCC_SYSDIV(x)  (((x) & 0xf) << 23)
#define LM3_SYSTEM_RCC_USESYSDIV  (1 << 22)
#define LM3_SYSTEM_RCC_PWRDN      (1 << 13)
#define LM3_SYSTEM_RCC_BYPASS     (1 << 11)
#define LM3_SYSTEM_RCC_XTAL(x)    (((x) & 0xf) << 6)
#define LM3_SYSTEM_RCC_OSCSRC(x)  (((x) & 0x3) << 4)
#define LM3_SYSTEM_RCC_IOSCDIS    (1 << 1)
#define LM3_SYSTEM_RCC_MOSCDIS    (1 << 0)
#define LM3_SYSTEM_PLLCFG      REG32(0x400fe064)
#define LM3_SYSTEM_RCGC0       REG32(0x400fe100)
#define LM3_SYSTEM_RCGC0_PWM      (1 << 20)
#define LM3_SYSTEM_RCGC0_ADC      (1 << 16)
#define LM3_SYSTEM_RCGC0_MAXADCSPD(n) (((n) & 3) << 8)
#define LM3_SYSTEM_RCGC0_WDT      (1 << 3)
#define LM3_SYSTEM_RCGC1       REG32(0x400fe104)
#define LM3_SYSTEM_RCGC1_COMP0    (1 << 24)
#define LM3_SYSTEM_RCGC1_TIMER2   (1 << 18)
#define LM3_SYSTEM_RCGC1_TIMER1   (1 << 17)
#define LM3_SYSTEM_RCGC1_TIMER0   (1 << 16)
#define LM3_SYSTEM_RCGC1_I2C0     (1 << 12)
#define LM3_SYSTEM_RCGC1_SSIO     (1 << 4)
#define LM3_SYSTEM_RCGC1_UART1    (1 << 1)
#define LM3_SYSTEM_RCGC1_UART0    (1 << 0)
#define LM3_SYSTEM_RCGC2       REG32(0x400fe108)
#define LM3_SYSTEM_RCGC2_GPIOE    (1 << 4)
#define LM3_SYSTEM_RCGC2_GPIOD    (1 << 3)
#define LM3_SYSTEM_RCGC2_GPIOC    (1 << 2)
#define LM3_SYSTEM_RCGC2_GPIOB    (1 << 1)
#define LM3_SYSTEM_RCGC2_GPIOA    (1 << 0)
#define LM3_SYSTEM_SCGC0       REG32(0x400fe110)
#define LM3_SYSTEM_SCGC1       REG32(0x400fe114)
#define LM3_SYSTEM_SCGC2       REG32(0x400fe118)
#define LM3_SYSTEM_DCGC0       REG32(0x400fe120)
#define LM3_SYSTEM_DCGC1       REG32(0x400fe124)
#define LM3_SYSTEM_DCGC2       REG32(0x400fe128)
#define LM3_SYSTEM_DSLPCLKCFG  REG32(0x400fe144)
#define LM3_SYSTEM_PIOSCCAL    REG32(0x400fe150)
#define LM3_SYSTEM_LDOARST     REG32(0x400fe160)

enum clock_gate_offsets {
/* RCGC0, SCSC0, DCGC0 */
CGC_OFFSET_PWM =    0,
CGC_OFFSET_ADC =    1,
CGC_OFFSET_WDT =    2,

/* RCGC1, SCSC1, DCGC1 */
CGC_OFFSET_COMP0 =  3,
CGC_OFFSET_TIMER2 = 4,
CGC_OFFSET_TIMER1 = 5,
CGC_OFFSET_TIMER0 = 6,
CGC_OFFSET_I2C0 =   7,
CGC_OFFSET_SSIO =   8,
CGC_OFFSET_UART1 =  9,
CGC_OFFSET_UART0 =  10,

/* RCGC2, SCSC2, DCGC2 */
CGC_OFFSET_GPIOA =  11,
CGC_OFFSET_GPIOB =  12,
CGC_OFFSET_GPIOC =  13,
CGC_OFFSET_GPIOD =  14,
CGC_OFFSET_GPIOE =  15
};

/* IRQ numbers */
#define LM3_IRQ_GPIOA            0
#define LM3_IRQ_GPIOB            1
#define LM3_IRQ_GPIOC            2
#define LM3_IRQ_GPIOD            3
#define LM3_IRQ_GPIOE            4
#define LM3_IRQ_UART0            5
#define LM3_IRQ_UART1            6
#define LM3_IRQ_SSI0             7
#define LM3_IRQ_I2C0             8
/* 9 reserved */
#define LM3_IRQ_PWM0             9
#define LM3_IRQ_PWM1            10
#define LM3_IRQ_PWM2            11
/* 13 reserved */
#define LM3_IRQ_ADC0_SS0        14
#define LM3_IRQ_ADC0_SS1        15
#define LM3_IRQ_ADC0_SS2        16
#define LM3_IRQ_ADC0_SS3        17
#define LM3_IRQ_WATCHDOG        18
#define LM3_IRQ_TIMER0A         19
#define LM3_IRQ_TIMER0B         20
#define LM3_IRQ_TIMER1A         21
#define LM3_IRQ_TIMER1B         22
#define LM3_IRQ_TIMER2A         23
#define LM3_IRQ_TIMER2B         24
#define LM3_IRQ_ACMP0           25
/* 26-27 reserved */
#define LM3_IRQ_SYSCTRL         28
#define LM3_IRQ_FLASHCTR        29

/* GPIO */
#define LM3_GPIO_PORTA_BASE         0x40004000
#define LM3_GPIO_PORTB_BASE         0x40005000
#define LM3_GPIO_PORTC_BASE         0x40006000
#define LM3_GPIO_PORTD_BASE         0x40007000
#define LM3_GPIO_PORTE_BASE         0x40024000
/*
 * Ports for passing to LM3GPIOREG(); abstracted from base addresses above so
 * that we can switch to/from AHB.
 */
#define LM3_GPIO_A LM3_GPIO_PORTA_BASE
#define LM3_GPIO_B LM3_GPIO_PORTB_BASE
#define LM3_GPIO_C LM3_GPIO_PORTC_BASE
#define LM3_GPIO_D LM3_GPIO_PORTD_BASE
#define LM3_GPIO_E LM3_GPIO_PORTE_BASE
#define LM3GPIOREG(port, offset)      REG32((port) + (offset))
#define LM3_GPIO_DATA(port, mask)     LM3GPIOREG(port, ((mask) << 2))
#define LM3_GPIO_DIR(port)            LM3GPIOREG(port, 0x400)
#define LM3_GPIO_IS(port)             LM3GPIOREG(port, 0x404)
#define LM3_GPIO_IBE(port)            LM3GPIOREG(port, 0x408)
#define LM3_GPIO_IEV(port)            LM3GPIOREG(port, 0x40c)
#define LM3_GPIO_IM(port)             LM3GPIOREG(port, 0x410)
#define LM3_GPIO_RIS(port)            LM3GPIOREG(port, 0x414)
#define LM3_GPIO_MIS(port)            LM3GPIOREG(port, 0x418)
#define LM3_GPIO_ICR(port)            LM3GPIOREG(port, 0x41c)
#define LM3_GPIO_AFSEL(port)          LM3GPIOREG(port, 0x420)
#define LM3_GPIO_DR2R(port)           LM3GPIOREG(port, 0x500)
#define LM3_GPIO_DR4R(port)           LM3GPIOREG(port, 0x504)
#define LM3_GPIO_DR8R(port)           LM3GPIOREG(port, 0x508)
#define LM3_GPIO_ODR(port)            LM3GPIOREG(port, 0x50c)
#define LM3_GPIO_PUR(port)            LM3GPIOREG(port, 0x510)
#define LM3_GPIO_PDR(port)            LM3GPIOREG(port, 0x514)
#define LM3_GPIO_SLR(port)            LM3GPIOREG(port, 0x518)
#define LM3_GPIO_DEN(port)            LM3GPIOREG(port, 0x51c)
#define LM3_GPIO_LOCK(port)           LM3GPIOREG(port, 0x520)
#define LM3_GPIO_CR(port)             LM3GPIOREG(port, 0x524)
#define LM3_GPIO_AMSEL(port)          LM3GPIOREG(port, 0x528)
#define LM3_GPIO_PCTL(port)           LM3GPIOREG(port, 0x52c)

/* Chip-independent aliases for port base addresses */
#define GPIO_A LM3_GPIO_A
#define GPIO_B LM3_GPIO_B
#define GPIO_C LM3_GPIO_C
#define GPIO_D LM3_GPIO_D
#define GPIO_E LM3_GPIO_E

#define DUMMY_GPIO_BANK 0

/* Value to write to LM3_GPIO_LOCK to unlock writes */
#define LM3_GPIO_LOCK_UNLOCK          0x4c4f434b

/* I2C */
#define LM3_I2C0_BASE                 0x40020000

#define LM3_I2C_BASE(n)               CONCAT3(LM3_I2C, n, _BASE)
#define LM3_I2C_REG(base, offset)     REG32((base) + (offset))

#define LM3_I2C_MSA(port)             LM3_I2C_REG(port, 0x000)
#define LM3_I2C_MCS(port)             LM3_I2C_REG(port, 0x004)
#define LM3_I2C_MCS_READ_BUSBSY       (1 << 6)
#define LM3_I2C_MCS_READ_IDLE         (1 << 5)
#define LM3_I2C_MCS_READ_ARBLST       (1 << 4)
#define LM3_I2C_MCS_READ_DATACK       (1 << 3)
#define LM3_I2C_MCS_READ_ADRACK       (1 << 2)
#define LM3_I2C_MCS_READ_ERROR        (1 << 1)
#define LM3_I2C_MCS_READ_BUSY         (1 << 0)
#define LM3_I2C_MCS_WRITE_ACK         (1 << 3)
#define LM3_I2C_MCS_WRITE_STOP        (1 << 2)
#define LM3_I2C_MCS_WRITE_START       (1 << 1)
#define LM3_I2C_MCS_WRITE_RUN         (1 << 0)
#define LM3_I2C_MDR(port)             LM3_I2C_REG(port, 0x008)
#define LM3_I2C_MDR_DATA(n)           ((n) & 0xff)
#define LM3_I2C_MTPR(port)            LM3_I2C_REG(port, 0x00c)
#define LM3_I2C_MTPR_TPR(n)           ((n) & 0x7f)
#define LM3_I2C_MIMR(port)            LM3_I2C_REG(port, 0x010)
#define LM3_I2C_MIMR_IM               (1 << 0)
#define LM3_I2C_MRIS(base)            LM3_I2C_REG(port, 0x014)
#define LM3_I2C_MRIS_RIS              (1 << 0)
#define LM3_I2C_MMIS(port)            LM3_I2C_REG(port, 0x018)
#define LM3_I2C_MMIS_MIS              (1 << 0)
#define LM3_I2C_MICR(port)            LM3_I2C_REG(port, 0x01c)
#define LM3_I2C_MICR_IC               (1 << 0)
#define LM3_I2C_MCR(port)             LM3_I2C_REG(port, 0x020)
#define LM3_I2C_MCR_SFE               (1 << 5)
#define LM3_I2C_MCR_MFE               (1 << 4)
#define LM3_I2C_MCR_LPBK              (1 << 0)

/* Timers */
/* Timers 0-2 are 16/32 bit */
#define LM3_TIMER0_BASE               0x40030000
#define LM3_TIMER1_BASE               0x40031000
#define LM3_TIMER2_BASE               0x40032000

#define LM3_TIMER_BASE(n)             CONCAT3(LM3_TIMER, n, _BASE)
#define LM3_TIMER_REG(base, offset)   REG32((base) + (offset))

#define LM3_TIMER_CFG(base)           LM3_TIMER_REG(base, 0x00)
#define LM3_TIMER_CFG_GPTMCFG(n)      ((n) & 7)
#define LM3_TIMER_TAMR(base)          LM3_TIMER_REG(base, 0x04)
#define LM3_TIMER_TAMR_TAAMS          (1 << 3)
#define LM3_TIMER_TAMR_TACMR          (1 << 2)
#define LM3_TIMER_TAMR_TAMR(n)        ((n) & 3)
#define LM3_TIMER_TBMR(base)          LM3_TIMER_REG(base, 0x08)
#define LM3_TIMER_TBMR_TBAMS          (1 << 3)
#define LM3_TIMER_TBMR_TBCMR          (1 << 2)
#define LM3_TIMER_TBMR_TBMR(n)        ((n) & 3)
#define LM3_TIMER_CTL(base)           LM3_TIMER_REG(base, 0x0c)
#define LM3_TIMER_CTL_TBPWML          (1 << 14)
#define LM3_TIMER_CTL_TBOTE           (1 << 13)
#define LM3_TIMER_CTL_TBEVENT(n)      (((n) & 3) << 10)
#define LM3_TIMER_CTL_TBSTALL         (1 << 9)
#define LM3_TIMER_CTL_TBEN            (1 << 8)
#define LM3_TIMER_CTL_TAPWML          (1 << 6)
#define LM3_TIMER_CTL_TAOTE           (1 << 5)
#define LM3_TIMER_CTL_RTCEN           (1 << 4)
#define LM3_TIMER_CTL_TAEVENT(n)      (((n) & 3) << 2)
#define LM3_TIMER_CTL_TASTALL         (1 << 1)
#define LM3_TIMER_CTL_TAEN            (1 << 0)
#define LM3_TIMER_IMR(base)           LM3_TIMER_REG(base, 0x18)
#define LM3_TIMER_IMR_CBEIM           (1 << 10)
#define LM3_TIMER_IMR_CBMIM           (1 << 9)
#define LM3_TIMER_IMR_TBTOIM          (1 << 8)
#define LM3_TIMER_IMR_RTCIM           (1 << 3)
#define LM3_TIMER_IMR_CAEIM           (1 << 2)
#define LM3_TIMER_IMR_CAMIM           (1 << 1)
#define LM3_TIMER_IMR_TATOIM          (1 << 0)
#define LM3_TIMER_RIS(base)           LM3_TIMER_REG(base, 0x1c)
#define LM3_TIMER_RIS_CBERIS          (1 << 10)
#define LM3_TIMER_RIS_CBMRIS          (1 << 9)
#define LM3_TIMER_RIS_TBTORIS         (1 << 8)
#define LM3_TIMER_RIS_RTCRIS          (1 << 3)
#define LM3_TIMER_RIS_CAERIS          (1 << 2)
#define LM3_TIMER_RIS_CAMRIS          (1 << 1)
#define LM3_TIMER_RIS_TATORIS         (1 << 0)
#define LM3_TIMER_MIS(base)           LM3_TIMER_REG(base, 0x20)
#define LM3_TIMER_MIS_CBEMIS          (1 << 10)
#define LM3_TIMER_MIS_CBMMIS          (1 << 9)
#define LM3_TIMER_MIS_TBTOMIS         (1 << 8)
#define LM3_TIMER_MIS_RTCMIS          (1 << 3)
#define LM3_TIMER_MIS_CAEMIS          (1 << 2)
#define LM3_TIMER_MIS_CAMMIS          (1 << 1)
#define LM3_TIMER_MIS_TATOMIS         (1 << 0)
#define LM3_TIMER_ICR(base)           LM3_TIMER_REG(base, 0x24)
#define LM3_TIMER_ICR_CBECINT         (1 << 10)
#define LM3_TIMER_ICR_CBMCINT         (1 << 9)
#define LM3_TIMER_ICR_TBTOCINT        (1 << 8)
#define LM3_TIMER_ICR_RTCCINT         (1 << 3)
#define LM3_TIMER_ICR_CAECINT         (1 << 2)
#define LM3_TIMER_ICR_CAMCINT         (1 << 1)
#define LM3_TIMER_ICR_TATOCINT        (1 << 0)
#define LM3_TIMER_TAILR(base)         LM3_TIMER_REG(base, 0x28)
#define LM3_TIMER_TAILR_TAILRH(n)     (((n) & 0xffff) << 16)
#define LM3_TIMER_TAILR_TAILRL(n)     (((n) & 0xffff) << 0)
#define LM3_TIMER_TBILR(base)         LM3_TIMER_REG(base, 0x2c)
#define LM3_TIMER_TBILR_TBILRL(n)     (((n) & 0xffff) << 0)
#define LM3_TIMER_TAMATCHR(base)      LM3_TIMER_REG(base, 0x30)
#define LM3_TIMER_TAMATCHR_TAMRH(n)   (((n) & 0xffff) << 16)
#define LM3_TIMER_TAMATCHR_TAMRL(n)   (((n) & 0xffff) << 0)
#define LM3_TIMER_TBMATCHR(base)      LM3_TIMER_REG(base, 0x34)
#define LM3_TIMER_TBMATCHR_TBMRL(n)   (((n) & 0xffff) << 0)
#define LM3_TIMER_TAPR(base)          LM3_TIMER_REG(base, 0x38)
#define LM3_TIMER_TAPR_TAPSR(n)       (((n) & 0xff) << 0)
#define LM3_TIMER_TBPR(base)          LM3_TIMER_REG(base, 0x3c)
#define LM3_TIMER_TBPR_TBPSR(n)       (((n) & 0xff) << 0)
#define LM3_TIMER_TAPMR(base)         LM3_TIMER_REG(base, 0x40)
#define LM3_TIMER_TAPMR_TAPSR(n)      (((n) & 0xff) << 0)
#define LM3_TIMER_TBPMR(base)         LM3_TIMER_REG(base, 0x44)
#define LM3_TIMER_TBPMR_TBPSMR(n)     (((n) & 0xff) << 0)
#define LM3_TIMER_TAR(base)           LM3_TIMER_REG(base, 0x48)
#define LM3_TIMER_TAR_TARH(n)         (((n) & 0xffff) << 16)
#define LM3_TIMER_TAR_TARL(n)         (((n) & 0xffff) << 0)
#define LM3_TIMER_TBR(base)           LM3_TIMER_REG(base, 0x4c)
#define LM3_TIMER_TBR_TBRL(n)         (((n) & 0xffff) << 0)


#define LM3_SYSTICK_CTRL                REG32(0xe000e010)
#define LM3_SYSTICK_RELOAD              REG32(0xe000e014)
#define LM3_SYSTICK_CURRENT             REG32(0xe000e018)

/* Watchdogs */
#define LM3_WATCHDOG0_BASE              0x40000000
static inline int lm4_watchdog_addr(int num, int offset)
{
	return offset + LM3_WATCHDOG0_BASE;
}
#define LM3WDTREG(num, offset)		REG32(lm3_watchdog_addr(num, offset))
#define LM3_WATCHDOG_LOAD(n)            LM3WDTREG(n, 0x000)
#define LM3_WATCHDOG_VALUE(n)           LM3WDTREG(n, 0x004)
#define LM3_WATCHDOG_CTL(n)             LM3WDTREG(n, 0x008)
#define LM3_WATCHDOG_ICR(n)             LM3WDTREG(n, 0x00c)
#define LM3_WATCHDOG_RIS(n)             LM3WDTREG(n, 0x010)
#define LM3_WATCHDOG_TEST(n)            LM3WDTREG(n, 0x418)
#define LM3_WATCHDOG_LOCK(n)            LM3WDTREG(n, 0xc00)

#endif /* __CROS_EC_REGISTERS_H */
