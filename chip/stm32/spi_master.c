/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * SPI master driver.
 */

#include "common.h"
#include "dma.h"
#include "gpio.h"
#include "shared_mem.h"
#include "spi.h"
#include "stm32-dma.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#if defined(CHIP_VARIANT_STM32F373)  || \
	defined(CHIP_FAMILY_STM32L4) || \
	defined(CHIP_VARIANT_STM32F76X)
#define HAS_SPI3
#else
#undef  HAS_SPI3
#endif

/* The second (and third if available) SPI port are used as master */
static stm32_spi_regs_t *SPI_REGS[] = {
#ifdef CONFIG_STM32_SPI1_MASTER
	STM32_SPI1_REGS,
#endif
	STM32_SPI2_REGS,
#ifdef HAS_SPI3
	STM32_SPI3_REGS,
#endif
};

#ifdef CHIP_FAMILY_STM32L4
/* DMA request mapping on channels */
static uint8_t dma_req[ARRAY_SIZE(SPI_REGS)] = {
#ifdef CONFIG_STM32_SPI1_MASTER
	/* SPI1 */ 1,
#endif
	/* SPI2 */ 1,
	/* SPI3 */ 3,
};
#endif

static struct mutex spi_mutex[ARRAY_SIZE(SPI_REGS)];

#define SPI_TRANSACTION_TIMEOUT_USEC (800 * MSEC)

/* Default DMA channel options */
#ifdef CHIP_FAMILY_STM32F4
#define F4_CHANNEL(ch)	STM32_DMA_CCR_CHANNEL(ch)
#else
#define F4_CHANNEL(ch)	0
#endif

static const struct dma_option dma_tx_option[] = {
#ifdef CONFIG_STM32_SPI1_MASTER
	{
		STM32_DMAC_SPI1_TX, (void *)&STM32_SPI1_REGS->dr,
		STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT
		| F4_CHANNEL(STM32_SPI1_TX_REQ_CH) | STM32_DMA_CCR_TCIE
	},
#endif
	{
		STM32_DMAC_SPI2_TX, (void *)&STM32_SPI2_REGS->dr,
		STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT
		| F4_CHANNEL(STM32_SPI2_TX_REQ_CH) | STM32_DMA_CCR_TCIE
	},
#ifdef HAS_SPI3
	{
		STM32_DMAC_SPI3_TX, (void *)&STM32_SPI3_REGS->dr,
		STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT
		| F4_CHANNEL(STM32_SPI3_TX_REQ_CH) | STM32_DMA_CCR_TCIE
	},
#endif
};

static const struct dma_option dma_rx_option[] = {
#ifdef CONFIG_STM32_SPI1_MASTER
	{
		STM32_DMAC_SPI1_RX, (void *)&STM32_SPI1_REGS->dr,
		STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT
		| F4_CHANNEL(STM32_SPI1_RX_REQ_CH) | STM32_DMA_CCR_TCIE
	},
#endif
	{
		STM32_DMAC_SPI2_RX, (void *)&STM32_SPI2_REGS->dr,
		STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT
		| F4_CHANNEL(STM32_SPI2_RX_REQ_CH) | STM32_DMA_CCR_TCIE
	},
#ifdef HAS_SPI3
	{
		STM32_DMAC_SPI3_RX, (void *)&STM32_SPI3_REGS->dr,
		STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT
		| F4_CHANNEL(STM32_SPI3_RX_REQ_CH) | STM32_DMA_CCR_TCIE
	},
#endif
};

static uint8_t spi_enabled[ARRAY_SIZE(SPI_REGS)];
static uint8_t spi_state[ARRAY_SIZE(SPI_REGS)];

#define SPI_STATE_RX_TCIE	(1 << 0)
#define SPI_STATE_TX_TCIE	(1 << 1)

static inline int spi_is_tx_done(stm32_spi_regs_t *spi)
{
	return !(spi->sr & (STM32_SPI_SR_FTLVL | STM32_SPI_SR_BSY));
}

static inline int spi_is_rx_done(stm32_spi_regs_t *spi)
{
	return !(spi->sr & (STM32_SPI_SR_FRLVL | STM32_SPI_SR_RXNE));
}

/**
 * Initialize SPI module, registers, and clocks
 *
 * - port: which port to initialize.
 */
