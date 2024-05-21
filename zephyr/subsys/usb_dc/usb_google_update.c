/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "queue.h"
#include "usb_stream.h"

#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/usb_device.h>

#include <usb_descriptor.h>
LOG_MODULE_REGISTER(usb_google_update, LOG_LEVEL_ERR);

#define GOOGLE_UPDATE_BULK_EP_MPS 64
#define GOOGLE_UPDATE_IN_EP_ADDR (0x01 | USB_EP_DIR_IN)
#define GOOGLE_UPDATE_OUT_EP_ADDR (0x02 | USB_EP_DIR_OUT)

enum google_update_ep_index {
	OUT_EP_IDX = 0,
	IN_EP_IDX,
	EP_NUM,
};

static uint8_t google_update_buf[GOOGLE_UPDATE_BULK_EP_MPS];

struct usb_google_update_config {
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor if0_out_ep;
	struct usb_ep_descriptor if0_in_ep;
} __packed;

#define INITIALIZER_IF(num_ep, iface_class, iface_subclass, iface_proto)	\
	{									\
		.bLength = sizeof(struct usb_if_descriptor),			\
		.bDescriptorType = USB_DESC_INTERFACE,				\
		.bInterfaceNumber = 0,						\
		.bAlternateSetting = 0,						\
		.bNumEndpoints = num_ep,					\
		.bInterfaceClass = iface_class,					\
		.bInterfaceSubClass = iface_subclass,				\
		.bInterfaceProtocol = iface_proto,				\
		.iInterface = 0,						\
	}

#define INITIALIZER_IF_EP(addr, attr, mps)				\
	{								\
		.bLength = sizeof(struct usb_ep_descriptor),		\
		.bDescriptorType = USB_DESC_ENDPOINT,			\
		.bEndpointAddress = addr,				\
		.bmAttributes = attr,					\
		.wMaxPacketSize = sys_cpu_to_le16(mps),			\
		.bInterval = 0,						\
	}

USBD_CLASS_DESCR_DEFINE(primary, 0)
struct usb_google_update_config google_update_cfg = {
	.if0 = INITIALIZER_IF(EP_NUM, USB_BCC_VENDOR,
			      USB_SUBCLASS_GOOGLE_UPDATE,
			      USB_PROTOCOL_GOOGLE_UPDATE),
	.if0_out_ep = INITIALIZER_IF_EP(GOOGLE_UPDATE_OUT_EP_ADDR,
					USB_DC_EP_BULK,
					GOOGLE_UPDATE_BULK_EP_MPS),
	.if0_in_ep = INITIALIZER_IF_EP(GOOGLE_UPDATE_IN_EP_ADDR, USB_DC_EP_BULK,
				       GOOGLE_UPDATE_BULK_EP_MPS),
};

static void google_update_cb(uint8_t ep,
			     enum usb_dc_ep_cb_status_code ep_status)
{
	const struct queue *usb_to_update = usb_update.producer.queue;
	uint32_t bytes_to_read;

	if (ep_status == USB_DC_EP_DATA_OUT) {
		usb_read(ep, NULL, 0, &bytes_to_read);
		if (bytes_to_read != 0) {
			usb_read(ep, google_update_buf, bytes_to_read, NULL);
			QUEUE_ADD_UNITS(usb_to_update, google_update_buf,
					bytes_to_read);

			LOG_HEXDUMP_DBG(google_update_buf, bytes_to_read,
					"Rx:");
		}
	}
}

static struct usb_ep_cfg_data ep_cfg[] = {
	[OUT_EP_IDX] = {
		.ep_cb = google_update_cb,
		.ep_addr = GOOGLE_UPDATE_OUT_EP_ADDR,
	},
	[IN_EP_IDX] = {
		.ep_cb = google_update_cb,
		.ep_addr = GOOGLE_UPDATE_IN_EP_ADDR,
	},
};

static void google_update_status_cb(struct usb_cfg_data *cfg,
				    enum usb_dc_status_code status,
				    const uint8_t *param)
{
	ARG_UNUSED(cfg);
	ARG_UNUSED(param);

	switch (status) {
	case USB_DC_CONFIGURED:
		LOG_DBG("USB device configured");
		google_update_cb(ep_cfg[OUT_EP_IDX].ep_addr,
				 USB_DC_EP_DATA_OUT);
		break;
	default:
		break;
	}
}

void updater_stream_written(struct consumer const *consumer, size_t count)
{
	while (!queue_is_empty(consumer->queue)) {
		queue_peek_units(consumer->queue, google_update_buf, 0, count);

		if (usb_write(ep_cfg[IN_EP_IDX].ep_addr, google_update_buf,
			      count, NULL)) {
			LOG_ERR("failed to send usb data");
		}

		LOG_HEXDUMP_DBG(google_update_buf, count, "Tx:");
		queue_advance_head(consumer->queue, count);
	}
}

static void google_update_interface_config(struct usb_desc_header *head,
					   uint8_t bInterfaceNumber)
{
	ARG_UNUSED(head);

	google_update_cfg.if0.bInterfaceNumber = bInterfaceNumber;
}

USBD_DEFINE_CFG_DATA(google_update_config) = {
	.usb_device_description = NULL,
	.interface_config = google_update_interface_config,
	.interface_descriptor = &google_update_cfg.if0,
	.cb_usb_status = google_update_status_cb,
	.interface = {
		.class_handler = NULL,
		.custom_handler = NULL,
		.vendor_handler = NULL,
	},
	.num_endpoints = ARRAY_SIZE(ep_cfg),
	.endpoint = ep_cfg,
};
