/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* INTC control module */

#include "csr.h"
#include "intc.h"
#include "registers.h"

/*
 * INTC_POL_EMPTY is a placeholder for empty slots.
 *
 * INTC_POL_HIGH: polarity is high, active low.
 * INTC_POL_LOW: polarity is low, active high.
 */
enum INTC_POL {
	INTC_POL_EMPTY = 0x0,
	INTC_POL_HIGH,
	INTC_POL_LOW,
};

/*
 * INTC_GRP_0 is reserved.  See swirq of syscall_handler() in
 * core/riscv-rv32i/task.c for more details.
 */
enum INTC_GROUP {
	INTC_GRP_0 = 0x0,
	INTC_GRP_1,
	INTC_GRP_2,
	INTC_GRP_3,
	INTC_GRP_4,
	INTC_GRP_5,
	INTC_GRP_6,
	INTC_GRP_7,
	INTC_GRP_8,
	INTC_GRP_9,
	INTC_GRP_10,
	INTC_GRP_11,
	INTC_GRP_12,
	INTC_GRP_13,
	INTC_GRP_14,
};

static struct {
	uint8_t group;
	uint8_t pol;
} irqs[SCP_INTC_IRQ_COUNT] = {
	/* 0 */
	[SCP_IRQ_GIPC_IN0]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_GIPC_IN1] 		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_GIPC_IN2] 		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_GIPC_IN3]		= { INTC_GRP_0, INTC_POL_HIGH },
	/* 4 */
	[SCP_IRQ_SPM]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_AP_CIRQ]		= { INTC_GRP_0, INTC_POL_LOW },
	[SCP_IRQ_EINT]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_PMIC]			= { INTC_GRP_0, INTC_POL_HIGH },
	/* 8 */
	[SCP_IRQ_UART0_TX]		= { INTC_GRP_12, INTC_POL_LOW },
	[SCP_IRQ_UART1_TX]		= { INTC_GRP_12, INTC_POL_LOW },
	[SCP_IRQ_I2C0]			= { INTC_GRP_0, INTC_POL_LOW },
	[SCP_IRQ_I2C1_0]		= { INTC_GRP_0, INTC_POL_LOW },
	/* 12 */
	[SCP_IRQ_BUS_DBG_TRACKER]	= { INTC_GRP_0, INTC_POL_LOW },
	[SCP_IRQ_CLK_CTRL]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_VOW]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_TIMER0]		= { INTC_GRP_6, INTC_POL_HIGH },
	/* 16 */
	[SCP_IRQ_TIMER1]		= { INTC_GRP_6, INTC_POL_HIGH },
	[SCP_IRQ_TIMER2]		= { INTC_GRP_6, INTC_POL_HIGH },
	[SCP_IRQ_TIMER3]		= { INTC_GRP_6, INTC_POL_HIGH },
	[SCP_IRQ_TIMER4]		= { INTC_GRP_6, INTC_POL_HIGH },
	/* 20 */
	[SCP_IRQ_TIMER5]		= { INTC_GRP_6, INTC_POL_HIGH },
	[SCP_IRQ_OS_TIMER]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_UART0_RX]		= { INTC_GRP_12, INTC_POL_HIGH },
	[SCP_IRQ_UART1_RX]		= { INTC_GRP_12, INTC_POL_HIGH },
	/* 24 */
	[SCP_IRQ_GDMA]			= { INTC_GRP_0, INTC_POL_LOW },
	[SCP_IRQ_AUDIO]			= { INTC_GRP_0, INTC_POL_LOW },
	[SCP_IRQ_MD_DSP]		= { INTC_GRP_0, INTC_POL_LOW },
	[SCP_IRQ_ADSP]			= { INTC_GRP_0, INTC_POL_LOW },
	/* 28 */
	[SCP_IRQ_CPU_TICK]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_SPI0]			= { INTC_GRP_0, INTC_POL_LOW },
	[SCP_IRQ_SPI1]			= { INTC_GRP_0, INTC_POL_LOW },
	[SCP_IRQ_SPI2]			= { INTC_GRP_0, INTC_POL_LOW },
	/* 32 */
	[SCP_IRQ_NEW_INFRA_SYS_CIRQ]	= { INTC_GRP_0, INTC_POL_LOW },
	[SCP_IRQ_DBG]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_CCIF0]			= { INTC_GRP_0, INTC_POL_LOW },
	[SCP_IRQ_CCIF1]			= { INTC_GRP_0, INTC_POL_LOW },
	/* 36 */
	[SCP_IRQ_CCIF2]			= { INTC_GRP_0, INTC_POL_LOW },
	[SCP_IRQ_WDT]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_USB0]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_USB1]			= { INTC_GRP_0, INTC_POL_HIGH },
	/* 40 */
	[SCP_IRQ_DPMAIF]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_INFRA]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_CLK_CTRL_CORE]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_CLK_CTRL2_CORE]	= { INTC_GRP_0, INTC_POL_HIGH },
	/* 44 */
	[SCP_IRQ_CLK_CTRL2]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_GIPC_IN4]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_PERIBUS_TIMEOUT]	= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_INFRABUS_TIMEOUT]	= { INTC_GRP_0, INTC_POL_HIGH },
	/* 48 */
	[SCP_IRQ_MET0]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_MET1]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_MET2]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_MET3]			= { INTC_GRP_0, INTC_POL_HIGH },
	/* 52 */
	[SCP_IRQ_AP_WDT]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_L2TCM_SEC_VIO]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_CPU_TICK1]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_MAD_DATAIN]		= { INTC_GRP_0, INTC_POL_HIGH },
	/* 56 */
	[SCP_IRQ_I3C0_IBI_WAKE]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_I3C1_IBI_WAKE]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_I3C2_IBI_WAKE]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_APU_ENGINE]		= { INTC_GRP_0, INTC_POL_LOW },
	/* 60 */
	[SCP_IRQ_MBOX0]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_MBOX1]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_MBOX2]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_MBOX3]			= { INTC_GRP_0, INTC_POL_HIGH },
	/* 64 */
	[SCP_IRQ_MBOX4]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_SYS_CLK_REQ]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_BUS_REQ]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_APSRC_REQ]		= { INTC_GRP_0, INTC_POL_HIGH },
	/* 68 */
	[SCP_IRQ_APU_MBOX]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_DEVAPC_SECURE_VIO]	= { INTC_GRP_0, INTC_POL_LOW },
	/* 72 */
	/* 76 */
	[SCP_IRQ_I2C1_2]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_I2C2]			= { INTC_GRP_0, INTC_POL_HIGH },
	/* 80 */
	[SCP_IRQ_AUD2AUDIODSP]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_AUD2AUDIODSP_2]	= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_CONN2ADSP_A2DPOL]	= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_CONN2ADSP_BTCVSD]	= { INTC_GRP_0, INTC_POL_HIGH },
	/* 84 */
	[SCP_IRQ_CONN2ADSP_BLEISO]	= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_PCIE2ADSP]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_APU2ADSP_ENGINE]	= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_APU2ADSP_MBOX]		= { INTC_GRP_0, INTC_POL_HIGH },
	/* 88 */
	[SCP_IRQ_CCIF3]			= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_I2C_DMA0]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_I2C_DMA1]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_I2C_DMA2]		= { INTC_GRP_0, INTC_POL_HIGH },
	/* 92 */
	[SCP_IRQ_I2C_DMA3]		= { INTC_GRP_0, INTC_POL_HIGH },
};

