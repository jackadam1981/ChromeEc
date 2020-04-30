/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "clock.h"
#include "dma.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "queue_policies.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "usart_rx_dma.h"
#include "usart_host_command.h"
#include "usart-stm32f4.h"
#include "util.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_HOSTCMD, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_HOSTCMD, format, ## args)

/*
 * Timeout to wait for complete request packet
 *
 * This value determines how long we should wait for entire packet to arrive.
 * USART host command handler should wait for at least 75% of
 * EC_MSG_DEADLINE_MS, before declaring timeout and dropping the packet.
 *
 * This timeout should be less than host's driver timeout to make sure that
 * last packet can be successfully discarded before AP attempts to resend
 * request. AP driver waits for EC_MSG_DEADLINE_MS = 200 before attempting a
 * retry.
 */
#define USART_CMD_RX_TIMEOUT (150 * MSEC)

/*
 * Timeout to wait for overrun bytes on USART
 *
 * This values determines how long call to process_request should be deferred
 * in case host is sending extra bytes. This value is based on DMA buffer size.
 *
 * There is no guarantee that AP will send continuous bytes on usart. Wait
 * for USART_CMD_DEFERRED_TIMEOUT_US to check if host is sending extra bytes.
 * Note: This value affects the response latency.
 */
#define USART_CMD_DEFERRED_TIMEOUT 300

/*
 * Max data size for a version 3 request/response packet.  This is big enough
 * to handle a request/response header, flash write offset/size, and 512 bytes
 * of flash data.
 */
#define USART_MAX_REQUEST_SIZE 0x220
#define USART_MAX_RESPONSE_SIZE 0x220

/*
 * FIFO size for USART DMA. Should be big enough to handle worst case
 * data processing
 */
#define USART_DMA_FIFO_SIZE 0x110

/* Local definitions */

/*
 * Raw USART RX/TX byte buffers.
 */
static uint8_t usart_in_bytes[USART_MAX_REQUEST_SIZE] __aligned(4);
static uint8_t usart_out_bytes[USART_MAX_RESPONSE_SIZE] __aligned(4);

/*
 * Maintain head position of in buffer
 * Head always starts with zero and goes up to max bytes.
 * Once the buffer contents are read, it should go back to zero.
 */
static uint16_t usart_in_head;

/*
 * Once the header is received, get the datalen
 */
static uint16_t usart_in_datalen;

/*
 * Maintain head position of out buffer
 * Head always starts from zero and goes up to max bytes.
 * Head is moved by tx interrupt handler to response size sent by host command
 * task. Once all the bytes are sent (head == tail) both should go back to 0.
 */
static uint16_t usart_out_head;

/*
 * Once the response is ready, get the datalen
 */
static uint16_t usart_out_datalen;

/*
 * Enumeration to maintain different states of incoming request from
 * host
 */
static enum uart_host_command_state {
	/*
	 * USART host command handler not enabled.
	 */
	USART_HOST_CMD_STATE_DISABLED,

	/*
	 * Ready to receive next request
	 * This state represents USART layer is initialized and ready to
	 * receive host request. Once the response is sent, current_state is
	 * reset to this state to accept next packet.
	 */
	USART_HOST_CMD_READY_TO_RX,

	/*
	 * Receiving request
	 * After first byte is received current_state is moved to receiving
	 * state until all the header bytes + datalen bytes are received.
	 * If host_request_timeout was called in this state, it would be
	 * because of an underrun situation.
	 */
	USART_HOST_CMD_RECEIVING,

	/*
	 * Receiving complete
	 * Once all the header bytes + datalen bytes are received, current_state
	 * is moved to complete. Ideally, host should wait for response or retry
	 * timeout before sending anymore bytes, otherwise current_state will
	 * be moved to overrun to represent extra bytes sent by host.
	 */
	USART_HOST_CMD_COMPLETE,

	/*
	 * Processing request
	 * Once the process_request starts processing usart_in_bytes,
	 * current_state is moved to processing state. Host should not send
	 * any bytes in this state as it would be considered contiguous
	 * request.
	 */
	USART_HOST_CMD_PROCESSING,

	/*
	 * Sending response
	 * Once host task is ready with the response bytes, current_state is
	 * moved to sending state.
	 */
	USART_HOST_CMD_SENDING,

	/*
	 * Received bad data
	 * If bad packet header is received, current_state is moved to rx_bad
	 * state and after rx_timeout all the bytes are dropped.
	 */
	USART_HOST_CMD_RX_BAD,

