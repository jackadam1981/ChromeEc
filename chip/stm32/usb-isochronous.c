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

#if 0
static int read_ctr_tx(struct usb_isochronous_config const *config)
{
	return STM32_USB_EP(config->endpoint) & EP_TX_CTR;
}
#endif

/* Write @count bytes into @config queue from @src, returns number of bytes
 * actually written to @config queue.
 */
size_t usb_isochronous_write_queue(struct usb_isochronous_config const *config,
				   void *src,
				   size_t src_count)
{
	int ep = config->endpoint;
	const int dtog_value = GET_TX_DTOG(config);
	uintptr_t app_addr = APP_ADDR(config, dtog_value);
	size_t app_count = APP_COUNT(config, dtog_value);
	size_t written_count;
	int i;

	for (i = 0; i < src_count; i += 2) {
		((uint8_t *)src)[i] = dtog_value;
	}
	CPRINTS("STIMIM: %s called\n", __func__);
	CPRINTS("STIMIM: USB_EP = %04x\n", STM32_USB_EP(config->endpoint));

	if (app_count >= config->tx_size) {
		CPRINTS("STIMIM: BUFFER FULL\n");
		/* buffer full, cannot write anymore */
		written_count = 0;
		goto END;
	}

	written_count = MIN(src_count, config->tx_size - app_count);
#if 0
	memcpy_to_usbram((void *) usb_sram_addr((usb_uint *) (app_addr +
							      app_count)),
			 src,
			 written_count);
#endif
	memcpy_to_usbram((void *) app_addr,
			 src,
			 written_count);

	app_count += written_count;
	SET_APP_COUNT(config, dtog_value, app_count);

END:
	/* enable TX */
	STM32_USB_EP(ep) = ((ep <<  0) | /* Endpoint Addr */
			    EP_TX_VALID |
			    (2 <<  9) | /* ISO EP */
			    EP_RX_DISAB);
	CPRINTS("STIMIM: USB_EP = %04x\n", STM32_USB_EP(config->endpoint));
	/*
	 * Wake the host. This is required to prevent a race between EP getting
	 * reloaded and host suspending the device, as, ideally, we never want
	 * to have EP loaded during suspend, to avoid reporting stale data.
	 */
	usb_wake();

	CPRINTS("STIMIM: written_count = %u\n", (unsigned) written_count);
	return written_count;
}


void usb_isochronous_init(struct usb_isochronous_config const *config)
{
	int i;
	int ep = config->endpoint;
	SET_APP_ADDR(config, 0, usb_sram_addr(config->tx_ram_0));
	SET_APP_COUNT(config, 0, config->tx_size);
	SET_APP_ADDR(config, 1, usb_sram_addr(config->tx_ram_1));
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
	STM32_TOGGLE_EP(config->endpoint, 0, 0, 0);
	hook_call_deferred(config->deferred, 0);
}

#if 0
static void clear_ctr_tx(struct usb_isochronous_config const *config)
{
	STM32_TOGGLE_EP(config->endpoint, EP_TX_MASK, 0, 0);
}
#endif
void usb_isochronous_deferred(struct usb_isochronous_config const *config)
{
	int ep = config->endpoint;
	CPRINTS("STIMIM: %s called\n", __func__);
	CPRINTS("STIMIM: USB_EP = %04x\n", STM32_USB_EP(config->endpoint));
	STM32_USB_EP(ep) = ((ep <<  0) | /* Endpoint Addr */
			    EP_TX_VALID |
			    (2 <<  9) | /* ISO EP */
			    // 0x40 |  /* DTOG_TX */
			    EP_RX_DISAB);
	CPRINTS("STIMIM: USB_EP = %04x\n", STM32_USB_EP(config->endpoint));
	SET_APP_COUNT(config, GET_TX_DTOG(config), 0);
#if 0
	/* Try to push more data into application buffer */
	if (read_ctr_tx(config)) {
		CPRINTS("STIMIM: toggle ep\n");
		/* Transfer complete, switch buffer.
		 * This will clear CTR_TX bit.
		 */
		STM32_TOGGLE_EP(config->endpoint, EP_TX_MASK, 0, EP_TX_VALID);
	}
#endif
}
