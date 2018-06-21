/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Transfer bootblock over SPI by emulating eMMC protocol.
 */
#ifdef SECTION_IS_RO
#include "bootblock.h"
#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "task.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

static stm32_spi_regs_t *const spi_emmc = STM32_SPI2_REGS;
/* 1024 bytes of buffer is enough for ~0.6ms @ 13Mhz */
#define SPI_RX_BUF_SIZE 1024
#define SPI_RX_BUF_SIZE_32 (SPI_RX_BUF_SIZE/4)
static uint32_t in_msg[SPI_RX_BUF_SIZE_32] __aligned(4);

/* Macros to advance in the circular buffer */
#define RX_BUF_NEXT_32(i) (((i) + 1) & (SPI_RX_BUF_SIZE_32 - 1))
#define RX_BUF_DEC_32(i, j) (((i) - (j)) & (SPI_RX_BUF_SIZE_32 - 1))
#define RX_BUF_PREV_32(i) RX_BUF_DEC_32((i), 1)

static int bootblock_transfer_try;

enum emmc_cmd {
	EMMC_ERROR = -1,
	EMMC_IDLE = 0,
	EMMC_PRE_IDLE,
	EMMC_BOOT,
};

static const struct dma_option dma_tx_option = {
	STM32_DMAC_SPI2_TX, (void *)&STM32_SPI2_REGS->dr,
	STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT
};

static const struct dma_option dma_rx_option = {
	STM32_DMAC_SPI2_RX, (void *)&STM32_SPI2_REGS->dr,
	STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT |
	STM32_DMA_CCR_CIRC
};

static void bootblock_transfer(void)
{
	dma_chan_t *txdma = dma_get_channel(STM32_DMAC_SPI2_TX);

	dma_prepare_tx(&dma_tx_option, bootblock_get_size(),
		       bootblock_get_data());
	dma_go(txdma);

	bootblock_transfer_try++;
	CPRINTS("eMMC transfer(%d)", bootblock_transfer_try);
}

static void bootblock_stop(void)
{
	dma_disable(STM32_DMAC_SPI2_TX);
	while ((spi_emmc->sr & STM32_SPI_SR_FTLVL) != 0)
		;
	spi_emmc->dr = 0xff;
	spi_emmc->dr = 0xff;
	spi_emmc->dr = 0xff;
	spi_emmc->dr = 0xff;
}

static uint32_t letobe(uint32_t val)
{
	return (val & 0x000000ff) << 24 | (val & 0x0000ff00) << 8 |
	       (val & 0x00ff0000) >> 8  | (val & 0xff000000) >> 24;
}

static enum emmc_cmd emmc_parse_command(int index)
{
	int32_t shift0, mask1;
	uint32_t data[3];

	mask1 = 0x00000000;

	/* Figure out alignment (cmd starts with 01) */
	if (in_msg[index] == 0xffffffff)
		return EMMC_ERROR;

	data[0] = letobe(in_msg[index]);
	index = RX_BUF_NEXT_32(index);
	data[1] = letobe(in_msg[index]);
	index = RX_BUF_NEXT_32(index);
	data[2] = letobe(in_msg[index]);

	/* Number of leading ones. */
	shift0 = __builtin_clz(~data[0]);

	if (shift0 > 0)
		mask1 = (int32_t)0x80000000 >> (shift0-1);

	data[0] = (data[0] << shift0) | ((data[1] & mask1) >> (32-shift0));
	data[1] = (data[1] << shift0) | ((data[2] & mask1) >> (32-shift0));

	if (data[0] == 0x40000000 && data[1] == 0x0095ffff) {
		/* 400000000095 GO_IDLE_STATE */
		CPRINTS("eMMC goIdle");
		return EMMC_IDLE;
	}

	if (data[0] == 0x40f0f0f0 && data[1] == 0xf0fdffff) {
		/* 40f0f0f0f0fd GO_PRE_IDLE_STATE */
		CPRINTS("eMMC goPreIdle");
		return EMMC_PRE_IDLE;
	}

	if (data[0] == 0x40ffffff && data[1] == 0xfae5ffff) {
		/* 40fffffffae5 BOOT_INITIATION */
		CPRINTS("eMMC bootInit");
		return EMMC_BOOT;
	}

	CPRINTS("eMMC error");
	return EMMC_ERROR;
}

void emmc_cmd_interrupt(enum gpio_signal signal)
{
	task_wake(TASK_ID_EMMC);
}

void emmc_task(void *u)
{
	/* Both are 32-bit indexes. */
	int dma_pos, i;
	dma_chan_t *rxdma = dma_get_channel(STM32_DMAC_SPI2_RX);
	int tx = 0;
	enum emmc_cmd cmd;

	gpio_enable_interrupt(GPIO_EMMC_CMD);

	dma_start_rx(&dma_rx_option, sizeof(in_msg), in_msg);

	/* Enable internal chip select */
	spi_emmc->cr1 &= ~STM32_SPI_CR1_SSI;

	while (1) {
		/* Wait for a command */
		task_wait_event(-1);

		/* Since we round down, in theory we sould not  */
		dma_pos = dma_bytes_done(rxdma, sizeof(in_msg)) / 4;
		i = RX_BUF_PREV_32(dma_pos);

		/*
		 * By now, bus should be idle again (it takes <10us to transmit
		 * a command).
		 */
		if (in_msg[i] != 0xffffffff) {
			CPRINTF("?");
			/* TODO: We should probably just retry. */
			continue;
		}

		/* Look for a command, from the end of the buffer. */
		while (i != dma_pos && in_msg[i] == 0xffffffff)
			i = RX_BUF_PREV_32(i);

		/* We missed the command? */
		if (i == dma_pos) {
			CPRINTF("!");
			continue;
		}

		/* We found the end, now find the beginning. */
		i = RX_BUF_DEC_32(i, 2);
		while (i != dma_pos && in_msg[i] == 0xffffffff)
			i = RX_BUF_NEXT_32(i);

		cmd = emmc_parse_command(i);

		/* When not transferring, all we care about is EMMC_BOOT. */
		if (!tx) {
			if (cmd == EMMC_BOOT) {
				tx = 1;
				bootblock_transfer();
			}
		} else {
			/* Abort (should be Idle, but react to Pre-Idle too) */
			if (cmd == EMMC_IDLE || cmd == EMMC_PRE_IDLE) {
				bootblock_stop();
				tx = 0;
			}
		}
	}
}
#endif /* SECTION_IS_RO */
