/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* INTC control module */

#include "csr.h"
#include "intc.h"
#include "registers.h"

enum INTC_POL {
	INTC_POL_HIGH = 0x0,
	INTC_POL_LOW = 0x1,
};

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
	INTC_GRP_14
};

struct INTC_IRQ {
	uint8_t group;
	uint8_t pol;
} irqs[SCP_INTC_IRQ_COUNT] = {
	[SCP_IRQ_EINT]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_I2C0]		= { INTC_GRP_8, INTC_POL_LOW },
	[SCP_IRQ_I2C1]		= { INTC_GRP_8, INTC_POL_LOW },
	[SCP_IRQ_VOW]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_XGPT0]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_XGPT1]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_XGPT2]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_XGPT3]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_XGPT4]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_XGPT5]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_DMA]		= { INTC_GRP_8, INTC_POL_LOW },
	[SCP_IRQ_AUDIO]		= { INTC_GRP_8, INTC_POL_LOW },
	[SCP_IRQ_SPI0]		= { INTC_GRP_8, INTC_POL_LOW },
	[SCP_IRQ_SPI1]		= { INTC_GRP_8, INTC_POL_LOW },
	[SCP_IRQ_SPI2]		= { INTC_GRP_8, INTC_POL_LOW },
	[SCP_IRQ_WDT]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_INFRA]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_CLK_CTRL]	= { INTC_GRP_1, INTC_POL_HIGH },
	[SCP_IRQ_CLK_CTRL_2]	= { INTC_GRP_1, INTC_POL_HIGH },
	[SCP_IRQ_HALT]		= { INTC_GRP_0, INTC_POL_HIGH },
	[SCP_IRQ_MBOX0]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_MBOX1]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_MBOX2]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_MBOX3]		= { INTC_GRP_8, INTC_POL_HIGH },
	[SCP_IRQ_MBOX4]		= { INTC_GRP_8, INTC_POL_HIGH },
};

int chip_get_ec_int(void)
{
	return 0;
}

int chip_get_intc_group(int irq)
{
	/* TODO: identify invalid group */
	return irqs[irq].group;
}

int chip_enable_irq(int irq)
{
	unsigned int word, group, mask;

	word = SCP_INTC_WORD(irq);
	mask = (1 << SCP_INTC_BIT(irq));
	group = irqs[irq].group;

	/* disable interrupt */
	SCP_CORE0_INTC_IRQ_EN(word) &= ~mask;
	/* set polarity */
	if (irqs[irq].pol == INTC_POL_HIGH)
		SCP_CORE0_INTC_IRQ_POL(word) |= mask;
	else
		SCP_CORE0_INTC_IRQ_POL(word) &= ~mask;
	/* set group */
	SCP_CORE0_INTC_IRQ_GRP(group, word) |= mask;
	/* enable interrupt */
	SCP_CORE0_INTC_IRQ_EN(word) |= mask;

	return EC_SUCCESS;
}

int chip_disable_irq(int irq)
{
	unsigned int word, mask;

	word = SCP_INTC_WORD(irq);
	mask = (1 << SCP_INTC_BIT(irq));

	SCP_CORE0_INTC_IRQ_EN(word) &= ~mask;

	return EC_SUCCESS;
}

int chip_clear_pending_irq(int irq)
{
	unsigned int group;

	group = irqs[irq].group;
	/* write 1 to unblock IRQ? */
	SCP_CORE0_INTC_IRQ_CLR_TRG = (1 << group);

	/* setup a dummy operatation to re-order interrupts. */
	SET_CSR(CSR_VIC_MIMASK_G0, 0);

	return EC_SUCCESS;
}

int chip_trigger_irq(int irq)
{
	/* TODO: software interrupt? */
	return EC_SUCCESS;
}

void chip_init_irqs(void)
{
	unsigned int word, group;

	for (word = 0; word < SCP_INTC_GRP_LEN; ++word) {
		SCP_CORE0_INTC_IRQ_EN(word) = 0x0;
		SCP_CORE0_INTC_IRQ_WAKE_EN(word) = 0x0;
		SCP_CORE0_INTC_IRQ_POL(word) = 0x0;
	}

	for (group = 0; group < SCP_INTC_GRP_COUNT; ++group)
		for (word = 0; word < SCP_INTC_GRP_LEN; ++word)
			SCP_CORE0_INTC_IRQ_GRP(group, word) = 0x0;

	/* GVIC init, needed? */
	SET_CSR(CSR_VIC_MIMASK_G0, 0xffffffff);
	SET_CSR(CSR_VIC_MILSEL_G0, 0xffffffff);
	SET_CSR(CSR_VIC_MIWAKEUP_G0, 0xffffffff);

	/* disable all interrupts until setup is done */
	CLEAR_CSR_RAW(mie, MIP_MEIP);
	CLEAR_CSR_RAW(mie, MIP_MSIP);
	CLEAR_CSR_RAW(mie, MIP_MTIP);
	CLEAR_CSR_RAW(mie, MIP_LTIP);
	CLEAR_CSR_RAW(mie, MIP_AXI);
	CLEAR_CSR_RAW(mie, MIP_LINT_0);
	CLEAR_CSR_RAW(mie, MIP_LINT_1);
	CLEAR_CSR_RAW(mie, MIP_LINT_2);
	CLEAR_CSR_RAW(mie, MIP_LINT_3);
	CLEAR_CSR_RAW(mie, MIP_LINT_4);
	CLEAR_CSR_RAW(mie, MIP_LINT_5);
	CLEAR_CSR_RAW(mie, MIP_LINT_6);
	CLEAR_CSR_RAW(mie, MIP_LINT_7);
}
