/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __GOOGLE_VENDOR_H
#define __GOOGLE_VENDOR_H

#include <zephyr/drivers/usb/udc.h>
#include <zephyr/usb/usbd.h>

#define AUTO_EP_IN 0x80
#define AUTO_EP_OUT 0x00

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

struct google_desc {
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor out_ep;
	struct usb_ep_descriptor in_ep;
} __packed;

struct google_data {
	struct google_desc *const desc;
	const struct usb_desc_header **const fs_desc;
	atomic_t state;
	struct k_thread tx_thread_data;
	struct k_thread rx_thread_data;
	struct k_sem sync_sem;
};

enum {
	GFAKE_DEV_CLASS_ENABLED = 0,
	GUPDATE_DEV_CLASS_ENABLED,
	GUPDATE_DEV_CLASS_OUT_BUSY,
	GI2C_DEV_CLASS_ENABLED,
};

uint8_t google_get_in_ep(struct usbd_class_data *const c_data)
{
	struct google_data *data = usbd_class_get_private(c_data);
	struct google_desc *desc = data->desc;

	return desc->in_ep.bEndpointAddress;
}

uint8_t google_get_out_ep(struct usbd_class_data *const c_data)
{
	struct google_data *data = usbd_class_get_private(c_data);
	struct google_desc *desc = data->desc;

	return desc->out_ep.bEndpointAddress;
}

#endif /* __GOOGLE_VENDOR_H */
