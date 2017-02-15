/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
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

mec17xx_dma_chan_t *dma_get_channel(enum dma_channel channel)
{
	mec17xx_dma_regs_t *dma = MEC17XX_DMA_REGS;

	return &dma->chan[channel];
}

void dma_disable(enum dma_channel channel)
{
	mec17xx_dma_chan_t *chan = dma_get_channel(channel);

	if (chan->ctrl & (1 << 0))
		chan->ctrl &= ~(1 << 0);

	if (chan->act == 1)
		chan->act = 0;
}

void dma_disable_all(void)
{
	int ch;
	mec17xx_dma_regs_t *dma;

	for (ch = 0; ch < MEC17XX_DMAC_COUNT; ch++) {
		mec17xx_dma_chan_t *chan = dma_get_channel(ch);
		/* Abort any current transfer. */
		chan->ctrl |= (1 << 25);
		/* Disable the channel. */
		chan->ctrl &= ~(1 << 0);
		chan->act = 0;
	}

	/* Soft-reset the block. */
	dma = MEC17XX_DMA_REGS;
	dma->ctrl |= 0x2;
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
 * @note MEC17xx DMA does not require address aliasing
 */
static void prepare_channel(mec17xx_dma_chan_t *chan, unsigned count,
		void *periph, void *memory, unsigned flags)
{
	int xfer_size = (flags >> 20) & 0x7;

	if (chan->ctrl & (1 << 0))
		chan->ctrl &= ~(1 << 0);

	chan->act |= 0x1;
	chan->dev = (uint32_t)periph;
	chan->mem_start = (uint32_t)memory;
	chan->mem_end = (uint32_t)memory + (xfer_size * count);
	chan->ctrl = flags;
}

void dma_go(mec17xx_dma_chan_t *chan)
{
	/* Flush data in write buffer so that DMA can get the lastest data */
	asm volatile("dsb;");

	/* Fire it up */
	chan->ctrl |= MEC17XX_DMA_RUN;
}

void dma_prepare_tx(const struct dma_option *option, unsigned count,
		    const void *memory)
{
	mec17xx_dma_chan_t *chan = dma_get_channel(option->channel);

	/*
	 * Cast away const for memory pointer; this is ok because we know
	 * we're preparing the channel for transmit.
	 */
	prepare_channel(chan, count, option->periph, (void *)memory,
			MEC17XX_DMA_INC_MEM | MEC17XX_DMA_TO_DEV |
			MEC17XX_DMA_DEV(option->channel) | option->flags);
}

void dma_start_rx(const struct dma_option *option, unsigned count,
		  void *memory)
{
	mec17xx_dma_chan_t *chan;

	chan = dma_get_channel(option->channel);

	prepare_channel(chan, count, option->periph, memory,
			MEC17XX_DMA_INC_MEM | MEC17XX_DMA_DEV(option->channel) |
			option->flags);
	dma_go(chan);
}

int dma_bytes_done(mec17xx_dma_chan_t *chan, int orig_count)
{
	int xfer_size = (chan->ctrl >> 20) & 0x7;

	if (!(chan->ctrl & MEC17XX_DMA_RUN))
		return 0;
	return orig_count - (chan->mem_end - chan->mem_start) / xfer_size;
}

void dma_init(void)
{
	mec17xx_dma_regs_t *dma = MEC17XX_DMA_REGS;
	dma->ctrl |= 0x1;
}

int dma_wait(enum dma_channel channel)
{
	mec17xx_dma_chan_t *chan = dma_get_channel(channel);
	timestamp_t deadline;

	if (chan->act == 0)
		return EC_SUCCESS;

	deadline.val = get_time().val + DMA_TRANSFER_TIMEOUT_US;
	while (!(chan->int_status & 0x4)) {
		if (deadline.val <= get_time().val)
			return EC_ERROR_TIMEOUT;

		udelay(DMA_POLLING_INTERVAL_US);
	}
	return EC_SUCCESS;
}

void dma_clear_isr(enum dma_channel channel)
{
	mec17xx_dma_chan_t *chan = dma_get_channel(channel);

	chan->int_status |= 0x4;
}