static int spi_master_initialize(int port)
{
	int i, div = 0;

	stm32_spi_regs_t *spi = SPI_REGS[port];

	/*
	 * Set SPI master, baud rate, and software slave control.
	 * */
	for (i = 0; i < spi_devices_used; i++)
		if ((spi_devices[i].port == port) &&
		    (div < spi_devices[i].div))
			div = spi_devices[i].div;
	spi->cr1 = STM32_SPI_CR1_MSTR | STM32_SPI_CR1_SSM | STM32_SPI_CR1_SSI |
		(div << 3);

#ifdef CHIP_FAMILY_STM32L4
	dma_select_channel(dma_tx_option[port].channel, dma_req[port]);
	dma_select_channel(dma_rx_option[port].channel, dma_req[port]);
#endif
	/*
	 * Configure 8-bit datasize, set FRXTH, enable DMA, enable RXNE
	 * interrupt, enable TXE interrupt, and enable NSS output
	 */
	spi->cr2 = STM32_SPI_CR2_RXNEIE | STM32_SPI_CR2_TXEIE |
			STM32_SPI_CR2_TXDMAEN | STM32_SPI_CR2_RXDMAEN |
			STM32_SPI_CR2_FRXTH | STM32_SPI_CR2_DATASIZE(8);

#ifdef CONFIG_SPI_HALFDUPLEX
	spi->cr1 |= STM32_SPI_CR1_BIDIMODE | STM32_SPI_CR1_BIDIOE;
#endif

	for (i = 0; i < spi_devices_used; i++) {
		if (spi_devices[i].port != port)
			continue;
		/* Drive SS high */
		gpio_set_level(spi_devices[i].gpio_cs, 1);
	}

	/* Set flag */
	spi_enabled[port] = 1;

	return EC_SUCCESS;
}

/**
 * Shutdown SPI module
 */
static int spi_master_shutdown(int port)
{
	int rv = EC_SUCCESS;

	stm32_spi_regs_t *spi = SPI_REGS[port];
	char dummy __attribute__((unused));

	/* Set flag */
	spi_enabled[port] = 0;

	/* Disable DMA streams */
	dma_disable(dma_tx_option[port].channel);
	dma_disable(dma_rx_option[port].channel);

	/* Disable SPI */
	spi->cr1 &= ~STM32_SPI_CR1_SPE;

	/* Read until FRLVL[1:0] is empty */
	while (!spi_is_rx_done(spi))
		dummy = spi->dr;

	/* Disable DMA buffers */
	spi->cr2 &= ~(STM32_SPI_CR2_TXDMAEN | STM32_SPI_CR2_RXDMAEN);

	return rv;
}

int spi_enable(int port, int enable)
{
	if (enable == spi_enabled[port])
		return EC_SUCCESS;
	if (enable)
		return spi_master_initialize(port);
	else
		return spi_master_shutdown(port);
}

static void spi_rx_dma_done(void *data)
{
	int port = (int)data;
	stm32_spi_regs_t *spi = SPI_REGS[port];

	dma_disable_tc_interrupt(dma_rx_option[port].channel);
#ifdef CONFIG_SPI_HALFDUPLEX
	/* Stop receiving */
	spi->cr1 |= STM32_SPI_CR1_BIDIOE;
#endif
	spi->cr1 &= ~STM32_SPI_CR1_SPE;

	/* Clear out the FIFO. */
	while (!spi_is_rx_done(spi))
		(void) (uint8_t) spi->dr;

	// TODO(stimim): use a task event?
	spi_state[port] |= SPI_STATE_RX_TCIE;
}

static void spi_tx_dma_done(void *data)
{
	int port = (int)data;
	stm32_spi_regs_t *spi = SPI_REGS[port];

	dma_disable_tc_interrupt(dma_tx_option[port].channel);
	while (!spi_is_tx_done(spi))
		; /* wait for TX complete */
	spi->cr1 &= ~STM32_SPI_CR1_SPE;
	// TODO(stimim): use a task event?
	spi_state[port] |= SPI_STATE_TX_TCIE;
}

static int spi_dma_start(int port, const uint8_t *txdata,
			 uint8_t *rxdata, int len)
{
	stm32_spi_regs_t *spi = SPI_REGS[port];
	dma_chan_t *txdma;
	/* Set up RX DMA */
	if (rxdata) {
		spi_state[port] &= ~SPI_STATE_RX_TCIE;
		dma_enable_tc_interrupt_callback(dma_rx_option[port].channel,
						 spi_rx_dma_done,
						 (void *)port);
		dma_start_rx(&dma_rx_option[port], len, rxdata);
#ifdef CONFIG_SPI_HALFDUPLEX
		spi->cr1 &= ~(STM32_SPI_CR1_BIDIOE | STM32_SPI_CR1_BIDIMODE);
		spi->cr1 |= STM32_SPI_CR1_BIDIMODE;
#endif
		spi->cr1 |= STM32_SPI_CR1_SPE;
	}
	/* Set up TX DMA */
	if (txdata) {
#ifdef CONFIG_SPI_HALFDUPLEX
		spi->cr1 |= STM32_SPI_CR1_BIDIMODE | STM32_SPI_CR1_BIDIOE;
#endif
		spi->cr1 |= STM32_SPI_CR1_SPE;
		spi_state[port] &= ~SPI_STATE_TX_TCIE;
		dma_enable_tc_interrupt_callback(dma_tx_option[port].channel,
						 spi_tx_dma_done,
						 (void *)port);
		txdma = dma_get_channel(dma_tx_option[port].channel);
		dma_prepare_tx(&dma_tx_option[port], len, txdata);
		dma_go(txdma);
	}

	return EC_SUCCESS;
}

