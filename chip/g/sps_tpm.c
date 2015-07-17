/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "sps.h"
#include "tpm_registers.h"
#include "util.h"

/*
 * This implements the TCG's TPM SPI Hardware Protocol on the SPI bus, using
 * the Cr50 SPS (SPI slave) controller. This turns out to be very similar to
 * the EC host command protocol, which is itself similar to HDLC. All of those
 * protocols provide ways to identify data frames over transports that don't
 * provide them natively. That's the nice thing about standards: there are so
 * many to choose from.
 *
 * ANYWAY, The TPM protocol is this (note that master clocks the bus and
 * master and slave tramsmit data simultaneously):
 *
 * The master sends 4 bytes:       [R/W+size-1] [Addr] [Addr] [Addr]
 * The slave returns 4 bytes:          [xx]      [xx]   [xx]   [xx]
 *
 * Bytes sent by the master define the direction and size (1-64 bytes) of the
 * data transfer and the address of the register to access.
 *
 * The final bit of the 4th slave response byte determines whether or not the
 * slave needs some extra time. If that bit is 1, the master can IMMEDIATELY
 * clock in (or out) the number of bytes it specified with byte 0.
 *
 * If the final bit of the 4th response byte is 0, the master clocks eight
 * more bits and looks again at the new received byte. It repeats this (clock
 * 8 bits, look at last bit) as long as every eighth bit is 0.
 *
 * When the slave is ready to proceed with the data transfer it returns a 1
 * for the final bit of the response byte, at which point the master has to
 * resume transferring valid data.
 *
 * NOTE: If the master is attempting a write, then the byte that it clocks into
 * the slave while waiting for the stall bit to go high should be the FIRST
 * BYTE of the data it's trying to send. That is, for writes, the slave
 * essentially ACKs the first data byte as it signals for the rest of the data
 * transfer to continue.
 *
 * But if the master is attempting a read, then the slave sends the first valid
 * byte of its data immediately AFTER raising the stall bit.
 *
 * So here's what a 4-byte write to register 0xAABBCC might look like:
 *
 *   xfer:  1  2  3  4  5  6  7  8  9 10 11
 *   MOSI: 03 aa bb cc 11 11 11 11 22 33 44
 *   MISO: xx xx xx x0 x0 x0 x0 x1 xx xx xx
 *
 * Bit 0 of MISO xfer #4 is 0, indicating that the slave needs to stall. The
 * slave stalled for four bytes before it was ready to continue accepting the
 * input data from the master. The slave accepted the first data byte AND
 * released the stall in xfer #8.
 *
 * Here's a 4-byte read from register 0xAABBCC:
 *
 *   xfer:  1  2  3  4  5  6  7  8  9 10 11 12
 *   MOSI: 83 aa bb cc xx xx xx xx xx xx xx xx
 *   MISO: xx xx xx x0 x0 x0 x0 x1 11 22 33 44
 *
 * As before, the slave stalled the read for four bytes and indicated it was
 * done stalling at xfer #8. But the slave sent the first data byte at xfer
 * #9, so the byte xfer count is one more for read than write. If it were
 * possible to transfer data with no stalls in either direction, the length
 * would be the same:
 *
 *   xfer:  1  2  3  4  5  6  7  8
 *   MOSI: 03 aa bb cc 11 22 33 44
 *   MISO: xx xx xx x1 xx xx xx xx
 *
 *   xfer:  1  2  3  4  5  6  7  8
 *   MOSI: 83 aa bb cc xx xx xx xx
 *   MISO: xx xx xx x1 11 22 33 44
 *
 * Also note that the ONLY place where a stall can be signaled is the last bit
 * of the fourth byte of the transaction. Once the stall is released, there's
 * no stopping the rest of the data transfer.
 */

#define TPM_STALL_ASSERT   0x00
#define TPM_STALL_DEASSERT 0x01


/* Console output macros */
#define CPUTS(outstr) cputs(CC_TPM, outstr)
#define CPRINTS(format, args...) cprints(CC_TPM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_TPM, format, ## args)

/*
 * Incoming messages are collected here until they're ready to process. The
 * buffer will start with a four-byte header, followed by whatever data
 * is sent by the master (none for a read, 1 to 64 bytes for a write).
 */
#define RXBUF_MAX 512				/* chosen arbitrarily */
static uint8_t rxbuf[RXBUF_MAX];
static unsigned int rxbuf_count;		/* num bytes received */
static unsigned int rxbuf_needed;		/* num bytes we'd like */

/*
 * Outgoing messages are shoved in here. We need a TPM_STALL_DEASSERT byte to
 * mark the start of the data stream before the data itself.
 */
#define TXBUF_MAX 512				/* chosen arbitrarily */
static uint8_t txbuf[1 + TXBUF_MAX];

static enum sps_state {
	/* Receiving header */
	SPS_TPM_STATE_RECEIVING_HEADER,

	/* Receiving data. */
	SPS_TPM_STATE_RECEIVING_WRITE_DATA,

