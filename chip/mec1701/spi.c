/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SPI master module for MEC17xx QMSPI */

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

#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)

#define SPI_BYTE_TRANSFER_TIMEOUT_US (3 * MSEC)
#define SPI_BYTE_TRANSFER_POLL_INTERVAL_US 100

/*
 * Receive Device ID's for DMA Channels
 * These numbers are also used as DMA channel number.
*/
#define SPI_RX_DMA_CHANNEL(port) (MEC17XX_DMAC_SPI0_RX + ((uint32_t)(port) << 1))
#define SPI_TX_DMA_CHANNEL(port) (MEC17XX_DMAC_SPI0_TX + ((uint32_t)(port) << 1))


/* only regular image needs mutex, LFW does not have scheduling */
/* TODO: Move SPI locking to common code */

#ifndef LFW
static struct mutex spi_mutex;
#endif

/* Support all QMSPI controller with one port only.
 * port 0 = QMSPI SHD_SPI Port
 * Reason is this file is compiled into lfw and must 
 * be kept small.
 * Make sure CONFIG_SPI_FLASH_PORT = 0 in board.h
 */
#if 1
static const struct dma_option spi_tx_option[] = {
	{
		SPI_TX_DMA_CHANNEL(2),
		(void *)&MEC17XX_QMSPI0_TX_FIFO8,
		MEC17XX_DMA_XFER_SIZE(1)
	}
};
#endif

static const struct dma_option spi_rx_option[] = {
	{
		SPI_RX_DMA_CHANNEL(2),
		(void *)&MEC17XX_QMSPI0_RX_FIFO8,
		MEC17XX_DMA_XFER_SIZE(1)
	},
};


static void soft_reset(int port)
{
	if (0 == port) {
		MEC17XX_QMSPI0_MODE = MEC17XX_QMSPI_M_SOFT_RESET;
		/* delay by read back and write to read-only */
		MEC17XX_QMSPI0_BUFCNT_STS = MEC17XX_QMSPI0_MODE;
		MEC17XX_QMSPI0_MODE = (MEC17XX_QMSPI_M_ACTIVATE + \
			       MEC17XX_QMSPI_M_SPI_MODE0 + \
			       MEC17XX_QMSPI_M_SPI_CLKDIV_12M);
	}
}


/*
 * Wait up to SPI_BYTE_TRANSFER_TIMEOUT_US for QMSPI TX FIFO
 * buffer full status to clear. QMSPI.Status.TransmitBufferFull is
 * read-only.
 */
#if 0 /* Not used */
static int qmspi_wait_tx_fifo_not_full(void)
{
	timestamp_t deadline;

	deadline.val = get_time().val + SPI_BYTE_TRANSFER_TIMEOUT_US;
	while (0 != (MEC17XX_QMSPI0_STS & MEC17XX_QMSPI_STS_TX_BUFF_FULL)) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}
#endif

/*
 * Wait up to SPI_BYTE_TRANSFER_TIMEOUT_US for QMSPI TX FIFO
 * empty status to be set. QMSPI.Status.TransmitBufferEmpty is
 * read-only.
 */
static int qmspi_wait_tx_fifo_empty(void)
{
	timestamp_t deadline;

	deadline.val = get_time().val +
		(SPI_BYTE_TRANSFER_TIMEOUT_US * MEC17XX_QMSPI_TX_FIFO_LEN);
	while (0 == (MEC17XX_QMSPI0_STS & MEC17XX_QMSPI_STS_DONE)) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}


/*
 *
 */
static int qmspi_tx_dma(const uint8_t *txdata, int txlen, uint32_t flags)
{
	int ret = EC_SUCCESS;
	mec17xx_dma_chan_t *chan = dma_get_channel(spi_tx_option[0].channel);

	if ((NULL != txdata) && (0 != txlen)) {
		MEC17XX_QMSPI0_CTRL = MEC17XX_QMSPI_CTRL_1X +
			MEC17XX_QMSPI_CTRL_TX_DATA +
			MEC17XX_QMSPI_CTRL_TX_DMA_1B +
			MEC17XX_QMSPI_CTRL_XFRU_1B;

		if (flags & (1ul << 8)) {
			MEC17XX_QMSPI0_CTRL |= MEC17XX_QMSPI_CTRL_CLOSE;
		}

		dma_prepare_tx(&spi_tx_option[0], txlen, txdata);

		MEC17XX_QMSPI0_STS = 0xffffffff;
		MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_START;

		dma_go(chan);

		ret = dma_wait(spi_tx_option[0].channel);
		if (EC_SUCCESS != ret)
			return ret;

		/* DMA is done wait for QMSPI TX FIFO to empty */
		ret = qmspi_wait_tx_fifo_empty();

	}

	return ret;
}

