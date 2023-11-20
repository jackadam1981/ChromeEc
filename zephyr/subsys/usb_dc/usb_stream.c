/* Copyright 2023 The ChromiumOS Authors
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
LOG_MODULE_REGISTER(usb_google_update, LOG_LEVEL_DBG);

#define GOOGLE_UPDATE_BULK_EP_MPS 64
#define GOOGLE_UPDATE_OUT_EP_ADDR (0x02 | USB_EP_DIR_OUT)
#define GOOGLE_UPDATE_IN_EP_ADDR (0x01 | USB_EP_DIR_IN)

#define GOOGLE_UPDATE_OUT_EP_IDX 0
#define GOOGLE_UPDATE_IN_EP_IDX 1

// static void usb_update_proc_queue(void);
// DECLARE_DEFERRED(usb_update_proc_queue);

static uint8_t google_update_buf[1024];
// BUILD_ASSERT(sizeof(google_update_buf) == CONFIG_USB_REQUEST_BUFFER_SIZE);

struct usb_google_update_config {
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor if0_out_ep;
	struct usb_ep_descriptor if0_in_ep;
} __packed;

#define INITIALIZER_IF(num_ep, iface_class, iface_subclass, iface_proto)				\
	{								\
		.bLength = sizeof(struct usb_if_descriptor),		\
		.bDescriptorType = USB_DESC_INTERFACE,			\
		.bInterfaceNumber = 0,					\
		.bAlternateSetting = 0,					\
		.bNumEndpoints = num_ep,				\
		.bInterfaceClass = iface_class,				\
		.bInterfaceSubClass = iface_subclass,				\
		.bInterfaceProtocol = iface_proto,				\
		.iInterface = 0,					\
	}

#define INITIALIZER_IF_EP(addr, attr, mps)			\
	{								\
		.bLength = sizeof(struct usb_ep_descriptor),		\
		.bDescriptorType = USB_DESC_ENDPOINT,			\
		.bEndpointAddress = addr,				\
		.bmAttributes = attr,					\
		.wMaxPacketSize = sys_cpu_to_le16(mps),			\
		.bInterval = 0,					\
	}

USBD_CLASS_DESCR_DEFINE(primary, 0)
struct usb_google_update_config google_update_cfg = {
	.if0 = INITIALIZER_IF(2, USB_BCC_VENDOR, USB_SUBCLASS_GOOGLE_UPDATE,
			      USB_PROTOCOL_GOOGLE_UPDATE),
	.if0_out_ep = INITIALIZER_IF_EP(GOOGLE_UPDATE_OUT_EP_ADDR,
					USB_DC_EP_BULK,
					GOOGLE_UPDATE_BULK_EP_MPS),
	.if0_in_ep = INITIALIZER_IF_EP(GOOGLE_UPDATE_IN_EP_ADDR, USB_DC_EP_BULK,
				       GOOGLE_UPDATE_BULK_EP_MPS),
};

static void google_update_out_cb(uint8_t ep,
				 enum usb_dc_ep_cb_status_code ep_status)
{
	LOG_INF("%s ITE Debug %d", __func__, __LINE__);

	uint32_t bytes_to_read;

	usb_read(ep, NULL, 0, &bytes_to_read);
	if (bytes_to_read != 0) {
		usb_read(ep, google_update_buf, bytes_to_read, NULL);
		LOG_HEXDUMP_INF(google_update_buf, bytes_to_read, "Rx:");

		const struct queue *usb_to_update = usb_update.producer.queue;
		QUEUE_ADD_UNITS(usb_to_update, google_update_buf, bytes_to_read);
	}
}

static void google_update_in_cb(uint8_t ep,
				enum usb_dc_ep_cb_status_code ep_status)
{
	LOG_INF("%s ITE Debug %d", __func__, __LINE__);

	if (usb_write(ep, google_update_buf, GOOGLE_UPDATE_BULK_EP_MPS, NULL)) {
		LOG_INF("ep 0x%x", ep);
	}
	LOG_HEXDUMP_INF(google_update_buf, GOOGLE_UPDATE_BULK_EP_MPS, "Tx:");
}

static struct usb_ep_cfg_data ep_cfg[] = {
	{
		.ep_cb = google_update_out_cb,
		.ep_addr = GOOGLE_UPDATE_OUT_EP_ADDR,
	},
	{
		.ep_cb = google_update_in_cb,
		.ep_addr = GOOGLE_UPDATE_IN_EP_ADDR,
	},
};

static void google_update_status_cb(struct usb_cfg_data *cfg,
				    enum usb_dc_status_code status,
				    const uint8_t *param)
{
	ARG_UNUSED(cfg);

	switch (status) {
	case USB_DC_CONFIGURED:
		// hook_call_deferred(&usb_update_proc_queue_data, 0);
		LOG_INF("google update interface configured");
		google_update_out_cb(ep_cfg[GOOGLE_UPDATE_OUT_EP_IDX].ep_addr,
				     0);
		break;
	default:
		break;
	}
}

int custom_handle_req(struct usb_setup_packet *setup, int32_t *len,
		      uint8_t **data)
{
	LOG_INF("custom request: bRequest 0x%x bmRequestType 0x%x len %d",
		setup->bRequest, setup->bmRequestType, *len);
	LOG_HEXDUMP_INF(setup, 8, "setup:");

	return -EINVAL;
}

static int vendor_handle_req(struct usb_setup_packet *setup, int32_t *len,
			     uint8_t **data)
{
	LOG_INF("vendor request: bRequest 0x%x bmRequestType 0x%x len %d",
		setup->bRequest, setup->bmRequestType, *len);
	LOG_HEXDUMP_INF(setup, 8, "setup:");

	return -EINVAL;
}

static void google_update_interface_config(struct usb_desc_header *head,
					   uint8_t bInterfaceNumber)
{
	ARG_UNUSED(head);
	LOG_INF("google update update bInterfaceNumber 0x%x", bInterfaceNumber);

	google_update_cfg.if0.bInterfaceNumber = bInterfaceNumber;
}

// static void usb_update_proc_queue(void)
// {
// 	google_update_out_cb(ep_cfg[GOOGLE_UPDATE_OUT_EP_IDX].ep_addr, 0);

// 	hook_call_deferred(&usb_update_proc_queue_data, 0);
// }

void updater_stream_written(struct consumer const *consumer, size_t count)
{
	LOG_INF("%s ITE Debug %d", __func__, __LINE__);

	while (!queue_is_empty(consumer->queue)) {
		struct queue_chunk chunk =
			queue_get_read_chunk(consumer->queue);

		LOG_HEXDUMP_INF(chunk.buffer, chunk.count, "try to Tx:");
		if (usb_write(GOOGLE_UPDATE_IN_EP_ADDR, chunk.buffer, chunk.count, NULL)) {
			LOG_ERR("failed to send usb data");
		}
		queue_advance_head(consumer->queue, chunk.count);
	}
}

USBD_DEFINE_CFG_DATA(google_update_config) = {
	.usb_device_description = NULL,
	.interface_config = google_update_interface_config,
	.interface_descriptor = &google_update_cfg.if0,
	.cb_usb_status = google_update_status_cb,
	.interface = {
		.class_handler = NULL,
		.custom_handler = custom_handle_req,
		.vendor_handler = vendor_handle_req,
	},
	.num_endpoints = ARRAY_SIZE(ep_cfg),
	.endpoint = ep_cfg,
};
