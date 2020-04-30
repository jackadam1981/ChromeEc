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
#define CPRINTS(format, args...) cprints(CC_USART, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USART, format, ## args)


/*
 * Timeout to wait for USART request packet
 *
 * This value determines how long we should wait for entire packet to arrive.
 * Worst case ( 115200 bps ) max packet receive time 47 ms. TL should wait for
 * atleast 2x of that time, before declaring timeout and dropping the packet.
 *
 * This timeout should be less than host's driver timeout to make sure that
 * last packet can be successfully discarded before AP attempts to resend
 * request. AP should wait for EC_MSG_DEADLINE_MS = 200 before attempting a
 * retry.
 */
#define USART_CMD_RX_TIMEOUT_US 100000

/*
 * Timeout to wait for overrun bytes on USART
 *
 * This values determines how long call to process_request should be deferred
 * in case host is sending extra bytes. This value is based on worst case
 * baudrate ( 115200 bps ) time to send ~ 10 bytes.
 */
#define USART_CMD_DEFERRED_TIMEOUT_US 1000

/*
 * Max data size for a version 3 request/response packet.  This is big enough
 * to handle a request/response header, flash write offset/size, and 512 bytes
 * of flash data.
 */
#define USART_MAX_REQUEST_SIZE 0x220
#define USART_MAX_RESPONSE_SIZE 0x220

/* Local definitions */

/*
 * Raw USART RX/TX byte buffers.
 */
static uint8_t tl_in_usart_bytes[USART_MAX_REQUEST_SIZE];
static uint8_t tl_out_usart_bytes[USART_MAX_RESPONSE_SIZE];

/*
 * Maintain head position of in buffer
 * Head always starts with zero and goes upto max bytes.
 * Once the buffer contents are read, it should go back to zero.
 */
static uint16_t tl_in_usart_head;

/*
 * Once the header is received, get the datalen
 */
static uint16_t tl_in_datalen;

/*
 * Maintain head position of out buffer
 * Head always starts from zero and goes upto max bytes.
 * Head is moved by tx interrupt handler to response size sent by host command
 * task. Once all the bytes are sent (head == tail) both should go back to 0.
 */
static uint16_t tl_out_usart_head;

/*
 * Once the response is ready, get the datalen
 */
static uint16_t tl_out_datalen;

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

    /* Receiving complete */
    TL_USART_RX_COMPLETE,

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

/*
 * Configure USART structure with TL hardware, interrupt handlers, baudrate.
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

/* This diagram is the state machine representation of TL USART layer.
 *
 *
 * STATE = TL_USART_STATE_DISABLED
 *
 * Initialize TL USART and local variables
 *
 * STATE = TL_USART_READY_TO_RX
 *
 *     |-------------EC_MSG_DEADLINE_MS = 200--------------->|
 *     |
 *     |-------------------USART_CMD_RX_TIMEOUT_US--->|
 *     |          Underrun if request not complete -->|
 *     |                                              |<-- TL ready to rx
 *     |____REQUEST____                                      ____REQUEST____
 *     |     |         |                                    |     |         |
 *     | HDR | DATA    |                                    | HDR | DATA    |
 *     |_____|_________|                                    |_____|_________|
 *     |
 *     |<-- Request packet start
 *           |<-- HDR received, TL will wait for data len bytes
 *
 * STATE = TL_USART_RECEIVING
 *                     |
 *                     |<-- Request packet end (data rx complete)
 *                     |
 * In case of underrun, overrun or packet timeout, STATE = TL_USART_RX_BAD
 * Ignore data, print status and packet on console and ready to rx.-----------
 *                     |                                                     |
 * STATE = TL_USART_PROCESSING                                               |
 *                    >|  |<-- Deferred call time to process request         |
 *                     |  |    Process request                               |
 * Send ec_host_request to host command task                                 |
 *                        |<-- Packet sent to host command task              |
 *                       >|  |<-- host command task process time             |
 *                           |<-- host command task ready for response       |
 * STATE = TL_USART_SENDING                                                  |
 *                           |____RESPONSE____                               |
 *                           |     |          |                              |
 *                           | HDR | DATA     |                              |
 *                           |_____|__________|                              |
 *                                            |                              |
 *                                            |<-- Response send complete    |
 *                                                                           |
 * STATE = TL_USART_READY_TO_RX                                          <----
 */