/*
 * Transmit in chunks of MEC17XX_QMSPI_TX_FIFO_LEN.
 * If close != 0 then chip select will be de-asserted when transmit is done.
 * On timeout error force QMSPI to stop and de-assert chip select.
 * flags for QMSPI:
 *	b[0] = 0(full duplex), 1(dual)
 *	b[7:1] = 0
 *	b[8] = 0(leave open), 1(close)
 *	b[31:9] = 0 reserved
 *
 * NOTES: this routine is called with txdata containing SPI opcode +
 * parameters.
 * SPI opcode could be SPI write enable, SPI read status,
 * SPI write status, SPI read data, SPI write data, etc.
 * All the commands fill fit in the 8-byte TX FIFO except for
 * SPI write data.
 *
 * SPI command = txdata[0]
 * Transmit up to 8-bytes(TX FIFO length)
 * if txdata[0] == 0x0B or 0x3B then
 *	transmit 8 dummy clocks with all IO pins tri-stated
 * if close flag set then
 *
 * Using Descriptors:
 *	Page Program (02h)
 *		Control register for DMA write of txdata containing
 *			opcode, 24-bit address, and up to 256 bytes of data with DMA
 *	Write Status 1 or 2, other 2-byte commands
 *		Control register for opcode + 1 data byte
 *	Read Status 1 or 2, Write Enable, Write Disable other 1-byte commands
 *		Control register for opcode
 *	SPI Read 1-1-1 (03h)
 *		Control register for FIFO write of opcode + 24-bit address = 4 bytes
 *	SPI Read 1-1-1-fast (0Bh)
 *		2 descriptors
 *			opcode + 24-bit address
 *			8 dummy clocks
 *	SPI Read 1-1-2-fast (3Bh)
 *		2 descriptors
 *			opcode +  24-bit address
 *			8 dummy clocks
 *
 */
static int qmspi_tx(const uint8_t *txdata, int txlen, uint32_t flags)
{
	uint32_t i, ndescr, ndummy;
	int ret = EC_SUCCESS;

	if ((NULL != txdata) && (txlen > 0)) {
		MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_CLR_FIFOS;
		MEC17XX_QMSPI0_STS = 0xffffffff;

		switch (txdata[0]) {
		case 0x0B: /* Read 1-1-1-fast */
			ndescr = 2;
			ndummy = 1;
			break;
		case 0x3B: /* Read 1-1-2 fast */
			ndescr = 2;
			ndummy = 2;
			break;
		default:
			if (txlen < MEC17XX_QMSPI_TX_FIFO_LEN) {
				ndescr = 1;
				ndummy = 0;
			} else {
				return qmspi_tx_dma(txdata, txlen, flags);
			}
			break;
		}

		i = 0;

		MEC17XX_QMSPI0_CTRL = MEC17XX_QMSPI_CTRL_DESCR_MODE_EN;
		MEC17XX_QMSPI0_DESCR(0) = MEC17XX_QMSPI_CTRL_1X +
			MEC17XX_QMSPI_CTRL_TX_DATA +
			MEC17XX_QMSPI_CTRL_XFRU_1B +
			(1ul << MEC17XX_QMSPI_CTRL_NEXT_DESCR_BITPOS) +
			((uint32_t)txlen << MEC17XX_QMSPI_CTRL_NUM_UNITS_BITPOS);

		switch (ndummy) {
		case 1:
			MEC17XX_QMSPI0_DESCR(1) = MEC17XX_QMSPI_CTRL_1X +
				MEC17XX_QMSPI_CTRL_TX_DIS +
				MEC17XX_QMSPI_CTRL_XFRU_1B +
				MEC17XX_QMSPI_CTRL_DESCR_LAST +
				(2ul << MEC17XX_QMSPI_CTRL_NEXT_DESCR_BITPOS)+
				(1ul << MEC17XX_QMSPI_CTRL_NUM_UNITS_BITPOS);
			break;
		case 2:
			MEC17XX_QMSPI0_DESCR(1) = MEC17XX_QMSPI_CTRL_2X +
				MEC17XX_QMSPI_CTRL_TX_DIS +
				MEC17XX_QMSPI_CTRL_XFRU_1B +
				MEC17XX_QMSPI_CTRL_DESCR_LAST +
				(2ul << MEC17XX_QMSPI_CTRL_NEXT_DESCR_BITPOS)+
				(2ul << MEC17XX_QMSPI_CTRL_NUM_UNITS_BITPOS);
			break;
		default:
			MEC17XX_QMSPI0_DESCR(0) |= MEC17XX_QMSPI_CTRL_DESCR_LAST;
			break;
		}

		if (flags & (1ul << 8)) { /* close? */
			MEC17XX_QMSPI0_DESCR(ndescr - 1) |= MEC17XX_QMSPI_CTRL_CLOSE;
		}

		/* fill TX FIFO */
		for (i = 0; i < (uint32_t)txlen; i++) {
			MEC17XX_QMSPI0_TX_FIFO8 = txdata[i];
		}

		MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_START;

		ret = qmspi_wait_tx_fifo_empty();
		if (EC_SUCCESS != ret) {
			MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_STOP;
			return ret;
		}
	}

	return ret;
}