#include "console.h"
volatile int ec_int;

int chip_get_ec_int(void)
{
	unsigned int word, group, sta;

	if (!SCP_CORE0_INTC_IRQ_OUT)
		goto error;

	group = __builtin_ctz(SCP_CORE0_INTC_IRQ_OUT);

	for (word = 0; word < SCP_INTC_GRP_LEN; ++word) {
		sta = SCP_CORE0_INTC_IRQ_GRP_STA(group, word);
		if (sta) {
			ec_int = __fls(sta);
			return ec_int;
		}
	}

error:
	/* unreachable */
	ccprints("%s: unreachable", __func__);
	cflush();
	return 0;
}

int chip_get_intc_group(int irq)
{
	return irqs[irq].group;
}

/* TODO: take care of returning value or none */
/* TODO: generalize intc.h for rv32i */
int chip_enable_irq(int irq)
{
	unsigned int word, group, mask;

	if (irqs[irq].pol == INTC_POL_EMPTY)
		return EC_ERROR_UNKNOWN;

	word = SCP_INTC_WORD(irq);
	mask = BIT(SCP_INTC_BIT(irq));
	group = irqs[irq].group;

	/* disable interrupt */
	SCP_CORE0_INTC_IRQ_EN(word) &= ~mask;
	/* set polarity */
	switch (irqs[irq].pol) {
	case INTC_POL_HIGH:
		SCP_CORE0_INTC_IRQ_POL(word) &= ~mask;
		break;
	case INTC_POL_LOW:
		SCP_CORE0_INTC_IRQ_POL(word) |= mask;
		break;
	}
	/* set group */
	SCP_CORE0_INTC_IRQ_GRP(group, word) |= mask;
	/* set as a wakeup source */
	SCP_CORE0_INTC_SLP_WAKE_EN(word) |= mask;
	/* enable interrupt */
	SCP_CORE0_INTC_IRQ_EN(word) |= mask;

	return EC_SUCCESS;
}

