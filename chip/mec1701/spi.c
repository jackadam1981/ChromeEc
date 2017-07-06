/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SPI master module for MEC1322 */

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


/* only regular image needs mutex, LFW does not have scheduling */
/* TODO: Move SPI locking to common code */
#ifndef LFW
static struct mutex spi_mutex;
#endif


#ifdef CONFIG_MEC1701_GP_SPI
#ifndef LFW
/*
 * GP-SPI
 */
static const struct dma_option gpspi_rx_option[] = {
	{
		MEC17XX_DMAC_SPI0_RX,
		(void *)&MEC17XX_SPI_RD(0),
		MEC17XX_DMA_XFER_SIZE(1) + MEC17XX_DMA_INC_MEM
	},
	{
		MEC17XX_DMAC_SPI1_RX,
		(void *)&MEC17XX_SPI_RD(1),
		MEC17XX_DMA_XFER_SIZE(1) + MEC17XX_DMA_INC_MEM
	},
};

static int gpspi_wait_byte(const int port)
{
	timestamp_t deadline;

	deadline.val = get_time().val + SPI_BYTE_TRANSFER_TIMEOUT_US;
	while ((MEC17XX_SPI_SR(port) & 0x3) != 0x3) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}

static int gpspi_tx(const int port, const uint8_t *txdata, int txlen)
{
	int i;
	int ret;
	uint8_t dummy = 0;

	ret = EC_SUCCESS;
	for (i = 0; i < txlen; ++i) {
		MEC17XX_SPI_TD(port) = txdata[i];
		ret = gpspi_wait_byte(port);
		if (ret != EC_SUCCESS)
			return ret;
		dummy += MEC17XX_SPI_RD(port);
	}

	return ret;
}

static int gpspi_transaction_async(const struct spi_device_t *spi_device,
				const uint8_t *txdata, int txlen,
				uint8_t *rxdata, int rxlen)
{
	int port = spi_device->port;
	int ret = EC_SUCCESS;

	gpio_set_level(spi_device->gpio_cs, 0);

	/* Disable auto read */
	MEC17XX_SPI_CR(port) &= ~(1 << 5);

	ret = gpspi_tx(port, txdata, txlen);
	if (ret != EC_SUCCESS)
		return ret;

	/* Enable auto read */
	MEC17XX_SPI_CR(port) |= 1 << 5;

	if (rxlen != 0) {
		dma_start_rx(&gpspi_rx_option[port], rxlen, rxdata);
		MEC17XX_SPI_TD(port) = 0;
	}
	return ret;
}

static int gpspi_transaction_flush(const struct spi_device_t *spi_device)
{
	int port = spi_device->port;
	enum dma_channel channel = gpspi_rx_option[port].channel;
	int ret = dma_wait(channel);

	timestamp_t deadline;

	/* Disable auto read */
	MEC17XX_SPI_CR(port) &= ~(1 << 5);

	deadline.val = get_time().val + SPI_BYTE_TRANSFER_TIMEOUT_US;
	/* Wait for FIFO empty SPISR_TXBE */
	while ((MEC17XX_SPI_SR(port) & 0x01) != 0x1) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}

	dma_disable(channel);
	dma_clear_isr(channel);
	if (MEC17XX_SPI_SR(port) & 0x2)
		port = MEC17XX_SPI_RD(port);

	gpio_set_level(spi_device->gpio_cs, 1);

	return ret;
}

/* UNUSED */
#if 0
static int gpspi_transaction(const struct spi_device_t *spi_device,
		      const uint8_t *txdata, int txlen,
		      uint8_t *rxdata, int rxlen)
{
	int ret;

	ret = gpspi_transaction_async(spi_device, txdata, txlen, rxdata, rxlen);
	if (ret)
		return ret;
	ret = gpspi_transaction_flush(spi_device);

	return ret;
}
#endif

