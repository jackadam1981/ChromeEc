/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for MEC17xx processor
 */

#ifndef __CROS_EC_REGISTERS_H
#define __CROS_EC_REGISTERS_H

#include "common.h"

#define DEBUG_ESPI	1

/* Helper function for RAM address aliasing */
#define MEC17XX_RAM_ALIAS(x)   \
	((x) >= 0x118000 ? (x) - 0x118000 + 0x20000000 : (x))

/* EC Chip Configuration */
#define MEC17XX_CHIP_BASE      0x400fff00
#define MEC17XX_CHIP_DEV_ID    REG8(MEC17XX_CHIP_BASE + 0x20)
#define MEC17XX_CHIP_DEV_REV   REG8(MEC17XX_CHIP_BASE + 0x21)


/* Power/Clocks/Resets */
#define MEC17XX_PCR_BASE       0x40080100

#define MEC17XX_PCR_SYS_SLP_CTL     REG32(MEC17XX_PCR_BASE + 0x00)
#define MEC17XX_PCR_PROC_CLK_CTL    REG32(MEC17XX_PCR_BASE + 0x04)
#define MEC17XX_PCR_SLOW_CLK_CTL    REG32(MEC17XX_PCR_BASE + 0x08)
#define MEC17XX_PCR_CHIP_OSC_ID     REG32(MEC17XX_PCR_BASE + 0x0C)
/* TODO #define MEC17XX_PCR_CHIP_PWR_RST    REG32(MEC17XX_PCR_BASE + 0x10) */
#define MEC17XX_PCR_PWR_RST_STS     REG32(MEC17XX_PCR_BASE + 0x10)
#define MEC17XX_PCR_PWR_RST_CTL     REG32(MEC17XX_PCR_BASE + 0x14)
#define MEC17XX_PCR_SYS_RST         REG32(MEC17XX_PCR_BASE + 0x18)
#define MEC17XX_PCR_SLP_EN0         REG32(MEC17XX_PCR_BASE + 0x30)
#define MEC17XX_PCR_SLP_EN1         REG32(MEC17XX_PCR_BASE + 0x34)
#define MEC17XX_PCR_SLP_EN2         REG32(MEC17XX_PCR_BASE + 0x38)
#define MEC17XX_PCR_SLP_EN3         REG32(MEC17XX_PCR_BASE + 0x3C)
#define MEC17XX_PCR_SLP_EN4         REG32(MEC17XX_PCR_BASE + 0x40)
#define MEC17XX_PCR_CLK_REQ0        REG32(MEC17XX_PCR_BASE + 0x50)
#define MEC17XX_PCR_CLK_REQ1        REG32(MEC17XX_PCR_BASE + 0x54)
#define MEC17XX_PCR_CLK_REQ2        REG32(MEC17XX_PCR_BASE + 0x58)
#define MEC17XX_PCR_CLK_REQ3        REG32(MEC17XX_PCR_BASE + 0x5C)
#define MEC17XX_PCR_CLK_REQ4        REG32(MEC17XX_PCR_BASE + 0x60)
#define MEC17XX_PCR_RST_EN0         REG32(MEC17XX_PCR_BASE + 0x70)
#define MEC17XX_PCR_RST_EN1         REG32(MEC17XX_PCR_BASE + 0x74)
#define MEC17XX_PCR_RST_EN2         REG32(MEC17XX_PCR_BASE + 0x78)
#define MEC17XX_PCR_RST_EN3         REG32(MEC17XX_PCR_BASE + 0x7C)
#define MEC17XX_PCR_RST_EN4         REG32(MEC17XX_PCR_BASE + 0x80)

#define MEC17XX_PCR_SLP_EN(x)	REG32(MEC17XX_PCR_BASE + 0x30 + ((x)<<2))
#define MEC17XX_PCR_CLK_REQ(x)	REG32(MEC17XX_PCR_BASE + 0x30 + ((x)<<2))
#define MEC17XX_PCR_RST_EN(x)	REG32(MEC17XX_PCR_BASE + 0x30 + ((x)<<2))

/* Set/clear PCR sleep enable bit for single device
 * x = register 0<=x<=4
 * d = device bit position
 */
#define MEC17XX_PCR_SLP_EN_DEV(x,d) MEC17XX_PCR_SLP_EN(x) |= (1ul << (d))
#define MEC17XX_PCR_SLP_DIS_DEV(x,d) MEC17XX_PCR_SLP_EN(x) &= ~(1ul << (d))


/* Slow Clock Control Mask */
#define MEC17xx_PCR_SLOW_CLK_CTL_MASK	0x03FFul

/* Command all blocks to sleep */
#define MEC17XX_PCR_SLP_EN0_SLEEP   0x00000007

/* Command all blocks to sleep */
#define MEC17XX_PCR_SLP_EN1_SLEEP   0xeff00ff7
#define MEC17XX_PCR_SLP_EN1_PWM8    (1 << 27)
#define MEC17XX_PCR_SLP_EN1_PWM7    (1 << 26)
#define MEC17XX_PCR_SLP_EN1_PWM6    (1 << 25)
#define MEC17XX_PCR_SLP_EN1_PWM5    (1 << 24)
#define MEC17XX_PCR_SLP_EN1_PWM4    (1 << 23)
#define MEC17XX_PCR_SLP_EN1_PWM3    (1 << 22)
#define MEC17XX_PCR_SLP_EN1_PWM2    (1 << 21)
#define MEC17XX_PCR_SLP_EN1_PWM1    (1 << 20)
#define MEC17XX_PCR_SLP_EN1_PWM0    (1 << 4)

/* Command all blocks to sleep */
#define MEC17XX_PCR_SLP_EN2_SLEEP	0x0007f007

/* Command all blocks to sleep */
#define MEC17XX_PCR_SLP_EN3_SLEEP	0xfffffee8
#define MEC17XX_PCR_SLP_EN3_PWM9    (1ul << 31)

/* Command all blocks to sleep */
#define MEC17XX_PCR_SLP_EN4_SLEEP	0x0000ffff
#define MEC17XX_PCR_SLP_EN4_PWM10   (1ul << 0)
#define MEC17XX_PCR_SLP_EN4_PWM11   (1ul << 1)

/* Allow all blocks to request clocks */
#define MEC17XX_PCR_SLP_EN0_WAKE    (~0x00000007)
#define MEC17XX_PCR_SLP_EN1_WAKE    (~0xeff00ff7)
#define MEC17XX_PCR_SLP_EN2_WAKE	(~0x0007f007)
#define MEC17XX_PCR_SLP_EN3_WAKE	(~0xfffffee8)
#define MEC17XX_PCR_SLP_EN4_WAKE	(~0x0000ffff)


/* Bit definitions for MEC17XX_PCR_SLP_EN1/CLK_REQ1/RST_EN1 */

/* Bit definitions for MEC17XX_PCR_SLP_EN2/CLK_REQ2/RST_EN2 */

/* Bit definitions for MEC17XX_PCR_SLP_EN3/CLK_REQ3/RST_EN3 */
#define MEC17XX_PCR_SLP_EN1_PKE		(1ul << 26)
#define MEC17XX_PCR_SLP_EN1_NDRNG	(1ul << 27)
#define MEC17XX_PCR_SLP_EN1_AES_SHA	(1ul << 28)
#define MEC17XX_PCR_SLP_EN1_ALL_CRYPTO	(0x07ul << 26)

/* Bit definitions for MEC17XX_PCR_SLP_EN4/CLK_REQ4/RST_EN4 */


/* Bit defines for MEC17XX_PCR_PWR_RST_STS */
#define MEC17XX_PWR_RST_STS_VTR     (1 << 6)
#define MEC17XX_PWR_RST_STS_VBAT    (1 << 5)

/* Bit defines for MEC17XX_PCR_PWR_RST_CTL */
#define MEC17XX_PCR_PWR_HOST_RST_SEL_BITPOS	8
#define MEC17XX_PCR_PWR_HOST_RST_LRESET		1
#define MEC17XX_PCR_PWR_HOST_RST_ESPI_PLTRST	0


/* Bit defines for MEC17XX_PCR_SYS_RST */
#define MEC17XX_PCR_SYS_SOFT_RESET  (1 << 8)


/* TFDP */
#define MEC17XX_TFDP_BASE	0x40008c00
#define MEC17XX_TFDP_DATA	REG8(MEC17XX_TFDP_BASE + 0x00)
#define MEC17XX_TFDP_CTRL	REG8(MEC17XX_TFDP_BASE + 0x04)


/* EC Subsystem */
#define MEC17XX_EC_BASE			0x4000fc00
#define MEC17XX_EC_AHB_ERR		REG32(MEC17XX_EC_BASE + 0x04)
#define MEC17XX_EC_AHB_ERR_EN		REG32(MEC17XX_EC_BASE + 0x14)
#define MEC17XX_EC_INT_CTRL		REG32(MEC17XX_EC_BASE + 0x18)
#define MEC17XX_EC_TRACE_EN		REG32(MEC17XX_EC_BASE + 0x1c)
#define MEC17XX_EC_JTAG_EN		REG32(MEC17XX_EC_BASE + 0x20)
#define MEC17XX_EC_WDT_CNT		REG32(MEC17XX_EC_BASE + 0x28)
#define MEC17XX_EC_AES_SHA_SWAP_CTRL	REG8(MEC17XX_EC_BASE + 0x2c)
#define MEC17XX_EC_CRYPTO_SRESET	REG8(MEC17XX_EC_BASE + 0x5c)
#define MEC17XX_EC_GPIO_BANK_PWR	REG8(MEC17XX_EC_BASE + 0x64)

/* MEC17XX_EC_CRYPTO_SRESET bit definitions */
#define MEC17XX_CRYPTO_NDRNG_SRST		0x01
#define MEC17XX_CRYPTO_PKE_SRST			0x02
#define MEC17XX_CRYPTO_AES_SHA_SRST		0x04
#define MEC17XX_CRYPTO_ALL_SRST			0x07

/* MEC17XX_GPIO_BANK_PWR bit definitions */
#define MEC17XX_EC_GPIO_BANK_PWR_VTR1_18	(0x01)
#define MEC17XX_EC_GPIO_BANK_PWR_VTR2_18	(0x02)
#define MEC17XX_EC_GPIO_BANK_PWR_VTR3_18	(0x04)


/* AHB ERR Enable */
#define MEC17XX_EC_AHB_ERROR_ENABLE	0
#define MEC17XX_EC_AHB_ERROR_DISABLE	1


/* Interrupt aggregator */
#define MEC17XX_INT_BASE       0x4000e000
/* #define MEC17XX_INTx_BASE(x)   (MEC17XX_INT_BASE + (((x) - 8) * 0x14)) */
#define MEC17XX_INTx_BASE(x)   (MEC17XX_INT_BASE + ((x)<<4) + ((x)<<2) - 160)
#define MEC17XX_INT_SOURCE(x)  REG32(MEC17XX_INTx_BASE(x) + 0x0)
#define MEC17XX_INT_ENABLE(x)  REG32(MEC17XX_INTx_BASE(x) + 0x4)
#define MEC17XX_INT_RESULT(x)  REG32(MEC17XX_INTx_BASE(x) + 0x8)
#define MEC17XX_INT_DISABLE(x) REG32(MEC17XX_INTx_BASE(x) + 0xc)
#define MEC17XX_INT_BLK_EN     REG32(MEC17XX_INT_BASE + 0x200)
#define MEC17XX_INT_BLK_DIS    REG32(MEC17XX_INT_BASE + 0x204)
#define MEC17XX_INT_BLK_IRQ    REG32(MEC17XX_INT_BASE + 0x208)

/* Bits for INT=13(GIRQ13) registers */
/*    SMBus[0:3] = bits[0:3] */
#define MEC17XX_INT13_SMB(x)		(1ul << (x))


/* Bits for INT=14(GIRQ14) registers */
/* DMA channels 0-13 */
#define MEC17XX_INT14_DMA(x)		(1ul << (x))


/* Bits for INT=15(GIRQ15) registers */
/*  UART[0:1] = bits[0:1] */
#define MEC17XX_INT15_UART(x)		(1ul << ((x) & 0x01))
/*  EMI[0:2] = bits[2:4] */
#define MEC17XX_INT15_EMI(x)		(1ul << (2 + (x)))
/*  ACPI_EC[0:4] IBF = bits[5,7,9,11,13]
 *  ACPI_EC[0:4] OBE = bits[6,8,10,12,14] */
#define MEC17XX_INT15_ACPI_EC_IBF(x)	(1ul << (5 + ((x) << 1)))
#define MEC17XX_INT15_ACPI_EC_OBE(x)	(1ul << (6 + ((x) << 1)))
#define MEC17XX_INT15_ACPI_PM1_CTL	(1ul << 15)
#define MEC17XX_INT15_ACPI_PM1_EN	(1ul << 16)
#define MEC17XX_INT15_ACPI_PM1_STS	(1ul << 17)
#define MEC17XX_INT15_8042_OBE		(1ul << 18)
#define MEC17XX_INT15_8042_IBF		(1ul << 19)
#define MEC17XX_INT15_MAILBOX		(1ul << 20)
#define MEC17XX_INT15_P80(x)		(1ul << (22 + (x) & 0x01)))


/* Bits for INT=16(GIRQ16) registers */
#define MEC17XX_INT16_PKE_ERR		(1ul << 0)
#define MEC17XX_INT16_PKE_DONE		(1ul << 1)
#define MEC17XX_INT16_RNG_DONE		(1ul << 2)
#define MEC17XX_INT16_AES_DONE		(1ul << 3)
#define MEC17XX_INT16_HASH_DONE		(1ul << 4)


/* Bits for INT=17(GIRQ17) registers */
#define MEC17XX_INT17_PECI		(1ul << 0)
/*    TACH[0:2] = bits[1:3] */
#define MEC17XX_INT17_TACH(x)		(1ul << (1 + (x)))
/*    RPMFAN_FAIL[0:1] = bits[4,6] */
#define MEC17XX_INT17_RPMFAN_FAIL(x)	(1ul << (4 + ((x) << 1)))
/*    RPMFAN_STALL[0:1] = bits[5,7] */
#define MEC17XX_INT17_RPMFAN_STALL(x)	(1ul << (5 + ((x) << 1)))
#define MEC17XX_INT17_ADC_SINGLE	(1ul << 8)
#define MEC17XX_INT17_ADC_REPEAT	(1ul << 9)
/*    RCIC[0:2] = bits[10:12] */
#define MEC17XX_INT17_RCID(x)		(1ul << (10 + (x)))
#define MEC17XX_INT17_LED_WDT(x)	(1ul << (13 + (x)))


/* Bits for INT=18(GIRQ18) registers */
#define MEC17XX_INT18_LPC		(1ul << 0)
#define MEC17XX_INT18_QMSPI0		(1ul << 1)
/*    SPI_TX[0:1] = bits[2,4] */
#define MEC17XX_INT18_SPI_TX(x)		(1ul << (2 + ((x) << 1)))
/*    SPI_RX[0:1] = bits[3,5] */
#define MEC17XX_INT18_SPI_RX(x)		(1ul << (3 + ((x) << 1)))


