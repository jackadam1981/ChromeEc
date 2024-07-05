/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb_dc.h"

#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/usb_device.h>

#include <usb_descriptor.h>

LOG_MODULE_REGISTER(usb_google_dummy, LOG_LEVEL_DBG);

#define AUTO_EP_IN 0x80

enum google_dummy_ep_index {
	IN_EP_IDX,
	EP_NUM,
};

struct usb_google_dummy_config {
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor if0_in_ep;
} __packed;

#define INITIALIZER_IF(num_ep, iface_class, iface_subclass, iface_proto)      \
	{                                                                     \
		.bLength = sizeof(struct usb_if_descriptor),                  \
		.bDescriptorType = USB_DESC_INTERFACE, .bInterfaceNumber = 0, \
		.bAlternateSetting = 0, .bNumEndpoints = num_ep,              \
		.bInterfaceClass = iface_class,                               \
		.bInterfaceSubClass = iface_subclass,                         \
		.bInterfaceProtocol = iface_proto, .iInterface = 0,           \
	}

#define INITIALIZER_IF_EP(addr, attr, mps)                              \
	{                                                               \
		.bLength = sizeof(struct usb_ep_descriptor),            \
		.bDescriptorType = USB_DESC_ENDPOINT,                   \
		.bEndpointAddress = addr, .bmAttributes = attr,         \
		.wMaxPacketSize = sys_cpu_to_le16(mps), .bInterval = 0, \
	}
#define USB_SUBCLASS_GOOGLE_DUMMY 0xFF
#define USB_PROTOCOL_GOOGLE_DUMMY 0xFF

#define DEFINE_GDUMMY_DESCR(x, _)                                          \
	USBD_CLASS_DESCR_DEFINE(primary, gdummy##x)                        \
	struct usb_google_dummy_config google_dummy_cfg_##x = {            \
		/* Interface descriptor */                                 \
		.if0 = INITIALIZER_IF(EP_NUM, USB_BCC_VENDOR,              \
				      USB_SUBCLASS_GOOGLE_DUMMY,           \
				      USB_PROTOCOL_GOOGLE_DUMMY),          \
		.if0_in_ep = INITIALIZER_IF_EP(AUTO_EP_IN, USB_DC_EP_BULK, \
					       USB_MAX_FS_BULK_MPS),       \
	}

#define INITIALIZER_EP_DATA(cb, addr)         \
	{                                     \
		.ep_cb = cb, .ep_addr = addr, \
	}

#define DEFINE_GDUMMY_EP(x, _)                                             \
	static struct usb_ep_cfg_data gdummy_ep_data_##x[] = {             \
		INITIALIZER_EP_DATA(usb_transfer_ep_callback, AUTO_EP_IN), \
	}

#define DEFINE_GDUMMY_CFG_DATA(x, _) \
	USBD_DEFINE_CFG_DATA(google_dummy_config_##x) = {			\
		.usb_device_description = NULL,				\
		.interface_config = google_dummy_interface_config,		\
		.interface_descriptor = &google_dummy_cfg_##x.if0,		\
		.interface = {						\
			.class_handler = NULL,		\
			.custom_handler = NULL,	\
		},							\
		.num_endpoints = ARRAY_SIZE(gdummy_ep_data_##x),		\
		.endpoint = gdummy_ep_data_##x,				\
	}

static void google_dummy_interface_config(struct usb_desc_header *head,
					  uint8_t bInterfaceNumber)
{
	struct usb_if_descriptor *if_desc = (struct usb_if_descriptor *)head;
	struct usb_google_dummy_config *desc =
		CONTAINER_OF(if_desc, struct usb_google_dummy_config, if0);

	desc->if0.bInterfaceNumber = bInterfaceNumber;
}

LISTIFY(CONFIG_USB_GDUMMY_DEVICE_COUNT, DEFINE_GDUMMY_DESCR, (;), _);
LISTIFY(CONFIG_USB_GDUMMY_DEVICE_COUNT, DEFINE_GDUMMY_EP, (;), _);
LISTIFY(CONFIG_USB_GDUMMY_DEVICE_COUNT, DEFINE_GDUMMY_CFG_DATA, (;), _);
