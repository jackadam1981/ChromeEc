/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __GOOGLE_VENDOR_H
#define __GOOGLE_VENDOR_H

#include <zephyr/drivers/usb/udc.h>
#include <zephyr/usb/usbd.h>

#define GOOGLE_EP_FS_MPS 64

#define AUTO_EP_IN 0x80
#define AUTO_EP_OUT 0x00

struct google_desc {
	struct usb_if_descriptor if0;
	struct usb_ep_descriptor out_ep;
	struct usb_ep_descriptor in_ep;
} __packed;

struct google_data {
	struct google_desc *const desc;
	const struct usb_desc_header **const fs_desc;
	atomic_t state;
	struct k_thread tx_thread_data;
	struct k_thread rx_thread_data;
	struct k_sem sync_sem;
};

enum {
	GFAKE_DEV_CLASS_ENABLED = 0,
	GUPDATE_DEV_CLASS_ENABLED,
	GUPDATE_DEV_CLASS_OUT_BUSY,
	GI2C_DEV_CLASS_ENABLED,
	GI2C_DEV_CLASS_OUT_BUSY,
};


#endif /* __GOOGLE_VENDOR_H */
