/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for Rotor MCU
 */

#ifndef __CROS_EC_REGISTERS_H
#define __CROS_EC_REGISTERS_H

#include "common.h"

/* GPIO */
#define DUMMY_GPIO_BANK 0

#define GPIO_A 0xEF022000
#define GPIO_B 0xEF022100
#define GPIO_C 0xEF022200
#define GPIO_D 0xEF022300
#define GPIO_E 0xEF022400

#define ROTOR_MCU_GPIO_PLR(b)		REG32((b) + 0x0)
#define ROTOR_MCU_GPIO_PDR(b)		REG32((b) + 0x4)
#define ROTOR_MCU_GPIO_PSR(b)		REG32((b) + 0xC)
#define ROTOR_MCU_GPIO_HRIPR(b)	REG32((b) + 0x10)
#define ROTOR_MCU_GPIO_LFIPR(b)	REG32((b) + 0x14)
#define ROTOR_MCU_GPIO_ISR(b)		REG32((b) + 0x18)
#define ROTOR_MCU_GPIO_SDR(b)		REG32((b) + 0x1C)
#define ROTOR_MCU_GPIO_CDR(b)		REG32((b) + 0x20)
#define ROTOR_MCU_GPIO_SHRIPR(b)	REG32((b) + 0x24)
#define ROTOR_MCU_GPIO_CHRIPR(b)	REG32((b) + 0x28)
#define ROTOR_MCU_GPIO_SLFIPR(b)	REG32((b) + 0x2C)
#define ROTOR_MCU_GPIO_CLFIPR(b)	REG32((b) + 0x30)
#define ROTOR_MCU_GPIO_OLR(b)		REG32((b) + 0x34)
#define ROTOR_MCU_GPIO_DWER(b)		REG32((b) + 0x38)
#define ROTOR_MCU_GPIO_IMR(b)		REG32((b) + 0x3C)
#define ROTOR_MCU_GPIO_SIMR(b)		REG32((b) + 0x48)
#define ROTOR_MCU_GPIO_CIMR(b)		REG32((b) + 0x4C)
#define ROTOR_MCU_GPIO_ITER(b)		REG32((b) + 0x50)


/* MCU Pad Wrap */
#define ROTOR_MCU_PAD_WRAP_BASE	0xEF020000
#define ROTOR_MCU_IO_PAD_CFG(n)	REG32(ROTOR_MCU_PAD_WRAP_BASE + 0x8 + \
					      (n * 0x4))

#define GPIO_PAD_CFG_IDX(port, pin)	((((port % 0x2000) / 0x100) * 32) + pin)
#define GPIO_PAD_CFG_ADDR(port, pin)	((GPIO_PAD_CFG_IDX(port, pin) * 4) + \
					 ROTOR_MCU_PAD_WRAP_BASE + 8)
#define ROTOR_MCU_GPIO_PCFG(port, pin)	REG32(GPIO_PAD_CFG_ADDR(port, pin))


/* UART */
#define ROTOR_MCU_UART_CFG_BASE(n)	(0xED060000 + n*0x1000)
/* DLAB = 0 */
#define ROTOR_MCU_UART_RBR(n) /* R */	REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0x0)
#define ROTOR_MCU_UART_THR(n) /* W */	REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0x0)
#define ROTOR_MCU_UART_IER(n)		REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0x4)
/* DLAB = 1 */
#define ROTOR_MCU_UART_DLL(n)		REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0x0)
#define ROTOR_MCU_UART_DLH(n)		REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0x4)

#define ROTOR_MCU_UART_IIR(n) /* R */	REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0x8)
#define ROTOR_MCU_UART_FCR(n) /* W */	REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0x8)
#define ROTOR_MCU_UART_LCR(n)		REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0xC)
#define ROTOR_MCU_UART_MCR(n)		REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0x10)
#define ROTOR_MCU_UART_LSR(n) /* R */	REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0x14)
#define ROTOR_MCU_UART_MSR(n) /* R */	REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0x18)
#define ROTOR_MCU_UART_SCR(n)		REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0x1C)
#define ROTOR_MCU_UART_USR(n)		REG8(ROTOR_MCU_UART_CFG_BASE(n) + 0x7C)

/* Timers */
#define ROTOR_MCU_TMR_CFG_BASE(n)	(0xED020000 + n*0x1000)
#define ROTOR_MCU_TMR_TNLC(n)		REG32(ROTOR_MCU_TMR_CFG_BASE(n) + 0x0)
#define ROTOR_MCU_TMR_TNCV(n)		REG32(ROTOR_MCU_TMR_CFG_BASE(n) + 0x4)
#define ROTOR_MCU_TMR_TNCR(n)		REG32(ROTOR_MCU_TMR_CFG_BASE(n) + 0x8)
#define ROTOR_MCU_TMR_TNEOI(n)		REG32(ROTOR_MCU_TMR_CFG_BASE(n) + 0xC)
#define ROTOR_MCU_TMR_TNIS(n)		REG32(ROTOR_MCU_TMR_CFG_BASE(n) + 0x10)
/* What the difference between the two sets of registers?? */
#define ROTOR_MCU_TMR_TIS(n)		REG32(ROTOR_MCU_TMR_CFG_BASE(n) + 0xA0)
#define ROTOR_MCU_TMR_TEOI(n)		REG32(ROTOR_MCU_TMR_CFG_BASE(n) + 0xA4)
#define ROTOR_MCU_TMR_TRIS(n)		REG32(ROTOR_MCU_TMR_CFG_BASE(n) + 0xA8)
#define ROTOR_MCU_TMR_TNLC2(n)		REG32(ROTOR_MCU_TMR_CFG_BASE(n) + 0xB0)

/* Watchdog */
#define ROTOR_MCU_WDT_BASE		0xED010000
#define ROTOR_MCU_WDT_CR		REG8(ROTOR_MCU_WDT_BASE + 0x00)
#define ROTOR_MCU_WDT_TORR		REG8(ROTOR_MCU_WDT_BASE + 0x04)
#define ROTOR_MCU_WDT_CCVR		REG32(ROTOR_MCU_WDT_BASE + 0x08)
#define ROTOR_MCU_WDT_CRR		REG8(ROTOR_MCU_WDT_BASE + 0x0C)
#define ROTOR_MCU_WDT_STAT		REG8(ROTOR_MCU_WDT_BASE + 0x10)
#define ROTOR_MCU_WDT_EOI		REG8(ROTOR_MCU_WDT_BASE + 0x14)
/* To prevent accidental restarts, this magic value must be written to CRR. */
#define ROTOR_MCU_WDT_KICK		0x76

/* IRQ Numbers */
#define ROTOR_MCU_IRQ_TIMER_0	6
#define ROTOR_MCU_IRQ_TIMER_1	7
#define ROTOR_MCU_IRQ_WDT	14
#define ROTOR_MCU_IRQ_UART_0	16

#endif /* __CROS_EC_REGISTERS_H */
