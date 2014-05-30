/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "link_defs.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USB, outstr)
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)

#define MAX_PACKET_SIZE 64

#define N_ENDPOINTS 5

#define HID_REPORT_SIZE  8

/* USB Standard Device Descriptor */
static const struct usb_device_descriptor dev_desc = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200, /* v2.00 */
	.bDeviceClass = USB_CLASS_PER_INTERFACE,
	.bDeviceSubClass = 0x00,
	.bDeviceProtocol =0x00,
	.bMaxPacketSize0 = MAX_PACKET_SIZE,
	.idVendor = USB_VID_GOOGLE,
	.idProduct = CONFIG_USB_PID,
	.bcdDevice = 0x0200, /* 2.00 */
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1
};

/* USB Configuration Descriptor */
const struct usb_config_descriptor USB_CONF_DESC(conf) = {
	.bLength = USB_DT_CONFIG_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 57, /* no of returned bytes */
	.bNumInterfaces = USB_IFACE_COUNT,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80, /* bus powered */
	.bMaxPower = 250, /* MaxPower 500 mA */
};
#if 0
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
const struct usb_hid_descriptor USB_CUSTOM_DESC(USB_IFACE_HID, hid) = {
	.bLength = 9,
	.bDescriptorType = USB_HID_DT_HID,
	.bcdHID = 0x0100,
	.bCountryCode = 0x00, /* Hardware target country */
	.bNumDescriptors = 1,
	.desc = {{
		.bDescriptorType = USB_HID_DT_REPORT,
		.wDescriptorLength = 45
	}}
};
#endif
/* USB-Serial descriptors */
const struct usb_interface_descriptor USB_IFACE_DESC(USB_IFACE_SERIAL) = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_SERIAL, 82) = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x80 | USB_EP_SERIAL_TX,
	.bmAttributes = 0x02 /* Bulk IN */,
	.wMaxPacketSize = MAX_PACKET_SIZE,
	.bInterval = 10
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_SERIAL, 3) = {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = USB_EP_SERIAL_RX,
	.bmAttributes = 0x02 /* Bulk OUT */,
	.wMaxPacketSize = MAX_PACKET_SIZE,
	.bInterval = 0
};

const uint8_t usb_string_desc[] = {
	4, /* Descriptor size */
	USB_DT_STRING,
	0x09, 0x04 /* LangID = 0x0409: U.S. English */
};

/* HID : Report Descriptor */
static const uint8_t report_desc[] = {
0x05, 0x01, /* Usage Page (Generic Desktop) */
0x09, 0x06, /* Usage (Keyboard) */
0xA1, 0x01, /* Collection (Application) */
0x05, 0x07, /* Usage Page (Key Codes) */
0x19, 0xE0, /* Usage Minimum (224) */
0x29, 0xE7, /* Usage Maximum (231) */
0x15, 0x00, /* Logical Minimum (0) */
0x25, 0x01, /* Logical Maximum (1) */
0x75, 0x01, /* Report Size (1) */
0x95, 0x08, /* Report Count (8) */
0x81, 0x02, /* Input (Data, Variable, Absolute), ;Modifier byte */

0x95, 0x01, /* Report Count (1) */
0x75, 0x08, /* Report Size (8) */
0x81, 0x01, /* Input (Constant), ;Reserved byte */

0x95, 0x06, /* Report Count (6) */
0x75, 0x08, /* Report Size (8) */
0x15, 0x00, /* Logical Minimum (0) */
0x25, 0x65, /* Logical Maximum(101) */
0x05, 0x07, /* Usage Page (Key Codes) */
0x19, 0x00, /* Usage Minimum (0) */
0x29, 0x65, /* Usage Maximum (101) */
0x81, 0x00, /* Input (Data, Array), ;Key arrays (6 bytes) */
0xC0,       /* End Collection */
0x00 /* Padding */
};

/* USB SRAM addressing */
#ifdef CHIP_FAMILY_STM32F0
typedef uint16_t usb_uint;
#else
/* older chips use a weird addressing were 16-bit words are 32-bit appart */
typedef uint32_t usb_uint;
#endif

