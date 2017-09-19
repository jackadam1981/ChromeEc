/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* WebUSB platform descriptor */

#include "common.h"
#include "usb_descriptor.h"
#include "util.h"

#ifndef CONFIG_USB_BOS
#error "CONFIG_USB_BOS must be defined to use WebUSB descriptor"
#endif

const void *webusb_url = USB_URL_DESC(HTTPS, CONFIG_WEBUSB_URL);

/*
 * Platform Descriptor in the device Binary Object Store
 * as defined by USB 3.1 spec chapter 9.6.2.
 */
static struct {
	struct usb_bos_hdr_descriptor bos;
	struct usb_platform_descriptor platform;
	uint8_t ms_platform[0x1c];
} bos_desc = {
	.bos = {
		.bLength = USB_DT_BOS_SIZE,
		.bDescriptorType = USB_DT_BOS,
		.wTotalLength = (USB_DT_BOS_SIZE + USB_DT_PLATFORM_SIZE + 0x1c),
		.bNumDeviceCaps = 2,  /* platform caps */
	},
	.platform = {
		.bLength = USB_DT_PLATFORM_SIZE,
		.bDescriptorType = USB_DT_DEVICE_CAPABILITY,
		.bDevCapabilityType = USB_DC_DTYPE_PLATFORM,
		.bReserved = 0,
		.PlatformCapUUID = USB_PLAT_CAP_WEBUSB,
		.bcdVersion = 0x0100,
		.bVendorCode = 0x01,
		.iLandingPage = 1,
	},
	.ms_platform = {
	// Microsoft OS 2.0 Platform Capability Descriptor (MS_VendorCode 0x02)
	0x1C,  // Length
	USB_DT_DEVICE_CAPABILITY,  // Device Capability descriptor
	USB_DC_DTYPE_PLATFORM,  // Platform Capability descriptor
	0x00,  // Reserved
	0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C,
	0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,  // MS OS 2.0 GUID
	0x00, 0x00, 0x03, 0x06,  // Windows version (8.1) (0x06030000)
	0x2e, 0x00,  // Descriptor set length
	0x02,  // Vendor request code
	0x00   // Alternate enumeration code
	},
};

const uint8_t ms_os20_desc[] = {
	// Microsoft OS 2.0 descriptor set header (table 10)
	0x0A, 0x00,  // Descriptor size (10 bytes)
	0x00, 0x00,  // MS OS 2.0 descriptor set header
	0x00, 0x00, 0x03, 0x06,  // Windows version (8.1) (0x06030000)
	0x2e, 0x00,  // Size, MS OS 2.0 descriptor set

	// Microsoft OS 2.0 configuration subset header
	0x08, 0x00,  // Descriptor size (8 bytes)
	0x01, 0x00,  // MS OS 2.0 configuration subset header
	0x00,        // bConfigurationValue
	0x00,        // Reserved
	0x24, 0x00,  // Size, MS OS 2.0 configuration subset

	// Microsoft OS 2.0 function subset header
	0x08, 0x00,  // Descriptor size (8 bytes)
	0x02, 0x00,  // MS OS 2.0 function subset header

	0x02,  // First interface number

	0x00,        // Reserved
	0x1c, 0x00,  // Size, MS OS 2.0 function subset

	// Microsoft OS 2.0 compatible ID descriptor (table 13)
	0x14, 0x00,  // wLength
	0x03, 0x00,  // MS_OS_20_FEATURE_COMPATIBLE_ID
	'W',  'I',  'N',  'U',  'S',  'B',  0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


const struct bos_context bos_ctx = {
	.descp = (void *)&bos_desc,
	.size = sizeof(bos_desc),
};