/* Bits for INT=19(GIRQ19) registers */
#define MEC17XX_INT19_ESPI_PC		(1ul << 0)
#define MEC17XX_INT19_ESPI_BM1		(1ul << 1)
#define MEC17XX_INT19_ESPI_BM2		(1ul << 2)
#define MEC17XX_INT19_ESPI_LTR		(1ul << 3)
#define MEC17XX_INT19_ESPI_OOB_TX	(1ul << 4)
#define MEC17XX_INT19_ESPI_OOB_RX	(1ul << 5)
#define MEC17XX_INT19_ESPI_FC		(1ul << 6)
#define MEC17XX_INT19_ESPI_RESET	(1ul << 7)
#define MEC17XX_INT19_ESPI_VW_EN	(1ul << 8)


/* Bits for INT=21(GIRQ21) registers */
#define MEC17XX_INT21_RTOS_TMR		(1ul << 0)
/*    HibernationTimer[0:1] = bits[1:2] */
#define MEC17XX_INT21_HIB_TMR(x)	(1ul << (1 + (x)))
#define MEC17XX_INT21_WEEK_ALARM	(1ul << 3)
#define MEC17XX_INT21_WEEK_SUB		(1ul << 4)
#define MEC17XX_INT21_WEEK_1SEC		(1ul << 5)
#define MEC17XX_INT21_WEEK_1SEC_SUB	(1ul << 6)
#define MEC17XX_INT21_WEEK_PWR_PRES	(1ul << 7)
#define MEC17XX_INT21_RTC		(1ul << 8)
#define MEC17XX_INT21_RTC_ALARM		(1ul << 9)
#define MEC17XX_INT21_VCI_OVRD		(1ul << 10)
/*    VCI_IN[0:6] = bits[11:17] */
#define MEC17XX_INT21_VCI_IN(x)		(1ul << (11 + (x)))
/*    PS2 Port Wake[0:4] =[0A,0B,1A,1B,2] = bits[18:22] */
#define MEC17XX_INT21_PS2_WAKE(x)	(1ul << (18 + (x)))
#define MEC17XX_INT21_KEYSCAN		(1ul << 25)


/* Bits for INT=23(GIRQ23) registers */
/*    16-bit Basic Timers[0:3] = bits[0:3] */
#define MEC17XX_INT23_BASIC_TMR16(x)	(1ul << (x))
/*    32-bit Basic Timers[0:1] = bits[4:5] */
#define MEC17XX_INT23_BASIC_TMR32(x)	(1ul << (4 + (x)))
/*    16-bit Counter-Timer[0:3] = bits[6:9] */
#define MEC17XX_INT23_CNT(x)		(1ul << (6 + (x)))
#define MEC17XX_INT23_CCT_TMR		(1ul << 10)
/*    CCT Capture events[0:5] = bits[11:16] */
#define MEC17XX_INT23_CCT_CAP(x)	(1ul << (11 + (x)))
/*    CCT Compare events[0:1] = bits[17:18] */
#define MEC17XX_INT23_CCT_CMP(x)	(1ul << (17 + (x)))


/* Bits for INT=24(GIRQ24) registers */
/*    Master-to-Slave v=[0:6], Source=[0:3] */
#define MEC17XX_INT24_MSVW_SRC(v,s)	(1ul << ((4 * (v)) + (s)))

/* Bits for INT25(GIRQ25) registers */
/*    Master-to-Slave v=[7:10], Source=[0:3] */
#define MEC17XX_INT25_MSVW_SRC(v,s)	(1ul << ((4 * ((v)-7)) + (s)))

/* End MEC17XX_INTxy bit definitions */

/* UART Peripheral */
#define MEC17XX_UART_CONFIG_BASE(x)     (0x400f2700 + ((x) * 0x400))
#define MEC17XX_UART_RUNTIME_BASE(x)    (0x400f2400 + ((x) * 0x400))

#define MEC17XX_UART_ACT(x)     REG8(MEC17XX_UART_CONFIG_BASE(x) + 0x30)
#define MEC17XX_UART_CFG(x)     REG8(MEC17XX_UART_CONFIG_BASE(x) + 0xf0)

/* DLAB=0 */
#define MEC17XX_UART_RB(x) /*R*/    REG8(MEC17XX_UART_RUNTIME_BASE(x) + 0x0)
#define MEC17XX_UART_TB(x) /*W*/    REG8(MEC17XX_UART_RUNTIME_BASE(x) + 0x0)
#define MEC17XX_UART_IER(x)         REG8(MEC17XX_UART_RUNTIME_BASE(x) + 0x1)
/* DLAB=1 */
#define MEC17XX_UART_PBRG0(x)       REG8(MEC17XX_UART_RUNTIME_BASE(x) + 0x0)
#define MEC17XX_UART_PBRG1(x)       REG8(MEC17XX_UART_RUNTIME_BASE(x) + 0x1)

#define MEC17XX_UART_FCR(x) /*W*/   REG8(MEC17XX_UART_RUNTIME_BASE(x) + 0x2)
#define MEC17XX_UART_IIR(x) /*R*/   REG8(MEC17XX_UART_RUNTIME_BASE(x) + 0x2)
#define MEC17XX_UART_LCR(x)         REG8(MEC17XX_UART_RUNTIME_BASE(x) + 0x3)
#define MEC17XX_UART_MCR(x)         REG8(MEC17XX_UART_RUNTIME_BASE(x) + 0x4)
#define MEC17XX_UART_LSR(x)         REG8(MEC17XX_UART_RUNTIME_BASE(x) + 0x5)
#define MEC17XX_UART_MSR(x)         REG8(MEC17XX_UART_RUNTIME_BASE(x) + 0x6)
#define MEC17XX_UART_SCR(x)         REG8(MEC17XX_UART_RUNTIME_BASE(x) + 0x7)
/*
 * UART[0:1] connected to GIRQ15 bits[0:1]
 */
#define MEC17XX_UART_GIRQ		15
#define MEC17XX_UART_GIRQ_BIT(x)	(1ul << (x))

/* Bit defines for MEC17XX_UARTx_LSR */
#define MEC17XX_LSR_TX_EMPTY     (1 << 5)


/* GPIO */
#define MEC17XX_GPIO_BASE      0x40081000

/* TODO is this used? If yes then try to implement without divides
static inline uintptr_t gpio_port_base(int port_id)
{
	int oct = (port_id / 10) * 8 + port_id % 10;
	return MEC17XX_GPIO_BASE + (oct * 0x20);
}
*/
/* TODO MEC17xx each Port contains 32 GPIO's.
 * GPIO Control 1 registers are 4-byte registers starting at MEC17XX_GPIO_BASE
 *
 * index = octal GPIO number from MEC17xx specification.
 * port/bank = index >> 5
 * id = index & 0x1F
 *
 * The port/bank, id pair may also be used to access GPIO's via
 * parallel I/O registers if GPIO control is configured for parallel I/O.
 *
 * From ec/chip/mec1701/config_chip.h
 * #define GPIO_PIN(index) ((index) >> 5), ((index) & 0x1F)
 *
 * GPIO Control 1 Address = 0x40081000 + (((bank << 5) + id) << 2)
 *
 *
 * Example: GPIO043, Control 1 register address = 0x4008108c
 * port/bank = 0x23 >> 5 = 1
 * id        = 0x23 & 0x1F = 0x03
 * Control 1 Address = 0x40081000 + (((1 << 5) + 0x03) << 2) = 0x4008108c
 *
 * Example: GPIO235, Control 1 register address = 0x40081274
 * port/bank = 0x9d >> 5   = 4
 * id        = 0x9d & 0x1f = 0x1d
 * Control 1 Address = 0x40081000 + (((4 << 5) + 0x1d) << 2) = 0x40081274
 *
 */
#define MEC17XX_GPIO_CTL(port, id) REG32(MEC17XX_GPIO_BASE + (((port << 5) + id) << 2))

#define DUMMY_GPIO_BANK 0


/* Timer */
#define MEC17XX_TMR16_BASE(x)  (0x40000c00 + (x) * 0x20)
#define MEC17XX_TMR32_BASE(x)  (0x40000c80 + (x) * 0x20)

#define MEC17XX_TMR16_CNT(x)   REG32(MEC17XX_TMR16_BASE(x) + 0x0)
#define MEC17XX_TMR16_PRE(x)   REG32(MEC17XX_TMR16_BASE(x) + 0x4)
#define MEC17XX_TMR16_STS(x)   REG32(MEC17XX_TMR16_BASE(x) + 0x8)
#define MEC17XX_TMR16_IEN(x)   REG32(MEC17XX_TMR16_BASE(x) + 0xc)
#define MEC17XX_TMR16_CTL(x)   REG32(MEC17XX_TMR16_BASE(x) + 0x10)
#define MEC17XX_TMR32_CNT(x)   REG32(MEC17XX_TMR32_BASE(x) + 0x0)
#define MEC17XX_TMR32_PRE(x)   REG32(MEC17XX_TMR32_BASE(x) + 0x4)
#define MEC17XX_TMR32_STS(x)   REG32(MEC17XX_TMR32_BASE(x) + 0x8)
#define MEC17XX_TMR32_IEN(x)   REG32(MEC17XX_TMR32_BASE(x) + 0xc)
#define MEC17XX_TMR32_CTL(x)   REG32(MEC17XX_TMR32_BASE(x) + 0x10)
/* 16-bit Basic Timers[0:3] = GIRQ23 bits[0:3] */
#define MEC17XX_TMR16_GIRQ		23
#define MEC17XX_TMR16_GIRQ_BIT(x)	(1ul << (x))
/* 32-bit Basic Timers[0:1] = GIRQ23 bits[4:5] */
#define MEC17XX_TMR32_GIRQ		23
#define MEC17XX_TMR32_GIRQ_BIT(x)	(1ul << ((x) + 4))


/* Watchdog */
#define MEC17XX_WDG_BASE       0x40000000
#define MEC17XX_WDG_LOAD       REG16(MEC17XX_WDG_BASE + 0x0)
#define MEC17XX_WDG_CTL        REG8(MEC17XX_WDG_BASE + 0x4)
#define MEC17XX_WDG_KICK       REG8(MEC17XX_WDG_BASE + 0x8)
#define MEC17XX_WDG_CNT        REG16(MEC17XX_WDG_BASE + 0xc)


/* VBAT */
#define MEC17XX_VBAT_BASE               0x4000a400
#define MEC17XX_VBAT_STS                REG32(MEC17XX_VBAT_BASE + 0x0)
#define MEC17XX_VBAT_CE                 REG32(MEC17XX_VBAT_BASE + 0x8)
#define MEC17XX_VBAT_SCRATCH            REG32(MEC17XX_VBAT_BASE + 0xC)
#define MEC17XX_VBAT_MONOTONIC_CTR_LO   REG32(MEC17XX_VBAT_BASE + 0x20)
#define MEC17XX_VBAT_MONOTONIC_CTR_HI   REG32(MEC17XX_VBAT_BASE + 0x24)
#define MEC17XX_VBAT_VWIRE_BACKUP       REG32(MEC17XX_VBAT_BASE + 0x28)
#define MEC17XX_VBAT_RAM(x)    REG32(MEC17XX_VBAT_BASE + 0x400 + 4 * (x))

/* Bit definition for MEC17XX_VBAT_STS */
#define MEC17XX_VBAT_STS_SOFTRESET      (1 << 2)
#define MEC17XX_VBAT_STS_RESETI         (1 << 4)
#define MEC17XX_VBAT_STS_WDT	        (1 << 5)
#define MEC17XX_VBAT_STS_SYSRESETREQ	(1 << 6)
#define MEC17XX_VBAT_STS_VBAT_RST   	(1 << 7)

/* Bit definitions for MEC17XX_VBAT_CE */
#define MEC17XX_VBAT_CE_XOSEL_BITPOS    (3)
#define MEC17XX_VBAT_CE_XOSEL_MASK      (1ul << 3)
#define MEC17XX_VBAT_CE_XOSEL_PAR       (0ul << 3)
#define MEC17XX_VBAT_CE_XOSEL_SE        (1ul << 3)

#define MEC17XX_VBAT_CE_32K_SRC_BITPOS  (2)
#define MEC17XX_VBAT_CE_32K_SRC_MASK    (1ul << 2)
#define MEC17XX_VBAT_CE_32K_SRC_INT     (0ul << 2)
#define MEC17XX_VBAT_CE_32K_SRC_CRYS    (1ul << 2)

#define MEC17XX_VBAT_CE_EXT_32K_BITPOS  (1)
#define MEC17XX_VBAT_CE_EXT_32K_MASK    (1ul << 1)
#define MEC17XX_VBAT_CE_INT_32K         (0ul << 1)
#define MEC17XX_VBAT_CE_EXT_32K         (1ul << 1)

#define MEC17XX_VBAT_CE_32K_VTR_BITPOS  (0)
#define MEC17XX_VBAT_CE_32K_VTR_MASK    (1ul << 0)
#define MEC17XX_VBAT_CE_32K_VTR_ON      (0ul << 0)
#define MEC17XX_VBAT_CE_32K_VTR_OFF     (1ul << 0)


/* Miscellaneous firmware control fields
 * scratch pad index cannot be more than 32 as
 * mec17xx has 128 bytes = 32 indexes of scratchpad RAM
 */
#define MEC17XX_IMAGETYPE_IDX     31


/* LPC */
#define MEC17XX_LPC_CFG_BASE     0x400f3300
#define MEC17XX_LPC_ACT          REG8(MEC17XX_LPC_CFG_BASE + 0x30)
#define MEC17XX_LPC_SIRQ(x)      REG8(MEC17XX_LPC_CFG_BASE + 0x40 + (x))
#define MEC17XX_LPC_CFG_BAR      REG32(MEC17XX_LPC_CFG_BASE + 0x60)
#define MEC17XX_LPC_MAILBOX_BAR  REG32(MEC17XX_LPC_CFG_BASE + 0x64)
#define MEC17XX_LPC_8042_BAR     REG32(MEC17XX_LPC_CFG_BASE + 0x68)
#define MEC17XX_LPC_ACPI_EC0_BAR REG32(MEC17XX_LPC_CFG_BASE + 0x6C)
#define MEC17XX_LPC_ACPI_EC1_BAR REG32(MEC17XX_LPC_CFG_BASE + 0x70)
#define MEC17XX_LPC_ACPI_EC2_BAR REG32(MEC17XX_LPC_CFG_BASE + 0x74)
#define MEC17XX_LPC_ACPI_EC3_BAR REG32(MEC17XX_LPC_CFG_BASE + 0x78)
#define MEC17XX_LPC_ACPI_EC4_BAR REG32(MEC17XX_LPC_CFG_BASE + 0x7C)
#define MEC17XX_LPC_ACPI_PM1_BAR REG32(MEC17XX_LPC_CFG_BASE + 0x80)
#define MEC17XX_LPC_PORT92_BAR   REG32(MEC17XX_LPC_CFG_BASE + 0x84)
#define MEC17XX_LPC_UART0_BAR    REG32(MEC17XX_LPC_CFG_BASE + 0x88)
#define MEC17XX_LPC_UART1_BAR    REG32(MEC17XX_LPC_CFG_BASE + 0x8C)
#define MEC17XX_LPC_EMI0_BAR     REG32(MEC17XX_LPC_CFG_BASE + 0x90)
#define MEC17XX_LPC_EMI1_BAR     REG32(MEC17XX_LPC_CFG_BASE + 0x94)
#define MEC17XX_LPC_EMI2_BAR     REG32(MEC17XX_LPC_CFG_BASE + 0x98)
#define MEC17XX_LPC_P80DBG0_BAR  REG32(MEC17XX_LPC_CFG_BASE + 0x9C)
#define MEC17XX_LPC_P80DBG1_BAR  REG32(MEC17XX_LPC_CFG_BASE + 0xA0)
#define MEC17XX_LPC_RTC_BAR      REG32(MEC17XX_LPC_CFG_BASE + 0xA4)

