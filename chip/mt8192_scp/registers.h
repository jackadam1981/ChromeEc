/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Register map */

#ifndef __CROS_EC_REGISTERS_H
#define __CROS_EC_REGISTERS_H

#include "common.h"
#include "compile_time_macros.h"

#define DBG(x) do { \
	REG32(0x70026000) = x; \
	REG32(0x70026000) = 0xd; \
	REG32(0x70026000) = 0xa; \
} while(0)

static inline void busy_udelay(int usec)
{
	/*
	 * Delaying by busy-looping, for place that can't use udelay because of
	 * the clock not configured yet. The value 28 is chosen approximately
	 * from experiment.
	 */
	volatile int i = usec * 28;

	while (i--)
		;
}

#define SCP_REG_BASE			0x70000000

/* clock control */
#define SCP_CLK_CTRL_BASE		(SCP_REG_BASE + 0x21000)
/* clock source select */
#define SCP_CLK_SW_SEL			REG32(SCP_CLK_CTRL_BASE + 0x0000)
#define   CKSW_SEL_SYSTEM_CLOCK		0
#define   CKSW_SEL_32K			1
#define   CKSW_SEL_ULPOSC_CORE		2
#define   CKSW_SEL_ULPOSC_PERI		3
/* clock interrupt acknowledge */
#define SCP_CLK_IRQ_ACK			REG32(SCP_CLK_CTRL_BASE + 0x0010)
/* system clock counter value */
#define SCP_CLK_SYS_VAL			REG32(SCP_CLK_CTRL_BASE + 0x0014)
#define   CLK_SYS_VAL_MASK		(0x3ff << 0)
#define   CLK_SYS_VAL_VAL(v)		((v) & CLK_SYS_VAL_MASK)
/* ULPOSC clock counter value */
#define SCP_CLK_HIGH_VAL		REG32(SCP_CLK_CTRL_BASE + 0x0018)
#define   CLK_HIGH_VAL_MASK             (0x1f << 0)
#define   CLK_HIGH_VAL_VAL(v)		((v) & CLK_HIGH_VAL_MASK)
/* clock select slow */
#define SCP_CLK_SEL_SLOW		REG32(SCP_CLK_CTRL_BASE + 0x001C)
#define   CLK_SW_SEL_SLOW_MASK		0x3
#define   CLK_SW_SEL_SLOW_VAL(v)	((v) & CLK_SW_SEL_SLOW_MASK)
#define   CLK_DIVSW_SEL_SLOW_MASK	0x30
#define   CLK_DIVSW_SEL_SLOW_VAL(v)	((v) & CLK_DIVSW_SEL_SLOW_MASK)
/* sleep mode control */
#define SCP_SLEEP_CTRL                  REG32(SCP_CLK_CTRL_BASE + 0x0020)
#define   SLP_CTRL_EN			BIT(0)
#define   VREQ_COUNT_MASK		(0x7F << 1)
#define   VREQ_COUNT_VAL(v)		(((v) << 1) & VREQ_COUNT_MASK)
#define   SPM_SLP_MODE			BIT(8)
/* clock divider select */
#define SCP_CLK_DIV_SEL			REG32(SCP_CLK_CTRL_BASE + 0x0024)
/* clock gate */
#define SCP_SET_CLK_CG			REG32(SCP_CLK_CTRL_BASE + 0x0030)
#define   CG_TIMER_MCLK			BIT(0)
#define   CG_TIMER_BCLK			BIT(1)
#define   CG_MAD_MCLK			BIT(2)
#define   CG_I2C_MCLK			BIT(3)
#define   CG_I2C_BCLK			BIT(4)
#define   CG_GPIO_MCLK			BIT(5)
#define   CG_AP2P_MCLK			BIT(6)
#define   CG_UART0_MCLK			BIT(7)
#define   CG_UART0_BCLK			BIT(8)
#define   CG_UART0_RST			BIT(9)
#define   CG_UART1_MCLK			BIT(10)
#define   CG_UART1_BCLK			BIT(11)
#define   CG_UART1_RST			BIT(12)
#define   CG_SPI0			BIT(13)
#define   CG_SPI1			BIT(14)
#define   CG_SPI2			BIT(15)
#define   CG_DMA_CH0			BIT(16)
#define   CG_DMA_CH1			BIT(17)
#define   CG_DMA_CH2			BIT(18)
#define   CG_DMA_CH3			BIT(19)
#define   CG_I3C0			BIT(21)
#define   CG_I3C1			BIT(22)
#define   CG_DMA2_CH0			BIT(23)
#define   CG_DMA2_CH1			BIT(24)
#define   CG_DMA2_CH2			BIT(25)
#define   CG_DMA2_CH3			BIT(26)
/* wake clock select */
#define SCP_WAKE_CKSW_SEL		REG32(SCP_CLK_CTRL_BASE + 0x0040)
#define   WAKE_CKSW_SEL_NORMAL_MASK	0x3
#define   WAKE_CKSW_SEL_NORMAL_VAL(v)	((v) & WAKE_CKSW_SEL_NORMAL_MASK)
#define   WAKE_CKSW_SEL_SLOW_MASK	0x30
#define   WAKE_CKSW_SEL_SLOW_VAL(v)	((v) & WAKE_CKSW_SEL_SLOW_MASK)
/* UART clock select */
#define SCP_UART_CK_SEL			REG32(SCP_CLK_CTRL_BASE + 0x0044)
#define   UART0_CK_SEL_MASK		(0x3 << 0)
#define   UART0_CK_SEL_VAL(v)		((v) & UART0_CK_SEL_MASK)
#define   UART0_CK_SW_STATUS_SHIFT	8
#define   UART0_CK_SW_STATUS_MASK	(0xf << UART0_CK_SW_STATUS_SHIFT)
#define   UART1_CK_SEL_MASK		(0x3 << 16)
#define   UART1_CK_SEL_VAL(v)		((v) & UART1_CK_SEL_MASK)
#define   UART1_CK_SW_STATUS_SHIFT	24
#define   UART1_CK_SW_STATUS_MASK	(0xf << UART1_CK_SW_STATUS_SHIFT)
#define   UART_CK_SEL_26M		0
#define   UART_CK_SEL_32K		1
#define   UART_CK_SEL_ULPOSC_DIV_TO_26M	2
#define   UART_CK_SW_STATUS_26M		1
#define   UART_CK_SW_STATUS_32K		2
#define   UART_CK_SW_STATUS_ULPOS_DIV_TO_26M	4
/* VREQ control */
#define SCP_CPU_VREQ_CTRL		REG32(SCP_CLK_CTRL_BASE + 0x0054)
#define   VREQ_SEL			BIT(0)
#define   VREQ_VALUE			BIT(4)
#define   VREQ_EXT_SEL			BIT(8)
#define   VREQ_DVFS_SEL			BIT(16)
#define   VREQ_DVFS_VALUE		BIT(20)
#define   VREQ_DVFS_EXT_SEL		BIT(24)
#define   VREQ_SRCLKEN_SEL		BIT(27)
#define   VREQ_SRCLKEN_VALUE		BIT(28)
/* clock on control */
#define SCP_CLK_ON_CTRL			REG32(SCP_CLK_CTRL_BASE + 0x006C)
#define   HIGH_AO			BIT(0)
#define   HIGH_DIS_SUB			BIT(1)
#define   HIGH_CG_AO			BIT(2)
#define   HIGH_CORE_AO			BIT(4)
#define   HIGH_CORE_DIS_SUB		BIT(5)
#define   HIGH_CORE_CG_AO		BIT(6)
#define   HIGH_FINAL_VAL_MASK		(0x1f << 8)
#define   HIGH_FINAL_VAL_VAL(v)		((v) & HIGH_FINAL_VAL_MASK)
/* clock general control */
#define SCP_CLK_CTRL_GENERAL_CTRL	REG32(SCP_CLK_CTRL_BASE + 0x009C)
#define   VREQ_PMIC_WRAP_SEL		(0x2)
/* sleep control clock */
#define SCP_CLK_CTRL_SLP_CTRL		REG32(SCP_CLK_CTRL_BASE + 0x00A0)
/* fast wake count end */
#define SCP_FAST_WAKE_CNT_END		REG32(SCP_CLK_CTRL_BASE + 0x00A4)
#define   FAST_WAKE_CNT_END_MASK	0xfff
#define   FAST_WAKE_CNT_END_VAL(v)	((v) & FAST_WAKE_CNT_END_MASK)

