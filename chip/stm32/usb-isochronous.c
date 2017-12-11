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
#include "usb-isochronous.h"


/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)

/* Write @count bytes into @config queue from @src, returns number of bytes
 * actually written to @config queue.
 */
size_t usb_isochronous_write_queue(struct usb_isochronous_config const *config,
				   void *src,
				   size_t src_count)
{
	// int ep = config->endpoint;
	const int dtog_value = GET_TX_DTOG(config);
	uintptr_t app_addr = APP_ADDR(config, dtog_value);
	size_t app_count = APP_COUNT(config, dtog_value);
	size_t written_count;
	int i;

	for (i = 0; i < src_count; i += 2)
		((uint8_t *)src)[i] = !!dtog_value;

	if (app_count >= config->tx_size) {
		// CPRINTS("STIMIM: BUFFER FULL\n");
		/* buffer full, cannot write anymore */
		/* TODO(stimim): discard old values */
		written_count = 0;
		goto END;
	}

	written_count = MIN(src_count, config->tx_size - app_count);
	memcpy_to_usbram((void *) (app_addr + app_count),
			 src,
			 written_count);

	app_count += written_count;
	SET_APP_COUNT(config, dtog_value, app_count);

END:
	// CPRINTS("STIMIM: written_count = %u\n", (unsigned) written_count);
	return written_count;
}


void usb_isochronous_init(struct usb_isochronous_config const *config)
{
	int i;
	int ep = config->endpoint;

	SET_APP_ADDR(config, 0, usb_sram_addr(config->tx_ram_1));
	SET_APP_COUNT(config, 0, config->tx_size);
	SET_APP_ADDR(config, 1, usb_sram_addr(config->tx_ram_0));
	SET_APP_COUNT(config, 1, config->tx_size);

	for (i = 0; i < DIV_ROUND_UP(config->tx_size, 2); i++) {
		config->tx_ram_0[i] = i;
		config->tx_ram_1[i] = (1 << 8) | i;
	}
	STM32_USB_EP(ep) = ((ep <<  0) | /* Endpoint Addr */
			    EP_TX_VALID |
			    (2 <<  9) | /* ISO EP */
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
	CPRINTS("STIMIM: %s called\n", __func__);
	// CTR_TX should be set to 1 at this point.
	CPRINTS("STIMIM: %s USB_EP = %04x\n",
		__func__,
		STM32_USB_EP(config->endpoint));

	// clear TX
	STM32_TOGGLE_EP(config->endpoint, 0, 0, 0);
	// clear buffer count
	SET_APP_COUNT(config, GET_TX_DTOG(config), 0);

	CPRINTS("STIMIM: %s USB_EP = %04x\n",
		__func__,
		STM32_USB_EP(config->endpoint));

	// hook_call_deferred(config->deferred, 0);
}

void usb_isochronous_deferred(struct usb_isochronous_config const *config)
{
}
