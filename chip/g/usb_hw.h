/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USB_HW_H
#define __CROS_EC_USB_HW_H

/* Helpers for endpoint declaration */
#define _EP_HANDLER2(num, suffix) CONCAT3(ep_, num, suffix)
#define _EP_TX_HANDLER(num) _EP_HANDLER2(num, _tx)
#define _EP_RX_HANDLER(num) _EP_HANDLER2(num, _rx)
#define _EP_RESET_HANDLER(num) _EP_HANDLER2(num, _rst)

#define USB_DECLARE_EP(num, tx_handler, rx_handler, rst_handler)  \
	void _EP_TX_HANDLER(num)(void)				  \
		__attribute__ ((alias(STRINGIFY(tx_handler))));	  \
	void _EP_RX_HANDLER(num)(void)                            \
		__attribute__ ((alias(STRINGIFY(rx_handler))));	  \
	void _EP_RESET_HANDLER(num)(void)                         \
		__attribute__ ((alias(STRINGIFY(rst_handler))));

/* Arrays with all the endpoint callbacks */
extern void (*usb_ep_tx[]) (void);
extern void (*usb_ep_rx[]) (void);
extern void (*usb_ep_reset[]) (void);

/*
 * Declare any interface-specific control request handlers. These Setup packets
 * arrive on the control endpoint (EP0), but are handled by the interface code.
 * The callback must prepare the IN or OUT FIFOs, and return 0 if all went well
 * or non-zero to STALL the next stage (and thus indicate error to the host).
 */
struct usb_setup_packet;
#define _IFACE_HANDLER(num) CONCAT3(iface_, num, _request)
#define USB_DECLARE_IFACE(num, handler)				\
	int _IFACE_HANDLER(num)(struct usb_setup_packet *req)	\
		__attribute__ ((alias(STRINGIFY(handler))));

/* Array of interface handler callbacks */
static int (*usb_iface_request[]) (struct usb_setup_packet *req);

/*
 * The interface handler can call this to put data in the EP0 TX FIFO.
 * It returns 0 on success.
 */
int load_in_fifo(const void *source, uint32_t len);

#endif	/* __CROS_EC_USB_HW_H */
