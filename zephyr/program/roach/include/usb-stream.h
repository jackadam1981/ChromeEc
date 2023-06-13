
#include "consumer.h"
#include "producer.h"

/* one-wire UART adapter for firmware updater engine */

#define USB_MAX_PACKET_SIZE 64

struct usb_stream_config {
	struct consumer consumer;
	struct producer producer;
};

#define USB_STREAM_CONFIG_FULL(NAME, INTERFACE, INTERFACE_CLASS, \
		INTERFACE_SUBCLASS, INTERFACE_PROTOCOL, \
		INTERFACE_NAME, ENDPOINT, RX_SIZE, TX_SIZE, \
		RX_QUEUE, TX_QUEUE, RX_IDX, TX_IDX) \
	extern const struct consumer_ops consumer_ops_ ## NAME; \
	const struct producer_ops producer_ops_ ## NAME = { \
		.read = NULL \
	}; \
	const struct usb_stream_config NAME = { \
		.consumer = { \
			.queue = &TX_QUEUE, \
			.ops = &consumer_ops_ ## NAME, \
		}, \
		.producer = { \
			.queue = NULL, \
			.ops = &producer_ops_ ## NAME, \
		}, \
	};
