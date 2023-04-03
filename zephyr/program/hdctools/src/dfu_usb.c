/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dfu.h"

#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/class/usb_dfu.h>
#include <zephyr/usb/usb_device.h>

#define CONFIG_USB_DFU_DETACH_TIMEOUT 0xFFFF

#define DFU_DESC_ATTRIBUTES \
	(DFU_ATTR_MANIFESTATION_TOLERANT | DFU_ATTR_WILL_DETACH)

struct usb_dfu_config {
	struct usb_if_descriptor if0;
	struct dfu_runtime_descriptor dfu_descr;
} __packed;

struct usb_runtime_dfu_get_status_resp {
	uint8_t bStatus;
	uint8_t bwPollTimeout[3];
	uint8_t bState;
	uint8_t iString;
} __packed;

USBD_CLASS_DESCR_DEFINE(primary, 0) struct usb_dfu_config dfu_cfg = {
	/* Interface descriptor */
	.if0 = {
		.bLength = sizeof(struct usb_if_descriptor),
		.bDescriptorType = USB_DESC_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 0,
		.bInterfaceClass = USB_BCC_APPLICATION,
		.bInterfaceSubClass = DFU_SUBCLASS,
		.bInterfaceProtocol = DFU_RT_PROTOCOL,
		.iInterface = 0,
	},
	.dfu_descr = {
		.bLength = sizeof(struct dfu_runtime_descriptor),
		.bDescriptorType = DFU_FUNC_DESC,
		.bmAttributes = DFU_DESC_ATTRIBUTES,
		.wDetachTimeOut =
			sys_cpu_to_le16(CONFIG_USB_DFU_DETACH_TIMEOUT),
		.wTransferSize =
			sys_cpu_to_le16(CONFIG_USB_REQUEST_BUFFER_SIZE),
		.bcdDFUVersion =
			sys_cpu_to_le16(DFU_VERSION),
	},
};

/*
 * Handle the DFU Get Status response and Detach commands
 *
 * We don't need to support other commands in the runtime DFU interface.
 * The primary trait we need to communicate is it's in the appIDLE
 * mode and handle a request to reboot into the DFU mode when we see the
 * DFU_DETACH event.
 */
static int dfu_class_handle_req(struct usb_setup_packet *setup,
				int32_t *data_len, uint8_t **data)
{
	if (usb_reqtype_is_to_host(setup)) {
		if (setup->bRequest == DFU_GETSTATUS) {
			struct usb_runtime_dfu_get_status_resp response = {
				.bStatus = statusOK,
				.bState = appIDLE,
			};
			memcpy(*data, &response, sizeof(response));
			*data_len = sizeof(response);
			return 0;
		}
	} else {
		if (setup->bRequest == DFU_DETACH) {
			dfu_enter();
			return 0;
		}
	}
	return -EINVAL;
}

/*
 * Handle the Set Interface Request. No actions need to be done but accept
 * the request.
 */
static int dfu_custom_handle_req(struct usb_setup_packet *setup,
				 int32_t *data_len, uint8_t **data)
{
	if (setup->bRequest == USB_SREQ_SET_INTERFACE) {
		return 0;
	}
	return -EINVAL;
}

/*
 * Assign the interface number.
 */
static void dfu_interface_config(struct usb_desc_header *head,
				 uint8_t bInterfaceNumber)
{
	ARG_UNUSED(head);

	dfu_cfg.if0.bInterfaceNumber = bInterfaceNumber;
}

USBD_DEFINE_CFG_DATA(dfu_app_config) = {
	.usb_device_description = NULL,
	.interface_config = dfu_interface_config,
	.interface_descriptor = &dfu_cfg.if0,
	.cb_usb_status = NULL,
	.interface = {
		.class_handler = dfu_class_handle_req,
		.custom_handler = dfu_custom_handle_req,
	},
	.num_endpoints = 0,
};
