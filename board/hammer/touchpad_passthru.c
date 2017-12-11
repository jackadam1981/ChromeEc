/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "touchpad_passthru.h"
#include "usb-isochronous.h"
#include "util.h"

#include "board.h"
#include "link_defs.h"

#ifdef USB_TOUCHPAD_PASSTHRU

/* Console output macro */
#define CPRINTF(format, args...) cprintf(CC_USB, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)

struct touchpad_passthru_report report;
static size_t transmit_report_offset;
static uint8_t report_available;
/* a tiny header to let AP know if a packet is lost. */
static uint8_t packet_counter;

void touchpad_passthru_generate_event(void)
{
	static uint8_t cc;
	int i;

	if (transmit_report_offset) {
		CPRINTS("%s: transmitting report, skip new event.", __func__);
		return;
	}

	report.magic = 0x5566;
	for (i = 0; i < FRAME_SIZE; i++)
		report.frame[i] = (cc++);
	report_available = 1;
}


static size_t touchpad_usb_tx_callback(uintptr_t usb_addr, size_t tx_size) {
	size_t num_byte_available = sizeof(report) - transmit_report_offset;
	size_t count = 0;

	if (report_available && num_byte_available > 0) {
		memcpy_to_usbram((void *) usb_addr, &packet_counter, 1);
		packet_counter ++;
		count ++;

		num_byte_available = MIN(tx_size - 1, num_byte_available);
		memcpy_to_usbram((void *) usb_addr + 1,
				 ((uint8_t *) &report) + transmit_report_offset,
				 num_byte_available);
		transmit_report_offset += num_byte_available;
		count += num_byte_available;

		if (transmit_report_offset == sizeof(report)) {
			transmit_report_offset = 0;
			report_available = 0;
		}
	}
	return count;
}

/* declare interface */
USB_ISOCHRONOUS_CONFIG_FULL(usb_touchpad_passthru_config,
			    USB_IFACE_TOUCHPAD_PASSTHRU,
			    USB_CLASS_VENDOR_SPEC,
			    0,  /* subclass */
			    0,  /* protocol */
			    0,  /* interface name */
			    USB_EP_TOUCHPAD_PASSTHRU,
			    128,  // packet size
			    touchpad_usb_tx_callback)


#endif  /* USB_IFACE_TOUCHPAD_PASSTHRU */