struct endpoint {
	usb_uint tx_addr;
	usb_uint tx_count;
	usb_uint rx_addr;
	usb_uint rx_count;
};

static struct usb_sram {
	struct endpoint ep[N_ENDPOINTS];
	usb_uint ep0_tx[MAX_PACKET_SIZE / 2];
	usb_uint ep0_rx[MAX_PACKET_SIZE / 2];
	usb_uint ep1_tx[HID_REPORT_SIZE / 2];
	usb_uint ep2_tx[MAX_PACKET_SIZE / 2];
	usb_uint ep3_rx[MAX_PACKET_SIZE / 2];
} *desc = (void *)STM32_USB_CAN_SRAM_BASE;

/* Compute the address inside SRAM for the USB controller */
#define usb_sram_addr(x) \
	(offsetof(struct usb_sram, x) / (sizeof(usb_uint)/sizeof(uint16_t)))

static void copy_to_endpoint(const uint8_t *src, usb_uint *ebuf, int size)
{
	int i;

	for (i = 0; i < size / 2; i++, src += 2)
		*ebuf++ = src[0] | (src[1] << 8);
}

#define EP_MASK     0x0F0F
#define EP_TX_MASK  0x0030
#define EP_TX_VALID 0x0030
#define EP_TX_NAK   0x0020
#define EP_TX_STALL 0x0010
#define EP_TX_DISAB 0x0000
#define EP_RX_MASK  0x3000
#define EP_RX_VALID 0x3000
#define EP_RX_NAK   0x2000
#define EP_RX_STALL 0x1000
#define EP_RX_DISAB 0x0000

#define EP_STATUS_OUT 0x0100

#define EP_TX_RX_MASK (EP_TX_MASK | EP_RX_MASK)
#define EP_TX_RX_VALID (EP_TX_VALID | EP_RX_VALID)

#define TOGGLE_EP(n, mask, val, flags) \
	STM32_USB_EP(n) = (((STM32_USB_EP(n) & (EP_MASK | (mask))) \
			^ (val)) | (flags))

static int set_addr;