/* system control */
#define SCP_SYS_CTRL			REG32(SCP_REG_BASE + 0x24000)
#define   AUTO_DDREN			BIT(9)

/* UART */
#define SCP_UART_COUNT			2
#if 1
#define SCP_UART0_BASE			(SCP_REG_BASE + 0x26000)
#else
#define SCP_UART0_BASE			(0x61003000) /* AP UART1 */
#endif
#define SCP_UART1_BASE			(SCP_REG_BASE + 0x27000)
#define SCP_UART_BASE(n)		CONCAT3(SCP_UART, n, _BASE)
#define UART_REG(n, offset)		REG32_ADDR(SCP_UART_BASE(n))[offset]

/* CORE0 configurations */
#define SCP_CORE0_CFG_BASE		(SCP_REG_BASE + 0x30000)
#define SCP_CORE0_GENERAL_CTRL		REG32(SCP_CORE0_CFG_BASE + 0x001C)
#define   CPU_TIMER_INT_EN		BIT(0)
#define SCP_CORE0_WDT_IRQ		REG32(SCP_CORE0_CFG_BASE + 0x0030)
#define SCP_CORE0_WDT_CFG		REG32(SCP_CORE0_CFG_BASE + 0x0034)
#define   WDT_FREQ			33825
#define   WDT_MAX_PERIOD		0xFFFFF /* 31 seconds */
#define   WDT_PERIOD(ms)		(WDT_FREQ * (ms) / 1000)
#define   WDT_EN			BIT(31)
#define SCP_CORE0_WDT_KICK		REG32(SCP_CORE0_CFG_BASE + 0x0038)