/* LPC Generic Memory BAR's, 64-bit registers */
#define MEC17XX_LPC_SRAM0_BAR_LO    REG32(MEC17XX_LPC_CFG_BASE + 0xB0)
#define MEC17XX_LPC_SRAM0_BAR_HI    REG32(MEC17XX_LPC_CFG_BASE + 0xB4)
#define MEC17XX_LPC_SRAM1_BAR_LO    REG32(MEC17XX_LPC_CFG_BASE + 0xB8)
#define MEC17XX_LPC_SRAM1_BAR_HI    REG32(MEC17XX_LPC_CFG_BASE + 0xBC)

/*
 * LPC Logical Device Memory BAR's, 48-bit registers
 * Use 16-bit aligned access
 */
#define MEC17XX_LPC_MAILBOX_MEM_BAR_H0  REG32(MEC17XX_LPC_CFG_BASE + 0xC0)
#define MEC17XX_LPC_MAILBOX_MEM_BAR_H1  REG32(MEC17XX_LPC_CFG_BASE + 0xC2)
#define MEC17XX_LPC_MAILBOX_MEM_BAR_H2  REG32(MEC17XX_LPC_CFG_BASE + 0xC4)
#define MEC17XX_LPC_ACPI_EC0_MEM_BAR_H0 REG32(MEC17XX_LPC_CFG_BASE + 0xC6)
#define MEC17XX_LPC_ACPI_EC0_MEM_BAR_H1 REG32(MEC17XX_LPC_CFG_BASE + 0xC8)
#define MEC17XX_LPC_ACPI_EC0_MEM_BAR_H2 REG32(MEC17XX_LPC_CFG_BASE + 0xCA)
#define MEC17XX_LPC_ACPI_EC1_MEM_BAR_H0 REG32(MEC17XX_LPC_CFG_BASE + 0xCC)
#define MEC17XX_LPC_ACPI_EC1_MEM_BAR_H1 REG32(MEC17XX_LPC_CFG_BASE + 0xCE)
#define MEC17XX_LPC_ACPI_EC1_MEM_BAR_H2 REG32(MEC17XX_LPC_CFG_BASE + 0xD0)
#define MEC17XX_LPC_ACPI_EC2_MEM_BAR_H0 REG32(MEC17XX_LPC_CFG_BASE + 0xD2)
#define MEC17XX_LPC_ACPI_EC2_MEM_BAR_H1 REG32(MEC17XX_LPC_CFG_BASE + 0xD4)
#define MEC17XX_LPC_ACPI_EC2_MEM_BAR_H2 REG32(MEC17XX_LPC_CFG_BASE + 0xD6)
#define MEC17XX_LPC_ACPI_EC3_MEM_BAR_H0 REG32(MEC17XX_LPC_CFG_BASE + 0xD8)
#define MEC17XX_LPC_ACPI_EC3_MEM_BAR_H1 REG32(MEC17XX_LPC_CFG_BASE + 0xDA)
#define MEC17XX_LPC_ACPI_EC3_MEM_BAR_H2 REG32(MEC17XX_LPC_CFG_BASE + 0xDC)
#define MEC17XX_LPC_ACPI_EC4_MEM_BAR_H0 REG32(MEC17XX_LPC_CFG_BASE + 0xDE)
#define MEC17XX_LPC_ACPI_EC4_MEM_BAR_H1 REG32(MEC17XX_LPC_CFG_BASE + 0xE0)
#define MEC17XX_LPC_ACPI_EC4_MEM_BAR_H2 REG32(MEC17XX_LPC_CFG_BASE + 0xE2)
#define MEC17XX_LPC_ACPI_EMI0_MEM_BAR_H0 REG32(MEC17XX_LPC_CFG_BASE + 0xE4)
#define MEC17XX_LPC_ACPI_EMI0_MEM_BAR_H1 REG32(MEC17XX_LPC_CFG_BASE + 0xE6)
#define MEC17XX_LPC_ACPI_EMI0_MEM_BAR_H2 REG32(MEC17XX_LPC_CFG_BASE + 0xE8)
#define MEC17XX_LPC_ACPI_EMI1_MEM_BAR_H0 REG32(MEC17XX_LPC_CFG_BASE + 0xEA)
#define MEC17XX_LPC_ACPI_EMI1_MEM_BAR_H1 REG32(MEC17XX_LPC_CFG_BASE + 0xEC)
#define MEC17XX_LPC_ACPI_EMI1_MEM_BAR_H2 REG32(MEC17XX_LPC_CFG_BASE + 0xEE)
#define MEC17XX_LPC_ACPI_EMI2_MEM_BAR_H0 REG32(MEC17XX_LPC_CFG_BASE + 0xF0)
#define MEC17XX_LPC_ACPI_EMI2_MEM_BAR_H1 REG32(MEC17XX_LPC_CFG_BASE + 0xF2)
#define MEC17XX_LPC_ACPI_EMI2_MEM_BAR_H2 REG32(MEC17XX_LPC_CFG_BASE + 0xF4)


#define MEC17XX_LPC_RT_BASE      0x400f3100
#define MEC17XX_LPC_BUS_MONITOR  REG32(MEC17XX_LPC_RT_BASE + 0x4)
#define MEC17XX_LPC_CLK_CTRL     REG32(MEC17XX_LPC_RT_BASE + 0x10)
#define MEC17XX_LPC_BAR_INHIBIT  REG32(MEC17XX_LPC_RT_BASE + 0x10)
#define MEC17XX_LPC_BAR_INIT     REG32(MEC17XX_LPC_RT_BASE + 0x30)
#define MEC17XX_LPC_SRAM0_BAR    REG32(MEC17XX_LPC_RT_BASE + 0xf8)
#define MEC17XX_LPC_SRAM1_BAR    REG32(MEC17XX_LPC_RT_BASE + 0xfc)


/* EMI */
#define MEC17XX_EMI_BASE(x)     (0x400F4100 + ((x) << 10))
#define MEC17XX_EMI_H2E_MBX(x)  REG8(MEC17XX_EMI_BASE(x) + 0x0)
#define MEC17XX_EMI_E2H_MBX(x)  REG8(MEC17XX_EMI_BASE(x) + 0x1)
#define MEC17XX_EMI_MBA0(x)     REG32(MEC17XX_EMI_BASE(x) + 0x4)
#define MEC17XX_EMI_MRL0(x)     REG16(MEC17XX_EMI_BASE(x) + 0x8)
#define MEC17XX_EMI_MWL0(x)     REG16(MEC17XX_EMI_BASE(x) + 0xa)
#define MEC17XX_EMI_MBA1(x)     REG32(MEC17XX_EMI_BASE(x) + 0xc)
#define MEC17XX_EMI_MRL1(x)     REG16(MEC17XX_EMI_BASE(x) + 0x10)
#define MEC17XX_EMI_MWL1(x)     REG16(MEC17XX_EMI_BASE(x) + 0x12)
#define MEC17XX_EMI_ISR(x)      REG16(MEC17XX_EMI_BASE(x) + 0x14)
#define MEC17XX_EMI_HCE(x)      REG16(MEC17XX_EMI_BASE(x) + 0x16)

#define MEC17XX_EMI_RT_BASE(x)  (0x400F4000 + ((x) << 10))
#define MEC17XX_EMI_ISR_B0(x)   REG8(MEC17XX_EMI_RT_BASE(x) + 0x8)
#define MEC17XX_EMI_ISR_B1(x)   REG8(MEC17XX_EMI_RT_BASE(x) + 0x9)
#define MEC17XX_EMI_IMR_B0(x)   REG8(MEC17XX_EMI_RT_BASE(x) + 0xa)
#define MEC17XX_EMI_IMR_B1(x)   REG8(MEC17XX_EMI_RT_BASE(x) + 0xb)
/*
 * EMI[0:2] on GIRQ15 bits[2:4]
 */
#define MEC17XX_EMI_GIRQ	15
#define MEC17XX_EMI_GIRQ_BIT(x)	(1ul << ((x)+2))


/* Mailbox */
#define MEC17XX_MBX_RT_BASE    0x400f0000
#define MEC17XX_MBX_INDEX      REG8(MEC17XX_MBX_RT_BASE + 0x0)
#define MEC17XX_MBX_DATA       REG8(MEC17XX_MBX_RT_BASE + 0x1)

#define MEC17XX_MBX_BASE       0x400f0100
#define MEC17XX_MBX_H2E_MBX    REG8(MEC17XX_MBX_BASE + 0x0)
#define MEC17XX_MBX_E2H_MBX    REG8(MEC17XX_MBX_BASE + 0x4)
#define MEC17XX_MBX_ISR        REG8(MEC17XX_MBX_BASE + 0x8)
#define MEC17XX_MBX_IMR        REG8(MEC17XX_MBX_BASE + 0xc)
#define MEC17XX_MBX_REG(x)     REG8(MEC17XX_MBX_BASE + 0x10 + (x))
/*
 * Mailbox on GIRQ15 bit[20]
 */
#define MEC17XX_MBX_GIRQ	15
#define MEC17XX_MBX_GIRQ_BIT	(1ul << 20)


/* Port80 Capture */
#define MEC17XX_P80_BASE(x)		(0x400f8000 + ((x) << 10))
#define MEC17XX_P80_HOST_DATA(x)	REG8(MEC17XX_P80_BASE(x))
/* Data catpure with timestamp register */
#define MEC17XX_P80_CAP(x)		REG32(MEC17XX_P80_BASE(x) + 0x100)
#define MEC17XX_P80_CFG(x)		REG8(MEC17XX_P80_BASE(x) + 0x104)
#define MEC17XX_P80_STS(x)		REG8(MEC17XX_P80_BASE(x) + 0x108)
#define MEC17XX_P80_CNT(x)		REG32(MEC17XX_P80_BASE(x) + 0x10c)
#define MEC17XX_P80_CNT_GET(x)		(REG32(MEC17XX_P80_BASE(x) + 0x10c) >> 8)
#define MEC17XX_P80_CNT_SET(x,c)	(REG32(MEC17XX_P80_BASE(x) + 0x10c) = (c << 8))
#define MEC17XX_P80_ACTIVATE(x)		REG8(MEC17XX_P80_BASE(x) + 0x330)
/*
 * Port80 [0:1] GIRQ15 bits[22:23]
 */
#define MEC17XX_P80_GIRQ	15
#define MEC17XX_P80_GIRQ_BIT(x)	(1ul << ((x) + 22))

/* Port80 Data register bits
 * bits[7:0] = data captured on Host write
 * bits[31:8] = optional time stamp
*/
#define MEC17XX_P80_CAP_DATA_MASK		0xFFul
#define MEC17XX_P80_CAP_TS_BITPOS		8
#define MEC17XX_P80_CAP_TS_MASK0		0xfffffful
#define MEC17XX_P80_CAP_TS_MASK			((MEC17XX_P80_CAP_TS_MASK0) << (MEC17XX_P80_CAP_TS_BITPOS))

/* Port80 Configuration register bits */
#define MEC17XX_P80_FLUSH_FIFO_WO		(1u << 1)
#define MEC17XX_P80_RESET_TIMESTAMP_WO		(1u << 2)
#define MEC17XX_P80_TIMEBASE_BITPOS		3
#define MEC17XX_P80_TIMEBASE_MASK0		0x03
#define MEC17XX_P80_TIMEBASE_MASK		((MEC17XX_P80_TIMEBASE_MASK0) << (MEC17XX_P80_TIMEBASE_BITPOS))
#define MEC17XX_P80_TIMEBASE_750KHZ		(0x03 << (MEC17XX_P80_TIMEBASE_BITPOS))
#define MEC17XX_P80_TIMEBASE_1500KHZ		(0x02 << (MEC17XX_P80_TIMEBASE_BITPOS))
#define MEC17XX_P80_TIMEBASE_3MHZ		(0x01 << (MEC17XX_P80_TIMEBASE_BITPOS))
#define MEC17XX_P80_TIMEBASE_6MHZ		(0x00 << (MEC17XX_P80_TIMEBASE_BITPOS))
#define MEC17XX_P80_TIMER_ENABLE		(1u << 5)
#define MEC17XX_P80_FIFO_THRHOLD_MASK		(3u << 6)
#define MEC17XX_P80_FIFO_THRHOLD_1		(0u << 6)
#define MEC17XX_P80_FIFO_THRHOLD_4		(1u << 6)
#define MEC17XX_P80_FIFO_THRHOLD_8		(2u << 6)
#define MEC17XX_P80_FIFO_THRHOLD_14		(3u << 6)
#define MEC17XX_P80_FIFO_LEN			16

/* Port80 Status register bits, read-only */
#define MEC17XX_P80_STS_NOT_EMPTY		0x01
#define MEC17XX_P80_STS_OVERRUN			0x02

/* Port80 Count register bits */
#define MEC17XX_P80_CNT_BITPOS			8
#define MEC17XX_P80_CNT_MASK0			0xfffffful
#define MEC17XX_P80_CNT_MASK			((MEC17XX_P80_CNT_MASK0) << (MEC17XX_P80_CNT_BITPOS))



/* PWM */
#define MEC17XX_PWM_BASE(x)    (0x40005800 + ((x) << 4))
#define MEC17XX_PWM_ON(x)      REG32(MEC17XX_PWM_BASE(x) + 0x00)
#define MEC17XX_PWM_OFF(x)     REG32(MEC17XX_PWM_BASE(x) + 0x04)
#define MEC17XX_PWM_CFG(x)     REG32(MEC17XX_PWM_BASE(x) + 0x08)


/* ACPI */
#define MEC17XX_ACPI_EC_BASE(x)     (0x400f0800 + ((x) << 10))
#define MEC17XX_ACPI_EC_EC2OS(x, y) REG8(MEC17XX_ACPI_EC_BASE(x) + 0x100 + (y))
#define MEC17XX_ACPI_EC_STATUS(x)   REG8(MEC17XX_ACPI_EC_BASE(x) + 0x104)
#define MEC17XX_ACPI_EC_BYTE_CTL(x) REG8(MEC17XX_ACPI_EC_BASE(x) + 0x105)
#define MEC17XX_ACPI_EC_OS2EC(x, y) REG8(MEC17XX_ACPI_EC_BASE(x) + 0x108 + (y))

