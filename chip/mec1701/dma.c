/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "dma.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_DMA, outstr)
#define CPRINTS(format, args...) cprints(CC_DMA, format, ## args)

dma_chan_t *dma_get_channel(enum dma_channel channel)
{
	dma_chan_t *pd = NULL;

	if (channel < MEC17XX_DMAC_COUNT) {
		pd = (dma_chan_t *)(MEC17XX_DMA_BASE + MEC17XX_DMA_CH_OFS +
				(channel << MEC17XX_DMA_CH_OFS_BITPOS));
	}

	return pd;
}

void dma_disable(enum dma_channel channel)
{
	if (channel < MEC17XX_DMAC_COUNT) {
		if (MEC17XX_DMA_CH_CTRL(channel) & MEC17XX_DMA_RUN)
			MEC17XX_DMA_CH_CTRL(channel) &= ~(MEC17XX_DMA_RUN);

		if (MEC17XX_DMA_CH_ACT(channel)	& MEC17XX_DMA_ACT_EN)
			MEC17XX_DMA_CH_ACT(channel) = 0;
	}
}

void dma_disable_all(void)
{
	uint16_t ch;

	for (ch = 0; ch < MEC17XX_DMAC_COUNT; ch++) {
		/* Abort any current transfer. */
		MEC17XX_DMA_CH_CTRL(ch) |= MEC17XX_DMA_ABORT;
		/* Disable the channel. */
		MEC17XX_DMA_CH_CTRL(ch) &= ~(MEC17XX_DMA_RUN);
		MEC17XX_DMA_CH_ACT(ch) = 0;
	}

	/* Soft-reset the block. */
	MEC17XX_DMA_MAIN_CTRL = MEC17XX_DMA_MAIN_CTRL_SRST;
	MEC17XX_DMA_MAIN_CTRL;
	MEC17XX_DMA_MAIN_CTRL = MEC17XX_DMA_MAIN_CTRL_ACT;
}

/**
 * Prepare a channel for use and start it
 *
 * @param chan		Channel to read
 * @param count		Number of bytes to transfer
 * @param periph	Pointer to peripheral data register
 * @param memory	Pointer to memory address for receive/transmit
 * @param flags		DMA flags for the control register, normally:
 *				MEC17XX_DMA_INC_MEM | MEC17XX_DMA_TO_DEV for tx
 *				MEC17XX_DMA_INC_MEM for rx
 *				Plus transfer unit length(1, 2, or 4) in
 *				bits[22:20]
 * @note MEC17xx DMA does not require address aliasing. Because count is the
 * number of bytes to transfer memory start - memory end = count.
 */
static void prepare_channel(enum dma_channel ch, unsigned count,
		void *periph, void *memory, unsigned flags)
{
	if (ch < MEC17XX_DMAC_COUNT) {

		MEC17XX_DMA_CH_CTRL(ch) = 0;
		MEC17XX_DMA_CH_MEM_START(ch) = (uint32_t)memory;
		MEC17XX_DMA_CH_MEM_END(ch) = (uint32_t)memory + count;

		MEC17XX_DMA_CH_DEV_ADDR(ch) = (uint32_t)periph;

		MEC17XX_DMA_CH_CTRL(ch) = flags;
		MEC17XX_DMA_CH_ACT(ch) = MEC17XX_DMA_ACT_EN;
	}
}

void dma_go(dma_chan_t *chan)
{
	/* Flush data in write buffer so that DMA can get the lastest data */
	asm volatile("dsb;");

	if (chan != NULL)
		chan->ctrl |= MEC17XX_DMA_RUN;
}

void dma_go_chan(enum dma_channel ch)
{
	asm volatile("dsb;");
	if (ch < MEC17XX_DMAC_COUNT)
		MEC17XX_DMA_CH_CTRL(ch) |= MEC17XX_DMA_RUN;
}

void dma_prepare_tx(const struct dma_option *option, unsigned count,
		    const void *memory)
{
	if (option != NULL)
		/*
		 * Cast away const for memory pointer; this is ok because we
		 * know we're preparing the channel for transmit.
		 */
		prepare_channel(option->channel, count, option->periph,
			(void *)memory,
			MEC17XX_DMA_INC_MEM |
			MEC17XX_DMA_TO_DEV |
			MEC17XX_DMA_DEV(option->channel) |
			option->flags);

}

void dma_start_rx(const struct dma_option *option, unsigned count,
		  void *memory)
{
	if (option != NULL) {
		prepare_channel(option->channel, count, option->periph,
				memory,
				MEC17XX_DMA_INC_MEM |
				MEC17XX_DMA_DEV(option->channel) |
				option->flags);
		dma_go_chan(option->channel);
	}
}

/*
 * Return the number of bytes transferred.
 * The number of bytes transferred can be easily determinted
 * from the difference in DMA memory start address register
 * and memory end address register. No need to look at DMA
 * transfer size field because the hardware increments memory
 * start address by unit size on each unit tranferred.
 * Why is a signed integer being used for a count value?
 */
int dma_bytes_done(dma_chan_t *chan, int orig_count)
{
	int bcnt = 0;

	if (chan != NULL) {
		if (chan->ctrl & MEC17XX_DMA_RUN)
			bcnt = (int)chan->mem_end;
			bcnt -= (int)chan->mem_start;
			bcnt = orig_count - bcnt;
	}

	return bcnt;
}

int dma_bytes_done_chan(enum dma_channel ch, uint32_t orig_count)
{
	uint32_t cnt;

	cnt = 0;
	if (ch < MEC17XX_DMAC_COUNT)
		if (MEC17XX_DMA_CH_CTRL(ch) & MEC17XX_DMA_RUN)
			cnt = (uint32_t)orig_count -
					(MEC17XX_DMA_CH_MEM_END(ch) -
					 MEC17XX_DMA_CH_MEM_START(ch));

	return (int)cnt;
}

/*
 * Initialize DMA block.
 * Soft-Reset block should clear after one clock but read-back to
 * be safe.
 * Set block activate bit after reset.
 */
void dma_init(void)
{
	MEC17XX_DMA_MAIN_CTRL = MEC17XX_DMA_MAIN_CTRL_SRST;
	MEC17XX_DMA_MAIN_CTRL;
	MEC17XX_DMA_MAIN_CTRL = MEC17XX_DMA_MAIN_CTRL_ACT;
}

int dma_wait(enum dma_channel channel)
{
	timestamp_t deadline;

	if (channel < MEC17XX_DMAC_COUNT) {
		if (MEC17XX_DMA_CH_ACT(channel) == 0)
			return EC_SUCCESS;

		deadline.val = get_time().val + DMA_TRANSFER_TIMEOUT_US;

		while (!(MEC17XX_DMA_CH_ISTS(channel) &
			 MEC17XX_DMA_STS_DONE)) {

			if (deadline.val <= get_time().val)
				return EC_ERROR_TIMEOUT;

			udelay(DMA_POLLING_INTERVAL_US);
		}
		return EC_SUCCESS;
	}

	return EC_ERROR_INVAL;
}

/*
 * Clear all interrupt status in specified DMA channel
 */
void dma_clear_isr(enum dma_channel channel)
{
	if (channel < MEC17XX_DMAC_COUNT)
		MEC17XX_DMA_CH_ISTS(channel) = 0x0f;
}
