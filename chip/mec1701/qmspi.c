/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* QMSPI master module for MEC1701 */

#include "common.h"
#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "registers.h"
#include "spi.h"
#include "timer.h"
#include "util.h"
#include "hooks.h"
#include "task.h"
#include "qmspi_chip.h"

#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)

#define QMSPI_BYTE_TRANSFER_TIMEOUT_US (3 * MSEC)
#define QMSPI_BYTE_TRANSFER_POLL_INTERVAL_US 100


static const struct dma_option qmspi_rx_option[1] = {
	{
		MEC17XX_DMAC_QMSPI0_RX,
		(void *)(MEC17XX_QMSPI0_RX_FIFO_ADDR),
		MEC17XX_DMA_XFER_SIZE(1) + MEC17XX_DMA_INC_MEM
	},
};


#ifdef LFW
/*
 * MEC17xx 32-bit timer 0 configured for 1us count down mode and no interrupt
 * in LFW environment. Don't need to sleep CPU in LFW.
 */
static int qmspi_wait(uint32_t mask, uint32_t mval)
{
	uint32_t t1, t2, td;

	t1 = MEC17XX_TMR32_CNT(0);

	while ((MEC17XX_QMSPI0_STS & mask) != mval) {
		t2 = MEC17XX_TMR32_CNT(0);
		if (t1 >= t2)
			td = t1 - t2;
		else
			td = t1 + (0xfffffffful - t2);
		if (td > QMSPI_BYTE_TRANSFER_TIMEOUT_US)
			return EC_ERROR_TIMEOUT;
	}
	return EC_SUCCESS;
}
#else

/*
 * This version uses the full EC_RO/RW timer infrastructure and it needs
 * a timer ISR to handle timer underflow. Without the ISR we observe false
 * timeouts when debugging with JTAG.
 */
