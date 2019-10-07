/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * SPI driver for Chrome EC.
 *
 * This uses FIFO mode to handle transmission and reception.
 */

#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SPI, format, ## args)

#define SPI_MAX_REQUEST_SIZE 0x100
#define SPI_MAX_RESPONSE_SIZE 0x100
#define SPI_MAXFIFO_DATA 128
#define SPI_PAST_END_LENGTH 4

/* Store read and write data of SPI by FIFO mode */
static uint32_t in_msg[SPI_MAX_REQUEST_SIZE/4];
static uint32_t out_msg[SPI_MAX_RESPONSE_SIZE/4];

/* The buffer index of read and write */
static uint8_t w_index;
static uint8_t r_index;

/* Parameters used by host protocols */
static struct host_packet spi_packet;
static int pkt_size;

static const uint8_t out_preamble[4] = {
	EC_SPI_PROCESSING,
	EC_SPI_PROCESSING,
	EC_SPI_PROCESSING,
	/* This is the byte which matters */
	EC_SPI_FRAME_START,
};

enum spi_state {
	/* SPI not enabled */
	SPI_STATE_DISABLED = 0,
	/* Setting up FIFO ready */
	SPI_STATE_RX_READY,
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

/* This routine handles spi recevied unexcepted data */
static void spi_bad_received_data(void)
{
	uint8_t i;

	state = SPI_STATE_RX_BAD;
	IT83XX_SPI_SPISRDR = EC_SPI_RX_BAD_DATA;
	CPRINTS("SPI rx bad data");

	CPRINTF("in_msg=[");
	for (i = 0; i < pkt_size; i++)
		CPRINTF("%02x ", in_msg[i]);
	CPRINTF("]\n");
}

/* Write response data of out_msg buffer to TX FIFO */
static void spi_response_host_data(uint16_t tx_size)
{
	int i, spi_status, fifo2_len = 0;

	if (tx_size >= SPI_MAXFIFO_DATA) {
		fifo2_len = tx_size - SPI_MAXFIFO_DATA;
		tx_size = SPI_MAXFIFO_DATA;
	}

	/* Tx FIFO reset and count monitor reset */
	IT83XX_SPI_TXFCR |= IT83XX_SPI_TXFR | IT83XX_SPI_TXFCMR;

	/* Interrupt status register */
	spi_status = IT83XX_SPI_ISR;
	CPRINTS("[SPI] spi_status %x", spi_status);

	/* CPU Tx FIFO1 and FIFO2 access */
	IT83XX_SPI_TXRXFAR = IT83XX_SPI_CPUTFA;

	/*
	 * The status is that SPI slave controller will read data
	 * from Tx FIFO1.
	 */
	if (IT83XX_SPI_TXFSR == 0x7) {

		for (i = 0; i < tx_size/4; i++)
			/* Set data from buffer to master */
			IT83XX_SPI_CPUWTFDB0 = out_msg[r_index + i];

		/* Index to next 128 bytes of read buffer */
		r_index += SPI_MAXFIFO_DATA;
	}

	/*
	 * The status is that SPI slave controller will read data
	 * from Tx FIFO2.
	 */
	if (IT83XX_SPI_TXFSR == 0x0d) {

		for (i = 0; i < fifo2_len/4; i++)
			/* Set data from buffer to master */
			IT83XX_SPI_CPUWTFDB0 = out_msg[r_index + i];
	}

	/*
	 * After writing data to Tx FIFO is finished, this bit will
	 * be to indicate the SPI slave controller.
	 */
	IT83XX_SPI_TXFCR |= IT83XX_SPI_TXFS;

	/* End Tx FIFO access */
	IT83XX_SPI_TXRXFAR = 0;

	/* SPI slave read Tx FIFO */
	IT83XX_SPI_FCR |= IT83XX_SPI_SPISRTXF;

	/* Reset read buffer index */
	r_index = 0;

}

/*
 * Called to send a response back to the host.
 *
 * Some commands can continue for a while. This function is called by
 * host_command when it completes.
 *
 */
static void spi_send_response_packet(struct host_packet *pkt)
{
	uint16_t tx_size;

	if (state != SPI_STATE_PROCESSING)
		return;

	/* Append our past-end byte, which we reserved space for. */
	((uint8_t *)pkt->response)[pkt->response_size + 0] = EC_SPI_PAST_END;

	tx_size = pkt->response_size + sizeof(out_preamble) +
				SPI_PAST_END_LENGTH;

	/* Write response data of out_msg buffer to TX FIFO */
	spi_response_host_data(tx_size);

	/* Transmit the reply */
	state = SPI_STATE_SENDING;
}

/* Parse header for version of spi-protocol */
static void spi_parse_header(void)
{
	/* Chip select is low = asserted */
	if (state != SPI_STATE_READY_TO_RECV) {
		/*
		 * AP started a transaction but we weren't ready for it.
		 * Tell AP we weren't ready, and ignore the received data.
		 */
		CPRINTS("SPI not ready");
		state = SPI_STATE_RX_BAD;
		IT83XX_SPI_SPISRDR = EC_SPI_NOT_READY;
		return;
	}

	/* We're now inside a transaction */
	state = SPI_STATE_RECEIVING;
	IT83XX_SPI_SPISRDR = EC_SPI_RECEIVING;

	/* TODO:
	 * Wait for version, command, length bytes
	 */

	if ((uint8_t)in_msg[0] == EC_HOST_REQUEST_VERSION) {
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

		/* Response must start with the preamble */
		memcpy(out_msg, out_preamble, sizeof(out_preamble));
		spi_packet.response = out_msg + sizeof(out_preamble)/4;

		/* Reserve space for frame start and trailing past-end byte */
		spi_packet.response_max = sizeof(out_msg);
		spi_packet.response_size = 0;
		spi_packet.driver_result = EC_RES_SUCCESS;

		/* Move to processing state */
		state = SPI_STATE_PROCESSING;
		IT83XX_SPI_SPISRDR = EC_SPI_PROCESSING;

		/* Go to common-layer to handle request */
		host_packet_receive(&spi_packet);

	} else {
		/* Invalid version number */
		return spi_bad_received_data();
	}
}

void spi_interrupt(void)
{
	int i, spi_status, fifo_len, fifo2_len = 0;

	/* If SPI slave is not enabled */
	if (state == SPI_STATE_DISABLED)
		return;

	/* TODO:
	 * AP asserts chip select (CS#)
	 */
	if (IT83XX_GPIO_GPDRO & 0x2) {
		CPRINTS("[SPI] GPIOO=%x", IT83XX_GPIO_GPDRO);
		/*
		 * If the buffer is still used by the host command.
		 */
		if (state == SPI_STATE_PROCESSING) {

			IT83XX_SPI_SPISRDR = EC_SPI_NOT_READY;
			return;
		}
	}

	/* Ready to receive */
	state = SPI_STATE_READY_TO_RECV;
	IT83XX_SPI_SPISRDR = EC_SPI_OLD_READY;

	/* Interrupt status register */
	spi_status = IT83XX_SPI_ISR;

	/* Rx FIFO count monitor */
	fifo_len = ((IT83XX_SPI_RXFCMB1 & 0x0f) << 8) |
			(IT83XX_SPI_RXFCMB0 & 0xff);

	if (fifo_len >= SPI_MAXFIFO_DATA) {
		fifo2_len = fifo_len - SPI_MAXFIFO_DATA;
		fifo_len = SPI_MAXFIFO_DATA;
	}

	/* SPI end detection interrupt */
	if (spi_status & IT83XX_SPI_ENDDETECTINT) {

		/*
		 * The status is that SPI slave controller writes data
		 * into Rx FIFO1 or Rx FIFO1 is full.
		 */
		if ((IT83XX_SPI_RXFSR == 0) ||
				(IT83XX_SPI_RXFSR & IT83XX_SPI_RXF1FS)) {

			/* CPU Rx FIFO1 access */
			IT83XX_SPI_TXRXFAR = IT83XX_SPI_CPURXF1A;

			for (i = 0; i < fifo_len/4; i++)
				/* Get data from master to buffer */
				in_msg[(w_index + i)] = IT83XX_SPI_RXFRDRB0;

			/* Index to next 128 bytes of write buffer */
			w_index += SPI_MAXFIFO_DATA/4;
		}

		/*
		 * The status is that SPI slave controller writes data
		 * into Rx FIFO2 or Rx FIFO2 is full.
		 */
		if ((IT83XX_SPI_RXFSR == IT83XX_SPI_RXFFSM) ||
				(IT83XX_SPI_RXFSR & IT83XX_SPI_RXF2FS)) {

			/* CPU Rx FIFO2 access */
			IT83XX_SPI_TXRXFAR = IT83XX_SPI_CPURXF2A;

			for (i = 0; i < fifo2_len/4; i++)
				/* Get data from master to buffer */
				in_msg[(w_index + i)] = IT83XX_SPI_RXFRDRB0;
		}
	}

	/* End Rx FIFO access */
	IT83XX_SPI_TXRXFAR = 0x00;

	/* Reset write buffer index */
	w_index = 0;

	/* Rx FIFO reset and count monitor reset */
	IT83XX_SPI_FCR = IT83XX_SPI_RXFR | IT83XX_SPI_RXFCMR;

	/* Write clear the slave status */
	IT83XX_SPI_ISR = spi_status;

	/* Parse header for version of spi-protocol */
	spi_parse_header();

	/* Clear the interrupt status */
	task_clear_pending_irq(IT83XX_IRQ_SPI_SLAVE);
}

static void spi_slave_enable(void)
{
	/*
	 * Memory controller configuration register 3.
	 * bit6 : SPI pin function select (0b:Enable, 1b:Mask)
	 */
	IT83XX_GCTRL_MCCR3 |= IT83XX_GCTRL_SPISLVPFE;

	/*
	 * Interrupt mask register (0b:Enable, 1b:Mask)
	 * bit2 : SPI end detection interrupt mask
	 */
	IT83XX_SPI_IMR &= ~IT83XX_SPI_EDIM;

	/* Set dummy blcoked byte */
	IT83XX_SPI_HPR2 = 0x00;

	/* Set FIFO data target count */
	IT83XX_SPI_FTCB0R = 0xff;
	IT83XX_SPI_FTCB1R = 0x0f;

	/* Interrupt status register(write one to clear) */
	IT83XX_SPI_ISR = 0xff;

	/* SPI slave controller enable */
	IT83XX_SPI_SPISGCR = IT83XX_SPI_SPISCEN;

	/* Setting up FIFO ready */
	state = SPI_STATE_RX_READY;
	IT83XX_SPI_SPISRDR = EC_SPI_RX_READY;
}

static void spi_init(void)
{
	/* SPI not enabled */
	state = SPI_STATE_DISABLED;

	/* TODO:
	 * Enabling spi module for gpio configuration
	 * gpio_config_module(MODULE_SPI, 1);
	 *
	 * Temporarily presented in this way.
	 */
	IT83XX_GPIO_GPCRO0 &= ~(BIT(7) | BIT(6));
	IT83XX_GPIO_GPCRO1 &= ~(BIT(7) | BIT(6));
	IT83XX_GPIO_GPCRO2 &= ~(BIT(7) | BIT(6));
	IT83XX_GPIO_GPCRO3 &= ~(BIT(7) | BIT(6));

	/* To enable spi slave module */
	spi_slave_enable();

	/* Clear the interrupt status */
	task_clear_pending_irq(IT83XX_IRQ_SPI_SLAVE);

	/* Enable SPI interrupt */
	task_enable_irq(IT83XX_IRQ_SPI_SLAVE);
}
DECLARE_HOOK(HOOK_INIT, spi_init, HOOK_PRIO_INIT_SPI);

/* Get protocol information */
static enum ec_status spi_get_protocol_info(struct host_cmd_handler_args *args)
{
	struct ec_response_get_protocol_info *r = args->response;

	memset(r, 0, sizeof(*r));
	r->protocol_versions = BIT(3);
	r->max_request_packet_size = SPI_MAX_REQUEST_SIZE;
	r->max_response_packet_size = SPI_MAX_RESPONSE_SIZE;
	r->flags = EC_PROTOCOL_INFO_IN_PROGRESS_SUPPORTED;

	args->response_size = sizeof(*r);

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_PROTOCOL_INFO,
		spi_get_protocol_info,
		EC_VER_MASK(0));
