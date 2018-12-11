/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SPI module for Chrome EC.
 *
 * This uses DMA to handle transmission and reception.
 */

#include "console.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "spi.h"
#include "timer.h"
#include "gpio.h"
#include "util.h"
#include "host_command.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SPI, format, ## args)

#define EC_SPI_MAX_REQUEST_SIZE 0x100
#define EC_SPI_MAX_RESPONSE_SIZE 0x100

#define EC_SPI_FRAME_START_LENGTH 1

/* Store Master to Slave data */
static uint8_t in_msg[EC_SPI_MAX_REQUEST_SIZE]
			__attribute__ ((section(".h2ram.pool.spislvtx")));
/* Store Slave to Master data */
static uint8_t out_msg[EC_SPI_MAX_RESPONSE_SIZE]
			__attribute__ ((section(".h2ram.pool.spislvrx")));

/* Parameters used by host protocols */
static struct host_packet spi_packet;

static int pkt_size;

static void spi_init(void);

enum spi_state {
	/* SPI not enabled */
	SPI_STATE_DISABLED = 0,

	/* Ready to receive next request */
	SPI_STATE_READY_TO_RECV,

	/* Receiving request */
	SPI_STATE_RECEIVING,

	/* Processing request */
	SPI_STATE_PROCESSING,

	/* Sending response */
	SPI_STATE_SENDING,

	/* Received bad data */
	SPI_STATE_RX_BAD,
} state;

static void fillout_status(uint8_t byte)
{
	/* Select FIFO Mode */
	IT83XX_SPI_SLAVECTRL0 &= ~IT83XX_SPI_SLAVEMODE;

	IT83XX_SPI_SLAVECTRL1 |= IT83XX_SPI_CPUDIREN;
	IT83XX_SPI_SLAVECTRL1 |= IT83XX_SPI_FIFOPNTRST;

	//CPRINTS("[SPI] status0 %x", byte);
	IT83XX_SPI_SLAVEDATA0 = byte<<24 | byte<<16 | byte<<8 | byte;

	IT83XX_SPI_SLAVECTRL1 |= IT83XX_SPI_FIFOPNTRST;
	IT83XX_SPI_SLAVECTRL1 = 0x00;

	/* Select DMA Mode */
	IT83XX_SPI_SLAVECTRL0 |= IT83XX_SPI_SLAVEMODE;

	IT83XX_SPI_SLAVECTRL1 |= IT83XX_SPI_SRAMENB;

	/* Setting SPI Slave read data from FIFO function */
	//IT83XX_SPI_SLAVECTRL1 |= IT83XX_SPI_FIFODIRCTRL;
}

static void initFIFO(void)
{
	/* Select FIFO Mode */
	IT83XX_SPI_SLAVECTRL0 &= ~IT83XX_SPI_SLAVEMODE;
	IT83XX_SPI_SLAVECTRL0 |= IT83XX_SPI_SLAVECTRLEN;

	IT83XX_SPI_SLAVECTRL1 |= IT83XX_SPI_SRAMENB;
	IT83XX_SPI_SLAVECTRL1 |= IT83XX_SPI_CPUDIREN;
	IT83XX_SPI_SLAVECTRL1 |= IT83XX_SPI_FIFOPNTRST;

	IT83XX_SPI_SLAVEDATA0 = 0x00000000;

	IT83XX_SPI_SLAVECTRL1 |= IT83XX_SPI_FIFOPNTRST;
	IT83XX_SPI_SLAVECTRL1 = 0x00;
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

	if (state != SPI_STATE_PROCESSING)
		return;

	/* Append our past-end byte, which we reserved space for. */
	((uint8_t *)pkt->response)[pkt->response_size + 0] = EC_SPI_PAST_END;

	/* Transmit the reply */
	state = SPI_STATE_SENDING;
}

