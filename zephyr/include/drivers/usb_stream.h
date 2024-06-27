/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "consumer.h"
#include "producer.h"
#include <zephyr/sys/printk.h>

struct usb_stream_config {
	uint8_t subclass;
	void (*usb_written)(struct consumer const *consumer, size_t count);
	struct consumer consumer;
	struct producer producer;
};

void usb_written_usb_update(struct consumer const *consumer, size_t count);
void usb_written_i2c_usb_(struct consumer const *consumer, size_t count);
__maybe_unused static void usb_stream_written(struct consumer const *consumer, size_t count)
{
	struct usb_stream_config const *config =
		DOWNCAST(consumer, struct usb_stream_config, consumer);

	printk("%s ITE Debug %d subclass 0x%x\n", __func__, __LINE__, config->subclass);
	config->usb_written(consumer, count);
}

#define USB_STREAM_CONFIG_FULL(NAME, INTERFACE, INTERFACE_CLASS,           \
			       INTERFACE_SUBCLASS, INTERFACE_PROTOCOL,     \
			       INTERFACE_NAME, ENDPOINT, RX_SIZE, TX_SIZE, \
			       RX_QUEUE, TX_QUEUE, RX_IDX, TX_IDX)         \
	static const struct consumer_ops consumer_ops_##NAME = {           \
		.written = usb_stream_written,                         \
	};                                                                 \
	static const struct producer_ops producer_ops_##NAME = {           \
		.read = NULL,                                              \
	};                                                                 \
	const struct usb_stream_config NAME = {                            \
		.subclass = INTERFACE_SUBCLASS, \
		.usb_written = usb_written_##NAME, \
		.consumer = {                                              \
			.queue = &TX_QUEUE,                                \
			.ops = &consumer_ops_ ## NAME,                     \
		},                                                         \
		.producer = {                                              \
			.queue = &RX_QUEUE,                                \
			.ops = &producer_ops_ ## NAME,                     \
		},                                                         \
	}

extern const struct usb_stream_config usb_update;
extern const struct usb_stream_config i2c_usb_;
