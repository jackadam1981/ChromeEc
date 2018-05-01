/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "queue_policies.h"
#include "reassembly.h"
#include "shared_mem.h"
#include "upgrade_fw.h"
#include "usb-stream.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)

/*
 * This file is an adaptation layer between the USB interface and the firmware
 * update engine. The engine expects to receive long blocks of data, 1K or so
 * in size, prepended by the offset where the data needs to be programmed into
 * the flash and a 4 byte integrity check value.
 *
 * The USB transfer, on the other hand, operates on much shorter chunks of
 * data, typically 64 bytes. This module uses the generic reassembly layer to
 * combine the chunks into a single PDU and verify its integrity.
 *
 * Reassembled PDUs are passed to fw_upgrade_command_handler() which reports
 * result by putting the return value into the same buffer where the PDU was
 * passed in. This wrapper retrieves the programmer's return value, and sends
 * it back to the host.
 */

struct consumer const upgrade_consumer;
struct usb_stream_config const usb_upgrade;

static struct queue const upgrade_to_usb = QUEUE_DIRECT(64, uint8_t,
						     null_producer,
						     usb_upgrade.consumer);
static struct queue const usb_to_upgrade = QUEUE_DIRECT(64, uint8_t,
						     usb_upgrade.producer,
						     upgrade_consumer);

USB_STREAM_CONFIG_FULL(usb_upgrade,
		       USB_IFACE_UPGRADE,
		       USB_CLASS_VENDOR_SPEC,
		       USB_SUBCLASS_GOOGLE_CR50,
		       USB_PROTOCOL_GOOGLE_CR50_NON_HC_FW_UPDATE,
		       USB_STR_UPGRADE_NAME,
		       USB_EP_UPGRADE,
		       USB_MAX_PACKET_SIZE,
		       USB_MAX_PACKET_SIZE,
		       usb_to_upgrade,
		       upgrade_to_usb)

/*
 * Actual data is 1024 bytes in size, let's allow some more room for headers,
 * checks, etc.
 */
#define MAX_UPDATE_PDU_SIZE 0x1100

static void upgrade_out_handler(struct consumer const *consumer, size_t count)
{
	static struct reassembly_context *ctx;
	enum reassembly_result result;
	size_t response_size = 0;
	struct reassembly_payload payload;
	uint8_t *response_buffer;

	/* New PDU starting? */
	if (!ctx) {
		char *buf;
		size_t buf_size = reassembly_overhead() + MAX_UPDATE_PDU_SIZE;

		if (shared_mem_acquire(buf_size, &buf) != EC_SUCCESS) {
			uint8_t resp_value;

			CPRINTS("%s: problem: failed to allocate %d bytes",
				__func__, buf_size);
			queue_advance_head(consumer->queue, count);
			resp_value = UPGRADE_MALLOC_ERROR;
			QUEUE_ADD_UNITS(&upgrade_to_usb, &resp_value, 1);
			return;
		}
		ctx = reassembly_register(buf, buf_size,
					  upgrade_pdu_sha1_check);
	}

	result = reassembly_feed(ctx, consumer->queue, count);
	reassembly_get_payload(ctx, &payload);
	switch(result) {
	case RS_NEED_MORE_DATA:
		return;
	case RS_SUCCESS:
		fw_upgrade_command_handler(payload.rs_data,
					   payload.rs_data_size,
					   &response_size);
		break;
	default:
/* NEED TO PROCESS ERRORS PROPERLY. */
		break;
	}

	/*
	 * There sure is one extra byte below payload, let's use that space to
	 * communicate reassembly result to the server.
	 */
	response_buffer = ((uint8_t *)payload.rs_data);
	response_buffer[-1] = result;
	QUEUE_ADD_UNITS(&upgrade_to_usb, response_buffer - 1, response_size + 1);
	shared_mem_release(ctx);
	ctx = NULL;
}

struct consumer const upgrade_consumer = {
	.queue = &usb_to_upgrade,
	.ops   = &((struct consumer_ops const) {
		.written = upgrade_out_handler,
	}),
};