/* This routine handles shi recevied unexcepted data */
static void spi_bad_received_data(void)
{
	uint8_t i;

	state = SPI_STATE_RX_BAD;
	fillout_status(EC_SPI_RX_BAD_DATA);

	CPRINTS("SPI rx bad data");
	CPRINTF("in_msg=[");
	for (i = 0; i < pkt_size; i++)
		CPRINTF("%02x ", in_msg[i]);
	CPRINTF("]\n");
}

/* Wait until we have received a certain number of bytes */
static int spi_receive(uint8_t *data, uint16_t size)
{
	int idx;

	/* FIFO Count Monitor */
	size = (IT83XX_SPI_FIFOCNTMON1 << 8) | IT83XX_SPI_FIFOCNTMON0;

	for (idx = 0; idx < size; idx++) {
		/* Restore data to msg buffer */
		*(data+idx) = *((uint8_t *)in_msg + idx);
	}

	return 1;
}

/* Parse header for version of spi-protocol */
static void spi_parse_header(void)
{
	uint8_t *rdata = NULL;
	uint16_t data_size = 0;

	/* Chip select is low = asserted */
	if (state != SPI_STATE_READY_TO_RECV) {
		/*
		 * AP started a transaction but we weren't ready for it.
		 * Tell AP we weren't ready, and ignore the received data.
		 */
		CPRINTS("SPI not ready");
		state = SPI_STATE_RX_BAD;
		fillout_status(EC_SPI_NOT_READY);
		return;
	}

	/* We're now inside a transaction */
	state = SPI_STATE_RECEIVING;
	fillout_status(EC_SPI_RECEIVING);

	/* Wait for the packet data */
	if (!spi_receive(rdata, data_size))
		return spi_bad_received_data();

	if (in_msg[0] == EC_HOST_REQUEST_VERSION) {
		/* Protocol version 3 */
		struct ec_host_request *r = (struct ec_host_request *)in_msg;

		/* Check how big the packet should be */
		pkt_size = host_request_expected_size(r);
		if (pkt_size == 0 || pkt_size > sizeof(in_msg))
			return spi_bad_received_data();

		/* Set up parameters for host request */
		spi_packet.send_response = spi_send_response_packet;

		spi_packet.request = in_msg;
		spi_packet.request_temp = NULL;
		spi_packet.request_max = sizeof(in_msg);
		spi_packet.request_size = pkt_size;

		/* Move FRAME_START to second byte */
		out_msg[0] = EC_SPI_FRAME_START;

		/* Response must start with the preamble */
		spi_packet.response = out_msg + EC_SPI_FRAME_START_LENGTH;

		/* Reserve space for frame start and trailing past-end byte */
		spi_packet.response_max = sizeof(out_msg);
		spi_packet.response_size = 0;
		spi_packet.driver_result = EC_RES_SUCCESS;

		/* Move to processing state */
		state = SPI_STATE_PROCESSING;
		fillout_status(EC_SPI_PROCESSING);

		/* Go to common-layer to handle request */
		host_packet_receive(&spi_packet);
	} else {
		/* Invalid version number */
		return spi_bad_received_data();
	}
}

static void spi_polling_handler(void)
{

	if ((IT83XX_SPI_INTSTATUS & IT83XX_SPI_DMATRSFDONE)) {
		//CPRINTS("[SPI] inmsg !!");

		IT83XX_SPI_INTSTATUS &= ~IT83XX_SPI_4BYTETXEMPTY;
		/* Clear detect DMA transfer done interrupt */
		IT83XX_SPI_INTSTATUS &= ~IT83XX_SPI_DMATRSFDONE;

		/* Wait DMA data transfer done */
		while (state == SPI_STATE_PROCESSING)
			;

		/* Clear read and write message buffer of DMA */
		memset(in_msg, 0, EC_SPI_MAX_REQUEST_SIZE);
		memset(out_msg, 0, EC_SPI_MAX_RESPONSE_SIZE);

		/* Ready to receive */
		state = SPI_STATE_READY_TO_RECV;
		fillout_status(EC_SPI_OLD_READY);

		//CPRINTS("[SPI] outmsg0 %x", REG8(0x8da00));

		/* Parse header for version of spi-protocol */
		spi_parse_header();

		//CPRINTS("[SPI] outmsg1 %x", REG8(0x8da00));
	}

}
DECLARE_HOOK(HOOK_TICK, spi_polling_handler, HOOK_PRIO_DEFAULT);

