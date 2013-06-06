/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>
#include "clock.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USB, outstr)
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)

#define MAX_PACKET_SIZE 64

#define N_ENDPOINTS 3

#define HID_REPORT_SIZE  8

/* USB Standard Device Descriptor */
static const uint8_t dev_desc[] = {
	0x12,   /* bLength */
	USB_DEVICE_DESCRIPTOR_TYPE,     /* bDescriptorType */
	0x00,
	0x02,   /* bcdUSB = 2.00 */
	0x00,   /* bDeviceClass: */
	0x00,   /* bDeviceSubClass:  */
	0x00,   /* bDeviceProtocol:  */
	0x40,   /* bMaxPacketSize0 */
	0xd1,
	0x18,   /* idVendor = 0x18d1 Google Inc. */
	0x06,
	0x50,   /* idProduct = 0x5006 */
	0x00,
	0x02,   /* bcdDevice = 2.00 */
	1,      /* Index of string descriptor describing manufacturer */
	2,      /* Index of string descriptor describing product */
	3,      /* Index of string descriptor describing the device's s/n */
	0x01    /* bNumConfigurations */
};
/* USB Configuration Descriptor */
static const uint8_t conf_desc[] = {
	0x09,   /* bLength: Configuration Descriptor size */
	USB_CONFIGURATION_DESCRIPTOR_TYPE,      /* bDescriptorType: Config */
	34,     /* wTotalLength:no of returned bytes */
	0x00,
	0x01,   /* bNumInterfaces: 1 interface */
	0x01,   /* bConfigurationValue: Configuration value */
	0x00,   /* iConfiguration: Index of string descriptor for the config */
	0x80,   /* bmAttributes: bus powered */
	250,   /* MaxPower 500 mA */

	/* Interface descriptor */
	0x09,         /*bLength: Interface Descriptor size*/
	USB_INTERFACE_DESCRIPTOR_TYPE,/*bDescriptorType: Interface desc. type*/
	0x00,         /*bInterfaceNumber: Number of Interface*/
	0x00,         /*bAlternateSetting: Alternate setting*/
	0x01,         /*bNumEndpoints*/
	0x03,         /*bInterfaceClass: HID*/
	0x01,         /*bInterfaceSubClass : Boot */
	0x01,         /*bInterfaceProtocol : keyboard */
	0,            /*iInterface: Index of string descriptor*/
	/* HID descriptor */
	0x09,         /*bLength: HID Descriptor size*/
	0x21, /*HID_DESCRIPTOR_TYPE*/ /*bDescriptorType: HID*/
	0x00,         /*bcdHID: HID Class Spec release number*/
	0x01,
	0x00,         /*bCountryCode: Hardware target country*/
	0x01,         /*bNumDescriptors: Number of HID class descs to follow*/
	0x22,         /*bDescriptorType*/
	45,/*wItemLength: Total length of Report descriptor*/
	0x00,
	/* HID interrupt endpoint */
	0x07,          /*bLength: Endpoint Descriptor size*/
	USB_ENDPOINT_DESCRIPTOR_TYPE, /*bDescriptorType:*/
	0x81,          /*bEndpointAddress: Endpoint Address (IN)*/
	0x03,          /*bmAttributes: Interrupt endpoint*/
	0x08,          /*wMaxPacketSize: 8 Byte max */
	0x00,
	0x28,          /*bInterval: Polling Interval (40 ms)*/
};

static const uint8_t string_desc[] = {
	4,
	USB_STRING_DESCRIPTOR_TYPE,
	0x09, 0x04 /* LangID = 0x0409: U.S. English */
};

USB_STRING_DESC(vendor_str, "Google Inc.");
USB_STRING_DESC(product_str, "Reston");
USB_STRING_DESC(version_str, "vXX.YYY");

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

struct endpoint {
	uint32_t tx_addr;
	uint32_t tx_count;
	uint32_t rx_addr;
	uint32_t rx_count;
};

