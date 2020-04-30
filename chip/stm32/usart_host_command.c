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
#include "usart.h"
#include "usart_host_command.h"
#include "usart-stm32f4.h"
#include "util.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_FP, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_FP, format, ## args)


/*
 * Timeout to wait for USART request packet
 *
 * This values how long we should wait to entire packet to arrive.
 * Based on the TL baudrate, contiguous USART max packet should take ~1800 us.
 * We should give 10 times this best case time to AP to send packet in case it
 * is not able to send contiguous bytes.
 * This timeout should be less than host's driver timeout to make sure that
 * last packet can be successfully discarded before AP attempts to resend
 * request.
 */
#define USART_CMD_RX_TIMEOUT_US 18000

/*
 * Max data size for a version 3 request/response packet.  This is big enough
 * to handle a request/response header, flash write offset/size, and 512 bytes
 * of flash data.
 */
#define USART_MAX_REQUEST_SIZE 0x220
#define USART_MAX_RESPONSE_SIZE 0x220

/* Local definitions */

/*
 * Enumeration to maintain different states of TL
 */
static enum tl_uart_state {
    /* TL not enabled (initial state, and when chipset is off) */
    TL_USART_STATE_DISABLED = 0,

    /* Ready to receive next request */
    TL_USART_READY_TO_RX,

    /* Receiving request */
    TL_USART_RECEIVING,

    /* Processing request */
    TL_USART_PROCESSING,

    /* Sending response */
    TL_USART_SENDING,

    /*
     * Received bad data
     * Conditions - Data underrun, Data overrun, Packet timeout
     *
     * transaction started before we were ready, or
     * packet header from host didn't parse properly.  Ignoring received
     * data.
     */
    TL_USART_RX_BAD,
} current_state;

/* In case HDR is received, store the expected data len to process */
uint16_t current_expected_data_len;
/*
 * Raw USART RX/TX byte buffers.
 */
static uint8_t tl_in_usart_bytes[USART_MAX_REQUEST_SIZE];
static uint8_t tl_out_usart_bytes[USART_MAX_RESPONSE_SIZE];


/*
 * STATE = TL_USART_STATE_DISABLED
 *
 * Initialize TL USART and local variables
 *
 * STATE = TL_USART_READY_TO_RX
 *
 *     |-------------EC_MSG_DEADLINE_MS = 200--------------->|
 *     |
 *     |               |---USART_CMD_RX_TIMEOUT_US--->|
 *     |               |                              |<-- TL ready to rx
 *     |____REQUEST____|                                     ____REQUEST____
 *     |     |         |                                    |     |         |
 *     | HDR | DATA    |                                    | HDR | DATA    |
 *     |_____|_________|                                    |_____|_________|
 *     |
 *     |<-- Request packet start
 *           |<-- HDR received
 *
 * STATE = TL_USART_RECEIVING
 *                     |
 *                     |<-- Request packet end (data rx complete)
 *                     |
 * In case of underrun, overrun or packet timeout, STATE = TL_USART_RX_BAD
 * Ignore data, print status and packet on console and ready to rx.__________
 * STATE = TL_USART_PROCESSING                                               |
 *                     |<-- Process request                                  |
 *                     |                                                     |
 * Send ec_host_request to host command task                                 |
 *                     |<-- Packet sent to host command task                 |
 *                    >|     |<-- host command task process time             |
 *                           |<-- host command task ready for response       |
 * STATE = TL_USART_SENDING                                                  |
 *                           |____RESPONSE____                               |
 *                           |     |          |                              |
 *                           | HDR | DATA     |                              |
 *                           |_____|__________|                              |
 *                                            |                              |
 *                                            |<-- Response send complete    |
 *                                                                           |
 * STATE = TL_USART_READY_TO_RX                                          <---|
 */

/*
 * Local functions
 */


/*
 *
 */
static struct usart_config const tl_usart = {
        &CONFIG_TL_UART_HW,
        &tl_usart_rx_interrupt,
        &tl_usart_tx_interrupt,
        &((struct usart_state){}),
        CONFIG_TL_UART_BAUD_RATE,
        0,
        .consumer = {
                .queue = NULL,
                .ops = NULL,
                },
        .producer = {
                .queue = NULL,
                .ops = NULL,
                }
};

/*
 * This function processes the incoming byte from tl usart.
 */
void tl_usart_rx_char(char* in_char) {

}


/*
 *
 */
void tl_usart_process_request(char* in_char) {
    /*  */
    struct ec_host_request *ec_request =
            (struct ec_host_request *)tl_in_usart_bytes;

    static struct host_packet tl_uart_packet;

    /* Currently we only support Protocol version 3 */
    if (ec_request->command_version == EC_HOST_REQUEST_VERSION) {
        int pkt_size;

		/* Wait for the rest of the command header */
		if (wait_for_bytes(rxdma, sizeof(*r), GPIO_SPI1_NSS))
			goto spi_event_error;

		/*
		 * Check how big the packet should be.  We can't just wait to
		 * see how much data the host sends, because it will keep
		 * sending dummy data until we respond.
		 */
		pkt_size = host_request_expected_size(r);
		if (pkt_size == 0 || pkt_size > sizeof(in_msg))
			goto spi_event_error;

		/* Wait for the packet data */
		if (wait_for_bytes(rxdma, pkt_size, GPIO_SPI1_NSS))
			goto spi_event_error;

		tl_uart_packet.send_response = tl_usart_process_response;

		tl_uart_packet.request = tl_in_usart_bytes;
		tl_uart_packet.request_temp = NULL;
		tl_uart_packet.request_max = sizeof(tl_in_usart_bytes);
		tl_uart_packet.request_size = pkt_size;
		tl_uart_packet.response = tl_out_usart_bytes;
		/* Reserve space for the preamble and trailing past-end byte */
		tl_uart_packet.response_max = sizeof(tl_in_usart_bytes);
		tl_uart_packet.response_size = 0;

		tl_uart_packet.driver_result = EC_RES_SUCCESS;

		/* Move to processing state */
		state = SPI_STATE_PROCESSING;
		tx_status(EC_SPI_PROCESSING);

		host_packet_receive(&tl_uart_packet);
		return;

	} else if (in_msg[0] >= EC_CMD_VERSION0) {
#else /* !defined(CONFIG_SPI_PROTOCOL_V2) */
		/* Protocol version 2 is deprecated. */
		CPRINTS("ERROR: Protocol V2 is not supported!");
#endif /* defined(CONFIG_SPI_PROTOCOL_V2) */
	}

 spi_event_error:

}

/*
 *
 */
void tl_usart_request_error(void) {
	/* Error, timeout, or protocol we can't handle.  Ignore data. */
	tx_status(EC_SPI_RX_BAD_DATA);
	state = SPI_STATE_RX_BAD;
	CPRINTS("SPI rx bad data");

	CPRINTF("in_msg=[");
	for (i = 0; i < dma_bytes_done(rxdma, sizeof(in_msg)); i++)
		CPRINTF("%02x ", in_msg[i]);
	CPRINTF("]\n");

}

/*
 *
 */
void tl_usart_process_response(char* in_char) {

}

/*
 * Exported functions
 */


/*
 *
 */
void usart_host_command_init(void) {

    /* TL starts in DISABLED state */
    current_state = TL_USART_STATE_DISABLED;

    /* Initialize transport uart */
    usart_init(&tl_usart);

    /* Initialize local variables */
    current_state = TL_USART_READY_TO_RX;
    current_expected_data_len = 0;

}
