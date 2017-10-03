/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

#include "config.h"
#include "board.h"
#include "common.h"
#include "compile_time_macros.h"
#include "usb_descriptor.h"

/*
 * Additional USB configuration descriptor.
 *
 * The configuration n 2 has the USB serial console interface just marked as
 * a raw pair of bulk endpoints, so it is not intercepted by any serial driver
 * and can be used through raw USB API (e.g. WebUSB).
 */
static const struct {
	struct usb_config_descriptor conf_desc;
	struct usb_interface_descriptor serial_iface;
	struct usb_endpoint_descriptor serial_ep_in;
	struct usb_endpoint_descriptor serial_ep_out;
#ifdef HAS_TASK_SNIFFER
	struct usb_interface_descriptor sniffer_iface;
	struct usb_endpoint_descriptor sniffer_ep_in;
#endif /* HAS_TASK_SNIFFER */
} usb_config_2 = {
	.conf_desc = {
		.bLength = USB_DT_CONFIG_SIZE,
		.bDescriptorType = USB_DT_CONFIGURATION,
		.wTotalLength = 0x0BAD, /* set at runtime */
		.bNumInterfaces = USB_IFACE_COUNT,
		.bConfigurationValue = 2,
		.iConfiguration = USB_STR_VERSION,
		.bmAttributes = 0x80, /* Reserved bit */
		.bMaxPower = (CONFIG_USB_MAXPOWER_MA / 2),
	},
	.serial_iface = {
		.bLength            = USB_DT_INTERFACE_SIZE,
		.bDescriptorType    = USB_DT_INTERFACE,
		.bInterfaceNumber   = USB_IFACE_CONSOLE,
		.bAlternateSetting  = 0,
		.bNumEndpoints      = 2,
		.bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
		.bInterfaceSubClass = 0, /* Not detected as 'Google' */
		.bInterfaceProtocol = 0, /* ... serial console.      */
		.iInterface         = USB_STR_CONSOLE_NAME,
	},
	.serial_ep_in = {
		.bLength            = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType    = USB_DT_ENDPOINT,
		.bEndpointAddress   = 0x80 | USB_EP_CONSOLE,
		.bmAttributes       = 0x02 /* Bulk IN */,
		.wMaxPacketSize     = USB_MAX_PACKET_SIZE,
		.bInterval          = 10
	},
	.serial_ep_out = {
		.bLength            = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType    = USB_DT_ENDPOINT,
		.bEndpointAddress   = USB_EP_CONSOLE,
		.bmAttributes       = 0x02 /* Bulk OUT */,
		.wMaxPacketSize     = USB_MAX_PACKET_SIZE,
		.bInterval          = 0
	},
#ifdef HAS_TASK_SNIFFER
	.sniffer_iface = {
		.bLength = USB_DT_INTERFACE_SIZE,
		.bDescriptorType = USB_DT_INTERFACE,
		.bInterfaceNumber = USB_IFACE_VENDOR,
		.bAlternateSetting = 0,
		.bNumEndpoints = 1,
		.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
		.bInterfaceSubClass = USB_CLASS_VENDOR_SPEC,
		.bInterfaceProtocol = 0,
		.iInterface = USB_STR_SNIFFER,
	},
	.sniffer_ep_in = {
		.bLength = USB_DT_ENDPOINT_SIZE,
		.bDescriptorType = USB_DT_ENDPOINT,
		.bEndpointAddress = 0x80 | USB_EP_SNIFFER,
		.bmAttributes = 0x02 /* Bulk IN */,
		.wMaxPacketSize = USB_MAX_PACKET_SIZE,
		.bInterval = 1
	},
#endif /* HAS_TASK_SNIFFER */
};

const uint8_t *usb_get_config_desc(uint8_t cfg, int *len)
{
	if (cfg == 1) {
		*len = sizeof(usb_config_2);
		return (const uint8_t *)&usb_config_2;
	}
	/* Fallback on the regular configuration descriptor */
	*len = USB_DESC_SIZE;
	return __usb_desc;
}