/* INTC */
#define SCP_INTC_WORD(irq)		((irq) >> 5) /* word length = 2^5 */
#define SCP_INTC_BIT(irq)		((irq) & 0x1F) /* bit shift =LSB[0:4] */
#define SCP_INTC_GRP_COUNT		15
#define SCP_INTC_GRP_LEN		3
#define SCP_INTC_GRP_GAP		4
#define SCP_INTC_IRQ_COUNT		71
#define SCP_INTC_IRQ_BASE		(SCP_REG_BASE + 0x32000)
#define SCP_CORE0_INTC_IRQ_STA(w)				\
	REG32_ADDR(SCP_INTC_IRQ_BASE + 0x0010)[w]
#define SCP_CORE0_INTC_IRQ_EN(w)				\
	REG32_ADDR(SCP_INTC_IRQ_BASE + 0x0020)[w]
#define SCP_CORE0_INTC_IRQ_POL(w)				\
	REG32_ADDR(SCP_INTC_IRQ_BASE + 0x0040)[w]
#define SCP_CORE0_INTC_IRQ_GRP(g, w)				\
	REG32_ADDR(SCP_INTC_IRQ_BASE + 0x0050 + (g << SCP_INTC_GRP_GAP))[w]
#define SCP_CORE0_INTC_IRQ_GRP_STA(g, w)			\
	REG32_ADDR(SCP_INTC_IRQ_BASE + 0x0150 + (g << SCP_INTC_GRP_GAP))[w]
#define SCP_CORE0_INTC_IRQ_WAKE_EN(w)				\
	REG32_ADDR(SCP_INTC_IRQ_BASE + 0x0240)[w]
#define SCP_CORE0_INTC_IRQ_CLR_TRG	REG32(SCP_INTC_IRQ_BASE + 0x0254)

/* secure control */
#define SCP_SEC_CTRL			REG32(SCP_REG_BASE + 0xA5000)
#define	  VREQ_SECURE_DIS		BIT(4)
/* memory remap */
#define SCP_R_REMAP_0X0123		REG32(SCP_REG_BASE + 0xA5060)
#define SCP_R_REMAP_0X4567		REG32(SCP_REG_BASE + 0xA5064)
#define SCP_R_REMAP_0X89AB		REG32(SCP_REG_BASE + 0xA5068)
#define SCP_R_REMAP_0XCDEF		REG32(SCP_REG_BASE + 0xA506C)

/* external address: AP */
#define AP_REG_BASE			0x60000000 /* 0x10000000 remap to 0x6 */
/* AP GPIO */
#define AP_GPIO_BASE			(AP_REG_BASE + 0x5000)
#define AP_GPIO_MODE11_SET		REG32(AP_GPIO_BASE + 0x03B4)
#define AP_GPIO_MODE11_CLR		REG32(AP_GPIO_BASE + 0x03B8)
#define AP_GPIO_MODE20_SET		REG32(AP_GPIO_BASE + 0x0444)
#define AP_GPIO_MODE20_CLR		REG32(AP_GPIO_BASE + 0x0448)

#define DUMMY_GPIO_BANK 0

/* IRQ numbers */
#define SCP_IRQ_EINT		6
#define SCP_IRQ_I2C0		10
#define SCP_IRQ_I2C1		11
#define SCP_IRQ_VOW		14
#define SCP_IRQ_XGPT0		15
#define SCP_IRQ_XGPT1		16
#define SCP_IRQ_XGPT2		17
#define SCP_IRQ_XGPT3		18
#define SCP_IRQ_XGPT4		19
#define SCP_IRQ_XGPT5		20
#define SCP_IRQ_DMA		24
#define SCP_IRQ_AUDIO		25
#define SCP_IRQ_SPI0		29
#define SCP_IRQ_SPI1		30
#define SCP_IRQ_SPI2		31
#define SCP_IRQ_WDT		37
#define SCP_IRQ_INFRA		41
#define SCP_IRQ_CLK_CTRL	42
#define SCP_IRQ_CLK_CTRL_2	43
#define SCP_IRQ_HALT		45
#define SCP_IRQ_MBOX0		60
#define SCP_IRQ_MBOX1		61
#define SCP_IRQ_MBOX2		62
#define SCP_IRQ_MBOX3		63
#define SCP_IRQ_MBOX4		64

#endif /* __CROS_EC_REGISTERS_H */