static void spi_init(void)
{
	uint8_t *wp;
	uint8_t *rp;

	/* Enabling spi module for gpio configuration */
	gpio_config_module(MODULE_SPI, 1);

	initFIFO();

	state = SPI_STATE_DISABLED;

	/* Select SPI Slave function
	 * bit[6] ESPI Pin Function Select
	 * 0b: Select ESPI function
	 * 1b: Select SPI Slave function
	 */
	IT83XX_GCTRL_MCCR3 = 0x40;

	IT83XX_SPI_SLAVECTRL1 |= IT83XX_SPI_FIFOPNTRST;
	IT83XX_SPI_INTSTATUS = 0x00;

	/* Interrupt Enable Status Register
	 * bit[5:0]
	 * 0b: Internal 4-byte Register Tx Empty Interrupt Enable
	 * 1b: Internal 4-byte Register Rx Full Interrupt Enable
	 * 2b: SPI Bus End Detected Interrupt Enable
	 * 3b: DMA Transfer Done Interrupt Enable
	 * 4b: SPI Slave Bus Busy Interrupt Enable
	 * 5b: FIFO Data Target Count Reached Interrupt Enabl
	 */
	IT83XX_SPI_INTENBSTATUS = 0x3F;

	IT83XX_SPI_SLAVECTRL0 |= IT83XX_SPI_SLAVECTRLEN;
	IT83XX_SPI_SLAVECTRL0 |= IT83XX_SPI_SLAVEMODE;

	IT83XX_SPI_SLAVECTRL1 |= IT83XX_SPI_SRAMENB;

	/* Set FIFO Data Target Count */
	IT83XX_SPI_FIFOTARCNT0 = 0xFF;
	IT83XX_SPI_FIFOTARCNT1 = 0x01;

	wp = (uint8_t *)in_msg;
	rp = (uint8_t *)out_msg;

	memset(in_msg, 0, EC_SPI_MAX_REQUEST_SIZE);
	memset(out_msg, 0, EC_SPI_MAX_RESPONSE_SIZE);

	//CPRINTS("[SPI]&WriteDMA=%x", in_msg);
	//CPRINTS("[SPI]&RriteDMA=%x", out_msg);

	/* DMA Write Target Address Byte1 Register */
	IT83XX_SPI_DMAWADDR1 = ((uint32_t)wp>>8) & 0x0f;
	/* DMA Write Target Address Byte0 Register */
	IT83XX_SPI_DMAWADDR0 = (uint32_t)wp;
	/* DMA Read Target Address Byte1 Register */
	IT83XX_SPI_DMARADDR1 = ((uint32_t)rp>>8) & 0x0f;
	/* DMA Read Target Address Byte0 Register */
	IT83XX_SPI_DMARADDR0 = (uint32_t)rp;

	/* Ready to receive */
	state = SPI_STATE_READY_TO_RECV;
	fillout_status(EC_SPI_OLD_READY);

}
DECLARE_HOOK(HOOK_INIT, spi_init, HOOK_PRIO_INIT_SPI);

static int command_spiread_slave(int argc, char **argv)
{
	int i = 0;

	while (i < IT83XX_SPI_FIFOCNTMON0) {
		CPRINTS("[SPI]Wr=%x, Rd=%x", in_msg[i], out_msg[i]);
		i++;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(spiread, command_spiread_slave,
			     "SPI",
			     "SPI read");
