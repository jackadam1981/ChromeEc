/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "config.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "link_defs.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_descriptor.h"
#include "usb_hid.h"

/* remaining size of descriptor data to transfer */
extern int desc_left;
/* pointer to descriptor data if any */
extern const uint8_t *desc_ptr;

/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)

#define HID_REPORT_SIZE  10

/* HID descriptors */
const struct usb_interface_descriptor USB_IFACE_DESC(USB_IFACE_HID) = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = USB_IFACE_HID,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_HID,
	.bInterfaceSubClass = USB_HID_SUBCLASS_BOOT,
	.bInterfaceProtocol = USB_HID_PROTOCOL_KEYBOARD,
	.iInterface = 0,
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_HID, 81) = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x80 | USB_EP_HID,
	.bmAttributes = 0x03 /* Interrupt endpoint */,
	.wMaxPacketSize = HID_REPORT_SIZE,
	.bInterval = 40 /* ms polling interval */
};

/* HID : Report Descriptor */
static const uint8_t report_desc[] = {
	0x05, 0x0D,        // Usage Page (Digitizer)
	0x09, 0x04,        // Usage (Touch Screen)
	0xA1, 0x01,        // Collection (Application)
	0x85, 0x01,        //   Report ID (1, Touch)
	0x09, 0x22,        //   Usage (Finger)
	0xA1, 0x02,        //   Collection (Logical)
	0x09, 0x42,        //     Usage (Tip Switch)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x75, 0x01,        //     Report Size (1)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x09, 0x32,        //     Usage (In Range)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x75, 0x05,        //     Report Size (5)
	0x09, 0x51,        //     Usage (0x51) Contact identifier
	0x25, 0x1F,        //     Logical Maximum (31)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	0x05, 0x09,        //     Usage Page (Button)
	0x19, 0x01,        //     Usage Minimum (0x01)
	0x29, 0x01,        //     Usage Maximum (0x01)
	0x15, 0x00,        //     Logical Minimum (0)
	0x25, 0x01,        //     Logical Maximum (1)
	0x75, 0x01,        //     Report Size (1)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x05, 0x0D,        // Usage Page (Digitizer)
	0x26, 0xFF, 0x00,  //     Logical Maximum (255)
	0x75, 0x08,        //     Report Size (8)
	0x09, 0x48,        //     Usage (WIDTH)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x09, 0x49,        //     Usage (HEIGHT)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
//	0x95, 0x01,        //     Report Count (1)
	0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
	0x75, 12,        //     Report Size (12)
	0x55, 0x0E,        //     Unit Exponent (-2)
	0x65, 0x11,        //     Unit (System: SI Linear, Length: Centimeter)
	0x09, 0x30,        //     Usage (X)
	0x35, 0x00,        //     Physical Minimum (0)
	0x26, 0x86, 0x0C,  //     Logical Maximum (3206)
	0x46, 0xF8, 0x03,  //     Physical Maximum (10.16 cm)
/*	0x95, 0x02,        //     Report Count (1)*/
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x26, 0xf7, 0x06,  //     Logical Maximum (1783)
	0x46, 0x36, 0x02,  //     Physical Maximum (5.66 cm)
	0x09, 0x31,        //     Usage (Y)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

	0x05, 0x0D,        // Usage Page (Digitizer)
	0x26, 0xFF, 0x00,  //     Logical Maximum (255)
	0x75, 8,           //     Report Size (8)
	0x09, 0x30,        //   Usage (Tip pressure)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)

	0xC0,              //   End Collection
	0x05, 0x0D,        // Usage Page (Digitizer)
	0x09, 0x54,        // Usage (Contact count)
	0x75, 0x08,        //     Report Size (8)
	0x95, 0x01,        //     Report Count (1)
	0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
	0x75, 8,          //     Report Size (8) /* PADDING */
        0x81, 0x03,        //     INPUT (Cnst,Ary,Abs)
	0x85, 0x0A,        //   Report ID (10)
	0x09, 0x55,        //   Usage (0x55) Contact Count Maximum
	0x25, 0x05,        //   Logical Maximum (5)
	0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
	0xC0,              //   End Collection
};

