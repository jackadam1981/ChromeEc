/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* DMA module for ISH */

#include "common.h"
#include "console.h"
#include "registers.h"
#include "ish_dma.h"
#include "util.h"

static int dma_err = 0;
static int dma_init_called = 0;

static int dma_poll(uint32_t addr, uint32_t expected, uint32_t mask)
{
	int retval = -1;

	while (1) {
		/* test condition */
		if ((read32(addr) & mask) == expected) {
			retval = DMA_RC_OK;
			break;
		}
	}

	return retval;
}

#define POLL_UNTIL_CLEARED(addr, mask) dma_poll((addr), 0, (mask))
#define POLL_UNTIL_SET(addr, mask) dma_poll((addr), (mask), (mask))

void ish_dma_ocp_timeout_disable(void)
{
	uint32_t ctrl = read32(OCP_AGENT_CONTROL);
	write32(OCP_AGENT_CONTROL, ctrl & OCP_RESPONSE_TO_DISABLE);
}

static inline uint32_t interrupt_lock(void)
{
	uint32_t eflags = 0;
	__asm__ volatile("pushfl;" /* save eflag value */
			 "popl  %0;"
			 "cli;"
			 : "=r"(eflags)); /* shut off interrupts */
	return eflags;
}

static inline void interrupt_unlock(uint32_t eflags)
{
	__asm__ volatile("pushl  %0;" /* restore elfag values */
			 "popfl;"
			 :
			 : "r"(eflags));
	(void)eflags;
}

void dma_configure_psize(void)
{
	/* Give chan0 512 bytes for high performance, and chan1 128 bytes. */
	write32(DMA_PSIZE_01,
		DMA_PSIZE_UPDATE |
			(DMA_PSIZE_CHAN1_SIZE << DMA_PSIZE_CHAN1_OFFSET) |
			(DMA_PSIZE_CHAN0_SIZE << DMA_PSIZE_CHAN0_OFFSET));
}

void ish_dma_init(void)
{
	uint32_t uma_msb;

	ish_dma_ocp_timeout_disable();

	/* configure DMA partition size */
	dma_configure_psize();

	/* set DRAM address 32 MSB for DMA transactions on UMA */
	uma_msb = read32(IPC_UMA_RANGE_LOWER_1);
	ish_dma_set_msb(PAGING_CHAN, uma_msb, uma_msb);

	dma_init_called++;
}

int ish_dma_copy(uint32_t chan, uint32_t dst, uint32_t src, uint32_t length,
		 dma_mode_t mode)
{
	uint32_t chan_reg = DMA_REG_BASE + (DMA_CH_REGS_SIZE * chan);
	int rc = DMA_RC_OK;
	uint32_t eflags;
	uint32_t chunk;

	__asm__ volatile("\twbinvd\n");

	PM_VNN_DRIVER_REQ(VNN_ID_DMA(chan));

	/*
	 * shut off interrupts to assure no simultanious
	 * access to DMA registers
	 */
	eflags = interrupt_lock();

	write32(MISC_CHID_CFG_REG, chan); /* Set channel to configure */

	mode |= NON_SNOOP;
	write32(MISC_DMA_CTL_REG(chan), mode); /* Set transfer direction */

	write32(DMA_CFG_REG, DMA_EN_MASK); /* Enable DMA module */
	write32(chan_reg + DMA_LLP, 0);    /* Linked lists are not used */
	write32(chan_reg + DMA_CTL,
		0 /* Set transfer parameters */
			| (DMA_CTL_TT_FC_M2M_DMAC << DMA_CTL_TT_FC_SHIFT) |
			(DMA_CTL_ADDR_INC << DMA_CTL_SINC_SHIFT) |
			(DMA_CTL_ADDR_INC << DMA_CTL_DINC_SHIFT) |
			(SRC_TR_WIDTH << DMA_CTL_SRC_TR_WIDTH_SHIFT) |
			(DEST_TR_WIDTH << DMA_CTL_DST_TR_WIDTH_SHIFT) |
			(SRC_BURST_SIZE << DMA_CTL_SRC_MSIZE_SHIFT) |
			(DEST_BURST_SIZE << DMA_CTL_DEST_MSIZE_SHIFT) |
			((chan == KERNEL_CHAN) ? DMA_CTL_INT_EN_MASK : 0));

	interrupt_unlock(eflags);
	while (length) {
		chunk = (length > DMA_MAX_BLOCK_SIZE) ? DMA_MAX_BLOCK_SIZE
						      : length;

		if (rc != DMA_RC_OK) {
			break;
		}

		eflags = interrupt_lock();
		write32(MISC_CHID_CFG_REG, chan); /* Set channel to configure */
		write32(chan_reg + DMA_CTL + 0x4,
			chunk); /* Set number of bytes to transfer */
		write32(chan_reg + DMA_DAR, dst); /* Destination address */
		write32(chan_reg + DMA_SAR, src); /* Source address */
		write32(DMA_EN_REG,		  /* Enable the channel */
			DMA_CH_EN_BIT(chan) | DMA_CH_EN_WE_BIT(chan));
		interrupt_unlock(eflags);

		rc = ish_wait_for_dma_done(
			chan); /* Wait for trans completion */

		dst += chunk;
		src += chunk;
		length -= chunk;

		if (chan == KERNEL_CHAN && dma_err) {
			rc = DMA_RC_HW;
			break;
		}
	}

	PM_VNN_DRIVER_DEREQ(VNN_ID_DMA(chan));
	return rc;
}