/* Requests on the control endpoint (aka EP0) */
static void ep0_rx(void)
{
	uint16_t req = desc->ep0_rx[0]; /* bRequestType | bRequest */

	/* interface specific requests */
	if ((req & USB_RECIP_MASK) == USB_RECIP_INTERFACE) {
		/* TODO move ME to USB HID */
		if ((req == (USB_DIR_IN | USB_RECIP_INTERFACE |
			    (USB_REQ_GET_DESCRIPTOR << 8))) &&
			    (desc->ep0_rx[1] == (USB_HID_DT_REPORT << 8))) {
			/* Setup : HID specific : Get Report descriptor */
			copy_to_endpoint(report_desc, desc->ep0_tx,
					 sizeof(report_desc));
			desc->ep[0].tx_count = MIN(desc->ep0_rx[3],
					   sizeof(report_desc));
			TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID,
				  EP_STATUS_OUT);
			CPRINTF("RPT %04x[l %04x]\n", STM32_USB_EP(0),
				desc->ep0_rx[3]);
		}
	}

	/* TODO check setup bit ? */
	if (req == (USB_DIR_IN | (USB_REQ_GET_DESCRIPTOR << 8))) {
		uint8_t type = desc->ep0_rx[1] >> 8;
		uint8_t idx = desc->ep0_rx[1] & 0xff;
		const uint8_t *str_desc;

		switch(type) {
		case USB_DT_DEVICE: /* Setup : Get device descriptor */
			copy_to_endpoint((void *)&dev_desc, desc->ep0_tx,
					 sizeof(dev_desc));
			desc->ep[0].tx_count =  sizeof(dev_desc);
			TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID,
				  EP_STATUS_OUT /*null OUT transaction */);
			break;
		case USB_DT_CONFIGURATION: /* Setup : Get configuration desc */
			copy_to_endpoint(__usb_desc, desc->ep0_tx,
					 58/*sizeof(conf_desc)*/);
			desc->ep[0].tx_count = MIN(desc->ep0_rx[3],
					 58/*sizeof(conf_desc)*/);
			TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID,
				  EP_STATUS_OUT /*null OUT transaction */);
			break;
		case USB_DT_STRING: /* Setup : Get string descriptor */
			if (idx >= USB_STR_COUNT) {
				/* The string does not exist : STALL */
				TOGGLE_EP(0, EP_TX_RX_MASK,
					  EP_RX_VALID | EP_TX_STALL, 0);
				return; /* don't remove the STALL */
			}
			str_desc = usb_strings[idx];
			copy_to_endpoint(str_desc, desc->ep0_tx, str_desc[0]);
			desc->ep[0].tx_count = MIN(desc->ep0_rx[3], str_desc[0]);
			TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID,
				  EP_STATUS_OUT /*null OUT transaction */);
			break;
		case USB_DT_DEVICE_QUALIFIER: /* Get device qualifier desc */
			/* Not high speed : STALL next IN used as handshake */
			TOGGLE_EP(0, EP_TX_RX_MASK, EP_RX_VALID | EP_TX_STALL,
				  0);
			break;
		default: /* unhandled descriptor */
			goto unknown_req;
		}
	} else if (req == (USB_DIR_IN | (USB_REQ_GET_STATUS << 8))) {
		uint16_t zero = 0;
		/* Get status */
		copy_to_endpoint((void *)&zero, desc->ep0_tx, 2);
		desc->ep[0].tx_count = 2;
		TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID,
			  EP_STATUS_OUT /*null OUT transaction */);
	} else if ((req & 0xff) == USB_DIR_OUT) {
		switch (req >> 8) {
		case USB_REQ_SET_ADDRESS:
			/* set the address after we got IN packet handshake */
			set_addr = desc->ep0_rx[1] & 0xff;
			/* need null IN transaction -> TX Valid */
			TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID, 0);
			break;
		case USB_REQ_SET_CONFIGURATION:
			/* uint8_t cfg = desc->ep0_rx[1] & 0xff; */
			/* null IN for handshake */
			desc->ep[0].tx_count = 0;
			TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID, 0);
			break;
		default: /* unhandled request */
			goto unknown_req;
		}

	} else {
		goto unknown_req;
	}

	return;
unknown_req:
	TOGGLE_EP(0, EP_TX_RX_MASK, EP_RX_VALID | EP_TX_STALL, 0);
}

static void ep0_tx(void)
{
	if (set_addr) {
		STM32_USB_DADDR = set_addr | 0x80;
		set_addr = 0;
		CPRINTF("SETAD %02x\n", STM32_USB_DADDR);
	}

	TOGGLE_EP(0, EP_TX_MASK, EP_TX_VALID, 0);
}

static void ep1_tx(void)
{
	uint16_t ep1 = STM32_USB_EP(1);
	/* clear IT */
	STM32_USB_EP(1) = (ep1 & EP_MASK);
	return;
}

static void ep2_tx(void)
{
	uint16_t ep2 = STM32_USB_EP(2);
	/* clear IT */
	STM32_USB_EP(2) = (ep2 & EP_MASK);
	return;
}

extern volatile char rx_buf[CONFIG_UART_RX_BUF_SIZE];
extern volatile int rx_buf_head;
extern volatile int rx_buf_tail;
#define RX_BUF_NEXT(i) (((i) + 1) & (CONFIG_UART_RX_BUF_SIZE - 1))
static void ep3_rx(void)
{
	int i;
	for (i = 0; i < (desc->ep[3].rx_count & 0x3ff); i++) {
		int rx_buf_next = RX_BUF_NEXT(rx_buf_head);
		if (rx_buf_next != rx_buf_tail) {
			/* Not working on old STM32 ... */
                        rx_buf[rx_buf_head] = ((uint8_t *)desc->ep3_rx)[i];
                        rx_buf_head = rx_buf_next;
                }
	}
	/* clear IT */
	TOGGLE_EP(3, EP_RX_MASK, EP_RX_VALID, 0);
	/* wake-up the console task */
	console_has_input();
	return;
}

