/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/usb_stream.h"
#include "hooks.h"
#include "queue.h"
#include "task.h"
#include "usb_dc.h"

#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/buf.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/usb_device.h>

#include <usb_descriptor.h>

enum gvendor_ep_index {
	OUT_EP_IDX = 0,
	IN_EP_IDX,
	EP_NUM,
};

LOG_MODULE_REGISTER(usb_google_vendor, LOG_LEVEL_INF);

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

struct usb_gvendor_config {
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor if0_out_ep;
	struct usb_ep_descriptor if0_in_ep;
} __packed;

__maybe_unused static void gvendor_read(uint8_t ep, int size, void *priv)
{
	ARG_UNUSED(priv);

	static uint8_t data[USB_MAX_FS_BULK_MPS];

	if (size > 0) {
		struct net_buf *buf;

		buf = net_buf_alloc(&i2c_rx_pool, K_NO_WAIT);
		if (!buf) {
			LOG_ERR("failed to allocate rx memory");
			return;
		}
		net_buf_add_mem(buf, data, size);
		net_buf_put(&rx_queue, buf);
	}

	/* Start a new read transfer */
	usb_transfer(ep, data, USB_MAX_FS_BULK_MPS, USB_TRANS_READ,
		     gvendor_read, NULL);
}

__maybe_unused static void gvendor_status_cb(struct usb_cfg_data *cfg,
					     enum usb_dc_status_code status,
					     const uint8_t *param)
{
	ARG_UNUSED(param);

	switch (status) {
	case USB_DC_CONFIGURED:
		LOG_DBG("USB device configured");
		gvendor_read(cfg->endpoint[OUT_EP_IDX].ep_addr, 0, NULL);
		break;
	default:
		break;
	}
}

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

#define USB_DC_GOOGLE_VENDOR_DEFINE(NAME, INTERFACE_SUBCLASS,                  \
				    INTERFACE_PROTOCOL)                        \
	NET_BUF_POOL_FIXED_DEFINE(NAME##_rx_pool, 2, USB_MAX_FS_BULK_MPS, 0,   \
				  NULL);                                       \
	NET_BUF_POOL_FIXED_DEFINE(NAME##_tx_pool, 2, USB_MAX_FS_BULK_MPS, 0,   \
				  NULL);                                       \
	static K_FIFO_DEFINE(NAME##_rx_queue);                                 \
	static K_FIFO_DEFINE(NAME##_tx_queue);                                 \
	static void google_i2c_read(uint8_t ep, int size, void *priv)          \
	{                                                                      \
		ARG_UNUSED(priv);                                              \
		static uint8_t data[USB_MAX_FS_BULK_MPS];                      \
		if (size > 0) {                                                \
			struct net_buf *buf;                                   \
			buf = net_buf_alloc(&NAME##_rx_pool, K_NO_WAIT);       \
			if (!buf) {                                            \
				return;                                        \
			}                                                      \
			net_buf_add_mem(buf, data, size);                      \
			net_buf_put(&NAME##_rx_queue, buf);                    \
		}                                                              \
		usb_transfer(ep, data, USB_MAX_FS_BULK_MPS, USB_TRANS_READ,    \
			     google_i2c_read, NULL);                           \
	};                                                                     \
	static void gvendor_status_cb(struct usb_cfg_data *cfg,                \
				      enum usb_dc_status_code status,          \
				      const uint8_t *param)                    \
	{                                                                      \
		ARG_UNUSED(param);                                             \
		switch (status) {                                              \
		case USB_DC_CONFIGURED:                                        \
			google_i2c_read(cfg->endpoint[OUT_EP_IDX].ep_addr, 0,  \
					NULL);                                 \
			break;                                                 \
		default:                                                       \
			break;                                                 \
		}                                                              \
	};                                                                     \
	static struct usb_ep_cfg_data CONCAT2(NAME, _ep_cfg)[] = { \
		[OUT_EP_IDX] = { \
			.ep_cb = usb_transfer_ep_callback, \
			.ep_addr = AUTO_EP_OUT, \
		}, \
		[IN_EP_IDX] = { \
			.ep_cb = usb_transfer_ep_callback, \
			.ep_addr = AUTO_EP_IN, \
		}, \
	};           \
	struct CONCAT2(NAME, _config) {                                        \
		struct usb_if_descriptor if0;                                  \
		struct usb_ep_descriptor if0_out_ep;                           \
		struct usb_ep_descriptor if0_in_ep;                            \
	} __packed;                                                            \
	USBD_CLASS_DESCR_DEFINE(primary, NAME)                                 \
	struct CONCAT2(NAME, _config) CONCAT2(NAME, _cfg) = {                  \
		.if0 = INITIALIZER_IF(EP_NUM, USB_BCC_VENDOR,                  \
				      INTERFACE_SUBCLASS, INTERFACE_PROTOCOL), \
		.if0_out_ep = INITIALIZER_IF_EP(AUTO_EP_OUT, USB_DC_EP_BULK,   \
						USB_MAX_FS_BULK_MPS),          \
		.if0_in_ep = INITIALIZER_IF_EP(AUTO_EP_IN, USB_DC_EP_BULK,     \
					       USB_MAX_FS_BULK_MPS),           \
	};                                                                     \
	static void CONCAT2(NAME, _interface_config)(                          \
		struct usb_desc_header * head, uint8_t bInterfaceNumber)       \
	{                                                                      \
		ARG_UNUSED(head);                                              \
		CONCAT2(NAME, _cfg).if0.bInterfaceNumber = bInterfaceNumber;   \
		return;                                                        \
	};                                                                     \
	USBD_DEFINE_CFG_DATA(CONCAT2(NAME, _config)) = {                                    \
		.usb_device_description = NULL, \
		.interface_config = &CONCAT2(NAME, _interface_config), \
		.interface_descriptor = &CONCAT2(NAME, _cfg).if0, \
		.cb_usb_status = gvendor_status_cb, \
		.interface = { \
			.class_handler = NULL, \
			.custom_handler = NULL, \
			.vendor_handler = NULL, \
		}, \
		.num_endpoints = ARRAY_SIZE(CONCAT2(NAME, _ep_cfg)), \
		.endpoint = CONCAT2(NAME, _ep_cfg), \
	};
