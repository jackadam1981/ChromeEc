/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "consumer.h"
#include "flash.h"
#include "queue.h"
#include "queue_policies.h"
#include "producer.h"
#include "task.h"
#include "usb-stream.h"
#include "usb_upgrade.h"
#include "system.h"
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


/* What are we doing? */
static struct usb_upgrade_reply what_now;
static uint32_t end_offset;

/* Called to deal with data from the host */
static void upgrade_out_handler(struct consumer const *consumer, size_t count)
{
	enum system_image_copy_t current_fw;
	uint8_t buf[USB_MAX_PACKET_SIZE] __aligned(sizeof(uint32_t));

	/* Let's see the data */
	count = QUEUE_REMOVE_UNITS(consumer->queue, buf, count);

	/* Starting a new update can happen at any time. */
	if (count == sizeof(uint32_t) &&
	    (*(uint32_t *)buf == UPGRADE_START)) {

		/* Nothing allowed by default */
		end_offset = 0;

		/* We want to replace the image that's not in use */
		current_fw = system_get_image_copy();
		switch (current_fw) {
		case SYSTEM_IMAGE_RW:
			what_now.status = UPGRADE_EXPECT_RW_B;
			what_now.offset = CONFIG_RW_B_MEM_OFF;
			break;
		case SYSTEM_IMAGE_RW_B:
			what_now.status = UPGRADE_EXPECT_RW_A;
			what_now.offset = CONFIG_RW_MEM_OFF;
			break;
		default:
			what_now.status = UPGRADE_FAILURE;
			what_now.offset = __LINE__;
		}

		/* Erase the target image */
		if (IS_EXPECT_RW(what_now.status)) {

			CPRINTS("FW update: erasing 0x%08x 0x%08x",
				what_now.offset, CONFIG_RW_SIZE);

			watchdog_reload();
			if (flash_erase(what_now.offset, CONFIG_RW_SIZE)) {
				what_now.status = UPGRADE_FAILURE;
				what_now.offset = __LINE__;
			} else {
				/* Erased, allow writes to proceed */
				end_offset = what_now.offset + CONFIG_RW_SIZE;
			}
		}

		goto reply;
	}

	/* If we get a full packet when we're expecting to write, do it */
	if (count == USB_MAX_PACKET_SIZE &&
	    IS_EXPECT_RW(what_now.status)) {

		/* Don't write too much */
		if (what_now.offset >= end_offset) {
			CPRINTS("Trying to write past 0x%08x", end_offset);
			what_now.status = UPGRADE_FAILURE;
			what_now.offset = __LINE__;
		} else {
			if (!(what_now.offset & 0x00003FFF))
				CPRINTS("FW update: 0x%08x", what_now.offset);

			watchdog_reload();
			if (flash_write(what_now.offset, count, buf)) {
				what_now.status = UPGRADE_FAILURE;
				what_now.offset = __LINE__;
			} else {
				/* Written, advance write pointer */
				what_now.offset += count;
			}
		}

		goto reply;

	}

	/* Host has to tell us when it's done */
	if (count == sizeof(uint32_t) &&
	    (*(uint32_t *)buf == UPGRADE_DONE)) {

		/* If we're updating, reset our state and reply */
		if (IS_EXPECT_RW(what_now.status)) {

			CPRINTS("FW update: host says that's all");

			what_now.status = 0;
			what_now.offset = 0;
			end_offset = 0;

			goto reply;
		}

		/* If we're NOT updating, reboot immediately */
		CPRINTS("reboot hard");
		cflush();
		system_reset(SYSTEM_RESET_HARD);

		return;
	}

	/* Any other traffic is an error */
	what_now.status = UPGRADE_FAILURE;
	what_now.offset = __LINE__;

	CPRINTS("%s:%d got %d bytes, don't know what to do",
		__FILE__, __LINE__, count);

reply:
	/* Send back a status reply after every message we get */
	QUEUE_ADD_UNITS(&upgrade_to_usb, &what_now, sizeof(what_now));
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