static inline int dma_is_enabled(const struct dma_option *option)
{
	/* dma_bytes_done() returns 0 if channel is not enabled */
	return dma_bytes_done(dma_get_channel(option->channel), -1);
}

static int spi_dma_wait(int port)
{
	timestamp_t timeout;
	stm32_spi_regs_t *spi = SPI_REGS[port];
	int rv = EC_SUCCESS;

	/* Wait for DMA transmission to complete */
	if (dma_is_enabled(&dma_tx_option[port])) {
		timeout.val = get_time().val + SPI_TRANSACTION_TIMEOUT_USEC;
		/* Wait for FIFO empty and BSY bit clear */
		while (!(spi_state[port] & SPI_STATE_TX_TCIE) ||
		       !spi_is_tx_done(spi))
			if (get_time().val > timeout.val) {
				rv = EC_ERROR_TIMEOUT;
				break;
			}
		/* Disable TX DMA */
		dma_disable(dma_tx_option[port].channel);
		spi->cr1 &= ~STM32_SPI_CR1_SPE;
		if (rv)
			return rv;
	}

	/* Wait for DMA reception to complete */
	if (dma_is_enabled(&dma_rx_option[port])) {
		timeout.val = get_time().val + SPI_TRANSACTION_TIMEOUT_USEC;
		/* Wait RX complete */
		while (!(spi_state[port] & SPI_STATE_RX_TCIE) ||
		       !spi_is_rx_done(spi))
			if (get_time().val > timeout.val) {
				rv = EC_ERROR_TIMEOUT;
				break;
			}
		/* Disable RX DMA */
		dma_disable(dma_rx_option[port].channel);
#ifdef CONFIG_SPI_HALFDUPLEX
		spi->cr1 |= STM32_SPI_CR1_BIDIOE;
#endif
		spi->cr1 &= ~STM32_SPI_CR1_SPE;
		if (rv)
			return rv;
	}
	return rv;
}

int spi_transaction_async(const struct spi_device_t *spi_device,
			  const uint8_t *txdata, int txlen,
			  uint8_t *rxdata, int rxlen)
{
	int rv = EC_SUCCESS;
	int port = spi_device->port;
	int full_readback = 0;

	stm32_spi_regs_t *spi = SPI_REGS[port];
	char *buf = NULL;

#ifndef CONFIG_SPI_HALFDUPLEX
	if (rxlen == SPI_READBACK_ALL) {
		buf = rxdata;
		full_readback = 1;
	} else {
		rv = shared_mem_acquire(MAX(txlen, rxlen), &buf);
		if (rv != EC_SUCCESS)
			return rv;
	}
#endif

	/* Drive SS low */
	gpio_set_level(spi_device->gpio_cs, 0);

	/* Clear out the FIFO. */
	while (!spi_is_rx_done(spi))
		(void) (uint8_t) spi->dr;

	rv = spi_dma_start(port, txdata, buf, txlen);
	if (rv != EC_SUCCESS)
		goto err_free;

	if (full_readback)
		return EC_SUCCESS;

	rv = spi_dma_wait(port);
	if (rv != EC_SUCCESS)
		goto err_free;

	if (rxlen) {
		rv = spi_dma_start(port, buf, rxdata, rxlen);
		if (rv != EC_SUCCESS)
			goto err_free;
	}
err_free:
#ifndef CONFIG_SPI_HALFDUPLEX
	if (!full_readback)
		shared_mem_release(buf);
#endif
	return rv;
}

int spi_transaction_flush(const struct spi_device_t *spi_device)
{
	int rv = spi_dma_wait(spi_device->port);

#ifdef CONFIG_SPI_HALFDUPLEX
	/* Disable receive-only mode */
	SPI_REGS[spi_device->port]->cr1 |= STM32_SPI_CR1_BIDIOE;
#endif

	/* Drive SS high */
	gpio_set_level(spi_device->gpio_cs, 1);

	return rv;
}

int spi_transaction_wait(const struct spi_device_t *spi_device)
{
	return spi_dma_wait(spi_device->port);
}

int spi_transaction(const struct spi_device_t *spi_device,
		    const uint8_t *txdata, int txlen,
		    uint8_t *rxdata, int rxlen)
{
	int rv;
	int port = spi_device->port;

	mutex_lock(spi_mutex + port);
	rv = spi_transaction_async(spi_device, txdata, txlen, rxdata, rxlen);
	rv |= spi_transaction_flush(spi_device);
	mutex_unlock(spi_mutex + port);

	return rv;
}
