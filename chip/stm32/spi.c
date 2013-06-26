/*
 * Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * SPI driver for Chrome EC.
 *
 * This uses DMA to handle transmission and reception.
 */

#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "registers.h"
#include "spi.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTF(format, args...) cprintf(CC_SPI, format, ## args)

/* DMA channel option */
static const struct dma_option dma_tx_option = {
	DMAC_SPI1_TX, (void *)&STM32_SPI1_REGS->dr,
	DMA_MSIZE_BYTE | DMA_PSIZE_HALF_WORD
};

static const struct dma_option dma_rx_option = {
	DMAC_SPI1_RX, (void *)&STM32_SPI1_REGS->dr,
	DMA_MSIZE_BYTE | DMA_PSIZE_HALF_WORD
};

/*
 * Timeout to wait for SPI command TODO(sjg@chromium.org): Support much slower
 * SPI clocks. For 4096 we have a delay of 4ms. For the largest message (68
 * bytes) this is 130KhZ, assuming that the master starts sending bytes as soon
 * as it drops NSS. In practice, this timeout seems adequately high for a 1MHz
 * clock which is as slow as we would reasonably want it.
 */
#define SPI_CMD_RX_TIMEOUT_US 8192

/* Maximum packet size we can handle, in bytes */
#define MAX_PACKET_SIZE 0x220

/* Value we output while stalling / processing the command */
#define SPI_MSG_STALL_BYTE 0xfd

/* Value which immediately precedes the response packet */
#define SPI_MSG_PREFIX_BYTE 0xec

/*
 * The AP blindly clocks back bytes over the SPI interface looking for the
 * prefix byte.  Search for "spi-frame-header" in U-boot to see how that's
 * implemented.  So this preamble must always precede the actual response
 * packet.  The preamble must be 32-bit aligned so that the response buffer is
 * also 32-bit aligned.
 */
static const uint8_t out_preamble[4] = {
	SPI_MSG_STALL_BYTE,
	SPI_MSG_STALL_BYTE,
	SPI_MSG_STALL_BYTE,
	SPI_MSG_PREFIX_BYTE,  /* This is the byte which matters */
};

/*
 * Our input and output buffers. These must be large enough for our largest
 * message, including protocol overhead.
 */
static uint8_t out_msg[MAX_PACKET_SIZE + sizeof(out_preamble)];
static uint8_t in_msg[MAX_PACKET_SIZE];
static uint8_t active;
static uint8_t enabled;
static struct host_packet spi_packet;

/**
 * Wait until we have received a certain number of bytes
 *
 * Watch the DMA receive channel until it has the required number of bytes,
 * or a timeout occurs
 *
 * We keep an eye on the NSS line - if this goes high then the transaction is
 * over so there is no point in trying to receive the bytes.
 *
 * @param rxdma		RX DMA channel to watch
 * @param needed	Number of bytes that are needed
 * @param nss_regs	GPIO register for NSS control line
 * @param nss_mask	Bit to check in GPIO register (when high, we abort)
 * @return 0 if bytes received, -1 if we hit a timeout or NSS went high
 */
static int wait_for_bytes(dma_channel_t *rxdma, int needed,
			  uint16_t *nss_reg, uint32_t nss_mask)
{
	timestamp_t deadline;

	ASSERT(needed <= sizeof(in_msg));
	deadline.val = 0;
	for (;;) {
		if (dma_bytes_done(rxdma, sizeof(in_msg)) >= needed)
			return 0;
		if (REG16(nss_reg) & nss_mask)
			return -1;
		if (!deadline.val) {
			deadline = get_time();
			deadline.val += SPI_CMD_RX_TIMEOUT_US;
		}
		if (timestamp_expired(deadline, NULL))
			return -1;
	}
}

/**
 * Get ready to receive a message from the master.
 *
 * Set up our RX DMA and disable our TX DMA. Set up the data output so that
 * we will send preamble bytes.
 */
static void setup_for_transaction(void)
{
	stm32_spi_regs_t *spi = STM32_SPI1_REGS;
	int dmac __attribute__((unused));

	/* No longer actively processing a transaction */
	active = 0;

	/* Write output value we'll send while stalling */
	spi->dr = SPI_MSG_STALL_BYTE;
	dma_disable(DMAC_SPI1_TX);

	/* Make sure input message won't be considered valid */
	*in_msg = 0;

	/* read a byte in case there is one, and the rx dma gets it */
	dmac = spi->dr;
	dma_start_rx(&dma_rx_option, sizeof(in_msg), in_msg);
}

/**
 * Called to send a response back to the host.
 *
 * Some commands can continue for a while. This function is called by
 * host_command when it completes.
 *
 */