	/* Finished rx processing, waiting for SPI transaction to finish. */
	SPS_TPM_STATE_PONDERING,

	/* Something went wrong. */
	SPS_TPM_STATE_RX_BAD,
} sps_tpm_state;

/* Set initial conditions to get ready to receive a command. */
static void init_new_cycle(void)
{
	rxbuf_count = 0;
	rxbuf_needed = 4;
	sps_tpm_state = SPS_TPM_STATE_RECEIVING_HEADER;
	sps_tx_status(TPM_STALL_ASSERT);
}

/* Extract R/W bit, register addresss, and data count from 4-byte header */
static int header_says_to_read(uint8_t *data, uint32_t *reg, uint32_t *count)
{
	uint32_t addr = data[1];		/* reg address is MSB first */
	addr = (addr << 8) + data[2];
	addr = (addr << 8) + data[3];
	*reg = addr;
	*count = (data[0] & 0x3f) + 1;		/* bits 5-0: 1 to 64 bytes */
	return !!(data[0] & 0x80);		/* bit 7: 1=read, 0=write */
}

/* actual RX FIFO handler (runs in interrupt context) */
static void process_rx_data(uint8_t *data, size_t data_size)
{
	uint32_t bytecount;
	uint32_t regaddr;

	/* We're collecting incoming bytes ... */
	if ((rxbuf_count + data_size) > RXBUF_MAX) {
		CPRINTS("TPM SPI input overflow: %d + %d > %d in state %d",
			rxbuf_count, data_size, RXBUF_MAX, sps_tpm_state);
		sps_tx_status(TPM_STALL_DEASSERT);
		sps_tpm_state = SPS_TPM_STATE_RX_BAD;
		return;
	}
	memcpy(rxbuf + rxbuf_count, data, data_size);
	rxbuf_count += data_size;

	/* Wait until we have enough. */
	if (rxbuf_count < rxbuf_needed)
		return;

	/* Okay, we have enough. Now what? */
	if (sps_tpm_state == SPS_TPM_STATE_RECEIVING_HEADER) {
		/* Got the header. What's it say to do? */
		if (header_says_to_read(rxbuf, &regaddr, &bytecount)) {
			/* Send the stall deassert manually */
			txbuf[0] = TPM_STALL_DEASSERT;

			/* Copy the register contents into the TXFIFO */
			/* TODO: This is blindly assuming TXFIFO has enough
			 * room. What can we do if it doesn't? */
			tpm_register_get(regaddr, txbuf + 1, bytecount);
			sps_transmit(txbuf, bytecount + 1);
			sps_tpm_state = SPS_TPM_STATE_PONDERING;
			return;
		}

		/* Master is writing. We need more data. */
		rxbuf_needed += bytecount;
		sps_tpm_state = SPS_TPM_STATE_RECEIVING_WRITE_DATA;

		/*
		 * Let the input continue. TODO: We do not know how many bytes
		 * the master has already clocked by now, we need to be able
		 * to figure it out and drop them from the receive stream.
		 */
		sps_tx_status(TPM_STALL_DEASSERT);

		/* Still need more bytes? */
		if (rxbuf_count < rxbuf_needed)
			return;

		/* Got 'em already, keep going... */
	}

	if (sps_tpm_state == SPS_TPM_STATE_RECEIVING_WRITE_DATA) {
		/*
		 * We have all the write data. Probably. I'm pretty sure there
		 * are a couple of ways we can end up either losing the first
		 * data byte or inserting a bunch of dummy writes between the
		 * header and the data.
		 *
		 * TODO: Prove me wrong, or fix the problems.
		 */
		tpm_register_put(regaddr, rxbuf + 4, bytecount);
		sps_tpm_state = SPS_TPM_STATE_PONDERING;
	}
}

static void tpm_rx_handler(uint8_t *data, size_t data_size, int cs_disabled)
{
	if ((sps_tpm_state == SPS_TPM_STATE_RECEIVING_HEADER) ||
	    (sps_tpm_state == SPS_TPM_STATE_RECEIVING_WRITE_DATA))
		process_rx_data(data, data_size);

	if (cs_disabled)
		init_new_cycle();
}

static void sps_tpm_enable(void)
{
	sps_register_rx_handler(SPS_GENERIC_MODE, tpm_rx_handler);
	init_new_cycle();
}

static void sps_tpm_disable(void)
{
	sps_tpm_state = SPS_TPM_STATE_PONDERING;
	sps_unregister_rx_handler();
}

static int command_sps_tpm(int argc, char **argv)
{
	if (argc > 1) {
		if (0 != strcasecmp(argv[1], "off"))
			return EC_ERROR_PARAM1;

		sps_tpm_disable();
		ccprintf("TPM SPI protocol disabled\n");
		return EC_SUCCESS;
	}

	sps_tpm_enable();
	ccprintf("TPM SPI protocol enabled\n");
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(spstpm, command_sps_tpm,
			"[off]",
			"Not sure yet...",
			NULL);