#define MEC17XX_ACPI_PM_RT_BASE     0x400f1c00
#define MEC17XX_ACPI_PM1_STS1       REG8(MEC17XX_ACPI_PM_RT_BASE + 0x0)
#define MEC17XX_ACPI_PM1_STS2       REG8(MEC17XX_ACPI_PM_RT_BASE + 0x1)
#define MEC17XX_ACPI_PM1_EN1        REG8(MEC17XX_ACPI_PM_RT_BASE + 0x2)
#define MEC17XX_ACPI_PM1_EN2        REG8(MEC17XX_ACPI_PM_RT_BASE + 0x3)
#define MEC17XX_ACPI_PM1_CTL1       REG8(MEC17XX_ACPI_PM_RT_BASE + 0x4)
#define MEC17XX_ACPI_PM1_CTL2       REG8(MEC17XX_ACPI_PM_RT_BASE + 0x5)
#define MEC17XX_ACPI_PM2_CTL1       REG8(MEC17XX_ACPI_PM_RT_BASE + 0x6)
#define MEC17XX_ACPI_PM2_CTL2       REG8(MEC17XX_ACPI_PM_RT_BASE + 0x7)
#define MEC17XX_ACPI_PM_EC_BASE     0x400f1d00
#define MEC17XX_ACPI_PM_STS         REG8(MEC17XX_ACPI_PM_EC_BASE + 0x10)
/* All ACPI EC controllers connected to GIRQ15
 * ACPI EC[0:4] IBF = GIRQ15 bits[5,7,9,11,13]
 * ACPI EC[0:4] OBE = GIRQ15 bits[6,8,10,12,14]
 * ACPI PM1_CTL = GIRQ15 bit[15]
 * ACPI PM1_EN  = GIRQ15 bit[16]
 * ACPI PM1_STS = GIRQ16 bit[17]
*/
#define MEC17XX_ACPI_EC_GIRQ		15
#define MEC17XX_ACPI_EC_IBF_GIRQ_BIT(x)	(1ul << (((x)<<1) + 5))
#define MEC17XX_ACPI_EC_OBE_GIRQ_BIT(x)	(1ul << (((x)<<1) + 6))
#define MEC17XX_ACPI_PM1_CTL_GIRQ_BIT	15
#define MEC17XX_ACPI_PM1_EN_GIRQ_BIT	16
#define MEC17XX_ACPI_PM1_STS_GIRQ_BIT	17


/* 8042 */
#define MEC17XX_8042_BASE      0x400f0400
#define MEC17XX_8042_OBF_CLR   REG8(MEC17XX_8042_BASE + 0x0)
#define MEC17XX_8042_H2E       REG8(MEC17XX_8042_BASE + 0x100)
#define MEC17XX_8042_E2H       REG8(MEC17XX_8042_BASE + 0x100)
#define MEC17XX_8042_STS       REG8(MEC17XX_8042_BASE + 0x104)
#define MEC17XX_8042_KB_CTRL   REG8(MEC17XX_8042_BASE + 0x108)
#define MEC17XX_8042_PCOBF     REG8(MEC17XX_8042_BASE + 0x114)
#define MEC17XX_8042_ACT       REG8(MEC17XX_8042_BASE + 0x330)
/*
 * 8042 [OBE:IBF] = GIRQ15 bits[18:19]
 */
#define MEC17XX_8042_GIRQ		15
#define MEC17XX_8042_OBE_GIRQ_BIT	(1ul << 18)
#define MEC17XX_8042_IBF_GIRQ_BIT	(1ul << 19)


/* FAN */
#define MEC17XX_FAN_BASE(x)         (0x4000a000 + ((x) << 7))
#define MEC17XX_FAN_SETTING(x)      REG8(MEC17XX_FAN_BASE(x) + 0x0)
#define MEC17XX_FAN_PWM_DIVIDE(x)   REG8(MEC17XX_FAN_BASE(x) + 0x1)
#define MEC17XX_FAN_CFG1(x)         REG8(MEC17XX_FAN_BASE(x) + 0x2)
#define MEC17XX_FAN_CFG2(x)         REG8(MEC17XX_FAN_BASE(x) + 0x3)
#define MEC17XX_FAN_GAIN(x)         REG8(MEC17XX_FAN_BASE(x) + 0x5)
#define MEC17XX_FAN_SPIN_UP(x)      REG8(MEC17XX_FAN_BASE(x) + 0x6)
#define MEC17XX_FAN_STEP(x)         REG8(MEC17XX_FAN_BASE(x) + 0x7)
#define MEC17XX_FAN_MIN_DRV(x)      REG8(MEC17XX_FAN_BASE(x) + 0x8)
#define MEC17XX_FAN_VALID_CNT(x)    REG8(MEC17XX_FAN_BASE(x) + 0x9)
#define MEC17XX_FAN_DRV_FAIL(x)     REG16(MEC17XX_FAN_BASE(x) + 0xa)
#define MEC17XX_FAN_TARGET(x)       REG16(MEC17XX_FAN_BASE(x) + 0xc)
#define MEC17XX_FAN_READING(x)      REG16(MEC17XX_FAN_BASE(x) + 0xe)
#define MEC17XX_FAN_BASE_FREQ(x)    REG8(MEC17XX_FAN_BASE(x) + 0x10)
#define MEC17XX_FAN_STATUS(x)       REG8(MEC17XX_FAN_BASE(x) + 0x11)
/*
 * FAN(RPM2PWM) all instances on GIRQ17
 * FAN[0:1] Fail  = GIRQ17 bits[4,6]
 * FAN[0:1] Stall = GIRQ17 bits[5,7]
 */
#define MEC17XX_FAN_GIRQ		17
#define MEC17XX_FAN_FAIL_GIRQ_BIT(x)	(1ul << (((x)<<1)+4))
#define MEC17XX_FAN_STALL_GIRQ_BIT(x)	(1ul << (((x)<<1)+5))


/* I2C */
#define MEC17XX_I2C_BASE(x)     (0x40004000 + ((x) << 10))
#define MEC17XX_I2C0_BASE       0x40004000
#define MEC17XX_I2C1_BASE       0x40004400
#define MEC17XX_I2C2_BASE       0x40004800
#define MEC17XX_I2C3_BASE       0x40004C00
#define MEC17XX_I2C_BASESEP     0x00000400
#define MEC17XX_I2C_ADDR(controller, offset) \
    (offset + MEC17XX_I2C_BASE(controller))

/*
 * MEC1701H 144-pin package has eleven ports distributed
 * among four controllers. Any port may be mapped to
 * any controller. Firmware must not map the same port
 * to multiple controllers.
 *
 * I2C00_SDA=GPIO_0003 I2C00_SCL=GPIO_0004
 * I2C01_SDA=GPIO_0005 I2C01_SCL=GPIO_0006 PINS NOT PRESENT
 * I2C02_SDA=GPIO_0154 I2C02_SCL=GPIO_0155
 * I2C03_SDA=GPIO_0007 I2C03_SCL=GPIO_0010
 * I2C04_SDA=GPIO_0143 I2C04_SCL=GPIO_0144
 * I2C05_SDA=GPIO_0141 I2C04_SCL=GPIO_0142
 * I2C06_SDA=GPIO_0132 I2C06_SCL=GPIO_0140
 * I2C07_SDA=GPIO_0012 I2C07_SCL=GPIO_0013
 * I2C08_SDA=GPIO_0147 I2C08_SCL=GPIO_0150
 * I2C09_SDA=GPIO_0145 I2C09_SCL=GPIO_0146
 * I2C10_SDA=GPIO_0130 I2C10_SCL=GPIO_0131
 *
 * The MEC17XX_I2Cn_m enums are programmed
 * into the SMBus controller configuration
 * register port selection field.
 * Because the enum's are no longer consecutive
 * we must make sure the are not used as indices
 * into an array.
 *
 * Locking must occur by-controller (not by-port).
 */
#define MEC17XX_I2C_PORT_MASK	0x003Dul

enum mec17xx_i2c_port {
	MEC17XX_I2C0_0 = 0,      /* Controller 0, port 0 */
	MEC17XX_I2C0_2 = 2,      /* Controller 0, port 2 */
	MEC17XX_I2C1_3 = 3,      /* Controller 1, port 3 */
	MEC17XX_I2C2_4 = 4,      /* Controller 2, port 4 */
	MEC17XX_I2C3_5 = 5,      /* Controller 3, port 5 */
	MEC17XX_I2C_PORT_COUNT = 5,
	MEC17XX_I2C_PORT_MAX = 6
};

#define MEC17XX_I2C_CTRL(ctrl)          REG8(MEC17XX_I2C_ADDR(ctrl, 0x0))
#define MEC17XX_I2C_STATUS(ctrl)        REG8(MEC17XX_I2C_ADDR(ctrl, 0x0))
#define MEC17XX_I2C_OWN_ADDR(ctrl)      REG16(MEC17XX_I2C_ADDR(ctrl, 0x4))
#define MEC17XX_I2C_DATA(ctrl)          REG8(MEC17XX_I2C_ADDR(ctrl, 0x8))
#define MEC17XX_I2C_MASTER_CMD(ctrl)    REG32(MEC17XX_I2C_ADDR(ctrl, 0xc))
#define MEC17XX_I2C_SLAVE_CMD(ctrl)     REG32(MEC17XX_I2C_ADDR(ctrl, 0x10))
#define MEC17XX_I2C_PEC(ctrl)           REG8(MEC17XX_I2C_ADDR(ctrl, 0x14))
#define MEC17XX_I2C_DATA_TIM_2(ctrl)    REG8(MEC17XX_I2C_ADDR(ctrl, 0x18))
#define MEC17XX_I2C_COMPLETE(ctrl)      REG32(MEC17XX_I2C_ADDR(ctrl, 0x20))
#define MEC17XX_I2C_IDLE_SCALE(ctrl)    REG32(MEC17XX_I2C_ADDR(ctrl, 0x24))
#define MEC17XX_I2C_CONFIG(ctrl)        REG32(MEC17XX_I2C_ADDR(ctrl, 0x28))
#define MEC17XX_I2C_BUS_CLK(ctrl)       REG16(MEC17XX_I2C_ADDR(ctrl, 0x2c))
#define MEC17XX_I2C_BLK_ID(ctrl)        REG8(MEC17XX_I2C_ADDR(ctrl, 0x30))
#define MEC17XX_I2C_REV(ctrl)           REG8(MEC17XX_I2C_ADDR(ctrl, 0x34))
#define MEC17XX_I2C_BB_CTRL(ctrl)       REG8(MEC17XX_I2C_ADDR(ctrl, 0x38))
#define MEC17XX_I2C_DATA_TIM(ctrl)      REG32(MEC17XX_I2C_ADDR(ctrl, 0x40))
#define MEC17XX_I2C_TOUT_SCALE(ctrl)    REG32(MEC17XX_I2C_ADDR(ctrl, 0x44))
#define MEC17XX_I2C_SLAVE_TX_BUF(ctrl)  REG8(MEC17XX_I2C_ADDR(ctrl, 0x48))
#define MEC17XX_I2C_SLAVE_RX_BUF(ctrl)  REG8(MEC17XX_I2C_ADDR(ctrl, 0x4c))
#define MEC17XX_I2C_MASTER_TX_BUF(ctrl) REG8(MEC17XX_I2C_ADDR(ctrl, 0x50))
#define MEC17XX_I2C_MASTER_RX_BUF(ctrl) REG8(MEC17XX_I2C_ADDR(ctrl, 0x54))
#define MEC17XX_I2C_WAKE_STS(ctrl)      REG8(MEC17XX_I2C_ADDR(ctrl, 0x60))
#define MEC17XX_I2C_WAKE_EN(ctrl)       REG8(MEC17XX_I2C_ADDR(ctrl, 0x64))
/* All I2C controller connected to GIRQ13 */
#define MEC17XX_I2C_GIRQ		13
/* I2C[0:3] -> GIRQ13 bits[0:3] */
#define MEC17XX_I2C_GIRQ_BIT(x)		(1ul << (x))


/* Keyboard scan matrix */
#define MEC17XX_KS_BASE        0x40009c00
#define MEC17XX_KS_KSO_SEL     REG32(MEC17XX_KS_BASE + 0x4)
#define MEC17XX_KS_KSI_INPUT   REG32(MEC17XX_KS_BASE + 0x8)
#define MEC17XX_KS_KSI_STATUS  REG32(MEC17XX_KS_BASE + 0xc)
#define MEC17XX_KS_KSI_INT_EN  REG32(MEC17XX_KS_BASE + 0x10)
#define MEC17XX_KS_EXT_CTRL    REG32(MEC17XX_KS_BASE + 0x14)
#define MEC17XX_KS_GIRQ		21
#define MEC17XX_KS_GIRQ_BIT	(1ul << 25)


/* ADC */
#define MEC17XX_ADC_BASE       0x40007c00
#define MEC17XX_ADC_CTRL       REG32(MEC17XX_ADC_BASE + 0x0)
#define MEC17XX_ADC_DELAY      REG32(MEC17XX_ADC_BASE + 0x4)
#define MEC17XX_ADC_STS        REG32(MEC17XX_ADC_BASE + 0x8)
#define MEC17XX_ADC_SINGLE     REG32(MEC17XX_ADC_BASE + 0xc)
#define MEC17XX_ADC_REPEAT     REG32(MEC17XX_ADC_BASE + 0x10)
#define MEC17XX_ADC_READ(x)    REG32(MEC17XX_ADC_BASE + 0x14 + ((x) * 0x4))
#define MEC17XX_ADC_GIRQ		17
#define MEC17XX_ADC_GIRQ_SINGLE_BIT	(1ul << 8)
#define MEC17XX_ADC_GIRQ_REPEAT_BIT	(1ul << 9)


/* Hibernation timer */
#define MEC17XX_HTIMER_BASE(x)      (0x40009800 + ((x) << 5))
#define MEC17XX_HTIMER_PRELOAD(x)   REG16(MEC17XX_HTIMER_BASE(x) + 0x0)
#define MEC17XX_HTIMER_CONTROL(x)   REG16(MEC17XX_HTIMER_BASE(x) + 0x4)
#define MEC17XX_HTIMER_COUNT(x)     REG16(MEC17XX_HTIMER_BASE(x) + 0x8)
/* All Hibernation timers connected to GIRQ21 */
#define MEC17XX_HTIMER_GIRQ		21
/* HTIMER[0:1] -> GIRQ21 bits[1:2] */
#define MEC17XX_HTIMER_GIRQ_BIT(x)	(1ul << ((x) + 1))