	/*
	 * Receiving data overrun bytes
	 * If extra bytes are received after current_state is in complete,
	 * host is sending extra bytes which indicates data overrun.
	 */
	USART_HOST_CMD_RX_OVERRUN,

} current_state __aligned(4);

/*
 * This diagram is the state machine representation of USART host
 * command layer.
 *
 * This layer is responsible for checking packet integrity of incoming bytes
 * on usart transceiver. It will only process packet header to check version,
 * data_len. This layer will not process payload bytes.
 *
 * STATE = USART_HOST_CMD_STATE_DISABLED
 *
 * Initialize USART and local variables
 *
 * STATE = USART_HOST_CMD_READY_TO_RX
 *
 *     |<---------- HOST RETRY TIMEOUT = 200 ms ---------->|
 *     |
 *     |--------------USART_CMD_RX_TIMEOUT------>|
 *     |     Underrun if request not complete -->|
 *     |                                         |<-- USART ready to rx
 *     |____REQUEST____                                      ____REQUEST____
 *     |     |         |                                    |     |         |
 *     | HDR | DATA    |                                    | HDR | DATA    |
 *     |_____|_________|                                    |_____|_________|
 *     |
 *     |<-- Request packet start
 *           |
 * STATE = USART_HOST_CMD_RECEIVING
 *           |
 *           |<-- HDR received, now we will wait for data len bytes
 *                     |
 * If bad packet is received, move state to rx_bad
 * STATE = USART_HOST_CMD_RX_BAD
 * Ignore data processing, print status on console and reset layer -----------
 *                     |                                                     |
 *                     |<-- Request packet end (data rx complete)            |
 *                     |                                                     |
 * If request_timeout is called, it represents packet underrun               |
 * Ignore data processing, print status on console and reset layer -----------
 *                     |                                                     |
 * STATE = USART_HOST_CMD_COMPLETE                                           |
 *                     |                                                     |
 *                     |<-- Deferred call to process request                 |
 *                     |                                                     |
 * If extra byte is received, move state to overrun                          |
 * STATE = USART_HOST_CMD_RX_OVERRUN                                         |
 * Ignore data processing, print status on console and reset layer -----------
 *                     |                                                     |
 *                  -->|  |<-- USART_CMD_DEFERRED_TIMEOUT                    |
 *                        |    Start process request                         |
 *                        |                                                  |
 * STATE = USART_HOST_CMD_PROCESSING                                         |
 *                        |                                                  |
 * Send ec_host_request to host command task                                 |
 *                        |<-- Packet sent to host command task              |
 *                       >|  |<-- host command task process time             |
 *                           |<-- host command task ready for response       |
 *                           |                                               |
 * STATE = USART_HOST_CMD_SENDING                                            |
 *                           |                                               |
 *                           |____RESPONSE____                               |
 *                           |     |          |                              |
 *                           | HDR | DATA     |                              |
 *                           |_____|__________|                              |
 *                                            |                              |
 *                                            |<-- Response send complete    |
 *                                                                           |
 * STATE = USART_HOST_CMD_READY_TO_RX          <------------------------------
 */

/*
 * Local function definition
 */
static void usart_host_command_reset(void);
static void usart_host_command_request_timeout(void);
static void usart_host_command_process_request(void);
static void usart_host_command_process_response(struct host_packet *pkt);
/*
 * Local variable declaration
 */

/*
 * Configure dma instance for rx
 *
 * STM32_DMAS_USART1_RX is the DMA channel to be used for reception. This DMA
 * channel is for the USART peripheral.
 *
 * A unnamed, valid, empty usart_rx_dma_state structure is required to manage
 * DMA based transmission.
 *
 * USART_DMA_FIFO_SIZE is size of the valid, unnamed  DMA circular buffer.
 * This buffer is large enough to process worst case interrupt latency this
 * layer can encounter.
 */
static struct usart_rx_dma const usart_host_command_rx_dma = {
	.usart_rx = {
		.producer_ops = {
				.read = NULL,
			},
		.init      = usart_rx_dma_init,
		.interrupt = usart_host_command_rx_dma_interrupt,
		.info      = USART_RX_DMA_INFO,
	},
	.state       = &((struct usart_rx_dma_state) {}),
	.fifo_buffer = ((uint8_t[USART_DMA_FIFO_SIZE]) {}),
	.fifo_size   = USART_DMA_FIFO_SIZE,
	.channel     = STM32_DMAS_USART1_RX,
};

