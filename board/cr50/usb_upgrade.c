/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "consumer.h"
#include "endian.h"
#include "flash.h"
#include "producer.h"
#include "queue.h"
#include "queue_policies.h"
#include "shared_mem.h"
#include "system.h"
#include "task.h"
#include "usb-stream.h"
#include "usb_upgrade.h"
#include "watchdog.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)

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
		       UNOFFICIAL_USB_SUBCLASS_GOOGLE_CR50,
		       0xff,			/* vendor-specific protocol */
		       USB_STR_UPGRADE_NAME,
		       USB_EP_UPGRADE,
		       USB_MAX_PACKET_SIZE,
		       USB_MAX_PACKET_SIZE,
		       usb_to_upgrade,
		       upgrade_to_usb)


enum rx_state {
	rx_idle,
	rx_inside_block,
	rx_outside_block,
	rx_awaiting_reset
};
struct upgrade_command {
	uint32_t  block_digest;
	uint32_t  block_base;
};

struct update_pdu_header {
	uint32_t block_size;
	union {
		struct upgrade_command cmd;
		uint32_t resp;
	};
	/* The actual payload goes here. */
};

enum rx_state rx_state_ = rx_idle;
static uint8_t *block_buffer;
static uint32_t block_size;
static uint32_t block_index;

/* Called to deal with data from the host */
static void upgrade_out_handler(struct consumer const *consumer, size_t count)
{
	struct update_pdu_header updu;
	size_t resp_size;
	uint32_t resp_value;

	if (rx_state_ == rx_idle) {
		/* This better be the first block, of zero size. */
		if (count != sizeof(struct update_pdu_header)) {
			CPRINTS("FW update: wrong first block size %d\n",
				count);
			return;
		}
		QUEUE_REMOVE_UNITS(consumer->queue, &updu, count);

		CPRINTS("FW update: starting...\n");

		fw_upgrade_command_handler(&updu.cmd, count -
					   offsetof(struct update_pdu_header, cmd),
					   &resp_size);

		/* Let the host know what upgrader had to say. */
		QUEUE_ADD_UNITS(&upgrade_to_usb, &updu.resp, resp_size);

		if (resp_size == 4)
			rx_state_ = rx_outside_block;
		return;
	}

	if (rx_state_ == rx_awaiting_reset) {
		CPRINTS("reboot hard");
		cflush();
		system_reset(SYSTEM_RESET_HARD);
		while(1);
	}

	if (rx_state_ == rx_outside_block) {
		/* Expecting to receive the beginning of the block. */
		if (count == 4) {
			uint32_t command;

			QUEUE_REMOVE_UNITS(consumer->queue, &command,
					   sizeof(command));
			if (command == UPGRADE_DONE) {
				CPRINTS("FW update: done\n");
				resp_value = 0;
				QUEUE_ADD_UNITS(&upgrade_to_usb, &resp_value,
						sizeof(resp_value));
				rx_state_ = rx_awaiting_reset;
				return;
			}
		}

		if (count < sizeof(updu)) {
			CPRINTS("FW update: error: first chunk of %d bytes\n",
				count);
			rx_state_ = rx_idle;
			return;
		}

		QUEUE_REMOVE_UNITS(consumer->queue, &updu, sizeof(updu));

		/* Let's allocate a large enough buffer. */
		block_size = be32toh(updu.block_size) -
			offsetof(struct update_pdu_header, cmd);
		if (shared_mem_acquire(block_size, (char **)&block_buffer)
		    != EC_SUCCESS) {
			/* report out of memory here. */
			CPRINTS("FW update: error: failed to allocate %d bytes.\n",
				block_size);
			return;
		}
		block_index = sizeof(updu) -
			offsetof(struct update_pdu_header, cmd);
		memcpy(block_buffer, &updu.cmd, block_index);
		QUEUE_REMOVE_UNITS(consumer->queue,
				   block_buffer + block_index,
				   count - sizeof(updu));
		block_index += count - sizeof(updu);
		block_size -= block_index;
		rx_state_ = rx_inside_block;
		return;
	}

	/* Must be inside block. */
	QUEUE_REMOVE_UNITS(consumer->queue,
			   block_buffer + block_index, count);
	block_index += count;
	block_size -= count;

	if (block_size)
		return;	/* More to come. */

	fw_upgrade_command_handler(block_buffer, block_index, &resp_size);

	shared_mem_release(block_buffer);
	resp_value = block_buffer[0];
	QUEUE_ADD_UNITS(&upgrade_to_usb, &resp_value, sizeof(resp_value));
	rx_state_ = rx_outside_block;
}

static void upgrade_flush(struct consumer const *consumer)
{
}

struct consumer const upgrade_consumer = {
	.queue = &usb_to_upgrade,
	.ops   = &((struct consumer_ops const) {
		.written = upgrade_out_handler,
		.flush   = upgrade_flush,
	}),
};