/* General Purpose SPI (GP-SPI) */
#define MEC17XX_SPI_BASE(port) (0x40009400 + ((port) << 7))
#define MEC17XX_SPI_AR(port)   REG8(MEC17XX_SPI_BASE(port) + 0x00)
#define MEC17XX_SPI_CR(port)   REG8(MEC17XX_SPI_BASE(port) + 0x04)
#define MEC17XX_SPI_SR(port)   REG8(MEC17XX_SPI_BASE(port) + 0x08)
#define MEC17XX_SPI_TD(port)   REG8(MEC17XX_SPI_BASE(port) + 0x0c)
#define MEC17XX_SPI_RD(port)   REG8(MEC17XX_SPI_BASE(port) + 0x10)
#define MEC17XX_SPI_CC(port)   REG8(MEC17XX_SPI_BASE(port) + 0x14)
#define MEC17XX_SPI_CG(port)   REG8(MEC17XX_SPI_BASE(port) + 0x18)
/* All GP-SPI controllers connected to GIRQ18 */
#define MEC17XX_SPI_GIRQ		18
/* SPI[0:1] TXBE -> GIRQ18 bits[2,4]
 * SPI[0:1] RXBF -> GIRQ18 bits[3,5]
*/
#define MEC17XX_SPI_GIRQ_TXBE_BIT(x)	(1ul << (((x) << 1) + 2))
#define MEC17XX_SPI_GIRQ_RXBF_BIT(x)	(1ul << (((x) << 1) + 3))


/* Quad Master SPI (QMSPI) */
#define MEC17XX_QMSPI0_BASE		0x40005400
#define MEC17XX_QMSPI0_MODE		REG32(MEC17XX_QMSPI0_BASE + 0x00)
#define MEC17XX_QMSPI0_CTRL		REG32(MEC17XX_QMSPI0_BASE + 0x04)
#define MEC17XX_QMSPI0_EXE		REG8(MEC17XX_QMSPI0_BASE + 0x08)
#define MEC17XX_QMSPI0_IFCTRL		REG8(MEC17XX_QMSPI0_BASE + 0x0C)
#define MEC17XX_QMSPI0_STS		REG32(MEC17XX_QMSPI0_BASE + 0x10)
#define MEC17XX_QMSPI0_BUFCNT_STS	REG32(MEC17XX_QMSPI0_BASE + 0x14)
#define MEC17XX_QMSPI0_IEN		REG32(MEC17XX_QMSPI0_BASE + 0x18)
#define MEC17XX_QMSPI0_BUFCNT_TRIG	REG32(MEC17XX_QMSPI0_BASE + 0x1C)
#define MEC17XX_QMSPI0_TX_FIFO8		REG8(MEC17XX_QMSPI0_BASE + 0x20)
#define MEC17XX_QMSPI0_TX_FIFO16	REG16(MEC17XX_QMSPI0_BASE + 0x20)
#define MEC17XX_QMSPI0_TX_FIFO32	REG32(MEC17XX_QMSPI0_BASE + 0x20)
#define MEC17XX_QMSPI0_RX_FIFO8		REG8(MEC17XX_QMSPI0_BASE + 0x24)
#define MEC17XX_QMSPI0_RX_FIFO16	REG16(MEC17XX_QMSPI0_BASE + 0x24)
#define MEC17XX_QMSPI0_RX_FIFO32	REG32(MEC17XX_QMSPI0_BASE + 0x24)
#define MEC17XX_QMSPI0_DESCR(x)		REG32(MEC17XX_QMSPI0_BASE + 0x30 + ((x)<<2))
#define MEC17XX_QMSPI_GIRQ		18
#define MEC17XX_QMSPI_GIRQ_BIT		(1ul << 1)

#define MEC17XX_QMSPI_MAX_DESCR		5

/* Bits in MEC17XX_QMSPI0_MODE */
#define MEC17XX_QMSPI_M_ACTIVATE        (1ul << 0)
#define MEC17XX_QMSPI_M_SOFT_RESET      (1ul << 1)
#define MEC17XX_QMSPI_M_SPI_MODE_MASK   (0x7ul << 8)
#define MEC17XX_QMSPI_M_SPI_MODE0       (0x0ul << 8)
#define MEC17XX_QMSPI_M_SPI_MODE3       (0x3ul << 8)
#define MEC17XX_QMSPI_M_SPI_MODE0_48M   (0x4ul << 8)
#define MEC17XX_QMSPI_M_SPI_MODE3_48M   (0x7ul << 8)
/* clock divider is 8-bit field in bits[23:16]
 * [1, 255] -> 48MHz / [1, 255], 0 -> 48MHz / 256 */
#define MEC17XX_QMSPI_M_SPI_CLKDIV_48M  (1ul << 16)
#define MEC17XX_QMSPI_M_SPI_CLKDIV_24M  (2ul << 16)
#define MEC17XX_QMSPI_M_SPI_CLKDIV_16M  (3ul << 16)
#define MEC17XX_QMSPI_M_SPI_CLKDIV_12M  (4ul << 16)
#define MEC17XX_QMSPI_M_SPI_CLKDIV_8M   (6ul << 16)
#define MEC17XX_QMSPI_M_SPI_CLKDIV_6M   (8ul << 16)
#define MEC17XX_QMSPI_M_SPI_CLKDIV_1M   (48ul << 16)
#define MEC17XX_QMSPI_M_SPI_CLKDIV_188K (0x100ul << 16)

/* Bits in MEC17XX_QMSPI0_CTRL and MEC17XX_QMSPI_DESCR(x) */
#define MEC17XX_QMSPI_CTRL_1X           (0ul << 0) /* Full Duplex */
#define MEC17XX_QMSPI_CTRL_2X           (1ul << 0) /* Dual IO */
#define MEC17XX_QMSPI_CTRL_4X           (2ul << 0) /* Quad IO */
#define MEC17XX_QMSPI_CTRL_TX_DIS       (0ul << 2)
#define MEC17XX_QMSPI_CTRL_TX_DATA      (1ul << 2)
#define MEC17XX_QMSPI_CTRL_TX_ZEROS     (2ul << 2)
#define MEC17XX_QMSPI_CTRL_TX_ONES      (3ul << 2)
#define MEC17XX_QMSPI_CTRL_TX_DMA_DIS   (0ul << 4)
#define MEC17XX_QMSPI_CTRL_TX_DMA_1B    (1ul << 4)
#define MEC17XX_QMSPI_CTRL_TX_DMA_2B    (2ul << 4)
#define MEC17XX_QMSPI_CTRL_TX_DMA_4B    (3ul << 4)
#define MEC17XX_QMSPI_CTRL_RX_DIS       (0ul << 6)
#define MEC17XX_QMSPI_CTRL_RX_EN        (1ul << 6)
#define MEC17XX_QMSPI_CTRL_RX_DMA_DIS   (0ul << 7)
#define MEC17XX_QMSPI_CTRL_RX_DMA_1B    (1ul << 7)
#define MEC17XX_QMSPI_CTRL_RX_DMA_2B    (2ul << 7)
#define MEC17XX_QMSPI_CTRL_RX_DMA_4B    (3ul << 7)
#define MEC17XX_QMSPI_CTRL_NO_CLOSE     (0ul << 9)
#define MEC17XX_QMSPI_CTRL_CLOSE        (1ul << 9)
#define MEC17XX_QMSPI_CTRL_XFRU_BITS    (0ul << 10)
#define MEC17XX_QMSPI_CTRL_XFRU_1B      (1ul << 10)
#define MEC17XX_QMSPI_CTRL_XFRU_4B      (2ul << 10)
#define MEC17XX_QMSPI_CTRL_XFRU_16B     (3ul << 10)
/* Control */
#define MEC17XX_QMSPI_CTRL_START_DESCR_BITPOS (12)
#define MEC17XX_QMSPI_CTRL_START_DESCR_MASK (0xFul << 12)
#define MEC17XX_QMSPI_CTRL_DESCR_MODE_EN (1ul << 16)
/* Descriptors, indicates the current descriptor is the last */
#define MEC17XX_QMSPI_CTRL_NEXT_DESCR_BITPOS 12
#define MEC17XX_QMSPI_CTRL_NEXT_DESCR_MASK0 0xFul
#define MEC17XX_QMSPI_CTRL_NEXT_DESCR_MASK (MEC17XX_QMSPI_CTRL_NEXT_DESCR_MASK0 << 12)
#define MEC17XX_QMSPI_CTRL_DESCR_LAST   (1ul << 16)
/*
 * Total transfer length is the count in this field
 * scaled by units in MEC17XX_QMSPI_CTRL_XFRU_xxxx
 */
#define MEC17XX_QMSPI_CTRL_NUM_UNITS_BITPOS (17)
#define MEC17XX_QMSPI_CTRL_NUM_UNITS_MASK   (0x7FFFul << 17)

/* Bits in MEC17XX_QMSPI0_EXE */
#define MEC17XX_QMSPI_EXE_START         (1 << 0)
#define MEC17XX_QMSPI_EXE_STOP          (1 << 1)
#define MEC17XX_QMSPI_EXE_CLR_FIFOS     (1 << 2)

/* MEC17XX QMSPI FIFO Sizes */
#define MEC17XX_QMSPI_TX_FIFO_LEN	8
#define MEC17XX_QMSPI_RX_FIFO_LEN	8

/* Bits in MEC17XX_QMSPI0_STS and MEC17XX_QMSPI0_IEN */
#define MEC17XX_QMSPI_STS_DONE          (1ul << 0)
#define MEC17XX_QMSPI_STS_DMA_DONE      (1ul << 1)
#define MEC17XX_QMSPI_STS_TX_BUFF_ERR   (1ul << 2)
#define MEC17XX_QMSPI_STS_RX_BUFF_ERR   (1ul << 3)
#define MEC17XX_QMSPI_STS_PROG_ERR      (1ul << 4)
#define MEC17XX_QMSPI_STS_TX_BUFF_FULL  (1ul << 8)
#define MEC17XX_QMSPI_STS_TX_BUFF_EMPTY (1ul << 9)
#define MEC17XX_QMSPI_STS_TX_BUFF_REQ   (1ul << 10)
#define MEC17XX_QMSPI_STS_TX_BUFF_STALL (1ul << 11) /* status only */
#define MEC17XX_QMSPI_STS_RX_BUFF_FULL  (1ul << 12)
#define MEC17XX_QMSPI_STS_RX_BUFF_EMPTY (1ul << 13)
#define MEC17XX_QMSPI_STS_RX_BUFF_REQ   (1ul << 14)
#define MEC17XX_QMSPI_STS_RX_BUFF_STALL (1ul << 15) /* status only */
#define MEC17XX_QMSPI_STS_ACTIVE        (1ul << 16) /* status only */

/* Bits in MEC17XX_QMSPI0_BUFCNT (read-only) */
#define MEC17XX_QMSPI_BUFCNT_TX_BITPOS  (0)
#define MEC17XX_QMSPI_BUFCNT_TX_MASK    (0xFFFFul)
#define MEC17XX_QMSPI_BUFCNT_RX_BITPOS  (16)
#define MEC17XX_QMSPI_BUFCNT_RX_MASK    (0xFFFFul << 16)

/* eSPI */

/* eSPI IO Component Base Address */
#define MEC17XX_ESPI_IO_BASE		0x400f3400

/* Peripheral Channel Registers */
#define MEC17XX_ESPI_PC_STATUS		REG32(MEC17XX_ESPI_IO_BASE + 0x114)
#define MEC17XX_ESPI_PC_IEN		REG32(MEC17XX_ESPI_IO_BASE + 0x118)
#define MEC17XX_ESPI_PC_BAR_INHIBIT_LO	REG32(MEC17XX_ESPI_IO_BASE + 0x120)
#define MEC17XX_ESPI_PC_BAR_INHIBIT_HI	REG32(MEC17XX_ESPI_IO_BASE + 0x124)
#define MEC17XX_ESPI_PC_BAR_INIT_LD_0C	REG16(MEC17XX_ESPI_IO_BASE + 0x128)
#define MEC17XX_ESPI_PC_EC_IRQ		REG8(MEC17XX_ESPI_IO_BASE + 0x12C)

/* LTR Registers */
#define MEC17XX_ESPI_IO_LTR_STATUS	REG16(MEC17XX_ESPI_IO_BASE + 0x220)
#define MEC17XX_ESPI_IO_LTR_IEN		REG8(MEC17XX_ESPI_IO_BASE + 0x224)
#define MEC17XX_ESPI_IO_LTR_CTRL	REG16(MEC17XX_ESPI_IO_BASE + 0x228)
#define MEC17XX_ESPI_IO_LTR_MSG		REG16(MEC17XX_ESPI_IO_BASE + 0x22C)

/* OOB Channel Registers */
#define MEC17XX_ESPI_OOB_RX_ADDR_LO	REG32(MEC17XX_ESPI_IO_BASE + 0x240)
#define MEC17XX_ESPI_OOB_RX_ADDR_HI	REG32(MEC17XX_ESPI_IO_BASE + 0x244)
#define MEC17XX_ESPI_OOB_TX_ADDR_LO	REG32(MEC17XX_ESPI_IO_BASE + 0x248)
#define MEC17XX_ESPI_OOB_TX_ADDR_HI	REG32(MEC17XX_ESPI_IO_BASE + 0x24C)
#define MEC17XX_ESPI_OOB_RX_LEN		REG32(MEC17XX_ESPI_IO_BASE + 0x250)
#define MEC17XX_ESPI_OOB_TX_LEN		REG32(MEC17XX_ESPI_IO_BASE + 0x254)
#define MEC17XX_ESPI_OOB_RX_CTL		REG32(MEC17XX_ESPI_IO_BASE + 0x258)
#define MEC17XX_ESPI_OOB_RX_IEN		REG8(MEC17XX_ESPI_IO_BASE + 0x25C)
#define MEC17XX_ESPI_OOB_RX_STATUS	REG32(MEC17XX_ESPI_IO_BASE + 0x260)
#define MEC17XX_ESPI_OOB_TX_CTL		REG32(MEC17XX_ESPI_IO_BASE + 0x264)
#define MEC17XX_ESPI_OOB_TX_IEN		REG8(MEC17XX_ESPI_IO_BASE + 0x268)
#define MEC17XX_ESPI_OOB_TX_STATUS	REG32(MEC17XX_ESPI_IO_BASE + 0x26C)

/* Flash Channel Registers */
#define MEC17XX_ESPI_FC_ADDR_LO		REG32(MEC17XX_ESPI_IO_BASE + 0x280)
#define MEC17XX_ESPI_FC_ADDR_HI		REG32(MEC17XX_ESPI_IO_BASE + 0x284)
#define MEC17XX_ESPI_FC_BUF_ADDR_LO	REG32(MEC17XX_ESPI_IO_BASE + 0x288)
#define MEC17XX_ESPI_FC_BUF_ADDR_HI	REG32(MEC17XX_ESPI_IO_BASE + 0x28C)
#define MEC17XX_ESPI_FC_XFR_LEN		REG32(MEC17XX_ESPI_IO_BASE + 0x290)
#define MEC17XX_ESPI_FC_CTL		REG32(MEC17XX_ESPI_IO_BASE + 0x294)
#define MEC17XX_ESPI_FC_IEN		REG8(MEC17XX_ESPI_IO_BASE + 0x298)
#define MEC17XX_ESPI_FC_CONFIG		REG32(MEC17XX_ESPI_IO_BASE + 0x29C)
#define MEC17XX_ESPI_FC_STATUS		REG32(MEC17XX_ESPI_IO_BASE + 0x2A0)