static struct usb_sram {
	struct endpoint ep[N_ENDPOINTS];
	uint32_t ep0_tx[MAX_PACKET_SIZE / 2];
	uint32_t ep0_rx[MAX_PACKET_SIZE / 2];
	uint32_t ep1_tx[HID_REPORT_SIZE / 2];
} *desc = (void *)STM32_USB_CAN_SRAM_BASE;

static void copy_to_endpoint(const uint8_t *src, uint32_t *ebuf, int size)
{
	int i;

	for (i = 0; i < size / 2; i++, src += 2)
		*ebuf++ = src[0] | (src[1] << 8);
}

static void handle_request(uint16_t istr)
{
	int dir = istr & 0x0010;
	int ep = istr & 0x000f;
	static int set_addr;

	if ((ep == 0) && dir) {
		/* TODO check setup bit ? */
		if ((desc->ep0_rx[0] == 0x0680) &&
			(desc->ep0_rx[1] == 0x0100)) {
			/* Setup : Get device descriptor */
			copy_to_endpoint(dev_desc, desc->ep0_tx,
					 sizeof(dev_desc));
			desc->ep[0].tx_count =  sizeof(dev_desc);
			STM32_USB_EP(0) = (STM32_USB_EP(0) & ~0xF070) |
				  (1 << 4) /* toggle TX: NAK->VALID */ |
				  (1 << 12) /* toggle RX: NAK->VALID */ |
				  (1 << 8) /* STATUS OUT */;
			/* need null OUT transaction -> STATUS_OUT */
			CPRINTF("DEVD %04x\n", STM32_USB_EP(0));
		} else if ((desc->ep0_rx[0] == 0x0680) &&
			(desc->ep0_rx[1] == 0x0200)) {
			/* Setup : Get configuration descriptor */
			copy_to_endpoint(conf_desc, desc->ep0_tx,
					 sizeof(conf_desc));
			desc->ep[0].tx_count = MIN(desc->ep0_rx[3],
					 sizeof(conf_desc));
			STM32_USB_EP(0) = (STM32_USB_EP(0) & ~0xF070) |
				  (1 << 4) /* toggle TX: NAK->VALID */ |
				  (1 << 12) /* toggle RX: NAK->VALID */ |
				  (1 << 8) /* STATUS OUT */;
			/* need null OUT transaction -> STATUS_OUT */
			CPRINTF("CFG %04x[l %04x]\n", STM32_USB_EP(0),
				desc->ep0_rx[3]);
		} else if ((desc->ep0_rx[0] == 0x0680) &&
			((desc->ep0_rx[1] >> 8) == 0x03)) {
			/* Setup : Get string descriptor */
			int idx = desc->ep0_rx[1] & 0xff;
			const uint8_t *str_desc = NULL;
			switch (idx) {
			case 0:
				str_desc = string_desc;
				break;
			case 1:
				str_desc = (const uint8_t *)&vendor_str;
				break;
			case 2:
				str_desc = (const uint8_t *)&product_str;
				break;
			case 3:
				str_desc = (const uint8_t *)&version_str;
				break;
			}
			if (!str_desc) {
				uint16_t ep0 = STM32_USB_EP(0);
				STM32_USB_EP(0) = (ep0 & ~0xF0F0) |
					/* toggle RX: ->VALID */
					((ep0 & (1 << 12)) ? 0 : (1<<12)) |
					/* toggle RX: ->VALID */
					((ep0 & (1 << 13)) ? 0 : (1<<13)) |
					/* toggle TX: ->STALL */
					((ep0 & (1 << 4)) ? 0 : (1<<4)) |
					/* toggle TX: ->STALL */
					((ep0 & (1 << 5)) ? (1<<5) : 0);
				return; /* dont remove the STALL */
			}
			copy_to_endpoint(str_desc, desc->ep0_tx, str_desc[0]);
			desc->ep[0].tx_count = MIN(desc->ep0_rx[3],
						   str_desc[0]);
			STM32_USB_EP(0) = (STM32_USB_EP(0) & ~0xF070) |
				  (1 << 4)  /* toggle TX: NAK->VALID */ |
				  (1 << 12) /* toggle RX: NAK->VALID */ |
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
			STM32_USB_EP(0) = (STM32_USB_EP(0) & ~0xF070) |
				  (1 << 4)  /* toggle TX: NAK->VALID */ |
				  (1 << 12) /* toggle RX: NAK->VALID */ |
				  (1 << 8)  /* STATUS OUT */;
			/* need null OUT transaction -> STATUS_OUT */
			CPRINTF("RPT %04x[l %04x]\n", STM32_USB_EP(0),
				desc->ep0_rx[3]);
		} else if ((desc->ep0_rx[0] == 0x0680) &&
			(desc->ep0_rx[1] == 0x0600)) {
			/* Setup : Get device qualifier descriptor */
			/* Not high speed : STALL next IN used as handshake */
			uint16_t ep0 = STM32_USB_EP(0);
			STM32_USB_EP(0) = (ep0 & ~0xF0F0) |
				  /* toggle RX: ->VALID */
				  ((ep0 & (1 << 12)) ? 0 : (1<<12)) |
				  /* toggle RX: ->VALID */
				  ((ep0 & (1 << 13)) ? 0 : (1<<13)) |
				  /* toggle TX: ->STALL */
				  ((ep0 & (1 << 4)) ? 0 : (1<<4)) |
				  /* toggle TX: ->STALL */
				  ((ep0 & (1 << 5)) ? (1<<5) : 0);

			CPRINTF("DEVQ %04x\n", STM32_USB_EP(0));
			return; /* dont remove the STALL */
		} else if (desc->ep0_rx[0] == 0x0900) {
			/* SET_CONFIGURATION */
			uint8_t cfg = desc->ep0_rx[1] & 0xff;
			CPRINTF("SetCFG %d\n", cfg);
			/* null IN for handshake */
			desc->ep[0].tx_count = 0;
		} else if (desc->ep0_rx[0] == 0x0500) {
			/* SET_ADDRESS */
			uint8_t addr = desc->ep0_rx[1] & 0xff;

			/* STM32_USB_DADDR = addr | 0x80; */
			/* set the address after we got IN packet handshake */
			set_addr = addr;
			CPRINTF("ADDR %02x/%d\n", addr, addr);
			STM32_USB_EP(0) = (STM32_USB_EP(0) & ~0xF070) |
				  (1 << 4) /* toggle TX: NAK->VALID */ |
				  (1 << 12) /* toggle RX: NAK->VALID */;
			/* need null IN transaction -> TX Valid */
		}
	}
	if ((ep == 0) && !dir && set_addr) {
		STM32_USB_DADDR = set_addr | 0x80;
		set_addr = 0;
		CPRINTF("SETAD %02x\n", STM32_USB_DADDR);
	}
	if (ep == 1) {
		uint16_t ep1 = STM32_USB_EP(1);
		CPRINTF("%d:%d/%x %x %04x\n", ep, !!dir, desc->ep[ep].rx_count,
			desc->ep[ep].tx_count, ep1);
		/* clear IT */
		STM32_USB_EP(1) = (ep1 & ~0xF0F0);
		return;
	}
	if (1) {
		/* skip toggle bits, reset CTR_RX, CTR_TX */
		uint16_t ep0 = STM32_USB_EP(0);
		STM32_USB_EP(0) = (ep0 & ~0xF0F0) |
				  /* toggle RX: NAK->VALID */
				  ((ep0 & (1 << 12)) ? 0 : (1<<12)) |
				  /* toggle RX: NAK->VALID */
				  ((ep0 & (1 << 13)) ? 0 : (1<<13)) |
				  /* toggle TX: NAK->VALID */
				  ((ep0 & (1 << 4)) ? 0 : (1<<4)) |
				  /* toggle TX: NAK->VALID */
				  ((ep0 & (1 << 5)) ? 0 : (1<<5));
		CPRINTF("CTR EP0 %04x\n", STM32_USB_EP(0));
	}
}

