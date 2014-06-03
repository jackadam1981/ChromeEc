/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "debug.h"
#include "gpio.h"
#include "hooks.h"
#include "link_defs.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb.h"

#undef DECLARE_IRQ
#include "irq_handler.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USB, outstr)
#define CPRINTF(format, args...) /*debug_printf(format, ## args)*/

#define MAX_PACKET_SIZE 64

#define N_ENDPOINTS 5

#define HID_REPORT_SIZE  8

#define USB_EP_HID 1
#define USB_EP_SERIAL_TX 2
#define USB_EP_SERIAL_RX 3

const void * const usb_strings[] = {"abc", "!23", "def"};
#define USB_STR_COUNT 3

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
	.wTotalLength = 58, /* no of returned bytes */
	.bNumInterfaces = 2,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80, /* bus powered */
	.bMaxPower = 250, /* MaxPower 500 mA */
};
/* HID descriptors */
const struct usb_interface_descriptor USB_IFACE_DESC(USB_IFACE_HID) = {
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
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

#define TOGGLE_EP(n, mask, val, flags) \
	STM32_USB_EP(n) = (((STM32_USB_EP(n) & (EP_MASK | (mask))) \
			   ^ (val)) | (flags))

static int set_addr;

static void ep0_rx(void)
{
	/* TODO check setup bit ? */
	if ((desc->ep0_rx[0] == 0x0680) &&
		(desc->ep0_rx[1] == 0x0100)) {
		/* Setup : Get device descriptor */
		copy_to_endpoint((void *)&dev_desc, desc->ep0_tx,
				 sizeof(dev_desc));
		desc->ep[0].tx_count =  sizeof(dev_desc);
		STM32_USB_EP(0) = ((STM32_USB_EP(0)
				& (EP_MASK | EP_TX_MASK | EP_RX_MASK))
				^ (EP_RX_VALID | EP_TX_VALID)) |
				(1 << 8)  /* STATUS OUT */;
		/* need null OUT transaction -> STATUS_OUT */
		CPRINTF("DEVD %04x\n", STM32_USB_EP(0));
	} else if ((desc->ep0_rx[0] == 0x0680) &&
		(desc->ep0_rx[1] == 0x0200)) {
		/* Setup : Get configuration descriptor */
		copy_to_endpoint(__usb_desc, desc->ep0_tx,
				 58/*sizeof(conf_desc)*/);
		desc->ep[0].tx_count = MIN(desc->ep0_rx[3],
				 58/*sizeof(conf_desc)*/);
		STM32_USB_EP(0) = ((STM32_USB_EP(0)
				& (EP_MASK | EP_TX_MASK | EP_RX_MASK))
				^ (EP_RX_VALID | EP_TX_VALID)) |
				(1 << 8)  /* STATUS OUT */;
		/* need null OUT transaction -> STATUS_OUT */
		CPRINTF("CFG %04x[l %04x]\n", STM32_USB_EP(0),
			desc->ep0_rx[3]);
	} else if ((desc->ep0_rx[0] == 0x0680) &&
		((desc->ep0_rx[1] >> 8) == 0x03)) {
		/* Setup : Get string descriptor */
		uint8_t idx = desc->ep0_rx[1] & 0xff;
		const uint8_t *str_desc;
		if (idx >= USB_STR_COUNT) { /* The string does not exist */
			STM32_USB_EP(0) = (STM32_USB_EP(0)
				& (EP_MASK | EP_TX_MASK | EP_RX_MASK))
				^ (EP_RX_VALID | EP_TX_STALL);
			return; /* dont remove the STALL */
		}
		str_desc = usb_strings[idx];
		copy_to_endpoint(str_desc, desc->ep0_tx, str_desc[0]);
		desc->ep[0].tx_count = MIN(desc->ep0_rx[3], str_desc[0]);
		STM32_USB_EP(0) = ((STM32_USB_EP(0)
				& (EP_MASK | EP_TX_MASK | EP_RX_MASK))
				^ (EP_RX_VALID | EP_TX_VALID)) |
				(1 << 8)  /* STATUS OUT */;
		/* need null OUT transaction -> STATUS_OUT */
		CPRINTF("STR/%d %04x[l %04x]\n", idx, STM32_USB_EP(0),
			desc->ep0_rx[3]);
	} else if ((desc->ep0_rx[0] == 0x0681) &&
		(desc->ep0_rx[1] == 0x2200)) {
		/* Setup : HID specific : Get Report descriptor */
		copy_to_endpoint(report_desc, desc->ep0_tx,
				 sizeof(report_desc));
		desc->ep[0].tx_count = MIN(desc->ep0_rx[3],
					   sizeof(report_desc));
		STM32_USB_EP(0) = ((STM32_USB_EP(0)
				& (EP_MASK | EP_TX_MASK | EP_RX_MASK))
				^ (EP_RX_VALID | EP_TX_VALID)) |
				(1 << 8)  /* STATUS OUT */;
		/* need null OUT transaction -> STATUS_OUT */
		CPRINTF("RPT %04x[l %04x]\n", STM32_USB_EP(0),
			desc->ep0_rx[3]);
	} else if ((desc->ep0_rx[0] == 0x0680) &&
		(desc->ep0_rx[1] == 0x0600)) {
		/* Setup : Get device qualifier descriptor */
		/* Not high speed : STALL next IN used as handshake */
		STM32_USB_EP(0) = (STM32_USB_EP(0)
				& (EP_MASK | EP_TX_MASK | EP_RX_MASK))
				^ (EP_RX_VALID | EP_TX_STALL);
		CPRINTF("DEVQ %04x\n", STM32_USB_EP(0));
		return; /* dont remove the STALL */
	} else if ((desc->ep0_rx[0] == 0x0080) &&
		(desc->ep0_rx[1] == 0x0000)) {
		uint16_t zero = 0;
		/* Get status */
		copy_to_endpoint((void *)&zero, desc->ep0_tx, 2);
		desc->ep[0].tx_count = 2;
		STM32_USB_EP(0) = ((STM32_USB_EP(0)
				& (EP_MASK | EP_TX_MASK | EP_RX_MASK))
				^ (EP_RX_VALID | EP_TX_VALID)) |
				(1 << 8)  /* STATUS OUT */;
	} else if (desc->ep0_rx[0] == 0x0900) {
		/* SET_CONFIGURATION */
		/*uint8_t cfg = desc->ep0_rx[1] & 0xff;*/
		CPRINTF("SetCFG %d\n", cfg);
		/* null IN for handshake */
		desc->ep[0].tx_count = 0;
		STM32_USB_EP(0) = (STM32_USB_EP(0)
				& (EP_MASK | EP_TX_MASK | EP_RX_MASK))
				^ (EP_RX_VALID | EP_TX_VALID);
	} else if (desc->ep0_rx[0] == 0x0500) {
		/* SET_ADDRESS */
		uint8_t addr = desc->ep0_rx[1] & 0xff;

		/* STM32_USB_DADDR = addr | 0x80; */
		/* set the address after we got IN packet handshake */
		set_addr = addr;
		CPRINTF("ADDR %02x/%d\n", addr, addr);
		STM32_USB_EP(0) = (STM32_USB_EP(0)
				& (EP_MASK | EP_TX_MASK | EP_RX_MASK))
				^ (EP_RX_VALID | EP_TX_VALID);
		/* need null IN transaction -> TX Valid */
	} else {
		/* Unknown descriptor ... */
		STM32_USB_EP(0) = (STM32_USB_EP(0)
				& (EP_MASK | EP_TX_MASK | EP_RX_MASK))
				^ (EP_RX_VALID | EP_TX_STALL);
		CPRINTF("CTR EP0 %04x\n", STM32_USB_EP(0));
	}
}

static void ep0_tx(void)
{
	if (set_addr) {
		STM32_USB_DADDR = set_addr | 0x80;
		set_addr = 0;
		CPRINTF("SETAD %02x\n", STM32_USB_DADDR);
	}

	STM32_USB_EP(0) = (STM32_USB_EP(0) & (EP_MASK | EP_TX_MASK))
			^ EP_TX_VALID;
	CPRINTF("CTR EP0 %04x\n", STM32_USB_EP(0));
}

static void ep1_tx(void)
{
	uint16_t ep1 = STM32_USB_EP(1);
	CPRINTF("EP1 TX/%x %x %04x\n", desc->ep[1].rx_count,
		desc->ep[1].tx_count, ep1);
	/* clear IT */
	STM32_USB_EP(1) = (ep1 & EP_MASK);
	return;
}

static void ep2_tx(void)
{
	uint16_t ep2 = STM32_USB_EP(2);
	CPRINTF("EP2 TX/%x %x %04x\n", desc->ep[2].rx_count,
		desc->ep[2].tx_count, ep2);
	/* clear IT */
	STM32_USB_EP(2) = (ep2 & EP_MASK);
	return;
}

/*void console_handle_char(int c) { debug_printf("%c", c); }*/
void console_handle_char(int c);
static void ep3_rx(void)
{
	uint16_t ep3 = STM32_USB_EP(3);
	int i;
	for (i = 0; i < (desc->ep[3].rx_count & 0x3ff); i++)
		/* Not working on old STM32 ... */
#if 0
		console_handle_char(((uint8_t *)desc->ep3_rx)[i]);
#else
		console_handle_char(((uint8_t *)desc->ep3_rx)[((i & ~1) << 1) + (i & 1)]);
#endif
	/* clear IT */
	STM32_USB_EP(3) = (ep3 & (EP_MASK | EP_RX_MASK)) ^ EP_RX_VALID;
	return;
}

void set_keyboard_report(uint64_t rpt)
{
	uint16_t ep1 = STM32_USB_EP(1);
	copy_to_endpoint((const uint8_t *)&rpt, desc->ep1_tx, sizeof(rpt));
	/* enable TX */
	STM32_USB_EP(1) = (ep1 & (EP_MASK | EP_TX_MASK)) ^ EP_TX_VALID;
}

#if 1

#include "printf.h"

static int ep2_idx;
static int is_reset;

static int __tx_char(void *context, int c)
{
	usb_uint *buf = (usb_uint *)desc->ep2_tx;

	/* Do newline to CRLF translation */
	/*
	if (c == '\n' && __tx_char(NULL, '\r'))
		return 1;*/

	if (ep2_idx > 63)
		return 1;
	if (!(ep2_idx & 1))
		buf[ep2_idx/2] = c;
	else
		buf[ep2_idx/2] |= c << 8;

	ep2_idx++;

	return 0;
}

int usb_puts(const char *outstr)
{
	uint16_t ep2 = STM32_USB_EP(2);

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
	STM32_USB_EP(2) = (ep2 & (EP_MASK | EP_TX_MASK)) ^ EP_TX_VALID;
	/* Successful if we consumed all output */
	return *outstr ? EC_ERROR_OVERFLOW : EC_SUCCESS;
}

int usb_write_raw(const uint8_t *data, int size)
{
	uint16_t ep2 = STM32_USB_EP(2);
	int i;

	if (!is_reset)
		return 0;

	ep2_idx = 0;
	/* Put all characters in the output buffer */
	for (i = 0; i < size; ++i)
		if (__tx_char(NULL, data[i]) != 0)
			break;

	desc->ep[2].tx_count = ep2_idx;
	/* enable TX */
	STM32_USB_EP(2) = (ep2 & (EP_MASK | EP_TX_MASK)) ^ EP_TX_VALID;
	/* Successful if we consumed all output */
	return (ep2_idx != size) ? EC_ERROR_OVERFLOW : EC_SUCCESS;
}

int usb_vprintf(const char *format, va_list args)
{
	uint16_t ep2 = STM32_USB_EP(2);
	int rv;

	if (!is_reset)
		return 0;

	ep2_idx = 0;
	rv = vfnprintf(__tx_char, NULL, format, args);

	desc->ep[2].tx_count = ep2_idx;
	/* enable TX */
	STM32_USB_EP(2) = (ep2 & (EP_MASK | EP_TX_MASK)) ^ EP_TX_VALID;
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

void usb_reset(void)
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

void IRQ_HANDLER(STM32_IRQ_USB_LP)(void)
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
	CPRINTF("USB:%04x/%04x EP0 %04x FNR %04x\n", status, STM32_USB_ISTR,
		STM32_USB_EP(0), STM32_USB_FNR);
}

void usb_init(void)
{
	/* Enable USB device clock. */
	STM32_RCC_APB1ENR |= 0x00800000;

	/* PA11/PA12 need to be set in USB alternate mode in the board code */
	/* gpio_config_module(MODULE_USB, 1); */

	/* we need a proper 48MHz clock */
	/*clock_enable_module(MODULE_USB, 1);*/

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
#elif defined(CHIP_FAMILY_STM32F0)
	STM32_USB_BCDR |= 1 << 15 /* DPPU */;
#else
	/* hardwired or regular GPIO on other platforms */
#endif

	CPRINTF("USB init done\n");
}