/*
* flags for QMSPI:
*	b[0] = 0(full duplex), 1(dual)
*	b[7:1] = 0
*	b[8] = 0(leave open), 1(close)
*	b[31:9] = 0 reserved
 */
static int spi_tx(const int port, const uint8_t *txdata, int txlen, uint32_t flags)
{
	int ret;

	if (0 == port) {
		ret = qmspi_tx(txdata, txlen, flags);
	} else {
		ret = EC_ERROR_UNIMPLEMENTED;
	}

	return ret;
}


/*
 * Configure and start QMSPI receive using unlimited DMA mode.
 * Unlimited mode means QMSPI number of transfer units is set to 0
 * and the DMA channel controls the number of bytes transferred to
 * memory. Due to QMSPI not knowing the number of bytes it will issue
 * one byte's worth of extra clocks once DMA has finished.
 * This routine will leave SPI chip select asserted.
 * flags for QMSPI:
 *	b[0] = 0(full duplex), 1(dual)
 *	b[7:1] = 0
 *	b[8] = 0(leave open), 1(close)
 *	b[31:9] = 0 reserved
 */
static int qmspi_rx_dma(const int port, uint8_t *rxdata, int rxlen, uint32_t flags)
{
	int ret = EC_SUCCESS;

	if (rxlen) {
		MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_CLR_FIFOS;
		MEC17XX_QMSPI0_STS = 0xffffffff;

		MEC17XX_QMSPI0_CTRL = (MEC17XX_QMSPI_CTRL_RX_EN +
				MEC17XX_QMSPI_CTRL_RX_DMA_1B +
				MEC17XX_QMSPI_CTRL_XFRU_1B);

		if (flags & (1ul << 0)) {
			MEC17XX_QMSPI0_CTRL |= MEC17XX_QMSPI_CTRL_2X;
		} else {
			MEC17XX_QMSPI0_CTRL |= MEC17XX_QMSPI_CTRL_1X;
		}

		if (flags & (1ul << 8)) {
			MEC17XX_QMSPI0_CTRL |= MEC17XX_QMSPI_CTRL_CLOSE;
		}

		dma_start_rx(&spi_rx_option[port], rxlen, rxdata);

		MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_START;

		if (MEC17XX_QMSPI0_STS & MEC17XX_QMSPI_STS_PROG_ERR) {
			ret = EC_ERROR_UNKNOWN;
		}
	}

	return ret;
}

static int spi_rx_dma(const int port, uint8_t *rxdata, int rxlen, uint32_t flags)
{
	int ret;

	if (0 == port) {
		ret = qmspi_rx_dma(port, rxdata, rxlen, flags);
	} else {
		ret = EC_ERROR_UNIMPLEMENTED;
	}

	return ret;
}

/*
 * MEC17xx QMSPI controller asserts/de-asserts SPI chipselect.
 * Need to handle close of transaction for following cases:
 * transmit only - close when transmit is done
 * read only - set read to close when done
 * transmit and read - tranmit does NOT close, set read to
 * close when done.
 * TODO - Handle SPI Read commands other than 0x03.
 * 0x03 is limited to 33MHz and has no dummy clocks.
 * 0x0B = 1-1-1-fast requires 8 dummy clocks after address
 * 0x3B = 1-1-2-fast dual I/O on 8-dummy clocks & data phase
 * Dummy clocks tri-state I/O pins.
 * Dummy clocks require either polling for opcode and address transfer done
 * or descriptor mode.
 * Dummy clocks number of I/O pins should be the same as data phase.
 * Either use static globals or pass more parameters to spi_tx & spi_rx_dma.
 * Additional uint32_t flags for QMSPI:
 *	b[0] = 0(full duplex), 1(dual)
 *	b[7:1] = 0
 *	b[8] = 0(leave open), 1(close)
 *	b[31:9] = 0 reserved
 */