void set_keyboard_report(uint64_t rpt)
{
	copy_to_endpoint((const uint8_t *)&rpt, desc->ep1_tx, sizeof(rpt));
	/* enable TX */
	TOGGLE_EP(1, EP_TX_MASK, EP_TX_VALID, 0);
}

#if 1

#include "printf.h"

static int ep2_idx;
static int is_reset;

static int __tx_char(void *context, int c)
{
	uint16_t *buf = (uint16_t *)desc->ep2_tx;

	/* Do newline to CRLF translation */
	if (c == '\n' && __tx_char(NULL, '\r'))
		return 1;

	if (ep2_idx > 63)
		return 1;
	if (!(ep2_idx & 1))
		buf[ep2_idx/2] = c;
	else
		buf[ep2_idx/2] |= c << 8;
	ep2_idx++;

	return 0;
}

int usb_tx_char(void *context, int c)
{
	int ret;
	ep2_idx = 0;
	ret = __tx_char(context, c);
	desc->ep[2].tx_count = ep2_idx;
	/* enable TX */
	TOGGLE_EP(2, EP_TX_MASK, EP_TX_VALID, 0);

	return ret;
}

int usb_puts(const char *outstr)
{
	if (!is_reset)
		return 0;

	ep2_idx = 0;
	/* Put all characters in the output buffer */
	while (*outstr) {
		if (__tx_char(NULL, *outstr++) != 0)
			break;
	}

	desc->ep[2].tx_count = ep2_idx;
	/* enable TX */
	TOGGLE_EP(2, EP_TX_MASK, EP_TX_VALID, 0);
	/* Successful if we consumed all output */
	return *outstr ? EC_ERROR_OVERFLOW : EC_SUCCESS;
}

int usb_vprintf(const char *format, va_list args)
{
	int rv;

	if (!is_reset)
		return 0;

	ep2_idx = 0;
	rv = vfnprintf(__tx_char, NULL, format, args);

	desc->ep[2].tx_count = ep2_idx;
	/* enable TX */
	TOGGLE_EP(2, EP_TX_MASK, EP_TX_VALID, 0);
	return rv;
}
#endif

static void (* const ep_callback[32])(void) =
{
	/* TX endpoints */
	[0] = ep0_tx,
	[1] = ep1_tx,
	[2] = ep2_tx,
	[3] = NULL, /* RX only */
	/* RX endpoints */
	[16 + 0] = ep0_rx,
	[16 + 1] = NULL, /* TX only */
	[16 + 2] = NULL, /* TX only */
	[16 + 3] = ep3_rx,
};

static void usb_reset(void)
{
	STM32_USB_EP(0) = (1 << 9) /* control EP */ |
			  (2 << 4) /* TX NAK */ |
			  (3 << 12) /* RX VALID */;

	desc->ep[0].tx_addr = usb_sram_addr(ep0_tx);
	desc->ep[0].rx_addr = usb_sram_addr(ep0_rx);
	desc->ep[0].rx_count = 0x8000 | ((MAX_PACKET_SIZE/32-1) << 10);
	desc->ep[0].tx_count = 0;

	/* HID interrupt endpoint 1 */
	desc->ep[1].tx_addr = usb_sram_addr(ep1_tx);
	desc->ep[1].tx_count = 8;
	desc->ep1_tx[0] = 0;
	desc->ep1_tx[1] = 0;
	desc->ep1_tx[2] = 0;
	desc->ep1_tx[3] = 0;
	STM32_USB_EP(1) = (USB_EP_HID << 0) /*Endpoint Address: 1 */ |
			  (3 << 4) /* TX Valid */ |
			  (3 << 9) /* interrupt EP */ |
			  (0 << 12) /* RX Disabled */;

	/* Serial Bulk IN endpoint 2 */
	desc->ep[2].tx_addr = usb_sram_addr(ep2_tx);
	desc->ep[2].tx_count = 0;
	desc->ep[2].rx_count = 0;
	STM32_USB_EP(2) = (USB_EP_SERIAL_TX << 0) /*Endpoint Address: 2 */ |
			  (2 << 4) /* TX NAK */ |
			  (0 << 9) /* Bulk EP */ |
			  (0 << 12) /* RX Disabled */;

	/* Serial Bulk OUT endpoint 3 */
	desc->ep[3].rx_addr = usb_sram_addr(ep3_rx);
	desc->ep[3].tx_count = 0;
	desc->ep[3].rx_count = 0x8000 | ((MAX_PACKET_SIZE/32-1) << 10);
	STM32_USB_EP(3) = (USB_EP_SERIAL_RX << 0) /*Endpoint Address: 3 */ |
			  (0 << 4) /* TX Disabled */ |
			  (0 << 9) /* Bulk EP */ |
			  (3 << 12) /* RX VALID */;

	/*
	 * set the default address : 0
	 * as we are not configured yet
	 */
	STM32_USB_DADDR = 0 | 0x80;
	CPRINTF("RST EP0 %04x\n", STM32_USB_EP(0));
	is_reset = 1;
}