/*
 * Local function definition
 */
static void tl_usart_drop_request(void);
static void tl_usart_request_timeout(void);
void tl_usart_rx_char(char* in_char);
uint16_t tl_usart_tx_char(char* in_char);
static void tl_usart_process_request(void);
static void tl_usart_process_response(struct host_packet *pkt);

/*
 * Local function declaration
 */

/*
 * This function will be called only if request rx timed out.
 * Drop the packet and put tl state into RX_READY
 */
static void tl_usart_request_timeout(void) {
    /**/
    CPRINTS("TL USART ERROR: Request underrun detected.");

    tl_usart_drop_request();
}
DECLARE_DEFERRED(tl_usart_request_timeout);

/*
 * This function is called from interrupt handler after entire packet is
 * received.
 */
static void tl_usart_process_request(void) {
    /* Handle tl_in_usart_bytes as ec_host_request */
    struct ec_host_request *ec_request =
            (struct ec_host_request *)tl_in_usart_bytes;

    /* Prepare host_packet for host command task */
    static struct host_packet tl_uart_packet;

    /* Move to processing state */
    current_state = TL_USART_PROCESSING;

    /* Currently we only support Protocol version 3 */
    if (ec_request->struct_version == EC_HOST_REQUEST_VERSION) {
        tl_uart_packet.send_response = tl_usart_process_response;
        tl_uart_packet.request = tl_in_usart_bytes;
        tl_uart_packet.request_temp = NULL;
        tl_uart_packet.request_max = sizeof(tl_in_usart_bytes);
        tl_uart_packet.request_size = host_request_expected_size(ec_request);
        tl_uart_packet.response = tl_out_usart_bytes;
        tl_uart_packet.response_max = sizeof(tl_out_usart_bytes);
        tl_uart_packet.response_size = 0;
        tl_uart_packet.driver_result = EC_RES_SUCCESS;
        host_packet_receive(&tl_uart_packet);
    } else if (ec_request->command_version >= EC_CMD_VERSION0) {
        /* Protocol version 2 is deprecated. */
        CPRINTS("TL USART ERROR: Protocol V2 is not supported.");
    }
}
DECLARE_DEFERRED(tl_usart_process_request);

/*
 * This function is called from host command task after it is ready with a
 * response.
 */
static void tl_usart_process_response(struct host_packet *pkt) {
    /* Move to sending state */
    current_state = TL_USART_SENDING;

    tl_out_datalen = pkt->response_size;
    tl_out_usart_head = 0;

    /* Start sending response to host */
    tl_usart_tx_start(&tl_usart);
}

/*
 * This function will drop current request, clear buffers and
*/
static void tl_usart_drop_request(void) {
    /* Cancel deferred call to process_request */
    hook_call_deferred(&tl_usart_process_request_data, -1);

    /* Cancel deferred call to timeout handler */
    hook_call_deferred(&tl_usart_request_timeout_data,-1);

    /* Clear in buffer, head and datalen */
    tl_in_datalen = 0;
    tl_in_usart_head = 0;
    memset(tl_in_usart_bytes, 0, sizeof(*tl_in_usart_bytes));

    /* Clear out buffer, head and datalen */
    tl_out_datalen = 0;
    tl_out_usart_head = 0;
    memset(tl_out_usart_bytes, 0, sizeof(*tl_out_usart_bytes));

    /* Move to ready state*/
    current_state = TL_USART_READY_TO_RX;
}

/*
 * Exported functions
 */

/*
 * Initialize USART host command layer.
 */