/* VWire Channel Registers */
#define MEC17XX_ESPI_VW_STATUS		REG8(MEC17XX_ESPI_IO_BASE + 0x2B0)

/* Global Registers */
/* 32-bit register containing CAP_ID/CAP0/CAP1/PC_CAP */
#define MEC17XX_ESPI_IO_REG32_A		REG32(MEC17XX_ESPI_IO_BASE + 0x2E0)
#define MEC17XX_ESPI_IO_CAP_ID		REG8(MEC17XX_ESPI_IO_BASE + 0x2E0)
#define MEC17XX_ESPI_IO_CAP0		REG8(MEC17XX_ESPI_IO_BASE + 0x2E1)
#define MEC17XX_ESPI_IO_CAP1		REG8(MEC17XX_ESPI_IO_BASE + 0x2E2)
#define MEC17XX_ESPI_IO_PC_CAP		REG8(MEC17XX_ESPI_IO_BASE + 0x2E3)
/* 32-bit register containing VW_CAP/OOB_CAP/FC_CAP/PC_READY */
#define MEC17XX_ESPI_IO_REG32_B		REG32(MEC17XX_ESPI_IO_BASE + 0x2E4)
#define MEC17XX_ESPI_IO_VW_CAP		REG8(MEC17XX_ESPI_IO_BASE + 0x2E4)
#define MEC17XX_ESPI_IO_OOB_CAP		REG8(MEC17XX_ESPI_IO_BASE + 0x2E5)
#define MEC17XX_ESPI_IO_FC_CAP		REG8(MEC17XX_ESPI_IO_BASE + 0x2E6)
#define MEC17XX_ESPI_IO_PC_READY	REG8(MEC17XX_ESPI_IO_BASE + 0x2E7)
/* 32-bit register containing OOB_READY/FC_READY/RESET_STATUS/RESET_IEN */
#define MEC17XX_ESPI_IO_REG32_C		REG32(MEC17XX_ESPI_IO_BASE + 0x2E8)
#define MEC17XX_ESPI_IO_OOB_READY	REG8(MEC17XX_ESPI_IO_BASE + 0x2E8)
#define MEC17XX_ESPI_IO_FC_READY	REG8(MEC17XX_ESPI_IO_BASE + 0x2E9)
#define MEC17XX_ESPI_IO_RESET_STATUS	REG8(MEC17XX_ESPI_IO_BASE + 0x2EA)
#define MEC17XX_ESPI_IO_RESET_IEN	REG8(MEC17XX_ESPI_IO_BASE + 0x2EB)
/* 32-bit register containing PLTRST_SRC/VW_READY */
#define MEC17XX_ESPI_IO_REG32_D		REG32(MEC17XX_ESPI_IO_BASE + 0x2EC)
#define MEC17XX_ESPI_IO_PLTRST_SRC	REG8(MEC17XX_ESPI_IO_BASE + 0x2EC)
#define MEC17XX_ESPI_IO_VW_READY	REG8(MEC17XX_ESPI_IO_BASE + 0x2ED)


/* Bits in MEC17XX_ESPI_IO_CAP0 */
#define MEC17XX_ESPI_CAP0_PC_SUPP	0x01
#define MEC17XX_ESPI_CAP0_VW_SUPP	0x02
#define MEC17XX_ESPI_CAP0_OOB_SUPP	0x04
#define MEC17XX_ESPI_CAP0_FC_SUPP	0x08
#define MEC17XX_ESPI_CAP0_ALL_CHAN_SUPP	(MEC17XX_ESPI_CAP0_PC_SUPP |\
					       MEC17XX_ESPI_CAP0_VW_SUPP |\
					       MEC17XX_ESPI_CAP0_OOB_SUPP |\
					       MEC17XX_ESPI_CAP0_FC_SUPP)

/* Bits in MEC17XX_ESPI_IO_CAP1 */
#define MEC17XX_ESPI_CAP1_RW_MASK		0x37
#define MEC17XX_ESPI_CAP1_MAX_FREQ_MASK		0x07
#define MEC17XX_ESPI_CAP1_MAX_FREQ_20M		0x00
#define MEC17XX_ESPI_CAP1_MAX_FREQ_25M		0x01
#define MEC17XX_ESPI_CAP1_MAX_FREQ_33M		0x02
#define MEC17XX_ESPI_CAP1_MAX_FREQ_50M		0x03
#define MEC17XX_ESPI_CAP1_MAX_FREQ_66M		0x04
#define MEC17XX_ESPI_CAP1_IO_BITPOS		4
#define MEC17XX_ESPI_CAP1_IO_MASK0		0x03
#define MEC17XX_ESPI_CAP1_IO_MASK		(0x03ul << 4)
#define MEC17XX_ESPI_CAP1_IO1_VAL		0x00
#define MEC17XX_ESPI_CAP1_IO12_VAL		0x01
#define MEC17XX_ESPI_CAP1_IO24_VAL		0x02
#define MEC17XX_ESPI_CAP1_IO124_VAL		0x03
#define MEC17XX_ESPI_CAP1_IO1			(0x00 << 4)
#define MEC17XX_ESPI_CAP1_IO12			(0x01 << 4)
#define MEC17XX_ESPI_CAP1_IO24			(0x02 << 4)
#define MEC17XX_ESPI_CAP1_IO124			(0x03 << 4)


/* Bits in MEC17XX_ESPI_IO_RESET_STATUS and MEC17XX_ESPI_IO_RESET_IEN */
#define MEC17XX_ESPI_RST_PIN_MASK	0x02
#define MEC17XX_ESPI_RST_CHG_STS	1
#define MEC17XX_ESPI_RST_IEN		1


/* Bits in MEC17XX_ESPI_IO_PLTRST_SRC */
#define MEC17XX_ESPI_PLTRST_SRC_VW		0
#define MEC17XX_ESPI_PLTRST_SRC_PIN		1


/* eSPI Slave Activate Register
 * bit[0] = 0 de-active block is clock-gates
 * bit[0] = 1 block is powered and functional
 * */
#define MEC17XX_ESPI_ACTIVATE		REG8(MEC17XX_ESPI_IO_BASE + 0x330)


/* IO BAR's starting at offset 0x134
 * b[16]=virtualized R/W
 * b[15:14]=0 reserved RO
 * b[13:8]=Logical Device Number RO
 * b[7:0]=mask
 */
#define MEC17XX_ESPI_IO_BAR_CTL(x)	REG32(MEC17XX_ESPI_IO_BASE + 0x134 + ((x) << 2))
/* access mask field of eSPI IO BAR Control register */
#define MEC17XX_ESPI_IO_BAR_CTL_MASK(x)	REG8(MEC17XX_ESPI_IO_BASE + 0x134 + ((x) << 2))

/* IO BAR's starting at offset 0x334
 * b[31:16] = I/O address
 * b[15:1]=0 reserved
 * b[0] = valid
 */
#define MEC17XX_ESPI_IO_BAR(x)	REG32(MEC17XX_ESPI_IO_BASE + 0x334 + ((x) << 2))

#define MEC17XX_ESPI_IO_BAR_VALID(x)	REG8(MEC17XX_ESPI_IO_BASE + 0x334 + ((x) << 2) + 0)
#define MEC17XX_ESPI_IO_BAR_ADDR_LSB(x)	REG8(MEC17XX_ESPI_IO_BASE + 0x334 + ((x) << 2) + 2)
#define MEC17XX_ESPI_IO_BAR_ADDR_MSB(x)	REG8(MEC17XX_ESPI_IO_BASE + 0x334 + ((x) << 2) + 3)
#define MEC17XX_ESPI_IO_BAR_ADDR(x)	REG16(MEC17XX_ESPI_IO_BASE + 0x334 + ((x) << 2) + 2)

/* Indices for use in above macros */
#define MEC17XX_ESPI_IO_BAR_ID_CFG_PORT		0
#define MEC17XX_ESPI_IO_BAR_ID_MEM_CMPNT	1
#define MEC17XX_ESPI_IO_BAR_ID_MAILBOX		2
#define MEC17XX_ESPI_IO_BAR_ID_8042		3
#define MEC17XX_ESPI_IO_BAR_ID_ACPI_EC0		4
#define MEC17XX_ESPI_IO_BAR_ID_ACPI_EC1		5
#define MEC17XX_ESPI_IO_BAR_ID_ACPI_EC2		6
#define MEC17XX_ESPI_IO_BAR_ID_ACPI_EC3		7
#define MEC17XX_ESPI_IO_BAR_ID_ACPI_EC4		8
#define MEC17XX_ESPI_IO_BAR_ID_ACPI_PM1		9
#define MEC17XX_ESPI_IO_BAR_ID_P92		0xA
#define MEC17XX_ESPI_IO_BAR_ID_UART0		0xB
#define MEC17XX_ESPI_IO_BAR_ID_UART1		0xC
#define MEC17XX_ESPI_IO_BAR_ID_EMI0		0xD
#define MEC17XX_ESPI_IO_BAR_ID_EMI1		0xE
#define MEC17XX_ESPI_IO_BAR_ID_EMI		0xF
#define MEC17XX_ESPI_IO_BAR_P80_0		0x10
#define MEC17XX_ESPI_IO_BAR_P80_1		0x11
#define MEC17XX_ESPI_IO_BAR_RTC			0x12

/* eSPI Serial IRQ registers */
#define MEC17XX_ESPI_IO_SERIRQ_REG(x)	REG8(MEC17XX_ESPI_IO_BASE + 0x3ac + (x))
#define MEC17XX_ESPI_MBOX_SIRQ0		0
#define MEC17XX_ESPI_MBOX_SIRQ1		1
#define MEC17XX_ESPI_8042_SIRQ0		2
#define MEC17XX_ESPI_8042_SIRQ1		3
#define MEC17XX_ESPI_ACPI_EC0_SIRQ	4
#define MEC17XX_ESPI_ACPI_EC1_SIRQ	5
#define MEC17XX_ESPI_ACPI_EC2_SIRQ	6
#define MEC17XX_ESPI_ACPI_EC3_SIRQ	7
#define MEC17XX_ESPI_ACPI_EC4_SIRQ	8
#define MEC17XX_ESPI_UART0_SIRQ		9
#define MEC17XX_ESPI_UART1_SIRQ		10
#define MEC17XX_ESPI_EMI0_SIRQ0		11
#define MEC17XX_ESPI_EMI0_SIRQ1		12
#define MEC17XX_ESPI_EMI1_SIRQ0		13
#define MEC17XX_ESPI_EMI1_SIRQ1		14
#define MEC17XX_ESPI_EMI2_SIRQ0		15
#define MEC17XX_ESPI_EMI2_SIRQ1		16
#define MEC17XX_ESPI_RTC_SIRQ		17
#define MEC17XX_ESPI_EC_SIRQ		18

/* eSPI Virtual Wire Error Register */
#define MEC17XX_ESPI_IO_VW_ERROR	REG8(MEC17XX_ESPI_IO_BASE + 0x3f0)


/* eSPI Memory Component Base Address */
#define MEC17XX_ESPI_MEM_BASE		0x400f3800

/* eSPI Logical Device Memory Host BAR's
 * Each Logical Device implementing memory access has an 80-bit register.
 * b[0]=Valid
 * b[15:1]=0(reserved)
 * b[79:16]=eSPI bus memory address(Host address space)
 */
#define MEC17XX_ESPI_MBAR_MBOX_ID	0
#define MEC17XX_ESPI_MBAR_ACPI_EC0_ID	1
#define MEC17XX_ESPI_MBAR_ACPI_EC1_ID	2
#define MEC17XX_ESPI_MBAR_ACPI_EC2_ID	3
#define MEC17XX_ESPI_MBAR_ACPI_EC3_ID	4
#define MEC17XX_ESPI_MBAR_ACPI_EC4_ID	5
#define MEC17XX_ESPI_MBAR_EMI0_ID	6
#define MEC17XX_ESPI_MBAR_EMI1_ID	7
#define MEC17XX_ESPI_MBAR_EMI2_ID	8

#define MEC17XX_ESPI_MBAR_VALID(x)	REG8(MEC17XX_ESPI_MEM_BASE + 0x130 + ((x) << 3) + ((x) << 1))
#define MEC17XX_ESPI_MBAR_HOST_ADDR_0_15(x)	REG16(MEC17XX_ESPI_MEM_BASE + 0x132 + ((x) << 3) + ((x) << 1))
#define MEC17XX_ESPI_MBAR_HOST_ADDR_16_31(x)	REG16(MEC17XX_ESPI_MEM_BASE + 0x134 + ((x) << 3) + ((x) << 1))
#define MEC17XX_ESPI_MBAR_HOST_ADDR_32_47(x)	REG16(MEC17XX_ESPI_MEM_BASE + 0x136 + ((x) << 3) + ((x) << 1))
#define MEC17XX_ESPI_MBAR_HOST_ADDR_48_63(x)	REG16(MEC17XX_ESPI_MEM_BASE + 0x138 + ((x) << 3) + ((x) << 1))

/* eSPI SRAM BAR's
 * b[0,3,8:15] = 0 reserved
 * b[2:1] = access
 * b[7:4] = size
 * b[79:16] = Host address
 */
#define MEC17XX_ESPI_SRAM_BAR_CFG(x)	REG8(MEC17XX_ESPI_MEM_BASE + 0x1ac + ((x) << 3) + ((x) << 1))
#define MEC17XX_ESPI_SRAM_BAR_ADDR_0_15(x)	REG16(MEC17XX_ESPI_MEM_BASE + 0x1ae + ((x) << 3) + ((x) << 1))
#define MEC17XX_ESPI_SRAM_BAR_ADDR_16_31(x)	REG16(MEC17XX_ESPI_MEM_BASE + 0x1b0 + ((x) << 3) + ((x) << 1))
#define MEC17XX_ESPI_SRAM_BAR_ADDR_32_47(x)	REG16(MEC17XX_ESPI_MEM_BASE + 0x1b2 + ((x) << 3) + ((x) << 1))
#define MEC17XX_ESPI_SRAM_BAR_ADDR_48_63(x)	REG16(MEC17XX_ESPI_MEM_BASE + 0x1b4 + ((x) << 3) + ((x) << 1))

/* eSPI Memory Bus Master Registers */
#define MEC17XX_ESPI_BM_STATUS		REG32(MEC17XX_ESPI_MEM_BASE + 0x200)
#define MEC17XX_ESPI_BM_IEN		REG32(MEC17XX_ESPI_MEM_BASE + 0x204)
#define MEC17XX_ESPI_BM_CONFIG		REG32(MEC17XX_ESPI_MEM_BASE + 0x208)
#define MEC17XX_ESPI_BM1_CTL		REG32(MEC17XX_ESPI_MEM_BASE + 0x210)
#define MEC17XX_ESPI_BM1_HOST_ADDR_LO	REG32(MEC17XX_ESPI_MEM_BASE + 0x214)
#define MEC17XX_ESPI_BM1_HOST_ADDR_HI	REG32(MEC17XX_ESPI_MEM_BASE + 0x218)
#define MEC17XX_ESPI_BM1_EC_ADDR	REG32(MEC17XX_ESPI_MEM_BASE + 0x21c)
#define MEC17XX_ESPI_BM2_CTL		REG32(MEC17XX_ESPI_MEM_BASE + 0x224)
#define MEC17XX_ESPI_BM2_HOST_ADDR_LO	REG32(MEC17XX_ESPI_MEM_BASE + 0x228)
#define MEC17XX_ESPI_BM2_HOST_ADDR_HI	REG32(MEC17XX_ESPI_MEM_BASE + 0x22c)
#define MEC17XX_ESPI_BM2_EC_ADDR	REG32(MEC17XX_ESPI_MEM_BASE + 0x230)