/*
 * Configure USART structure with hardware, interrupt handlers, baudrate.
 */
static struct usart_config const tl_usart = {
	.hw	= &CONFIG_UART_HOST_COMMAND_HW,
	.rx	= &usart_host_command_rx_dma.usart_rx,
	.tx	= &usart_host_command_tx_interrupt,
	.state	= &((struct usart_state){}),
	.baud	= CONFIG_UART_HOST_COMMAND_BAUD_RATE,
	.flags	= 0,
};

/*
 * Local function declaration
 */

/*
 * This function will be called only if request rx timed out.
 * Drop the packet and put tl state into RX_READY
 */
static void usart_host_command_request_timeout(void)
{
	switch (current_state) {
	case USART_HOST_CMD_RECEIVING:
		/* If state is receiving then timeout was hit due to underrun */
		CPRINTS("USART HOST CMD ERROR: Request underrun detected.");
		break;

	case USART_HOST_CMD_RX_OVERRUN:
		/* If state is rx_overrun then timeout was hit because
		 * process request was cancelled and extra rx bytes were
		 * dropped
		 */
		CPRINTS("USART HOST CMD ERROR: Request overrun detected.");
		break;

	case USART_HOST_CMD_RX_BAD:
		/* If state is rx_bad then packet header was bad and process
		 * request was cancelled to drop all incoming bytes.
		 */
		CPRINTS("USART HOST CMD ERROR: Bad packet header detected.");
		break;

	default:
		CPRINTS("USART HOST CMD ERROR: Request timeout mishandled");
}

	/* Reset host command layer to accept new request */
	usart_host_command_reset();
}
DECLARE_DEFERRED(usart_host_command_request_timeout);

/*
 * This function is called from interrupt handler after entire packet is
 * received.
 */
static void usart_host_command_process_request(void)
{
	/* Handle usart_in_bytes as ec_host_request */
	struct ec_host_request *ec_request =
			(struct ec_host_request *)usart_in_bytes;

	/* Prepare host_packet for host command task */
	static struct host_packet uart_packet;

	/* Cancel deferred call to timeout handler as request was good */
	hook_call_deferred(
		&usart_host_command_request_timeout_data,
		-1);

	/* Move to processing state */
	current_state = USART_HOST_CMD_PROCESSING;

	/* Currently we only support Protocol version 3 */
	if (ec_request->struct_version == EC_HOST_REQUEST_VERSION) {
		uart_packet.send_response = usart_host_command_process_response;
		uart_packet.request = usart_in_bytes;
		uart_packet.request_temp = NULL;
		uart_packet.request_max = sizeof(usart_in_bytes);
		uart_packet.request_size =
				host_request_expected_size(ec_request);
		uart_packet.response = usart_out_bytes;
		uart_packet.response_max = sizeof(usart_out_bytes);
		uart_packet.response_size = 0;
		uart_packet.driver_result = EC_RES_SUCCESS;
		host_packet_receive(&uart_packet);
	} else if (ec_request->command_version >= EC_CMD_VERSION0) {
		/* Protocol version 2 is deprecated. */
		CPRINTS("USART HOST CMD ERROR: Only protocol V3+ supported.");
	}
}
DECLARE_DEFERRED(usart_host_command_process_request);

/*
 * This function is called from host command task after it is ready with a
 * response.
 */
static void usart_host_command_process_response(struct host_packet *pkt)
{
	/* Move to sending state */
	current_state = USART_HOST_CMD_SENDING;

	usart_out_datalen = pkt->response_size;
	usart_out_head = 0;

	/* Start sending response to host */
	usart_tx_start(&tl_usart);
}

/*
 * This function will drop current request, clear buffers
 */
static void usart_host_command_reset(void)
{
	/* Cancel deferred call to process_request */
	hook_call_deferred(
		&usart_host_command_process_request_data,
		-1);

	/* Cancel deferred call to timeout handler */
	hook_call_deferred(
		&usart_host_command_request_timeout_data,
		-1);

	/* Clear in buffer, head and datalen */
	usart_in_datalen = 0;
	usart_in_head = 0;
	memset(usart_in_bytes, 0, sizeof(*usart_in_bytes));

	/* Clear out buffer, head and datalen */
	usart_out_datalen = 0;
	usart_out_head = 0;
	memset(usart_out_bytes, 0, sizeof(*usart_out_bytes));

	/* Move to ready state*/
	current_state = USART_HOST_CMD_READY_TO_RX;
}

/*
 * Function to handle incoming bytes from DMA interrupt handler
 */