static int qmspi_wait(uint32_t mask, uint32_t mval)
{
	timestamp_t deadline;

	deadline.val = get_time().val + (QMSPI_BYTE_TRANSFER_TIMEOUT_US);

	while ((MEC17XX_QMSPI0_STS & mask) != mval) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;

		usleep(QMSPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}
#endif

/*
 * Transmit using QMSPI TX FIFO (depth = 8 bytes)
 * Poll with timeout for TX FIFO not full until
 * all bytes are written to TX FIFO.
 * Then poll for transmit done.
 * If close then stop QMSPI causing chip select to de-assert.
 */
static int qmspi_tx(const uint8_t *txdata, int txlen, int close)
{
	uint32_t ctrl, i, n;
	int rc;

	if (txlen == 0)
		return EC_SUCCESS;

	if (txdata == NULL)
		return EC_ERROR_INVAL;

	ctrl = (MEC17XX_QMSPI_CTRL_1X + MEC17XX_QMSPI_CTRL_TX_DATA +
		MEC17XX_QMSPI_CTRL_XFRU_1B);

	MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_CLR_FIFOS;

	while (txlen > 0) {
		n = (uint32_t)txlen;
		if (n > MEC17XX_QMSPI_CTRL_MAX_UNITS)
			n = MEC17XX_QMSPI_CTRL_MAX_UNITS;
		ctrl = n << MEC17XX_QMSPI_CTRL_NUM_UNITS_BITPOS;
		ctrl |= (MEC17XX_QMSPI_CTRL_1X + MEC17XX_QMSPI_CTRL_TX_DATA +
			MEC17XX_QMSPI_CTRL_XFRU_1B);
		MEC17XX_QMSPI0_CTRL = ctrl;
		MEC17XX_QMSPI0_STS = 0xfffffffful;
		MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_START;
		for (i = 0; i < n; i++) {
			MEC17XX_QMSPI0_TX_FIFO8 = *txdata++;
			rc = qmspi_wait(MEC17XX_QMSPI_STS_TX_BUFF_FULL, 0);
			if (rc != EC_SUCCESS) {
				MEC17XX_QMSPI0_EXE =
						MEC17XX_QMSPI_EXE_STOP;
				return EC_ERROR_TIMEOUT;
			}
		}

		rc = qmspi_wait(MEC17XX_QMSPI_STS_DONE, MEC17XX_QMSPI_STS_DONE);
		if (rc != EC_SUCCESS) {
			MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_STOP;
			return EC_ERROR_TIMEOUT;
		}

		txlen -= (int)(ctrl >> MEC17XX_QMSPI_CTRL_NUM_UNITS_BITPOS);
	}


	if (close) /* de-assert chip select */
		MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_STOP;

	return EC_SUCCESS;
}

/*
 * Called from mec1701/spi.c
 * Wait for QMSPI read using DMA to complete.
 * wait for QMSPI to read extra byte into its RX FIFO before
 * it notices DMA has stopped. We will spin on QMSPI read-only
 * Transfer Active status bit as it will clear when QMSPI closes
 * the transaction(de-asserts chip select).
 */
int qmspi_transaction_flush(const struct spi_device_t *spi_device)
{
	int ret;
	timestamp_t deadline;

	/* DMA subsystem has 100 ms timeout */
	ret = dma_wait(qmspi_rx_option[0].channel);
	if (ret != EC_SUCCESS)
		return ret;

	dma_disable(qmspi_rx_option[0].channel);
	dma_clear_isr(qmspi_rx_option[0].channel);

	deadline.val = get_time().val + QMSPI_BYTE_TRANSFER_TIMEOUT_US;

	while (MEC17XX_QMSPI0_STS & MEC17XX_QMSPI_STS_ACTIVE) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(QMSPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}

	/* clear QMSPI FIFO's */
	MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_CLR_FIFOS;
	MEC17XX_QMSPI0_STS = 0xffffffff;

	return ret;
}

/*
 * Called from mec1701/spi.c
 * Start an asynchronous SPI transaction.
 * SPI read command and address sent synchronously.
 * Data read asynchronously.
 * Use QMSPI DMA unlimited mode, QMSPI number of units field == 0.
 * DMA channel knows how many bytes are being read. QMSPI will
 * read ahead one byte resulting one extra byte being read into
 * QMSPI RX FIFO after DMA channel has stopped.
 * QMSPI RX FIFO status will be not empty.
 * DMA channel control register will have REQ status set due to
 * remaining byte in QMSPI RX FIFO.
 * The other option is to program transfer length in QMSPI but this
 * requires using descriptor mode. MEC17xx QMSPI has 5 descriptors each
 * with a 15-bit number of units field. For 1-byte units this equates to
 * a maximum transfer length of 5 * 0x7FFF = 0x27FFB (163835). This is not
 * large enough to read firmware! If we could ensure > 4-byte aligned
 * destination address and rxlen a multiple of 4 then we could use 4-byte
 * DMA mode increasing size to 5 * 4 * 0x7FFF = 0x9FFEC (655340).
 */
int qmspi_transaction_async(const struct spi_device_t *spi_device,
				const uint8_t *txdata, int txlen,
				uint8_t *rxdata, int rxlen)
{
	int close, ret;
	struct dma_option dmaop;

	/* soft reset the controller */
	MEC17XX_QMSPI0_MODE_ACT_SRST = MEC17XX_QMSPI_M_SOFT_RESET;
	MEC17XX_QMSPI0_MODE_ACT_SRST;
	MEC17XX_QMSPI0_MODE_FDIV = spi_device->div;
	MEC17XX_QMSPI0_MODE_ACT_SRST = MEC17XX_QMSPI_M_ACTIVATE;

	close = 1;
	if (rxlen > 0)
		close = 0;

	ret = qmspi_tx(txdata, txlen, close);
	if (ret != EC_SUCCESS)
		return ret;

	if (rxlen > 0) {
		dmaop.channel = qmspi_rx_option[0].channel;
		dmaop.periph = qmspi_rx_option[0].periph;
		dmaop.flags = qmspi_rx_option[0].flags;

		MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_CLR_FIFOS;
		MEC17XX_QMSPI0_STS = 0xffffffff;
		MEC17XX_QMSPI0_CTRL = (MEC17XX_QMSPI_CTRL_1X +
				MEC17XX_QMSPI_CTRL_RX_EN +
				MEC17XX_QMSPI_CTRL_CLOSE +
				MEC17XX_QMSPI_CTRL_XFRU_1B);

		if ((((uint32_t)rxdata | rxlen) & 0x03) == 0) {
			dmaop.flags &= ~(MEC17XX_DMA_XFER_SIZE_MASK);
			dmaop.flags |= MEC17XX_DMA_XFER_SIZE(4);
			MEC17XX_QMSPI0_CTRL |= MEC17XX_QMSPI_CTRL_RX_DMA_4B;
		} else
			MEC17XX_QMSPI0_CTRL |= MEC17XX_QMSPI_CTRL_RX_DMA_1B;

		dma_start_rx(&dmaop, (uint32_t)rxlen, rxdata);

		MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_START;
	}

	return ret;
}


/*
 * called by spi_enable in mec1701/spi.c
 */
int qmspi_enable(int port, int enable)
{
	uint8_t dummy __attribute__((unused)) = 0;

	if (enable) {
		gpio_config_module(MODULE_SPI_FLASH, 1);
		MEC17XX_QMSPI0_MODE_ACT_SRST = MEC17XX_QMSPI_M_SOFT_RESET;
		dummy = MEC17XX_QMSPI0_MODE_ACT_SRST;
		MEC17XX_QMSPI0_MODE = (MEC17XX_QMSPI_M_ACTIVATE +
				MEC17XX_QMSPI_M_SPI_MODE0 +
				MEC17XX_QMSPI_M_SPI_CLKDIV_12M);
	} else {
		gpio_config_module(MODULE_SPI_FLASH, 0);
		MEC17XX_QMSPI0_MODE_ACT_SRST = MEC17XX_QMSPI_M_SOFT_RESET;
		dummy = MEC17XX_QMSPI0_MODE_ACT_SRST;
		MEC17XX_QMSPI0_MODE_ACT_SRST = 0;
	}

	return EC_SUCCESS;
}