static void spi_send_response_packet(struct host_packet *pkt)
{
	dma_channel_t *txdma;

	/* If we are too late, don't bother */
	if (!active)
		return;

	/* Transmit the reply */
	txdma = dma_get_channel(DMAC_SPI1_TX);
	dma_prepare_tx(&dma_tx_option,
		       sizeof(out_preamble) + pkt->response_size, out_msg);
	dma_go(txdma);
}

/**
 * Handle an event on the NSS pin
 *
 * A falling edge of NSS indicates that the master is starting a new
 * transaction. A rising edge indicates that we have finsihed
 *
 * @param signal	GPIO signal for the NSS pin
 */
void spi_event(enum gpio_signal signal)
{
	dma_channel_t *rxdma;
	uint16_t *nss_reg;
	uint32_t nss_mask;

	/* If not enabled, ignore glitches on NSS */
	if (!enabled)
		return;

	/*
	 * If NSS is rising, we have finished the transaction, so prepare
	 * for the next.
	 */
	nss_reg = gpio_get_level_reg(GPIO_SPI1_NSS, &nss_mask);
	if (REG16(nss_reg) & nss_mask)
		goto spi_event_restart;

	/* Otherwise, NSS is low and we're now inside a transaction */
	active = 1;
	rxdma = dma_get_channel(DMAC_SPI1_RX);

	/* Wait for version, command, length bytes */
	if (wait_for_bytes(rxdma, 3, nss_reg, nss_mask))
		goto spi_event_restart;

	if (in_msg[0] == EC_HOST_REQUEST_VERSION) {
		/* Protocol version 3 */
		struct ec_host_request *r = (struct ec_host_request *)in_msg;
		int pkt_size;

		/* Wait for the rest of the command header */
		if (wait_for_bytes(rxdma, sizeof(*r), nss_reg, nss_mask))
			goto spi_event_restart;

		/*
		 * Check how big the packet should be.  We can't just wait to
		 * see how much data the host sends, because it will keep
		 * sending dummy data until we respond.
		 */
		pkt_size = host_request_expected_size(r);
		if (pkt_size == 0 || pkt_size > sizeof(in_msg))
			goto spi_event_restart;

		/* Wait for the packet data */
		if (wait_for_bytes(rxdma, pkt_size, nss_reg, nss_mask))
			goto spi_event_restart;

		spi_packet.send_response = spi_send_response_packet;

		spi_packet.request = in_msg;
		spi_packet.request_temp = NULL;
		spi_packet.request_max = sizeof(in_msg);
		spi_packet.request_size = pkt_size;

		/* Response must start with the preamble */
		memcpy(out_msg, out_preamble, sizeof(out_preamble));
		spi_packet.response = out_msg + sizeof(out_preamble);
		spi_packet.response_max =
			sizeof(out_msg) - sizeof(out_preamble);
		spi_packet.response_size = 0;

		spi_packet.driver_result = EC_RES_SUCCESS;

		host_packet_receive(&spi_packet);
		return;
	}

	/* If we're still here, old protocol version or garbage.  Ignore. */

 spi_event_restart:
	setup_for_transaction();
}

static void spi_init(void)
{
	stm32_spi_regs_t *spi = STM32_SPI1_REGS;

	/* 40 MHz pin speed */
	STM32_GPIO_OSPEEDR(GPIO_A) |= 0xff00;

	/* Enable clocks to SPI1 module */
	STM32_RCC_APB2ENR |= STM32_RCC_PB2_SPI1;

	/* Enable rx DMA and get ready to receive our first transaction */
	spi->cr2 = STM32_SPI_CR2_RXDMAEN | STM32_SPI_CR2_TXDMAEN;

	/* Enable the SPI peripheral */
	spi->cr1 |= STM32_SPI_CR1_SPE;

	gpio_enable_interrupt(GPIO_SPI1_NSS);
}
DECLARE_HOOK(HOOK_INIT, spi_init, HOOK_PRIO_DEFAULT);

static void spi_chipset_startup(void)
{
	/* Enable pullup and interrupts on NSS */
	gpio_set_flags(GPIO_SPI1_NSS, GPIO_INT_BOTH | GPIO_PULL_UP);

	/* Set SPI pins to alternate function */
	gpio_set_alternate_function(GPIO_A, 0xf0, GPIO_ALT_SPI);

	/* Set up for next transaction */
	setup_for_transaction();

	enabled = 1;
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, spi_chipset_startup, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_RESUME, spi_chipset_startup, HOOK_PRIO_DEFAULT);

static void spi_chipset_shutdown(void)
{
	enabled = active = 0;

	/* Disable pullup and interrupts on NSS */
	gpio_set_flags(GPIO_SPI1_NSS, 0);

	/* Set SPI pins to inputs so we don't leak power when AP is off */
	gpio_set_alternate_function(GPIO_A, 0xf0, -1);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, spi_chipset_shutdown, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, spi_chipset_shutdown, HOOK_PRIO_DEFAULT);
