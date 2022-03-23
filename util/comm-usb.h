/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * For hysterical raisins, there are several mechanisms for communicating with
 * the EC. This abstracts them.
 */

#ifndef __UTIL_COMM_USB_H
#define __UTIL_COMM_USB_H

#include "common.h"
#include "ec_commands.h"

enum exit_values {
	noop = 0,	  /* All up to date, no update needed. */
	all_updated = 1,  /* Update completed, reboot required. */
	rw_updated  = 2,  /* RO was not updated, reboot required. */
	update_error = 3  /* Something went wrong. */
};

struct usb_endpoint {
	struct libusb_device_handle *devh;
	int iface_num;
	uint8_t ep_num;
	int     chunk_len;
};

struct transfer_descriptor {
	/*
	 * offsets of section available for update (not currently active).
	 */
	uint32_t offset;

	struct usb_endpoint uep;
};

int parse_vidpid(const char *input, uint16_t *vid_ptr, uint16_t *pid_ptr);

/**
 * Initialize USB communication.
 *
 * @param vid  Vendor ID of the endpoint device.
 * @param pid  Product ID of the endpoint device.
 * @return     Zero if success or non-zero otherwise.
 */
int comm_init_usb(uint16_t vid, uint16_t pid);

/**
 * Clean up USB communication.
 */
void comm_cleanup_usb(void);

#endif  /* __UTIL_COMM_USB_H */