size_t usart_host_command_rx_add_units(struct queue const *q,
		const void *src, size_t count)
{
	/* Define ec_host_request pointer to process in bytes later*/
	struct ec_host_request *ec_request =
			(struct ec_host_request *) usart_in_bytes;

	/* Host can send extra bytes than in header data_len
	 * Only copy valid bytes in buffer
	 */
	if ((current_state == USART_HOST_CMD_READY_TO_RX) ||
	(current_state == USART_HOST_CMD_RECEIVING) ||
	((usart_in_head + count) < USART_MAX_REQUEST_SIZE)) {
		/* Copy all the bytes from DMA FIFO */
		memcpy(usart_in_bytes + usart_in_head,
			src, count);
	}

	/* Add incoming byte count to in_usart_head */
	usart_in_head += count;

	if (current_state == USART_HOST_CMD_READY_TO_RX) {
		/* Kick deferred call to request timeout handler */
		hook_call_deferred(
			&usart_host_command_request_timeout_data,
			USART_CMD_RX_TIMEOUT);

		/* Move current state to receiving */
		current_state = USART_HOST_CMD_RECEIVING;
	}

	if ((current_state == USART_HOST_CMD_RECEIVING) &&
	(usart_in_head >= sizeof(struct ec_host_request))) {
		/* Buffer has request header. Check header and get data_len */
		usart_in_datalen = host_request_expected_size(ec_request);

		if ((usart_in_datalen == 0) ||
		(usart_in_datalen > USART_MAX_REQUEST_SIZE)) {
			/* EC host request version not compatible or
			 * reserved byte is not zero.
			 */
			current_state = USART_HOST_CMD_RX_BAD;
		} else if (usart_in_datalen == sizeof(struct ec_host_request)) {
			/* If no data in request, packet is complete */
			current_state = USART_HOST_CMD_COMPLETE;

			/* Wait for deferred timeout and process */
			hook_call_deferred(
				&usart_host_command_process_request_data,
				USART_CMD_DEFERRED_TIMEOUT);
		}
	}

	if ((current_state == USART_HOST_CMD_RECEIVING) &&
	(usart_in_head == usart_in_datalen)) {
		/* All data len bytes received */
		current_state = USART_HOST_CMD_COMPLETE;

		/* Wait for deferred timeout and process */
		hook_call_deferred(
			&usart_host_command_process_request_data,
			USART_CMD_DEFERRED_TIMEOUT);
	}

	if (((usart_in_head > usart_in_datalen) &&
	(usart_in_datalen > sizeof(struct ec_host_request))) ||
			(current_state == USART_HOST_CMD_RX_OVERRUN)) {
		/* Cancel deferred call to process_request */
		hook_call_deferred(
			&usart_host_command_process_request_data,
			-1);

		/* Move state to overrun*/
		current_state = USART_HOST_CMD_RX_OVERRUN;
	}

	if (current_state == USART_HOST_CMD_PROCESSING) {
		/* Host should not send data before receiving a response.
		 * Since the request was already sent to host command task,
		 * just notify console about this. After response is sent
		 * dma will be cleared to handle next packet
		 */
		CPRINTS("USART HOST CMD ERROR: Contiguous packets detected.");
	}

	/* Return count to show all incoming bytes were processed */
	return count;
}

/*
 * Exported functions
 */

/*
 * Initialize USART host command layer.
 */
void usart_host_command_init(void)
{
	/* USART host command layer starts in DISABLED state */
	current_state = USART_HOST_CMD_STATE_DISABLED;

	/* Initialize transport uart */
	usart_init(&tl_usart);

	/* Initialize local variables */
	usart_in_head = 0;
	usart_in_datalen = 0;
	usart_out_head = 0;
	usart_out_datalen = 0;

	/* Move to ready state */
	current_state = USART_HOST_CMD_READY_TO_RX;
}

/*
 * This function processes the outgoing bytes from tl usart.
 */
size_t usart_host_command_tx_remove_unit(struct queue const *q,
		void *dest)
{
	size_t bytes_remaining = 0;

	if ((current_state == USART_HOST_CMD_SENDING)
	&& (usart_out_datalen != 0)) {
		*((uint8_t *) dest) =
			usart_out_bytes[usart_out_head++];

		bytes_remaining = usart_out_datalen - usart_out_head;

		if (bytes_remaining == 0)
			usart_host_command_reset();
	}

	/* Return count of bytes remaining in out buffer */
	return bytes_remaining;
}
