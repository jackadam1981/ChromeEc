/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __USBD_INIT_H
#define __USBD_INIT_H

#include <stdint.h>
#include <stdlib.h>

#include <zephyr/drivers/usb/udc.h>
#include <zephyr/usb/usbd.h>

struct hid_dev_t {
	const struct device *dev;
	atomic_t state;
	uint8_t report_protocol;

	struct queue report_queue;
	struct k_mutex *report_queue_mutex;
};

enum {
	HID_CLASS_IFACE_READY = 0,
	HID_CLASS_SUSPENDED,
	HID_EP_IN_BUSY,
};

typedef void (*msg_callback_t)(enum usbd_msg_type type);

int request_usb_wake(void);
int usb_msg_callback_register(msg_callback_t callback);

#endif /* __USBD_INIT_H */