void ish_dma_disable(void)
{
	unsigned channel;
	unsigned counter;

	/* Disable DMA on per-channel basis. */
	for (channel = 0; channel <= DMA_MAX_CHANNEL; channel++) {
		write32(MISC_CHID_CFG_REG, channel);
		if (read32(DMA_EN_REG) & DMA_CH_EN_BIT(channel)) {
			/* Write 0 to channel enable bit ... */
			write32(DMA_EN_REG, DMA_CH_EN_WE_BIT(channel));

			/* Wait till it shuts up. */
			counter = 0;
			while ((read32(DMA_EN_REG) & DMA_CH_EN_BIT(channel)) &&
			       counter < (UINT32_MAX / 64))
				counter++;
		}
	}
	write32(DMA_CLR_ERR_REG, 0xFFFFFFFF);
	write32(DMA_CLR_BLOCK_REG, 0xFFFFFFFF);

	write32(DMA_CFG_REG, 0); /* Disable DMA module */
}

int ish_wait_for_dma_done(uint32_t ch)
{
	return POLL_UNTIL_CLEARED(DMA_EN_REG, DMA_CH_EN_BIT(ch));
}

static int ish_dma_page_internal(uint32_t dst, uint32_t src,
				 dma_mode_t dma_mode)
{
	int rc;
	uint32_t eflags = interrupt_lock();

	if (!dma_init_called) {
		ish_dma_init();
	}

	/* Wait for DMA to be free */
	rc = POLL_UNTIL_CLEARED(DMA_EN_REG, DMA_CH_EN_BIT(PAGING_CHAN) |
						    DMA_CH_EN_BIT(KERNEL_CHAN));
	if (rc == DMA_RC_OK) {
		rc = ish_dma_copy(PAGING_CHAN, dst, src, PAGE_SIZE, dma_mode);
	}
	interrupt_unlock(eflags);
	return rc;
}

/* DMA page between DRAM and SRAM. */
int ish_dma_page(uint32_t dst, uint32_t src, int page_in)
{
	int ret = 0;

	ret = ish_dma_page_internal(dst, src,
				    (page_in ? UMA_TO_SRAM : SRAM_TO_UMA));

	return ret;
}

void ish_dma_set_msb(uint32_t chan, uint32_t dst_msb, uint32_t src_msb)
{
	uint32_t eflags = interrupt_lock();
	write32(MISC_CHID_CFG_REG, chan); /* Set channel to configure */
	write32(MISC_SRC_FILLIN_DMA(chan), src_msb);
	write32(MISC_DST_FILLIN_DMA(chan), dst_msb);
	interrupt_unlock(eflags);
}
