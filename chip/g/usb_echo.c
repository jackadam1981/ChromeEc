/* Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "config.h"
#include "console.h"
#include "link_defs.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb.h"

/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)

/* USB bulk descriptors */
const struct usb_interface_descriptor USB_IFACE_DESC(USB_IFACE_ECHO) =
{
	.bLength            = USB_DT_INTERFACE_SIZE,
	.bDescriptorType    = USB_DT_INTERFACE,
	.bInterfaceNumber   = USB_IFACE_ECHO,
	.bAlternateSetting  = 0,
	.bNumEndpoints      = 2,
	.bInterfaceClass    = USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass = 0xFF,
	.bInterfaceProtocol = 0x00,
	.iInterface         = USB_STR_ECHO_NAME,
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_ECHO, 0) =
{
	.bLength            = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = 0x80 | USB_EP_ECHO,
	.bmAttributes       = 0x02 /* Bulk IN */,
	.wMaxPacketSize     = USB_MAX_PACKET_SIZE,
	.bInterval          = 10
};
const struct usb_endpoint_descriptor USB_EP_DESC(USB_IFACE_ECHO, 1) =
{
	.bLength            = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    = USB_DT_ENDPOINT,
	.bEndpointAddress   = USB_EP_ECHO,
	.bmAttributes       = 0x02 /* Bulk OUT */,
	.wMaxPacketSize     = USB_MAX_PACKET_SIZE,
	.bInterval          = 0
};

static usb_uint ep_buf_tx[USB_MAX_PACKET_SIZE / 2] /*__usb_ram*/;
static usb_uint ep_buf_rx[USB_MAX_PACKET_SIZE / 2] /*__usb_ram*/;
static struct g_usb_desc ep_out_desc;
static struct g_usb_desc ep_in_desc;

static void echo_ep_tx(void)
{
	/* Make next packet available */
	ep_in_desc.flags = DIEPDMA_LAST | DIEPDMA_BS_HOST_RDY | DIEPDMA_IOC |
			   DIEPDMA_TXBYTES(USB_MAX_PACKET_SIZE);
	GR_USB_DIEPCTL(USB_EP_ECHO) |= DXEPCTL_CNAK | DXEPCTL_EPENA;
	/* clear IT */
	GR_USB_DIEPINT(USB_EP_ECHO) = 0xffffffff;
}

static void echo_ep_rx(void)
{
	/* TODO: read received packet */
#if 0
	int rx_size = USB_MAX_PACKET_SIZE
		    - (ep_out_desc.flags & DOEPDMA_RXBYTES_MASK);

	for (i = 0; i < rx_size; i++) {
			uint8_t byte = ((i & 1) ?
					       (ep_buf_rx[i >> 1] >> 8) :
					       (ep_buf_rx[i >> 1] & 0xff));
	}
#endif

	/* Ready to receive the next one */
	ep_out_desc.flags = DOEPDMA_RXBYTES(USB_MAX_PACKET_SIZE) |
			    DOEPDMA_LAST | DOEPDMA_BS_HOST_RDY | DOEPDMA_IOC;
	GR_USB_DOEPCTL(USB_EP_ECHO) |= DXEPCTL_CNAK | DXEPCTL_EPENA;
	/* clear IT */
	GR_USB_DOEPINT(USB_EP_ECHO) = 0xffffffff;
}

static void echo_ep_reset(void)
{
	int i;
	/* Put a pattern in our buffer */
	for (i = 0; i < USB_MAX_PACKET_SIZE / 2; i++)
		ep_buf_tx[i] = (('A' + (i>>3)) << 8) | ('0' + (i&7));

	ep_out_desc.flags = DOEPDMA_RXBYTES(USB_MAX_PACKET_SIZE) |
			    DOEPDMA_LAST | DOEPDMA_BS_HOST_RDY | DOEPDMA_IOC;
	ep_out_desc.addr = ep_buf_rx;
	GR_USB_DOEPDMA(USB_EP_ECHO) = (uint32_t)&ep_out_desc;
	ep_in_desc.flags = DIEPDMA_LAST | DIEPDMA_BS_HOST_BSY | DIEPDMA_IOC |
			   DIEPDMA_TXBYTES(USB_MAX_PACKET_SIZE);
	ep_in_desc.addr = ep_buf_tx;
	GR_USB_DIEPDMA(USB_EP_ECHO) = (uint32_t)&ep_in_desc;
	GR_USB_DOEPCTL(USB_EP_ECHO) = DXEPCTL_MPS(64) | DXEPCTL_USBACTEP |
					 DXEPCTL_EPTYPE_BULK |
					 DXEPCTL_CNAK | DXEPCTL_EPENA;
	GR_USB_DIEPCTL(USB_EP_ECHO) = DXEPCTL_MPS(64) | DXEPCTL_USBACTEP |
					 DXEPCTL_EPTYPE_BULK |
					 DXEPCTL_TXFNUM(USB_EP_ECHO);
	GR_USB_DAINTMSK |= (1<<USB_EP_ECHO) | (1 << (USB_EP_ECHO+16));

	/* enable IN packet reception */
	echo_ep_tx();
}

USB_DECLARE_EP(USB_EP_ECHO, echo_ep_tx, echo_ep_rx, echo_ep_reset);