int chip_disable_irq(int irq)
{
	unsigned int word, mask;

	word = SCP_INTC_WORD(irq);
	mask = BIT(SCP_INTC_BIT(irq));

	SCP_CORE0_INTC_IRQ_EN(word) &= ~mask;

	return EC_SUCCESS;
}

#include "console.h"
#include "csr.h"
int chip_clear_pending_irq(int irq)
{
	unsigned int group = irqs[irq].group;

#if 1
	ccprintf("before miems: ec_int=%d\n", ec_int);
	ccprintf("before miems: CSR_VIC_MIPEND_G0=%x\n", (unsigned int)READ_CSR(0x5d0));
	ccprintf("before miems: CORE0_TIMER_IRQ_CTRL(timer3)=%x\n", SCP_CORE0_TIMER_IRQ_CTRL(3));
	ccprintf("before miems: CORE0_TIMER_CUR_VAL(timer3)=%x\n", SCP_CORE0_TIMER_CUR_VAL(3));
	ccprintf("before miems: CORE0_TIMER_IRQ_CTRL(timer5)=%x\n", SCP_CORE0_TIMER_IRQ_CTRL(5));
	ccprintf("before miems: CORE0_TIMER_CUR_VAL(timer5)=%x\n", SCP_CORE0_TIMER_CUR_VAL(5));
	ccprintf("before miems: CORE0_INTC_IRQ_OUT=%x\n", SCP_CORE0_INTC_IRQ_OUT);
	ccprintf("before miems: CORE0_INTC_IRQ_STA0=%x\n", REG32(0x70032010));
	ccprintf("before miems: CORE0_INTC_IRQ_GRP6_STA0=%x\n", SCP_CORE0_INTC_IRQ_GRP_STA(6, 0));
	cflush();
#endif
	/* must clear interrupt source before writing this */
	WRITE_CSR(CSR_VIC_MIEMS, group);
#if 1
	ccprintf("after miems: ec_int=%d\n", ec_int);
	ccprintf("after miems: CSR_VIC_MIPEND_G0=%x\n", (unsigned int)READ_CSR(0x5d0));
	ccprintf("after miems: CORE0_TIMER_IRQ_CTRL(timer3)=%x\n", SCP_CORE0_TIMER_IRQ_CTRL(3));
	ccprintf("after miems: CORE0_TIMER_CUR_VAL(timer3)=%x\n", SCP_CORE0_TIMER_CUR_VAL(3));
	ccprintf("after miems: CORE0_TIMER_IRQ_CTRL(timer5)=%x\n", SCP_CORE0_TIMER_IRQ_CTRL(5));
	ccprintf("after miems: CORE0_TIMER_CUR_VAL(timer5)=%x\n", SCP_CORE0_TIMER_CUR_VAL(5));
	ccprintf("after miems: CORE0_INTC_IRQ_OUT=%x\n", SCP_CORE0_INTC_IRQ_OUT);
	ccprintf("after miems: CORE0_INTC_IRQ_STA0=%x\n", REG32(0x70032010));
	ccprintf("after miems: CORE0_INTC_IRQ_GRP6_STA0=%x\n", SCP_CORE0_INTC_IRQ_GRP_STA(6, 0));
	cflush();
#endif

	return EC_SUCCESS;
}

int chip_trigger_irq(int irq)
{
	ec_int = irq;
	return irqs[irq].group;
}

void chip_init_irqs(void)
{
	unsigned int word, group;

	/* INTC init */
	/* clear enable, wakeup, and polarity settings */
	for (word = 0; word < SCP_INTC_GRP_LEN; ++word) {
		SCP_CORE0_INTC_IRQ_EN(word) = 0x0;
		SCP_CORE0_INTC_SLP_WAKE_EN(word) = 0x0;
		SCP_CORE0_INTC_IRQ_POL(word) = 0x0;
	}
	/* clear group settings */
	for (group = 0; group < SCP_INTC_GRP_COUNT; ++group)
		for (word = 0; word < SCP_INTC_GRP_LEN; ++word)
			SCP_CORE0_INTC_IRQ_GRP(group, word) = 0x0;

	/* GVIC init */
	/* enable all groups */
	SET_CSR(CSR_VIC_MIMASK_G0, 0xffffffff);
	/* use level trigger */
	SET_CSR(CSR_VIC_MILSEL_G0, 0xffffffff);
	/* TODO: find what is it */
	SET_CSR(CSR_VIC_MIWAKEUP_G0, 0xffffffff);

	/* enable GVIC */
	SET_CSR(CSR_MCTREN, CSR_MCTREN_VIC);
}
