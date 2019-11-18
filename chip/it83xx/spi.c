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
#include "system.h"
#include "task.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SPI, outstr)
#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SPI, format, ## args)

/*
 * Max data size for a version 3 request/response packet.
 * This is big enough to handle a request/response header.
 */
#define SPI_MAX_REQUEST_SIZE 0x80
#define SPI_MAX_RESPONSE_SIZE 0xF8

#define EC_SPI_PAST_END_LENGTH 4

static const uint8_t out_preamble[] = {
	EC_SPI_PROCESSING,
	EC_SPI_PROCESSING,
	EC_SPI_PROCESSING,
	/* This is the byte which matters */
	EC_SPI_FRAME_START,
};

/* Store read and write data of SPI by FIFO mode */
static uint8_t in_msg[SPI_MAX_REQUEST_SIZE];
static uint32_t out_msg[(SPI_MAX_RESPONSE_SIZE +
		sizeof(out_preamble) + EC_SPI_PAST_END_LENGTH) / 4];

/* The buffer index of write */
static uint8_t buf_index;

/* Parameters used by host protocols */
static struct host_packet spi_packet;

/* Forward declaration */
static void reset_fifo_for_transfer(void);

enum spi_state {
	/* SPI not enabled */
	SPI_STATE_DISABLED = 0,
	/* Setting up FIFO */
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
static void spi_bad_received_data(int count)
{
	uint8_t i;

	/* We're now inside a transaction */
	IT83XX_SPI_SPISRDR = EC_SPI_RX_BAD_DATA;
	state = SPI_STATE_RX_BAD;

	CPRINTS("SPI rx bad data");

	CPRINTF("in_msg=[");
	for (i = 0; i < count; i++)
		CPRINTF("%02x ", in_msg[i]);
	CPRINTF("]\n");

	/* Reset fifo and prepare to for next transaction */
	reset_fifo_for_transfer();
}

/* Write response data from buffer(out_msg) to TX FIFO */
static void spi_response_host_data(uint16_t tx_size)
{
	int i, count;

	/* Tx FIFO reset and count monitor reset */
	IT83XX_SPI_TXFCR = IT83XX_SPI_TXFR | IT83XX_SPI_TXFCMR;

	/* CPU Tx FIFO1 and FIFO2 access */
	IT83XX_SPI_TXRXFAR = IT83XX_SPI_CPUTFA;

	if (tx_size % 4)
		count = tx_size / 4 + 1;
	else
		count = tx_size / 4;


	for (i = 0; i < count; i++)
		/* Set data from buffer to master */
		IT83XX_SPI_CPUWTFDB0 = out_msg[i];

	/*
	 * After writing data to Tx FIFO is finished, this bit will
	 * be to indicate the SPI slave controller.
	 */
	IT83XX_SPI_TXFCR = IT83XX_SPI_TXFS;

	/* End Tx FIFO access */
	IT83XX_SPI_TXRXFAR = 0;

	/* SPI slave read Tx FIFO */
	IT83XX_SPI_FCR = IT83XX_SPI_SPISRTXF;
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
	int i;
	uint16_t tx_size;

	if (state != SPI_STATE_PROCESSING)
		return;

	/* Append our past-end byte, which we reserved space for. */
	for (i = 0; i < EC_SPI_PAST_END_LENGTH; i++)
		((uint8_t *)pkt->response)[pkt->response_size + i]
			= EC_SPI_PAST_END;

	tx_size = pkt->response_size + sizeof(out_preamble) +
				EC_SPI_PAST_END_LENGTH;

	/* Write response data from buffer(out_msg) to TX FIFO */
	spi_response_host_data(tx_size);

	/* Transmit the reply */
	state = SPI_STATE_SENDING;
}

static int wait_rx_fifo_count(int count)
{
	uint16_t fifo_len;

	while(1) {
		/* Rx FIFO count monitor */
		fifo_len = ((IT83XX_SPI_RXFCMB1 & 0x0f) << 8) |
				(IT83XX_SPI_RXFCMB0 & 0xff);

		if(fifo_len >= count)
			return 0;

		if (IT83XX_GPIO_GPDRO & IT83XX_GPIO_CS) {
			CPRINTS("Ths Rx FIFO count is zero");
			return -1;
		}
	}
}

/* Store request data from RX FIFO to buffer(in_msg) */
static void spi_host_request_data(uint16_t count)
{
	int i;

	/* CPU Rx FIFO1 access */
	IT83XX_SPI_TXRXFAR = IT83XX_SPI_CPURXF1A;

	/*
	 * The status is that SPI slave controller writes data
	 * into Rx FIFO2 or Rx FIFO2 is full.
	 */
	if ((IT83XX_SPI_RXFSR == IT83XX_SPI_RXFFSM) ||
					(IT83XX_SPI_RXFSR & IT83XX_SPI_RXF2FS))
		/* CPU Rx FIFO2 access */
		IT83XX_SPI_TXRXFAR |= ~IT83XX_SPI_CPURXF1A |
								IT83XX_SPI_CPURXF2A;

	for (i = 0; i < count; i += 4) {
		/* Get data from master to buffer */
		in_msg[buf_index + i] = IT83XX_SPI_RXFRDRB0;
		in_msg[(buf_index + i + 1)] = IT83XX_SPI_RXFRDRB1;
		in_msg[(buf_index + i + 2)] = IT83XX_SPI_RXFRDRB2;
		in_msg[(buf_index + i + 3)] = IT83XX_SPI_RXFRDRB3;
	}

	/* Index to next write buffer byte */
	buf_index += count;
}

/* Parse header for version of spi-protocol */
static void spi_parse_header(void)
{
	uint16_t pkt_size;

	struct ec_host_request *r = (struct ec_host_request *)in_msg;

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

	/* EC has started receiving the request from the AP */
	IT83XX_SPI_SPISRDR = EC_SPI_RECEIVING;
	state = SPI_STATE_RECEIVING;

	/* Wait for version, command, length bytes */
	/*
	 * TODO: Rx FIFO does not receive the data, when the CPU
	 * starts accessing FIFO. So we will wait the data of
	 * maximum request size for 128 bytes at one time.
	 */
	if (wait_rx_fifo_count(SPI_MAX_REQUEST_SIZE))
		return spi_bad_received_data(SPI_MAX_REQUEST_SIZE);

	/* Store request data from RX FIFO to buffer(in_msg) */
	spi_host_request_data(sizeof(*r));

	/* Protocol version 3 */
	if (in_msg[0] == EC_HOST_REQUEST_VERSION) {

		/* Check how big the packet should be */
		pkt_size = host_request_expected_size(r);

		if (pkt_size == 0 || pkt_size > sizeof(in_msg))
			return spi_bad_received_data(pkt_size);

		/* Store request data from RX FIFO to buffer(in_msg) */
		spi_host_request_data(pkt_size - sizeof(*r));

		/* Set up parameters for host request */
		spi_packet.send_response = spi_send_response_packet;

		spi_packet.request = in_msg;
		spi_packet.request_temp = NULL;
		spi_packet.request_max = sizeof(in_msg);
		spi_packet.request_size = pkt_size;

		/* Response must start with the preamble */
		memcpy(out_msg, out_preamble, sizeof(out_preamble));

		spi_packet.response = out_msg + sizeof(out_preamble) / 4;

		/* Reserve space for frame start and trailing past-end byte */
		spi_packet.response_max = sizeof(out_msg)
			- sizeof(out_preamble) - EC_SPI_PAST_END_LENGTH;

		spi_packet.response_size = 0;
		spi_packet.driver_result = EC_RES_SUCCESS;

		/* Move to processing state */
		IT83XX_SPI_SPISRDR = EC_SPI_PROCESSING;
		state = SPI_STATE_PROCESSING;

		/* Go to common-layer to handle request */
		host_packet_receive(&spi_packet);

	} else {

		CPRINTS("Invalid version number");
		/* Invalid version number */
		return spi_bad_received_data(1);
	}
}

static void reset_fifo_for_transfer(void)
{
	/* Ready to receive */
	IT83XX_SPI_SPISRDR = EC_SPI_OLD_READY;
	state = SPI_STATE_READY_TO_RECV;

	/* Reset write buffer index */
	buf_index = 0;

	/* End Rx FIFO access */
	IT83XX_SPI_TXRXFAR = 0x00;

	/* Rx FIFO reset and count monitor reset */
	IT83XX_SPI_FCR = IT83XX_SPI_RXFR | IT83XX_SPI_RXFCMR;
}

void spi_event(enum gpio_signal signal)
{
	/* If SPI slave is not enabled */
	if (state == SPI_STATE_DISABLED)
		return;

	/*
	 * Check chip select(CS#) pin. If it's high,
	 * the AP ended the transaction data.
	 */
	if (IT83XX_GPIO_GPDRO & IT83XX_GPIO_CS) {

		/* Reset fifo and prepare to for next transaction */
		reset_fifo_for_transfer();

		enable_sleep(SLEEP_MASK_SPI);

		/* Set up for the next transaction */
		return;
	}
	disable_sleep(SLEEP_MASK_SPI);

	/* Parse header for version of spi-protocol */
	spi_parse_header();
}

static void spi_slave_enable(void)
{
	/*
	 * Memory controller configuration register 3.
	 * bit6 : SPI pin function select (0b:Enable, 1b:Mask)
	 */
	IT83XX_GCTRL_MCCR3 |= IT83XX_GCTRL_SPISLVPFE;

	/* Set dummy blcoked byte */
	IT83XX_SPI_HPR2 = 0x00;

	/* Set FIFO data target count */
	IT83XX_SPI_FTCB0R = 0x00;
	IT83XX_SPI_FTCB1R = 0x01;

	/* Interrupt status register(write one to clear) */
	IT83XX_SPI_ISR = 0xff;

	/* SPI slave controller enable */
	IT83XX_SPI_SPISGCR = IT83XX_SPI_SPISCEN;

	/* Setting up FIFO ready */
	state = SPI_STATE_RX_READY;
}

static void spi_chipset_startup(void)
{
	/* Set SPI pins to alternate function */
	gpio_config_module(MODULE_SPI, 1);

	/* Reset fifo and prepare to for next transaction */
	reset_fifo_for_transfer();

	/* Enable SPI interrupt */
	gpio_clear_pending_interrupt(GPIO_SPI0_CS);
	gpio_enable_interrupt(GPIO_SPI0_CS);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, spi_chipset_startup, HOOK_PRIO_DEFAULT);

static void spi_chipset_shutdown(void)
{
	state = SPI_STATE_DISABLED;

	/* Disable SPI interrupt */
	gpio_disable_interrupt(GPIO_SPI0_CS);

	/* Set SPI pins to inputs so we don't leak power when AP is off */
	gpio_config_module(MODULE_SPI, 0);

	/* Allow deep sleep when AP off */
	enable_sleep(SLEEP_MASK_SPI);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, spi_chipset_shutdown, HOOK_PRIO_DEFAULT);

static void spi_init(void)
{
	/* SPI not enabled */
	state = SPI_STATE_DISABLED;

	/* To enable spi slave module */
	spi_slave_enable();
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