const struct usb_hid_descriptor USB_CUSTOM_DESC(USB_IFACE_HID, hid) = {
	.bLength = 9,
	.bDescriptorType = USB_HID_DT_HID,
	.bcdHID = 0x0100,
	.bCountryCode = 0x00, /* Hardware target country */
	.bNumDescriptors = 1,
	.desc = {{
		.bDescriptorType = USB_HID_DT_REPORT,
		.wDescriptorLength = sizeof(report_desc)
	}}
};

static usb_uint hid_ep_buf[HID_REPORT_SIZE / 2] __usb_ram;

// 01138080 00000000
void set_hid_report(uint8_t* buffer, int length)
{
	memcpy_to_usbram((void *) usb_sram_addr(hid_ep_buf), buffer, length);
	/* enable TX */
	STM32_TOGGLE_EP(USB_EP_HID, EP_TX_MASK, EP_TX_VALID, 0);
}

static void hid_tx(void)
{
	uint16_t ep = STM32_USB_EP(USB_EP_HID);
	/* clear IT */
	STM32_USB_EP(USB_EP_HID) = (ep & EP_MASK);
	return;
}

static void hid_reset(void)
{
	/* HID interrupt endpoint 1 */
	btable_ep[USB_EP_HID].tx_addr = usb_sram_addr(hid_ep_buf);
	btable_ep[USB_EP_HID].tx_count = HID_REPORT_SIZE;
	hid_ep_buf[0] = 0;
	hid_ep_buf[1] = 0;
	hid_ep_buf[2] = 0;
	hid_ep_buf[3] = 0;
	STM32_USB_EP(USB_EP_HID) = (USB_EP_HID << 0) /*Endpoint Address*/ |
				   (3 << 4) /* TX Valid */ |
				   (3 << 9) /* interrupt EP */ |
				   (0 << 12) /* RX Disabled */;
}

USB_DECLARE_EP(USB_EP_HID, hid_tx, hid_tx, hid_reset);

static int hid_iface_request(usb_uint *ep0_buf_rx, usb_uint *ep0_buf_tx)
{
	/* FIXME: This does not know how to write descriptors > 64 bytes */
	if ((ep0_buf_rx[0] == (USB_DIR_IN | USB_RECIP_INTERFACE |
			      (USB_REQ_GET_DESCRIPTOR << 8))) &&
			      (ep0_buf_rx[1] == (USB_HID_DT_REPORT << 8))) {
		/* Setup : HID specific : Get Report descriptor */
		memcpy_to_usbram((void *) usb_sram_addr(ep0_buf_tx),
				report_desc,
				64);
		//ep0_buf_tx[1] = sizeof(report_desc);
		btable_ep[0].tx_count = 64;
		STM32_TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID,
				0);
		desc_ptr = report_desc+64;
		desc_left = sizeof(report_desc)-64;

		CPRINTF("RPT %04x[l %04x]\n", STM32_USB_EP(0),
			ep0_buf_rx[3]);
		return 0;
	}

	CPRINTF("RPT ?!!\n");
	return 1;
}
USB_DECLARE_IFACE(USB_IFACE_HID, hid_iface_request)

static int command_hid(int argc, char **argv)
{
	#if 0
	uint8_t keycode = 0x0a; /* 'G' key */

	if (argc >= 2) {
		char *e;
		keycode = strtoi(argv[1], &e, 16);
	        if (*e)
			return EC_ERROR_PARAM1;
	}

	/* press then release the key */
	set_keyboard_report((uint32_t)keycode << 16);
	udelay(50000);
	set_keyboard_report(0x000000);
	#endif

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(hid, command_hid,
			"[<HID keycode>]",
			"test USB HID driver");