/* eSPI Memory Logical Address EC BAR's
 * b[0] = Valid
 * b[2:1] = access
 * b[3] = 0 reserved
 * b[7:4] = size
 * b[15:8] = 0 reserved
 * b[47:16] = EC SRAM Address where Host address is mapped
 * b[79:48] = 0 reserved
 */
/* start at offset 0x330 */
#define MEC17XX_ESPI_MBAR_EC_VSIZE(x)	REG32(MEC17XX_ESPI_MEM_BASE + 0x330 + ((x) << 3) + ((x) << 1))
#define MEC17XX_ESPI_MBAR_EC_ADDR_0_15(x)	REG16(MEC17XX_ESPI_MEM_BASE + 0x332 + ((x) << 3) + ((x) << 1))
#define MEC17XX_ESPI_MBAR_EC_ADDR_16_31(x)	REG16(MEC17XX_ESPI_MEM_BASE + 0x334 + ((x) << 3) + ((x) << 1))
#define MEC17XX_ESPI_MBAR_EC_ADDR_32_47(x)	REG16(MEC17XX_ESPI_MEM_BASE + 0x336 + ((x) << 3) + ((x) << 1))



/* eSPI Virtual Wire Component Base Address */
#define MEC17XX_ESPI_VW_BASE		0x400f9c00

#define MEC17XX_ESPI_MSVW_BASE	(MEC17XX_ESPI_VW_BASE)
#define MEC17XX_ESPI_SMVW_BASE	((MEC17XX_ESPI_VW_BASE) + 0x200ul)

#define MEC17XX_ESPI_MSVW_LEN	12
#define MEC17XX_ESPI_SMVW_LEN	8

#define MEC17XX_ESPI_MSVW_ADDR(n)	((MEC17XX_ESPI_MSVW_BASE) + (n) * (MEC17XX_ESPI_MSVW_LEN))

#define MEC17XX_ESPI_MSVW_MTOS_BITPOS			4

#define MEC17XX_ESPI_MSVW_IRQSEL_LEVEL_LO		0
#define MEC17XX_ESPI_MSVW_IRQSEL_LEVEL_HI		1
#define MEC17XX_ESPI_MSVW_IRQSEL_DISABLED		4
#define MEC17XX_ESPI_MSVW_IRQSEL_RISING			0x0d
#define MEC17XX_ESPI_MSVW_IRQSEL_FALLING		0x0e
#define MEC17XX_ESPI_MSVW_IRQSEL_BOTH_EDGES		0x0f



/* Indices of Master to Slave Virtual Wire registers.
 * Registers are 96-bit.
 * MSVW_xy where xy = PCH VWire number.
 * Each PCH VWire number controls 4 virtual wires
 */
#define MSVW_I02	0
#define MSVW_I03	1
#define MSVW_I07	2
#define MSVW_I41	3
#define MSVW_I42	4
#define MSVW_I43	5
#define MSVW_I44	6
#define MSVW_I47	7
#define MSVW_I4A	8
#define MSVW_SPARE0	9
#define MSVW_SPARE1	10
#define MSVW_MAX	11

struct mec17xx_espi_msvw {
	union {
		uint32_t w0;
		uint8_t  w0b[4];
		struct {
			uint8_t  index;		/* eSPI Index value sent by eSPI master */
			uint8_t  mtos_src;	/* reset event */
			uint8_t  mtos_state;	/* b[7:4] loaded into SRC[0:3] on reset event */
			uint8_t  w0b3_rsvd;	/* reserved, read-only */
		};
	};
	union {
		uint32_t w1;
		uint8_t  src_irq_sel[4];
		struct {
			uint8_t  src0_irq_sel;	/* b[0:3]=IRQSel, b[7:4]=reserved, read-only */
			uint8_t  src1_irq_sel;
			uint8_t  src2_irq_sel;
			uint8_t  src3_irq_sel;
		};
	};
	union {
		uint32_t w2;
		uint8_t  src[4];
		struct {
			uint8_t  src0;	/* b[0]=SRC0, b[7:1]=reserved, read-only */
			uint8_t	 src1;	/* b[0]=SRC1, b[7:1]=reserved, read-only */
			uint8_t  src2;	/* b[0]=SRC2, b[7:1]=reserved, read-only */
			uint8_t  src3;	/* b[0]=SRC3, b[7:1]=reserved, read-only */
		};
	};
};

struct mec17xx_espi_msvws {
	struct mec17xx_espi_msvw msvw[MSVW_MAX];
};

#define MSVW00 *(volatile struct mec17xx_espi_msvw*)(MEC17XX_ESPI_MSVW_BASE)
#define MSVW01 *(volatile struct mec17xx_espi_msvw*)((MEC17XX_ESPI_MSVW_BASE) + (MEC17XX_ESPI_MSVW_LEN))
#define MSVW02 *(volatile struct mec17xx_espi_msvw*)((MEC17XX_ESPI_MSVW_BASE) + 2 * (MEC17XX_ESPI_MSVW_LEN))

#define MSVW(n) *(volatile struct mec17xx_espi_msvw*)((MEC17XX_ESPI_MSVW_BASE) + ((n) << 3) + ((n) << 2))


/* Access index value in byte 0 */
#define MEC17XX_ESPI_VW_M2S_INDEX(id)	REG8(MEC17XX_ESPI_VW_BASE +\
					     ((id) << 3) + ((id) << 2))

/* Access MTOS_SOURCE and MTOS_STATE in byte 1
 * MTOS_SOURCE = b[1:0] specifies reset source
 * MTOS_STATE = b[7:4] are states loaded into SRC[0:3] on reset event
 */
#define MEC17XX_ESPI_VW_M2S_MTOS(id)	REG8(MEC17XX_ESPI_VW_BASE + 1 +\
					     ((id) << 3) +\
					     ((id) << 2))

/* Access Index, MTOS Source, and MTOS State
 * Index in b[7:0], MTOS Source in b[9:8], MTOS State in b[15:12] */
#define MEC17XX_ESPI_VW_M2S_INDEX_MTOS(id)	REG16(MEC17XX_ESPI_VW_BASE +\
						      ((id) << 3) +\
						      ((id) << 2))
/* Access SRCn IRQ Select bit fields */
#define MEC17XX_ESPI_VW_M2S_IRQSEL00(id)	(REG8(MEC17XX_ESPI_VW_BASE + 4 +\
					     ((id) << 3) + ((id) << 2)))
#define MEC17XX_ESPI_VW_M2S_IRQSEL1(id)	(REG8(MEC17XX_ESPI_VW_BASE + 5 +\
					     ((id) << 3) + ((id) << 2)))
#define MEC17XX_ESPI_VW_M2S_IRQSEL2(id)	(REG8(MEC17XX_ESPI_VW_BASE + 6 +\
					     ((id) << 3) + ((id) << 2)))
#define MEC17XX_ESPI_VW_M2S_IRQSEL3(id)	(REG8(MEC17XX_ESPI_VW_BASE + 7 +\
					     ((id) << 3) + ((id) << 2)))


/* Access individual source bits */
#define MEC17XX_ESPI_VW_M2S_SRC0(id)	(REG8(MEC17XX_ESPI_VW_BASE + 8 +\
					     ((id) << 3) + ((id) << 2)) & 0x01)
#define MEC17XX_ESPI_VW_M2S_SRC1(id)	(REG8(MEC17XX_ESPI_VW_BASE + 9 +\
					     ((id) << 3) + ((id) << 2)) & 0x01)
#define MEC17XX_ESPI_VW_M2S_SRC2(id)	(REG8(MEC17XX_ESPI_VW_BASE + 10 +\
					     ((id) << 3) + ((id) << 2)) & 0x01)
#define MEC17XX_ESPI_VW_M2S_SRC3(id)	(REG8(MEC17XX_ESPI_VW_BASE + 11 +\
					     ((id) << 3) + ((id) << 2)) & 0x01)

/* Access all four Source bits as 32-bit value, Source bits must be
 * at bits[0,8,16,24] of 32-bit word */
#define MEC17XX_ESPI_VW_M2S_SRC_ALL(id)	REG32(MEC17XX_ESPI_VW_BASE + 8 +\
					       ((id) << 3) + ((id) << 2))

/* Write all four Source bits and let macro assemble 32-bit word */
#define MEC17XX_ESPI_VW_M2S_SRC(id,s0,s1,s2,s3)	REG32(MEC17XX_ESPI_VW_BASE +\
						      8 + ((id) << 3) +\
						      ((id) << 2)) =\
							((uint32_t)(s0) +\
							 (uint32_t)(s1)<<8 +\
							 (uint32_t)(s2)<<16 +\
							 (uint32_t)(s3)<<24)


/* Indices of Slave to Master Virtual Wire registers.
 * Registers are 64-bit.
 * SMVW_xy where xy = PCH VWire number.
 * Each PCH VWire number controls 4 virtual wires */
#define SMVW_I04	0
#define SMVW_I05	1
#define SMVW_I06	2
#define SMVW_I40	3
#define SMVW_I45	4
#define SMVW_I46	5
#define SMVW_SPARE0	6
#define SMVW_SPARE1	7
#define SMVW_SPARE2	8
#define SMVW_SPARE3	9
#define SMVW_SPARE4	10
#define SMVW_MAX	11

struct mec17xx_espi_smvw {
	union {
		uint32_t w0;
		uint8_t  b[4];
		struct {
			uint8_t  index;		/* eSPI Index value sent by eSPI master */
			uint8_t  stom_src;	/* reset event */
			uint8_t  stom_state;	/* b[7:4] loaded into SRC[0:3] on reset event */
			uint8_t  change;	/* b[3:0] = change bits for SRC[0:3] */
		};
	};
	union {
		uint32_t w1;
		uint8_t  src[4];
		struct {
			uint8_t  src0;	/* b[0]=SRC0, b[7:1]=reserved, read-only */
			uint8_t  src1;	/* b[0]=SRC1, b[7:1]=reserved, read-only */
			uint8_t  src2;	/* b[0]=SRC2, b[7:1]=reserved, read-only */
			uint8_t  src3;	/* b[0]=SRC3, b[7:1]=reserved, read-only */
		};
	};
};

struct mec17xx_espi_smvws {
	struct mec17xx_espi_smvw smvw[SMVW_MAX];
};

/* Access Index in b[7:0] of byte 0 */
#define MEC17XX_ESPI_VW_S2M_INDEX(id)	REG8(MEC17XX_ESPI_VW_BASE + 0x200 +\
					     ((id) << 3)))

/* Access STOM_SOURCE and STOM_STATE in byte 1
 * STOM_SOURCE = b[1:0]
 * STOM_STATE = b[7:4]
 */
#define MEC17XX_ESPI_VW_S2M_STOM(id)	REG8(MEC17XX_ESPI_VW_BASE +\
						     0x201 + ((id) << 3)))

/* Access Index, STOM_SOURCE, and STOM_STATE in bytes[1:0]
 * Index = b[7:0]
 * STOM_SOURCE = b[9:8]
 * STOM_STATE = [15:12]
 */
#define MEC17XX_ESPI_VW_S2M_INDEX_STOM(id)	REG16(MEC17XX_ESPI_VW_BASE +\
						      0x200 + ((id) << 3)))

/* Access Change[0:3] RO bits. Set to 1 if any of SRC[0:3] change */
#define MEC17XX_ESPI_VW_S2M_CHANGE(id)	REG8(MEC17XX_ESPI_VW_BASE + 0x202 +\
					      ((id) << 3)))

/* Access individual SRC bits
 * bit[0] = SRCn
 */
#define MEC17XX_ESPI_VW_S2M_SRC0(id) (REG8(MEC17XX_ESPI_VW_BASE + 0x204 +\
					  ((id) << 3))) & 0x01)

#define MEC17XX_ESPI_VW_S2M_SRC1(id) REG8(MEC17XX_ESPI_VW_BASE + 0x205 +\
					  ((id) << 3)))

#define MEC17XX_ESPI_VW_S2M_SRC2(id) REG8(MEC17XX_ESPI_VW_BASE + 0x206 +\
					  ((id) << 3)))

#define MEC17XX_ESPI_VW_S2M_SRC3(id) REG8(MEC17XX_ESPI_VW_BASE + 0x206 +\
					  ((id) << 3)))

/*
 * Access specified source bit as byte read/write.
 * Source bit is in bit[0] of byte.
 */
#define MEC17XX_ESPI_VW_S2M_SRC(id,src) REG8(MEC17XX_ESPI_VW_BASE + 0x204 +\
					  ((id) << 3) + ((src) & 0x03))


/* Access SRC[0:3] as 32-bit word
 * SRC0 = b[0]
 * SRC1 = b[8]
 * SRC2 = b[16]
 * SRC3 = b[24]
 */
#define MEC17XX_ESPI_VW_S2M_SRC_ALL(id,w)	(REG32(MEC17XX_ESPI_VW_BASE + 0x204 +\
							((id) << 3))) = w)

/* Write all four Source bits and let macro assemble 32-bit word */
#define MEC17XX_ESPI_VW_S2M_SRCS(id,s0,s1,s2,s3) \
	REG32(MEC17XX_ESPI_VW_BASE +\
		0x200 + ((id) << 3)) =\
		((uint32_t)(s0) +\
		 (uint32_t)(s1)<<8 +\
		 (uint32_t)(s2)<<16 +\
		 (uint32_t)(s3)<<24)


/*
 * eSPI RESET, channel enables and operations except Master-to-Slave
 * WWires are all on GIRQ19
 */
#define MEC17XX_ESPI_GIRQ		19
#define MEC17XX_ESPI_PC_GIRQ_BIT	(1ul << 0)
#define MEC17XX_ESPI_BM1_GIRQ_BIT	(1ul << 1)
#define MEC17XX_ESPI_BM2_GIRQ_BIT	(1ul << 2)
#define MEC17XX_ESPI_LTR_GIRQ_BIT	(1ul << 3)
#define MEC17XX_ESPI_OOB_TX_GIRQ_BIT	(1ul << 4)
#define MEC17XX_ESPI_OOB_RX_GIRQ_BIT	(1ul << 5)
#define MEC17XX_ESPI_FC_GIRQ_BIT	(1ul << 6)
#define MEC17XX_ESPI_RESET_GIRQ_BIT	(1ul << 7)
#define MEC17XX_ESPI_VW_EN_GIRQ_BIT	(1ul << 8)

/*
 * eSPI Master-to-Slave WWire interrupts are on GIRQ24 and GIRQ25
 */
