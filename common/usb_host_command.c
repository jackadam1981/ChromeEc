/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "consumer.h"
#include "ec_commands.h"
#include "queue_policies.h"
#include "host_command.h"
#include "stdbool.h"
#include "system.h"
#include "usb_api.h"
#include "usb-stream.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USB, outstr)
#define CPRINTS(format, args...) cprints(CC_USB, "USBHC: " format, ## args)

enum usbhc_state {
	/* SPI not enabled (initial state, and when chipset is off) */
	USBHC_STATE_DISABLED = 0,
	/* Ready to receive next request */
	USBHC_STATE_READY_TO_RX,
	/* Receiving request */
	USBHC_STATE_RECEIVING,
	/* Processing request */
	USBHC_STATE_PROCESSING,
	/* Sending response */
	USBHC_STATE_SENDING,
	/*
	 * Received bad data - transaction started before we were ready, or
	 * packet header from host didn't parse properly.  Ignoring received
	 * data.
	 */
	USBHC_STATE_RX_BAD,
} state;

struct consumer const hostcmd_consumer;
struct producer const hostcmd_producer;
struct usb_stream_config const usbhc_stream;

/* RX (Host->EC) */
static struct queue const usb_to_hostcmd = QUEUE_DIRECT(USBHC_MAX_REQUEST_SIZE,
							uint8_t,
							usbhc_stream.producer,
							hostcmd_consumer);
/* TX (EC->Host) */
static struct queue const hostcmd_to_usb = QUEUE_DIRECT(USBHC_MAX_RESPONSE_SIZE,
							uint8_t,
							hostcmd_producer,
							usbhc_stream.consumer);

USB_STREAM_CONFIG_FULL(usbhc_stream,
		       USB_IFACE_HOSTCMD,
		       USB_CLASS_VENDOR_SPEC,
		       USB_SUBCLASS_GOOGLE_HOSTCMD,
		       USB_PROTOCOL_GOOGLE_HOSTCMD,
		       USB_STR_HOSTCMD_NAME,
		       USB_EP_HOSTCMD,
		       USB_MAX_PACKET_SIZE,
		       USB_MAX_PACKET_SIZE,
		       usb_to_hostcmd,
		       hostcmd_to_usb)

static uint8_t in_msg[EC_USB_HOST_PACKET_SIZE];
static uint8_t out_msg[EC_USB_HOST_PACKET_SIZE];
static uint32_t out_size;
static struct host_packet usbhc_packet;
static struct ec_host_request *header = (struct ec_host_request *)in_msg;
static uint64_t prev_activity_timestamp;

static void usbhc_read(struct producer const *producer, size_t count)
{
	static uint32_t out_index;
	size_t len;

	len = MIN(producer->queue->buffer_units, out_size - out_index);
	len = MIN(count, len);

	/* If we're not sending, what's going on? */
	if (state != USBHC_STATE_SENDING)
		return;

	/* Put a piece of a response in the TX queue. */
	QUEUE_ADD_UNITS(producer->queue, out_msg + out_index, len);
	out_index += len;

	if (out_index < out_size)
		/* More data to send. */
		return;

	CPRINTS("TX complete (%u bytes)", out_index);
	out_index = 0;
	state = USBHC_STATE_READY_TO_RX;
}

struct producer const hostcmd_producer = {
	.queue = &hostcmd_to_usb,
	.ops   = &((struct producer_ops const) {
		.read = usbhc_read,
	}),
};

/**
 * Called to send a response back to the host.
 *
 * Some commands can continue for a while. This function is called by
 * host_command when it completes.
 */
static void usbhc_send_response_packet(struct host_packet *pkt)
{
	/*
	 * If we're not processing, then the AP has already terminated the
	 * transaction, and won't be listening for a response.
	 */
	if (state != USBHC_STATE_PROCESSING)
		return;

	if (sizeof(out_msg) < pkt->response_size) {
		CPRINTS("Reponse size exceeds TX buffer (%u)",
			pkt->response_size);
		return;
	}

	memcpy(out_msg, pkt->response, pkt->response_size);
	out_size = pkt->response_size;
	state = USBHC_STATE_SENDING;

	usbhc_read(&hostcmd_producer, hostcmd_to_usb.buffer_units);
}