void usart_host_command_init(void) {

    /* TL starts in DISABLED state */
    current_state = TL_USART_STATE_DISABLED;

    /* Initialize transport uart */
    usart_init(&tl_usart);

    /* Initialize local variables */
    tl_in_usart_head = 0;
    tl_in_datalen = 0;
    tl_out_usart_head = 0;
    tl_out_datalen = 0;

    /* Move to ready state */
    current_state = TL_USART_READY_TO_RX;
}

/*
 * This function processes the incoming byte from tl usart.
 */
void tl_usart_rx_char(char* in_char) {
    /* Define ec_host_request pointer to process in bytes later*/
    struct ec_host_request *ec_request =
            (struct ec_host_request *)tl_in_usart_bytes;

    if(current_state == TL_USART_READY_TO_RX) {
        /* This is the first byte of the request */
        tl_in_usart_head = 0; // Put head to start position

        /* Store incoming char in USART buffer */
        tl_in_usart_bytes[tl_in_usart_head++] = *in_char;

        /* Kick deferred call to request timeout handler */
        hook_call_deferred(&tl_usart_request_timeout_data,
                USART_CMD_RX_TIMEOUT_US);

        /* Move current state to receiving */
        current_state = TL_USART_RECEIVING;

    } else if ((current_state == TL_USART_RECEIVING) &&
    (tl_in_usart_head < sizeof(struct ec_host_request))) {
        /* Store incoming char in USART buffer */
        tl_in_usart_bytes[tl_in_usart_head++] = *in_char;

        if (tl_in_usart_head == sizeof(struct ec_host_request)) {
            /* Received all bytes of header */
            tl_in_datalen = host_request_expected_size(ec_request);

            if (tl_in_datalen == sizeof(struct ec_host_request)) {
                /* If not data in request, packet is complete */
                current_state = TL_USART_RX_COMPLETE;

                /* Cancel deferred call to timeout handler */
                hook_call_deferred(&tl_usart_request_timeout_data,-1);

                /* Wait for deferred timeout and process */
                hook_call_deferred(&tl_usart_process_request_data,
                        USART_CMD_DEFERRED_TIMEOUT_US);
            }
        }
    } else if ((current_state == TL_USART_RECEIVING) &&
    (tl_in_usart_head < tl_in_datalen)) {
        /* Store incoming char in USART buffer */
        tl_in_usart_bytes[tl_in_usart_head++] = *in_char;

        if (tl_in_usart_head == tl_in_datalen) {
            /* We received all the datalen, packet is complete */
            current_state = TL_USART_RX_COMPLETE;

            /* Cancel deferred call to timeout handler */
            hook_call_deferred(&tl_usart_request_timeout_data,-1);

            /* Wait for deferred timeout and process */
            hook_call_deferred(&tl_usart_process_request_data,
                    USART_CMD_DEFERRED_TIMEOUT_US);
        }
    } else if (current_state == TL_USART_RX_COMPLETE) {
        /* Request packet overrun detected.
         * Drop the packet and wait for next request. */
        CPRINTS("TL USART ERROR: Request overrun detected.");

        tl_usart_drop_request();
    } else {
        /* Host should not send data before receiving a response.
         * Since the request was already sent to host command task, just notify
         * console about this.
         */
         CPRINTS("TL USART ERROR: Contiguous request packet detected.");
    }
}

/*
 * This function processes the outgoing bytes from tl usart.
 */
uint16_t tl_usart_tx_char(char* in_char) {
    uint16_t valid_bytes = 0;
    if ((current_state == TL_USART_SENDING) && (tl_out_datalen != 0))
    {
        *in_char = tl_out_usart_bytes[tl_out_usart_head++];

        valid_bytes = tl_out_datalen - tl_out_usart_head;

        if (valid_bytes == 0) {
            current_state = TL_USART_READY_TO_RX;
            tl_out_datalen = 0;
            tl_out_usart_head = 0;
            memset(tl_out_usart_bytes, 0, sizeof(*tl_out_usart_bytes));
        }
    }
    return valid_bytes;
}