static int gpspi_enable(int port, int enable)
{
	if (enable) {
		gpio_config_module(MODULE_SPI_MASTER, 1);

		/* Set enable bit in SPI_AR */
		MEC17XX_SPI_AR(port) |= 0x1;

		/* Set SPDIN to 0 -> Full duplex */
		MEC17XX_SPI_CR(port) &= ~(0x3 << 2);

		/* Set CLKPOL, TCLKPH, RCLKPH to 0 */
		MEC17XX_SPI_CC(port) &= ~0x7;

		/* Set LSBF to 0 -> MSB first */
		MEC17XX_SPI_CR(port) &= ~0x1;
	} else {
		/* Clear enable bit in SPI_AR */
		MEC17XX_SPI_AR(port) &= ~0x1;

		gpio_config_module(MODULE_SPI_MASTER, 0);
	}

	return EC_SUCCESS;
}
/*
 * End GP-SPI
 */
#endif /* #ifndef LFW */
#endif /* #ifdef CONFIG_MEC1701_GP_SPI */

#if 0 /* not used */
static const struct dma_option spi_tx_option[1] = {
	{
		MEC17XX_DMAC_QMSPI0_TX,
		(void *)(MEC17XX_QMSPI0_TX_FIFO_ADDR),
		(MEC17XX_DMA_XFER_SIZE(1) + MEC17XX_DMA_INC_MEM +
			MEC17XX_DMA_TO_DEV)
	}
};
#endif

static const struct dma_option qmspi_rx_option[1] = {
	{
		MEC17XX_DMAC_QMSPI0_RX,
		(void *)(MEC17XX_QMSPI0_RX_FIFO_ADDR),
		MEC17XX_DMA_XFER_SIZE(1) + MEC17XX_DMA_INC_MEM
	},
};


/*
 * Wait for QMSPI TX FIFO to empty.
 */
#if 0 /* UNUSED */
static int wait_tx_fifo_empty(void)
{
	timestamp_t deadline;

	deadline.val = get_time().val + (SPI_BYTE_TRANSFER_TIMEOUT_US *
					 MEC17XX_QMSPI_TX_FIFO_LEN);
	while (!(MEC17XX_QMSPI0_STS & MEC17XX_QMSPI_STS_TX_BUFF_EMPTY)) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}
#endif

/*
 * Wait for QMSPI TX FIFO to be not full
 */
#if 0 /* UNUSED */
static int wait_tx_fifo_not_full(void)
{
	timestamp_t deadline;

	deadline.val = get_time().val + SPI_BYTE_TRANSFER_TIMEOUT_US;
	while (MEC17XX_QMSPI0_STS & MEC17XX_QMSPI_STS_TX_BUFF_FULL) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}
#endif

/*
 * wait for QMSPI status bit(s) to be set
 * Timeout is one byte time * TX FIFO maximum length
 */
#if 0
static int wait_fifo_not_full(void)
{
	timestamp_t deadline;

	deadline.val = get_time().val + (SPI_BYTE_TRANSFER_TIMEOUT_US *
					 MEC17XX_QMSPI_TX_FIFO_LEN);
	while ((MEC17XX_QMSPI0_STS & MEC17XX_QMSPI_STS_TX_BUFF_FULL) != 0) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}

static int wait_done(void)
{
	timestamp_t deadline;

	deadline.val = get_time().val + (SPI_BYTE_TRANSFER_TIMEOUT_US *
					 MEC17XX_QMSPI_TX_FIFO_LEN);
	while ((MEC17XX_QMSPI0_STS & MEC17XX_QMSPI_STS_DONE) == 0) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}
#endif

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
		if (td > SPI_BYTE_TRANSFER_TIMEOUT_US)
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

	deadline.val = get_time().val + (SPI_BYTE_TRANSFER_TIMEOUT_US);

	while ((MEC17XX_QMSPI0_STS & mask) != mval) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}
#endif

/*
 * Wait for QMSPI RX FIFO to become not empty
 */
#if 0 /* UNUSED */
static int wait_rx_fifo_not_empty(void)
{
	timestamp_t deadline;

	deadline.val = get_time().val + (SPI_BYTE_TRANSFER_TIMEOUT_US *
					 MEC17XX_QMSPI_RX_FIFO_LEN);
	while (MEC17XX_QMSPI0_STS & MEC17XX_QMSPI_STS_RX_BUFF_EMPTY) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}
#endif


/* MEC17XX_QMSPI_CTRL_MAX_UNITS */
static int spi_tx(const uint8_t *txdata, int txlen, int close)
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

