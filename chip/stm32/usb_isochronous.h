/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USB_ISOCHRONOUS_H
#define __CROS_EC_USB_ISOCHRONOUS_H

#include "common.h"
#include "compile_time_macros.h"
#include "hooks.h"
#include "usb_descriptor.h"
#include "usb_hw.h"

struct usb_isochronous_config;

/*
 * Currently, we only support TX direction for USB isochronous transfer.
 *
 * According to RM0091, isochronous transfer is always double buffered.
 * Addresses of buffers are pointed by `btable_ep[<endpoint>].tx_addr` and
 * `btable_ep[<endpoint>].rx_addr`.
 *
 * DTOG | USB Buffer | App Buffer
 * -----+------------+-----------
 *   0  | tx_addr    | rx_addr
 *   1  | rx_addr    | tx_addr
 *
 * That is, when DTOG bit is 0 (see `get_tx_dtog()`), USB hardware will read
 * from `tx_addr`, and our application can write new data to `rx_addr` at the
 * same time.
 *
 * Number of bytes in each buffer shall be tracked by `tx_count` and `rx_count`
 * respectively.
 *
 * `get_app_addr()`, `set_app_addr()`, `set_app_count()` help you to to select
 * the correct variable to use by given DTOG value, which is available by
 * `get_tx_dtog()`.
 */
static int get_tx_dtog(struct usb_isochronous_config const *config);

/*
 * Gets buffer address that can be used by software (application).
 *
 * The mapping between application buffer address and current TX DTOG value is
 * shown in table above.
 */
static usb_uint *get_app_addr(struct usb_isochronous_config const *config,
			      int dtog_value);

/*
 * Sets number of bytes written to application buffer.
 */
static void set_app_count(struct usb_isochronous_config const *config,
			  int dtog_value,
			  usb_uint count);

struct usb_isochronous_config {
	int endpoint;

	/* The task to wake up when a packet is sent.
	 *
	 * The task should write data to the buffer returned by `get_app_addr`,
	 * and call `set_app_count` to set number of bytes available in the
	 * buffer.
	 */
	int task_id;

	/*
	 * Received SET_INTERFACE request.
	 *
	 * @param  alternate_setting	new bAlternateSetting value.
	 * @param  interface		interface number.
	 * @return int			0 for success, -1 for unknown setting.
	 */
	int (*set_interface)(usb_uint alternate_setting, usb_uint interface);

	/* USB packet RAM buffer size. */
	size_t tx_size;
	/* USB packet RAM buffers. */
	usb_uint *tx_ram[2];
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
				    TASK_ID,			\
				    SET_INTERFACE)			\
	BUILD_ASSERT(TX_SIZE > 0);					\
	BUILD_ASSERT((TX_SIZE <   64 && (TX_SIZE & 0x01) == 0) ||	\
		     (TX_SIZE < 1024 && (TX_SIZE & 0x1f) == 0));	\
	/* Declare buffer */						\
	static usb_uint CONCAT2(NAME, _ep_tx_buffer_0)[TX_SIZE / 2] __usb_ram; \
	static usb_uint CONCAT2(NAME, _ep_tx_buffer_1)[TX_SIZE / 2] __usb_ram; \
	struct usb_isochronous_config const NAME = {			\
		.endpoint  = ENDPOINT,					\
		.task_id = TASK_ID,					\
		.set_interface = SET_INTERFACE,				\
		.tx_size   = TX_SIZE,					\
		.tx_ram    = {						\
			CONCAT2(NAME, _ep_tx_buffer_0),			\
			CONCAT2(NAME, _ep_tx_buffer_1),			\
		},							\
	};								\
	const struct usb_interface_descriptor				\
	USB_IFACE_DESC(INTERFACE) = {					\
		.bLength            = USB_DT_INTERFACE_SIZE,		\
		.bDescriptorType    = USB_DT_INTERFACE,			\
		.bInterfaceNumber   = INTERFACE,			\
		.bAlternateSetting  = 0,				\
		.bNumEndpoints      = 0,				\
		.bInterfaceClass    = INTERFACE_CLASS,			\
		.bInterfaceSubClass = INTERFACE_SUBCLASS,		\
		.bInterfaceProtocol = INTERFACE_PROTOCOL,		\
		.iInterface         = INTERFACE_NAME,			\
	};								\
	const struct usb_interface_descriptor				\
	USB_CONF_DESC(CONCAT3(iface, INTERFACE, _1iface)) = {		\
		.bLength            = USB_DT_INTERFACE_SIZE,		\
		.bDescriptorType    = USB_DT_INTERFACE,			\
		.bInterfaceNumber   = INTERFACE,			\
		.bAlternateSetting  = 1,				\
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
	static void CONCAT2(NAME, _ep_event)(enum usb_ep_event evt)	\
	{								\
		usb_isochronous_event(&NAME, evt);			\
	}								\
	static int CONCAT2(NAME, _handler)(usb_uint *rx, usb_uint *tx)	\
	{								\
		return usb_isochronous_iface_handler(&NAME, rx, tx);	\
	}								\
	USB_DECLARE_IFACE(INTERFACE, CONCAT2(NAME, _handler));		\
	USB_DECLARE_EP(ENDPOINT,					\
		       CONCAT2(NAME, _ep_tx),				\
		       CONCAT2(NAME, _ep_tx),				\
		       CONCAT2(NAME, _ep_event));			\

void usb_isochronous_tx(struct usb_isochronous_config const *config);
void usb_isochronous_event(struct usb_isochronous_config const *config,
			   enum usb_ep_event event);
int usb_isochronous_iface_handler(struct usb_isochronous_config const *config,
				  usb_uint *ep0_buf_rx,
				  usb_uint *ep0_buf_tx);

#endif /* __CROS_EC_USB_ISOCHRONOUS_H */