int spi_transaction_async(const struct spi_device_t *spi_device,
			  const uint8_t *txdata, int txlen,
			  uint8_t *rxdata, int rxlen)
{
	int port = spi_device->port;
	int ret = EC_SUCCESS;
	uint32_t flags = 0;

	if ((NULL != txdata) && (0 != txlen)) {
		if (0x3B == txdata[0]) {
			flags |= (1ul << 0);
		}
	}

	if (0 == rxlen) {
		flags |= (1ul << 8);
	}

	ret = spi_tx(port, txdata, txlen, flags);
	if (EC_SUCCESS != ret) {
		soft_reset(port);
		return ret;
	}

	flags |= (1ul << 8); /* always close read */
	if (rxlen) {
		ret = spi_rx_dma(port, rxdata, rxlen, flags);
	} else {
		soft_reset(port);
	}

	return ret;
}


/*
 * QMSPI.Status.Complete(done) == 1 indicates QMSPI is
 * done, no more SPI clocks will be generated and all
 * data transfers to/from memory are finished.
 *
 */
int spi_transaction_flush(const struct spi_device_t *spi_device)
{
	int port = spi_device->port;
	int ret = EC_SUCCESS;

	timestamp_t deadline;

	if (0 == port) {
		deadline.val = get_time().val + DMA_TRANSFER_TIMEOUT_US;
		/* Wait for QMSPI to complete. Set for read/write FIFO and DMA modes */
		while (0 == (MEC17XX_QMSPI0_STS & MEC17XX_QMSPI_STS_DONE)) {
			if (timestamp_expired(deadline, NULL))
				return EC_ERROR_TIMEOUT;
			usleep(DMA_POLLING_INTERVAL_US);
		}

		dma_disable(SPI_RX_DMA_CHANNEL(port));
		dma_clear_isr(SPI_RX_DMA_CHANNEL(port));
	} else {
		ret = EC_ERROR_UNIMPLEMENTED;
	}
	return ret;
}

/*
 * called by spi_flash_read, spi_flash_write, spi_flash_erase_block,
 * and spi_flash_write_enable in common/spi_flash.c
 *
 */
int spi_transaction(const struct spi_device_t *spi_device,
		    const uint8_t *txdata, int txlen,
		    uint8_t *rxdata, int rxlen)
{
	int ret;

#ifndef LFW
	mutex_lock(&spi_mutex);
#endif
	ret = spi_transaction_async(spi_device, txdata, txlen, rxdata, rxlen);
	if (ret)
		return ret;
	ret = spi_transaction_flush(spi_device);

#ifndef LFW
	mutex_unlock(&spi_mutex);
#endif
	return ret;
}

/*
 * Called to enable/disable SPI Port and controller for that port
 *
 * MEC17xx Quad-SPI Master (QMSPI)
 * Initialization sequence:
 * Soft-reset block
 * Set frequency divider
 * Set SPI signalling mode to SPI Mode 00:
 *	SPI clock is low when idle
 *	Transmit data changes on falling edge of SPI clock
 *	Capture input data on rising edge of SPI clock
 * Set block activate
 *
 * NOTE: Chromium EC code defines three SPI modules
 * MODULE_SPI
 * MODULE_SPI_FLASH
 * MODULE_SPI_MASTER
 *
 */
int spi_enable(int port, int enable)
{

	if (0 == port) {
		if (enable) {
			gpio_config_module(MODULE_SPI, 1);

			MEC17XX_QMSPI0_MODE = MEC17XX_QMSPI_M_SOFT_RESET;
			MEC17XX_QMSPI0_MODE;	/* read back delay */
			MEC17XX_QMSPI0_MODE = (MEC17XX_QMSPI_M_ACTIVATE +
				       MEC17XX_QMSPI_M_SPI_MODE0 +
				       MEC17XX_QMSPI_M_SPI_CLKDIV_12M);

		} else {
			MEC17XX_QMSPI0_MODE &= ~(MEC17XX_QMSPI_M_ACTIVATE);

			gpio_config_module(MODULE_SPI, 0);
		}

		return EC_SUCCESS;
	} else {
		return EC_ERROR_UNIMPLEMENTED;
	}	
}