static int qmspi_transaction_flush(const struct spi_device_t *spi_device)
{
	int ret;
	timestamp_t deadline;

	/* DMA subsystem has 100 ms timeout */
	ret = dma_wait(qmspi_rx_option[0].channel);
	if (ret != EC_SUCCESS)
		return ret;

	dma_disable(qmspi_rx_option[0].channel);
	dma_clear_isr(qmspi_rx_option[0].channel);

	/*
	 * wait for QMSPI to read extra byte into its RX FIFO before
	 * it notices DMA has stopped. We will spin on QMSPI read-only
	 * Transfer Active status bit as it will clear when QMSPI closes
	 * the transaction(de-asserts chip select).
	 */
	deadline.val = get_time().val + SPI_BYTE_TRANSFER_TIMEOUT_US;

	while (MEC17XX_QMSPI0_STS & MEC17XX_QMSPI_STS_ACTIVE) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(SPI_BYTE_TRANSFER_POLL_INTERVAL_US);
	}

	/* clear QMSPI FIFO's */
	MEC17XX_QMSPI0_EXE = MEC17XX_QMSPI_EXE_CLR_FIFOS;
	MEC17XX_QMSPI0_STS = 0xffffffff;

	return ret;
}

/*
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
static int qmspi_transaction_async(const struct spi_device_t *spi_device,
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

	ret = spi_tx(txdata, txlen, close);
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


int spi_transaction_async(const struct spi_device_t *spi_device,
				const uint8_t *txdata, int txlen,
				uint8_t *rxdata, int rxlen)
{
	int rc;

	if (spi_device == NULL)
		return EC_ERROR_INVAL;

	switch (spi_device->port) {
#ifdef CONFIG_MEC1701_GP_SPI
#ifndef LFW
	case MEC17XX_GPSPI0_PORT:
	case MEC17XX_GPSPI1_PORT:
		rc = gpspi_transaction_async(spi_device, txdata,
				txlen, rxdata, rxlen);
		break;
#endif
#endif
	case MEC17XX_QMSPI0_SHD_PORT:
	case MEC17XX_QMSPI0_PVT_PORT:
		rc = qmspi_transaction_async(spi_device, txdata,
				txlen, rxdata, rxlen);
		break;
	default:
		rc = EC_ERROR_INVAL;
	}

	return rc;
}

int spi_transaction_flush(const struct spi_device_t *spi_device)
{
	int rc;

	if (spi_device == NULL)
		return EC_ERROR_INVAL;

	switch (spi_device->port) {
#ifdef CONFIG_MEC1701_GP_SPI
#ifndef LFW
	case MEC17XX_GPSPI0_PORT:
	case MEC17XX_GPSPI1_PORT:
		rc = gpspi_transaction_flush(spi_device);
		break;
#endif
#endif
	case MEC17XX_QMSPI0_SHD_PORT:
	case MEC17XX_QMSPI0_PVT_PORT:
		rc = qmspi_transaction_flush(spi_device);
		break;
	default:
		rc = EC_ERROR_INVAL;
	}

	return rc;
}


/*
 * called from common/spi_flash.c
 */
int spi_transaction(const struct spi_device_t *spi_device,
		    const uint8_t *txdata, int txlen,
		    uint8_t *rxdata, int rxlen)
{
	int rc;

	if (spi_device == NULL)
		return EC_ERROR_INVAL;

#ifndef LFW
	mutex_lock(&spi_mutex);
#endif

	rc = spi_transaction_async(spi_device, txdata, txlen, rxdata, rxlen);

	if ((rc == EC_SUCCESS) && (rxlen > 0))
		rc = spi_transaction_flush(spi_device);

#ifndef LFW
	mutex_unlock(&spi_mutex);
#endif
	return rc;
}



static int qmspi_enable(int port, int enable)
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

/*
 * called from common/spi_flash.c
 */
int spi_enable(int port, int enable)
{
	int rc;

	switch (port) {
#ifdef CONFIG_MEC1701_GP_SPI
#ifndef LFW
	case MEC17XX_GPSPI0_PORT:
	case MEC17XX_GPSPI1_PORT:
		rc = gpspi_enable(port, enable);
		break;
#endif
#endif
	case MEC17XX_QMSPI0_SHD_PORT:
	case MEC17XX_QMSPI0_PVT_PORT:
		rc = qmspi_enable(port, enable);
		break;
	default:
		return EC_ERROR_INVAL;
	}

	return rc;
}

/*
 * Console commands
 */