void set_keyboard_report(uint64_t rpt)
{
	uint16_t ep1 = STM32_USB_EP(1);
	copy_to_endpoint((const uint8_t *)&rpt, desc->ep1_tx, sizeof(rpt));
	/* enable TX */
	STM32_USB_EP(1) = (ep1 & ~0xF0F0) |
			  /* toggle TX: NAK->VALID */
			  ((ep1 & (1 << 4)) ? 0 : (1<<4)) |
			  /* toggle TX: NAK->VALID */
			  ((ep1 & (1 << 5)) ? 0 : (1<<5));
}

void usb_interrupt(void)
{
	uint16_t status = STM32_USB_ISTR;

	if ((status & (1 << 10))) {
		/* USB reset */
		STM32_USB_EP(0) = (1 << 9) /* control EP */ |
				  (2 << 4) /* TX NAK */ |
				  (3 << 12) /* RX VALID */;

		desc->ep[0].tx_addr = offsetof(struct usb_sram, ep0_tx) / 2;
		desc->ep[0].rx_addr = offsetof(struct usb_sram, ep0_rx) / 2;
		desc->ep[0].rx_count = 0x8000 | ((MAX_PACKET_SIZE/32-1) << 10);
		desc->ep[0].tx_count = 0;

		/* HID interrupt endpoint 1 */
		desc->ep[1].tx_addr = offsetof(struct usb_sram, ep1_tx) / 2;
		desc->ep[1].tx_count = 8;
		desc->ep1_tx[0] = 0;
		desc->ep1_tx[1] = 0;
		desc->ep1_tx[2] = 0;
		desc->ep1_tx[3] = 0;
		STM32_USB_EP(1) = (1 << 0) /*Endpoint Address: 1 */ |
				  (3 << 4) /* TX Valid */ |
				  (3 << 9) /* interrupt EP */ |
				  (0 << 12) /* RX Disabled */;

		/*
		 * set the default address : 0
		 * as we are not configured yet
		 */
		STM32_USB_DADDR = 0 | 0x80;
		CPRINTF("RST EP0 %04x\n", STM32_USB_EP(0));
	}

	if (status & (1 << 15))
		handle_request(status);

	/* ack interrupts */
	STM32_USB_ISTR = 0;
	CPRINTF("USB:%04x/%04x EP0 %04x FNR %04x\n", status, STM32_USB_ISTR,
		STM32_USB_EP(0), STM32_USB_FNR);
}
DECLARE_IRQ(STM32_IRQ_USB_LP, usb_interrupt, 1);

static void usb_init(void)
{
	/* Enable USB device clock. */
	STM32_RCC_APB1ENR |= 0x00800000;

	/* PA11/PA12 need to be set in USB alternate mode in the board code */
	gpio_config_module(MODULE_USB, 1);

	/* we need a proper 48MHz clock */
	clock_enable_module(MODULE_USB, 1);

	/* power on sequence */

	/* set pull-up on DP for FS mode */
#ifdef CHIP_VARIANT_STM32L15X
	STM32_SYSCFG_PMC |= 1;
#else
	/* hardwired or regular GPIO on other platforms */
#endif

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
	/* task_enable_irq(STM32_IRQ_USB_HP); */
	/* task_enable_irq(STM32_IRQ_USB_FS_WAKEUP); */
	/* set interrupts mask : reset/correct tranfer/errors */
	STM32_USB_CNTR = 0xe400;

	CPRINTF("USB init done\n");
}
DECLARE_HOOK(HOOK_INIT, usb_init, HOOK_PRIO_DEFAULT);

static int command_usb(int argc, char **argv)
{

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(usb, command_usb,
			NULL,
			"debug USB controller",
			NULL);
