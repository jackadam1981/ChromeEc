/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map and API for STM32L processor dma registers
 */

#ifndef __STM32L_DMA
#define __STM32L_DMA

#include <stdint.h>

/*
 * Available DMA channels, numbered from 0
 *
 * TODO(sjg): Should we number these from 0?
 */
enum {
	DMAC_ADC,
	DMAC_SPI1_RX,
	DMAC_SPI1_TX,
	DMAC_SPI2_RX,
	DMAC_SPI2_TX,

	/* DMA1 has 7 channels, DMA2 has 5 */
	DMA_NUM_CHANNELS_1 = 7,
	DMA_NUM_CHANNELS_2 = 5,
	DMA_NUM_CHANNELS = DMA_NUM_CHANNELS_1 + DMA_NUM_CHANNELS_2,
};

/* A single channel of the DMA controller */
struct dma_channel {
	uint32_t	ccr;		/* Control */
	uint32_t	cndtr;		/* Number of data to transfer */
	uint32_t	cpar;		/* Peripheral address */
	uint32_t	cmar;		/* Memory address */
	uint32_t	reserved;
};

/* Registers for the DMA controller */
struct dma_ctlr {
	uint32_t	isr;
	uint32_t	ifcr;
	struct dma_channel chan[DMA_NUM_CHANNELS];
};

/* Defines for accessing DMA ccr */
#define DMA_PL_SHIFT		12
#define DMA_PL_MASK		(3 << DMA_PL_SHIFT)
enum {
	DMA_PL_LOW,
	DMA_PL_MEDIUM,
	DMA_PL_HIGH,
	DMA_PL_VERY_HIGH,
};

#define DMA_MINC_MASK		(1 << 7)
#define DMA_DIR_FROM_MEM_MASK	(1 << 4)
#define DMA_EN			(1 << 0)

/**
 * Start a DMA transfer to transmit data from memory to a peripheral
 *
 * @param channel	Channel number to read (DMAC_...)
 * @param count		Number of bytes to transfer
 * @param periph	Pointer to peripheral data register
 * @param memory	Pointer to memory address
 */
int dma_start_tx(unsigned channel, unsigned count, void *periph,
		 const void *memory);

/**
 * Start a DMA transfer to receive data to memory from a peripheral
 *
 * @param channel	Channel number to read (DMAC_...)
 * @param count		Number of bytes to transfer
 * @param periph	Pointer to peripheral data register
 * @param memory	Pointer to memory address
 */
int dma_start_rx(unsigned channel, unsigned count, void *periph,
		 const void *memory);

/**
 * Testing: Print out the data transferred by a channel
 *
 * @param channel	Channel number to read (DMAC_...)
 * @param buff		Start of DMA buffer
 */
void dma_check(int channel, char *buff);

/**
 * Testing: Test that DMA works correctly for memory to memory transfers
 */
void dma_test(void);

/**
 * Init DMA peripheral ready for use
 */
void dma_init(void);

#endif
