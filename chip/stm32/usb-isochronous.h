/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USB_ISOCHRONOUS_H
#define __CROS_EC_USB_ISOCHRONOUS_H

#include "compile_time_macros.h"
#include "hooks.h"
#include "usb_descriptor.h"
#include "usb_hw.h"

#include <stdint.h>

struct usb_isochronous_state {

};

struct usb_isochronous_config {
	struct usb_isochronous_state *volatile state;

	/*
	 * Endpoint index.
	 */
	int endpoint;

	/*
	 * Deferred function to call to handle USB and Queue request.
	 */
	const struct deferred_data *deferred;

	/*
	 * USB packet RAM buffers.
	 */
	size_t tx_size;
	usb_uint *tx_ram_0;
	usb_uint *tx_ram_1;
};

/*
 * Get packet buffer used by application software.
 *
 * According to RM0091,
 *
 * DTOG | USB buffer | App Buffer
 * ------------------------------
 *   0  |     TX     |    RX
 *   1  |     RX     |    TX
 */
#define GET_TX_DTOG(USB_ISOCHRONOUS_CONFIG_PTR) \
	((STM32_USB_EP((USB_ISOCHRONOUS_CONFIG_PTR)->endpoint) & EP_TX_DTOG))
#define APP_ADDR(USB_ISOCHRONOUS_CONFIG_PTR, DTOG_VALUE) \
	(DTOG_VALUE ?			\
	 btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].tx_addr :	\
	 btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].rx_addr)
#define APP_COUNT(USB_ISOCHRONOUS_CONFIG_PTR, DTOG_VALUE) \
	(DTOG_VALUE ?			\
	 btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].tx_count :	\
	 btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].rx_count)

#define SET_APP_ADDR(USB_ISOCHRONOUS_CONFIG_PTR, DTOG_VALUE, V) \
	(DTOG_VALUE ?			\
	 (btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].tx_addr = (V)) : \
	 (btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].rx_addr = (V)))
#define SET_APP_COUNT(USB_ISOCHRONOUS_CONFIG_PTR, DTOG_VALUE, V) \
	(DTOG_VALUE ?			\
	 (btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].tx_count = (V)) : \
	 (btable_ep[(USB_ISOCHRONOUS_CONFIG_PTR)->endpoint].rx_count = (V)))


/*
 * These function tables are defined by the USB isochronous driver and are used
 * to initialize the consumer and producer in the usb_isochronous_config.
 */
extern struct consumer_ops const usb_isochronous_consumer_ops;

#define USB_ISOCHRONOUS_CONFIG_FULL(NAME,				\
				    INTERFACE,				\
				    INTERFACE_CLASS,			\
				    INTERFACE_SUBCLASS,			\
				    INTERFACE_PROTOCOL,			\
				    INTERFACE_NAME,			\
				    ENDPOINT,				\
				    TX_SIZE)				\
	BUILD_ASSERT(TX_SIZE <= USB_MAX_PACKET_SIZE);			\
	BUILD_ASSERT(TX_SIZE > 0);					\
	BUILD_ASSERT((TX_SIZE <   64 && (TX_SIZE & 0x01) == 0) ||	\
		     (TX_SIZE < 1024 && (TX_SIZE & 0x1f) == 0));	\
	/* Declare buffer */						\
	static usb_uint CONCAT2(NAME, _ep_tx_buffer_0)[TX_SIZE / 2] __usb_ram; \
	static usb_uint CONCAT2(NAME, _ep_tx_buffer_1)[TX_SIZE / 2] __usb_ram; \
	static struct usb_isochronous_state CONCAT2(NAME, _state);	\
	static void CONCAT2(NAME, _deferred_)(void);			\
	DECLARE_DEFERRED(CONCAT2(NAME, _deferred_));			\
	struct usb_isochronous_config const NAME = {			\
		.state     = &CONCAT2(NAME, _state),			\
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

/* Write @count bytes into @config queue from @src, returns number of bytes
 * actually written to @config queue.
 */
size_t usb_isochronous_write_queue(struct usb_isochronous_config const *config,
				   void *src,
				   size_t src_count);

#endif /* __CROS_EC_USB_ISOCHRONOUS_H */
