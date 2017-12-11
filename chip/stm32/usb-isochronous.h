/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USB_ISOCHRONOUS_H
#define __CROS_EC_USB_ISOCHRONOUS_H

#include "compile_time_macros.h"
#include "hooks.h"
#include "queue.h"
#include "usb_descriptor.h"
#include "usb_hw.h"

#include <stdint.h>

/* Currently, we only support TX direction for USB isochronous transfer. */

struct usb_isochronous_state {
	/* The offset of next byte to be transmitted in object. */
	uint8_t object_offset;
};

struct usb_isochronous_config {
	struct usb_isochronous_state * volatile state;

	int endpoint;

	struct queue * const queue;
	size_t object_size;

	/*
	 * Deferred function to call to handle USB and Queue request.
	 */
	const struct deferred_data *deferred;

	/* USB packet RAM buffer size. */
	size_t tx_size;
	/* USB packet RAM buffers. */
	usb_uint *tx_ram_0;
	usb_uint *tx_ram_1;
};

/* Define an USB isochronous interface */
#define USB_ISOCHRONOUS_CONFIG_FULL(NAME,				\
				    INTERFACE,				\
				    INTERFACE_CLASS,			\
				    INTERFACE_SUBCLASS,			\
				    INTERFACE_PROTOCOL,			\
				    INTERFACE_NAME,			\
				    ENDPOINT,				\
				    TX_SIZE,				\
				    QUEUE_SIZE,				\
				    OBJECT_SIZE)			\
	BUILD_ASSERT(TX_SIZE > 0);					\
	BUILD_ASSERT((TX_SIZE <   64 && (TX_SIZE & 0x01) == 0) ||	\
		     (TX_SIZE < 1024 && (TX_SIZE & 0x1f) == 0));	\
	/* Declare buffer */						\
	static usb_uint CONCAT2(NAME, _ep_tx_buffer_0)[TX_SIZE / 2] __usb_ram; \
	static usb_uint CONCAT2(NAME, _ep_tx_buffer_1)[TX_SIZE / 2] __usb_ram; \
	static void CONCAT2(NAME, _deferred_)(void);			\
	DECLARE_DEFERRED(CONCAT2(NAME, _deferred_));			\
	struct queue CONCAT2(NAME, _queue) = QUEUE_NULL(		\
		QUEUE_SIZE, uint8_t);					\
	struct usb_isochronous_state CONCAT2(NAME, _state);		\
	struct usb_isochronous_config const NAME = {			\
		.queue = &CONCAT2(NAME, _queue),			\
		.object_size = OBJECT_SIZE,				\
		.state = &CONCAT2(NAME, _state),			\
		.endpoint  = ENDPOINT,					\
		.deferred  = &CONCAT2(NAME, _deferred__data),		\
		.tx_size   = TX_SIZE,					\
		.tx_ram_0  = CONCAT2(NAME, _ep_tx_buffer_0),		\
		.tx_ram_1  = CONCAT2(NAME, _ep_tx_buffer_1),		\
	};								\
	const struct usb_interface_descriptor				\
	USB_IFACE_DESC(INTERFACE) = {					\
		.bLength            = USB_DT_INTERFACE_SIZE,		\
		.bDescriptorType    = USB_DT_INTERFACE,			\
		.bInterfaceNumber   = INTERFACE,			\
		.bAlternateSetting  = 0,				\
		.bNumEndpoints      = 1,				\
		.bInterfaceClass    = INTERFACE_CLASS,			\
		.bInterfaceSubClass = INTERFACE_SUBCLASS,		\
		.bInterfaceProtocol = INTERFACE_PROTOCOL,		\
		.iInterface         = INTERFACE_NAME,			\
	};								\
	const struct usb_endpoint_descriptor				\
	USB_EP_DESC(INTERFACE, 0) = {					\
		.bLength          = USB_DT_ENDPOINT_SIZE,		\
		.bDescriptorType  = USB_DT_ENDPOINT,			\
		.bEndpointAddress = 0x80 | ENDPOINT,			\
		.bmAttributes     = 0x01 /* Isochronous IN */,		\
		.wMaxPacketSize   = TX_SIZE,				\
		.bInterval        = 1,					\
	};								\
	static void CONCAT2(NAME, _ep_tx)(void)				\
	{								\
		usb_isochronous_tx(&NAME);				\
	}								\
	static void CONCAT2(NAME, _ep_rx)(void)				\
	{								\
		return;							\
	}								\
	static void CONCAT2(NAME, _ep_event)(enum usb_ep_event evt)	\
	{								\
		usb_isochronous_event(&NAME, evt);			\
	}								\
	USB_DECLARE_EP(ENDPOINT,					\
		       CONCAT2(NAME, _ep_tx),				\
		       CONCAT2(NAME, _ep_rx),				\
		       CONCAT2(NAME, _ep_event));			\
	static void CONCAT2(NAME, _deferred_)(void)			\
	{								\
		usb_isochronous_deferred(&NAME);			\
	}

void usb_isochronous_deferred(struct usb_isochronous_config const *config);
void usb_isochronous_rx(struct usb_isochronous_config const *config);
void usb_isochronous_tx(struct usb_isochronous_config const *config);
void usb_isochronous_event(struct usb_isochronous_config const *config,
			   enum usb_ep_event event);

size_t usb_isochronous_write_queue(struct usb_isochronous_config const *config,
				   void *unit);

#endif /* __CROS_EC_USB_ISOCHRONOUS_H */
