/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "touchpad_passthru.h"
#include "usb-isochronous.h"
#include "board.h"
#include "link_defs.h"

#ifdef USB_TOUCHPAD_PASSTHRU

/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)

/* declare interface */
USB_ISOCHRONOUS_CONFIG_FULL(usb_touchpad_passthru_config,
			    USB_IFACE_TOUCHPAD_PASSTHRU,
			    USB_CLASS_VENDOR_SPEC,
			    0,  /* subclass */
			    0,  /* protocol */
			    0,  /* interface name */
			    USB_EP_TOUCHPAD_PASSTHRU,
			    USB_MAX_PACKET_SIZE,
			    struct touchpad_passthru_report)

void touchpad_passthru_generate_event(void)
{
	static uint8_t cc;
	struct touchpad_passthru_report report;
	int i;

	CPRINTS("%s called\n", __func__);
	for (i = 0; i < FRAME_SIZE; i++)
		report.frame[i] = (cc++);

	usb_isochronous_write_queue(&usb_touchpad_passthru_config,
				    (void *) &report);
}

#endif  /* USB_IFACE_TOUCHPAD_PASSTHRU */
