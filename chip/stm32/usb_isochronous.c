/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "stddef.h"
#include "common.h"
#include "config.h"
#include "link_defs.h"
#include "registers.h"
#include "util.h"
#include "usb_api.h"
#include "usb_hw.h"
#include "usb_isochronous.h"


/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)


/*
 * Gets current DTOG value of given `config`.
 */
int usb_isochronous_get_tx_dtog(struct usb_isochronous_config const *config)
{
	return !!(STM32_USB_EP(config->endpoint) & EP_TX_DTOG);
}

/*
 * Gets buffer address that can be used by software (application).
 *
 * The mapping between application buffer address and current TX DTOG value is
 * shown in table above.
 */
usb_uint *usb_isochronous_get_app_addr(
		struct usb_isochronous_config const *config,
		int dtog_value)
{
	return config->tx_ram[dtog_value];
}

/*
 * Sets number of bytes written to application buffer.
 */
void usb_isochronous_set_app_count(struct usb_isochronous_config const *config,
				   int dtog_value,
				   usb_uint count)
{
	if (dtog_value)
		btable_ep[config->endpoint].tx_count = count;
	else
		btable_ep[config->endpoint].rx_count = count;
}

void usb_isochronous_init(struct usb_isochronous_config const *config)
{
	int ep = config->endpoint;

	btable_ep[ep].tx_addr = usb_sram_addr(
			usb_isochronous_get_app_addr(config, 1));
	btable_ep[ep].rx_addr = usb_sram_addr(
			usb_isochronous_get_app_addr(config, 0));
	usb_isochronous_set_app_count(config, 0, 0);
	usb_isochronous_set_app_count(config, 1, 0);

	STM32_USB_EP(ep) = ((ep << 0) | /* Endpoint Addr */
			    EP_TX_VALID | /* start transmit */
			    (2 << 9) | /* ISO EP */
			    EP_RX_DISAB);
}

void usb_isochronous_event(struct usb_isochronous_config const *config,
			   enum usb_ep_event evt)
{
	if (evt == USB_EVENT_RESET)
		usb_isochronous_init(config);
}

void usb_isochronous_tx(struct usb_isochronous_config const *config)
{
	/*
	 * Clear CTR_TX, note that EP_TX_VALID will *NOT* be cleared by
	 * hardware, so we don't need to toggle it.
	 */
	STM32_TOGGLE_EP(config->endpoint, 0, 0, 0);
	/*
	 * Clear buffer count for buffer we just transmitted, so we do not
	 * transmit the data twice.
	 */
	usb_isochronous_set_app_count(config,
				      usb_isochronous_get_tx_dtog(config), 0);

	config->tx_callback(config);
}

int usb_isochronous_iface_handler(struct usb_isochronous_config const *config,
				  usb_uint *ep0_buf_rx,
				  usb_uint *ep0_buf_tx)
{
	int ret = -1;

	if (ep0_buf_rx[0] == (USB_DIR_OUT |
			      USB_TYPE_STANDARD |
			      USB_RECIP_INTERFACE |
			      USB_REQ_SET_INTERFACE << 8)) {
		ret = config->set_interface(ep0_buf_rx[1], ep0_buf_rx[2]);

		if (ret == 0) {
			/* ACK */
			btable_ep[0].tx_count = 0;
			STM32_TOGGLE_EP(0, EP_TX_RX_MASK, EP_TX_RX_VALID, 0);
		}
	}
	return ret;
}