void usb_interrupt(void)
{
	uint16_t status = STM32_USB_ISTR;

	if ((status & (1 << 10)))
		usb_reset();

	if (status & (1 << 15)) {
		int ep_slot = status & 0x001f;
		ep_callback[ep_slot]();
		/* TODO: do it in USB task */
		/* task_set_event(, 1 << ep_task); */
	}

	/* ack interrupts */
	STM32_USB_ISTR = 0;
	//CPRINTF("USB:%04x/%04x EP0 %04x FNR %04x\n", status, STM32_USB_ISTR,
	//	STM32_USB_EP(0), STM32_USB_FNR);
}
DECLARE_IRQ(STM32_IRQ_USB_LP, usb_interrupt, 1);

static void usb_init(void)
{
	/* Enable USB device clock. */
	STM32_RCC_APB1ENR |= 0x00800000;

	/* PA11/PA12 need to be set in USB alternate mode in the board code */
	/* gpio_config_module(MODULE_USB, 1); */

	/* we need a proper 48MHz clock */
	clock_enable_module(MODULE_USB, 1);

	/* power on sequence */

	/* keep FRES (USB reset) and remove PDWN (power down) */
	STM32_USB_CNTR = 0x01;
	udelay(1); /* startup time */
	/* reset FRES and keep interrupts masked */
	STM32_USB_CNTR = 0x00;
	/* clear pending interrupts */
	STM32_USB_ISTR = 0;

	/* set descriptors table offset in dedicated SRAM */
	STM32_USB_BTABLE = 0;

	/* EXTI18 is USB wake up interrupt */
	/* STM32_EXTI_RTSR |= 1 << 18; */
	/* STM32_EXTI_IMR |= 1 << 18; */

	/* Enable interrupt handlers */
	task_enable_irq(STM32_IRQ_USB_LP);
	/* set interrupts mask : reset/correct tranfer/errors */
	STM32_USB_CNTR = 0xe400;

	/* set pull-up on DP for FS mode */
#ifdef CHIP_VARIANT_STM32L15X
	STM32_SYSCFG_PMC |= 1;
#elif CHIP_FAMILY_STM32F0
	STM32_USB_BCDR |= 1 << 15 /* DPPU */;
#else
	/* hardwired or regular GPIO on other platforms */
#endif

	CPRINTF("USB init done\n");
}
DECLARE_HOOK(HOOK_INIT, usb_init, HOOK_PRIO_DEFAULT);

static int command_usb(int argc, char **argv)
{
        if (argc < 2)
                return EC_ERROR_PARAM1;

        if (!strcasecmp(argv[1], "hid")) {
		/* press then release 'G' key */
		set_keyboard_report(0x0a0000);
		udelay(50000);
		set_keyboard_report(0x000000);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(usb, command_usb,
			NULL,
			"debug USB controller",
			NULL);