#define MEC17XX_ESPI_MSVW_0_6_GIRQ	24
#define MEC17XX_ESPI_MSVW_7_10_GIRQ	25
/*
 * Four source bits, SRC[0:3] per Master-to-Slave register
 * v = MSVW [0:10]
 * n = VWire SRC bit = [0:3]
 */
#define MEC17XX_ESPI_MSVW_GIRQ(v)		(24 + ((v) > 6 ? 1 : 0))
#define MEC17XX_ESPI_MSVW_SRC_GIRQ_BIT(v,n)	((v) > 6 ? (1ul << (((v)-7)+(n)) : (1ul << ((v)+(n))))



/* DMA */
#define MEC17XX_DMA_BASE            0x40002400

/*
 * Available DMA channels.
 *
 * On MEC17XX, any DMA channel may serve any device. Since we have
 * 14 channels and 14 devices, we make each channel dedicated to the
 * device of the same number.
 */
enum dma_channel {
	/* Channel numbers */
	MEC17XX_DMAC_I2C0_SLAVE =  0,
	MEC17XX_DMAC_I2C0_MASTER = 1,
	MEC17XX_DMAC_I2C1_SLAVE =  2,
	MEC17XX_DMAC_I2C1_MASTER = 3,
	MEC17XX_DMAC_I2C2_SLAVE =  4,
	MEC17XX_DMAC_I2C2_MASTER = 5,
	MEC17XX_DMAC_I2C3_SLAVE =  6,
	MEC17XX_DMAC_I2C3_MASTER = 7,
	MEC17XX_DMAC_SPI0_TX =     8,
	MEC17XX_DMAC_SPI0_RX =     9,
	MEC17XX_DMAC_SPI1_TX =    10,
	MEC17XX_DMAC_SPI1_RX =    11,
	MEC17XX_DMAC_QMSPI0_TX =  12,
	MEC17XX_DMAC_QMSPI0_RX =  13,
	/* Channel count */
	MEC17XX_DMAC_COUNT =      14,
};

/* Registers for a single channel of the DMA controller */
struct mec17xx_dma_chan {
	uint32_t act;         /* Activate */
	uint32_t mem_start;   /* Memory start address */
	uint32_t mem_end;     /* Memory end address */
	uint32_t dev;         /* Device address */
	uint32_t ctrl;        /* Control */
	uint32_t int_status;  /* Interrupt status */
	uint32_t int_enabled; /* Interrupt enabled */
	uint32_t pad[9];      /* 0x1C - 0x3F */
};

/* Always use mec17xx_dma_chan_t so volatile keyword is included! */
typedef volatile struct mec17xx_dma_chan mec17xx_dma_chan_t;

/* Common code and header file must use this */
typedef mec17xx_dma_chan_t dma_chan_t;

/* Registers for the DMA controller */
struct mec17xx_dma_regs {
	uint32_t ctrl;
	uint32_t data;
	uint32_t pad[14];
	mec17xx_dma_chan_t chan[MEC17XX_DMAC_COUNT];
};

/* Always use mec17xx_dma_regs_t so volatile keyword is included! */
typedef volatile struct mec17xx_dma_regs mec17xx_dma_regs_t;

#define MEC17XX_DMA_REGS ((mec17xx_dma_regs_t *)MEC17XX_DMA_BASE)

/* Bits for DMA channel regs */
#define MEC17XX_DMA_ACT_EN		(1 << 0)
#define MEC17XX_DMA_XFER_SIZE(x)	((x) << 20)
#define MEC17XX_DMA_INC_DEV		(1 << 17)
#define MEC17XX_DMA_INC_MEM		(1 << 16)
#define MEC17XX_DMA_DEV(x)		((x) << 9)
#define MEC17XX_DMA_TO_DEV		(1 << 8)
#define MEC17XX_DMA_DONE		(1 << 2)
#define MEC17XX_DMA_RUN			(1 << 0)


/* MEC17xx SHA HW accelerator */
#define MEC17XX_SHA_BASE		(0x4000d000)
#define MEC17XX_SHA_MODE		REG32(MEC17XX_SHA_BASE + 0x00)
#define MEC17XX_SHA_NBLOCK		REG32(MEC17XX_SHA_BASE + 0x04)
#define MEC17XX_SHA_CTRL		REG32(MEC17XX_SHA_BASE + 0x08)
#define MEC17XX_SHA_STS_RO		REG32(MEC17XX_SHA_BASE + 0x0c)
#define MEC17XX_SHA_INIT_ADDR		REG32(MEC17XX_SHA_BASE + 0x18)
#define MEC17XX_SHA_DATA_ADDR		REG32(MEC17XX_SHA_BASE + 0x1c)
#define MEC17XX_SHA_RESULT_ADDR		REG32(MEC17XX_SHA_BASE + 0x20)

/* MEC17XX_SHA_MODE bit definitions */
#define MEC17XX_SHA1_MODE		(1ul << 1)
#define MEC17XX_SHA256_MODE		(1ul << 3)
#define MEC17XX_SHA512_MODE		(1ul << 5)

/* MEC17XX_SHA_CTRL bit defintions */
/* Set start to 1, hardware clears start when done */
#define MEC17XX_SHA_CTRL_START		(1ul << 0)

/* MEC17XX_SHA_STS_RO bit definitions */
/* bit[0] = 0 No bus error, 1 bus error */
#define MEC17XX_SHA_STS_RO_ERROR	(1ul << 0)

/* MEC17XX_SHA GIRQ definitions */
#define MEC17XX_SHA_GIRQ	16
#define MEC17XX_SHA_GIRQ_BIT	(1ul << 4)

/* MEC17XX SHA PCR sleep bit defintions */
#define MEC17XX_SHA_PCR_SLP_EN_IDX	3
#define MEC17XX_SHA_PCR_SLP_EN_BITPOS	28


/* IRQ Numbers */
#define MEC17XX_IRQ_GIRQ8        0
#define MEC17XX_IRQ_GIRQ9        1
#define MEC17XX_IRQ_GIRQ10       2
#define MEC17XX_IRQ_GIRQ11       3
#define MEC17XX_IRQ_GIRQ12       4
#define MEC17XX_IRQ_GIRQ13       5
#define MEC17XX_IRQ_GIRQ14       6
#define MEC17XX_IRQ_GIRQ15       7
#define MEC17XX_IRQ_GIRQ16       8
#define MEC17XX_IRQ_GIRQ17       9
#define MEC17XX_IRQ_GIRQ18       10
#define MEC17XX_IRQ_GIRQ19       11
#define MEC17XX_IRQ_GIRQ20       12
#define MEC17XX_IRQ_GIRQ21       13
/* GIRQ22 is not connected to NVIC, it wakes peripheral
 * subsystem but not ARM core */
#define MEC17XX_IRQ_GIRQ23       14
#define MEC17XX_IRQ_GIRQ24       15
#define MEC17XX_IRQ_GIRQ25       16
#define MEC17XX_IRQ_GIRQ26       17
/* 18 - 19 Not connected */
#define MEC17XX_IRQ_I2C_0        20
#define MEC17XX_IRQ_I2C_1        21
#define MEC17XX_IRQ_I2C_2        22
#define MEC17XX_IRQ_I2C_3        23
#define MEC17XX_IRQ_DMA_0        24
#define MEC17XX_IRQ_DMA_1        25
#define MEC17XX_IRQ_DMA_2        26
#define MEC17XX_IRQ_DMA_3        27
#define MEC17XX_IRQ_DMA_4        28
#define MEC17XX_IRQ_DMA_5        29
#define MEC17XX_IRQ_DMA_6        30
#define MEC17XX_IRQ_DMA_7        31
#define MEC17XX_IRQ_DMA_8        32
#define MEC17XX_IRQ_DMA_9        33
#define MEC17XX_IRQ_DMA_10       34
#define MEC17XX_IRQ_DMA_11       35
#define MEC17XX_IRQ_DMA_12       36
#define MEC17XX_IRQ_DMA_13       37
/* 38 - 39 Not connected */
#define MEC17XX_IRQ_UART0        40
#define MEC17XX_IRQ_UART1        41
#define MEC17XX_IRQ_EMI0         42
#define MEC17XX_IRQ_EMI1         43
#define MEC17XX_IRQ_EMI2         44
#define MEC17XX_IRQ_ACPIEC0_IBF  45
#define MEC17XX_IRQ_ACPIEC0_OBE  46
#define MEC17XX_IRQ_ACPIEC1_IBF  47
#define MEC17XX_IRQ_ACPIEC1_OBE  48
#define MEC17XX_IRQ_ACPIEC2_IBF  49
#define MEC17XX_IRQ_ACPIEC2_OBE  50
#define MEC17XX_IRQ_ACPIEC3_IBF  51
#define MEC17XX_IRQ_ACPIEC3_OBE  52
#define MEC17XX_IRQ_ACPIEC4_IBF  53
#define MEC17XX_IRQ_ACPIEC4_OBE  54
#define MEC17XX_IRQ_ACPIPM1_CTL  55
#define MEC17XX_IRQ_ACPIPM1_EN   56
#define MEC17XX_IRQ_ACPIPM1_STS  57
#define MEC17XX_IRQ_8042EM_OBF   58
#define MEC17XX_IRQ_8042EM_IBF   59
#define MEC17XX_IRQ_MAILBOX_DATA 60
/* 61 Not connected */
#define MEC17XX_IRQ_PORT80DBG0   62
#define MEC17XX_IRQ_PORT80DBG1   63
/* 64 Not connected */
#define MEC17XX_IRQ_PKE_ERR      65
#define MEC17XX_IRQ_PKE_END      66
#define MEC17XX_IRQ_NDRNG        67
#define MEC17XX_IRQ_AES          68
#define MEC17XX_IRQ_HASH         69
#define MEC17XX_IRQ_PECI_HOST    70
#define MEC17XX_IRQ_TACH_0       71
#define MEC17XX_IRQ_TACH_1       72
#define MEC17XX_IRQ_TACH_2       73
#define MEC17XX_IRQ_FAN0_FAIL    74
#define MEC17XX_IRQ_FAN0_STALL   75
#define MEC17XX_IRQ_FAN1_FAIL    76
#define MEC17XX_IRQ_FAN1_STALL   77
#define MEC17XX_IRQ_ADC_SNGL     78
#define MEC17XX_IRQ_ADC_RPT      79
#define MEC17XX_IRQ_RCID0        80
#define MEC17XX_IRQ_RCID1        81
#define MEC17XX_IRQ_RCID2        82
#define MEC17XX_IRQ_LED0_WDT     83
#define MEC17XX_IRQ_LED1_WDT     84
#define MEC17XX_IRQ_LED2_WDT     85
#define MEC17XX_IRQ_LED3_WDT     86
#define MEC17XX_IRQ_PHOT         87
#define MEC17XX_IRQ_PWRGRD0      88
#define MEC17XX_IRQ_PWRGRD1      89
#define MEC17XX_IRQ_LPC          90
#define MEC17XX_IRQ_QMSPI0       91
#define MEC17XX_IRQ_SPI0_TX      92
#define MEC17XX_IRQ_SPI0_RX      93
#define MEC17XX_IRQ_SPI1_TX      94
#define MEC17XX_IRQ_SPI1_RX      95
#define MEC17XX_IRQ_BCM0_ERR     96
#define MEC17XX_IRQ_BCM0_BUSY    97
#define MEC17XX_IRQ_BCM1_ERR     98
#define MEC17XX_IRQ_BCM1_BUSY    99
#define MEC17XX_IRQ_PS2_0        100
#define MEC17XX_IRQ_PS2_1        101
#define MEC17XX_IRQ_PS2_2        102
#define MEC17XX_IRQ_ESPI_PC      103
#define MEC17XX_IRQ_ESPI_BM1     104
#define MEC17XX_IRQ_ESPI_BM2     105
#define MEC17XX_IRQ_ESPI_LTR     106
#define MEC17XX_IRQ_ESPI_OOB_UP  107
#define MEC17XX_IRQ_ESPI_OOB_DN  108
#define MEC17XX_IRQ_ESPI_FC      109
#define MEC17XX_IRQ_ESPI_RESET   110
#define MEC17XX_IRQ_RTOS_TIMER   111
#define MEC17XX_IRQ_HTIMER0      112
#define MEC17XX_IRQ_HTIMER1      113
#define MEC17XX_IRQ_WEEK_ALARM   114
#define MEC17XX_IRQ_SUBWEEK      115
#define MEC17XX_IRQ_WEEK_SEC     116
#define MEC17XX_IRQ_WEEK_SUBSEC  117
#define MEC17XX_IRQ_WEEK_SYSPWR  118
#define MEC17XX_IRQ_RTC          119
#define MEC17XX_IRQ_RTC_ALARM    120
#define MEC17XX_IRQ_VCI_OVRD_IN  121
#define MEC17XX_IRQ_VCI_IN0      122
#define MEC17XX_IRQ_VCI_IN1      123
#define MEC17XX_IRQ_VCI_IN2      124
#define MEC17XX_IRQ_VCI_IN3      125
#define MEC17XX_IRQ_VCI_IN4      126
#define MEC17XX_IRQ_VCI_IN5      127
#define MEC17XX_IRQ_VCI_IN6      128
#define MEC17XX_IRQ_PS20A_WAKE   129
#define MEC17XX_IRQ_PS20B_WAKE   130
#define MEC17XX_IRQ_PS21A_WAKE   131
#define MEC17XX_IRQ_PS21B_WAKE   132
#define MEC17XX_IRQ_PS2_2_WAKE   133
#define MEC17XX_IRQ_ENVMON       134
#define MEC17XX_IRQ_KSC_INT      135
#define MEC17XX_IRQ_TIMER16_0    136
#define MEC17XX_IRQ_TIMER16_1    137
#define MEC17XX_IRQ_TIMER16_2    138
#define MEC17XX_IRQ_TIMER16_3    139
#define MEC17XX_IRQ_TIMER32_0    140
#define MEC17XX_IRQ_TIMER32_1    141
#define MEC17XX_IRQ_CNTR_TM0     142
#define MEC17XX_IRQ_CNTR_TM1     143
#define MEC17XX_IRQ_CNTR_TM2     144
#define MEC17XX_IRQ_CNTR_TM3     145
#define MEC17XX_IRQ_CCT_TMR      146
#define MEC17XX_IRQ_CCT_CAP0     147
#define MEC17XX_IRQ_CCT_CAP1     148
#define MEC17XX_IRQ_CCT_CAP2     149
#define MEC17XX_IRQ_CCT_CAP3     150
#define MEC17XX_IRQ_CCT_CAP4     151
#define MEC17XX_IRQ_CCT_CAP5     152
#define MEC17XX_IRQ_CCT_CMP0     153
#define MEC17XX_IRQ_CCT_CMP1     154
#define MEC17XX_IRQ_EEPROM       155
#define MEC17XX_IRQ_ESPI_VW_EN   156


/* Wake pin definitions, defined at board-level */
extern const enum gpio_signal hibernate_wake_pins[];
extern const int hibernate_wake_pins_used;

/* TODO MCHP DEBUG */
#include "tfdp.h"

#endif /* __CROS_EC_REGISTERS_H */