static void usbhc_process_packet(uint32_t pkt_size)
{
	usbhc_packet.send_response = usbhc_send_response_packet;
	usbhc_packet.request = in_msg;
	usbhc_packet.request_temp = NULL;
	usbhc_packet.request_max = sizeof(in_msg);
	usbhc_packet.request_size = pkt_size;

	usbhc_packet.response = out_msg;
	/* Reserve space for the preamble and trailing past-end byte */
	usbhc_packet.response_max = sizeof(out_msg);
	usbhc_packet.response_size = 0;
	usbhc_packet.driver_result = EC_RES_SUCCESS;

	/* Do we need to notify the host we're processing the command? */
	//tx_status(EC_SPI_PROCESSING);

	host_packet_receive(&usbhc_packet);
}

/*
 * Called when usb-stream copies incoming data to the usbhc_stream RX queue.
 */
static void usbhc_written(struct consumer const *consumer, size_t count)
{
	static uint32_t block_index;
	static uint32_t expected_size;
	uint64_t delta_time;

	/* How much time since the previous USB callback? */
	delta_time = get_time().val - prev_activity_timestamp;
	prev_activity_timestamp += delta_time;

	/* If timeout exceeds 5 seconds - let's start over. */
	if ((delta_time > 5000000) && state != USBHC_STATE_READY_TO_RX) {
		state = USBHC_STATE_READY_TO_RX;
		CPRINTS("Recovering after timeout");
	}

	switch (state) {
	case USBHC_STATE_READY_TO_RX:
		CPRINTS("Rx start. (count=%d)", count);
		block_index = 0;
		/* Only version 3 is supported. Using in_msg as a courtesy. */
		QUEUE_REMOVE_UNITS(consumer->queue, in_msg, count);
		CPRINTS("%ph", HEX_BUF(in_msg, count));
		if (in_msg[0] != EC_HOST_REQUEST_VERSION) {
			CPRINTS("Unsupported version: %u", in_msg[0]);
			return;
		}
		block_index += count;
		expected_size = host_request_expected_size(header);
		if (block_index < expected_size) {
			state = USBHC_STATE_RECEIVING;
		} else if (sizeof(in_msg) < expected_size) {
			CPRINTS("Expected data is too large (expected=%d)",
				expected_size);
			state = USBHC_STATE_RX_BAD;
		} else {
			if (expected_size < block_index) {
				CPRINTS("Packet is larger (expected=%d)",
					expected_size);
				// Not sure why we receive count=20 bytes for
				// an outsize=0 packet. Padding?
				//state = USBHC_STATE_RX_BAD;
				//return;
			}
			CPRINTS("Rx complete (%d bytes)", block_index);
			state = USBHC_STATE_PROCESSING;
			usbhc_process_packet(block_index);
		}
		return;
	case USBHC_STATE_RECEIVING:
		/* Continue to receive the remaining data. */
		break;
	case USBHC_STATE_PROCESSING:
		/*
		 * Take no action. Rx queue will be eventually full then the
		 * host will receive appropriate error (e.g. BUSY, UNAVAILABLE,
		 * etc.).
		 */
	case USBHC_STATE_SENDING:
		/*
		 * Take no action though we have resource to receive a new
		 * request. The host will get a buffer full error or timeout.
		 */
	case USBHC_STATE_RX_BAD:
	case USBHC_STATE_DISABLED:
		return;
	}

	/* Must be inside block. */
	CPRINTS("Received %d bytes", count);

	if (sizeof(in_msg) < block_index + count) {
		CPRINTS("Rx buffer overflow");
		state = USBHC_STATE_RX_BAD;
		return;
	}
	QUEUE_REMOVE_UNITS(consumer->queue, in_msg + block_index, count);
	block_index += count;

	if (block_index < expected_size)
		return;	/* More to come. */

	CPRINTS("RX complete (%d bytes)", block_index);

	if (expected_size < block_index) {
		CPRINTS("Packet is larger (expected=%d)", expected_size);
		/*
		 * We don't want to be RX_READY too soon because most likely
		 * more anomalous data will come. Setting to RX_BAD will make
		 * us time out, hoping the host will fix the situation.
		 */
		state = USBHC_STATE_RX_BAD;
		return;
	}

	/*
	 * Ok, the entire packet has been received. Parse it and pass it to the
	 * host command handler.
	 */
	state = USBHC_STATE_PROCESSING;
	usbhc_process_packet(block_index);
}

struct consumer const hostcmd_consumer = {
	.queue = &usb_to_hostcmd,
	.ops   = &((struct consumer_ops const) {
		.written = usbhc_written,
	}),
};

static enum ec_status host_command_protocol_info(struct host_cmd_handler_args
						 *args)
{
	return usb_get_protocol_info(args);
}
DECLARE_HOST_COMMAND(EC_CMD_GET_PROTOCOL_INFO,
		     host_command_protocol_info,
		     EC_VER_MASK(0));
