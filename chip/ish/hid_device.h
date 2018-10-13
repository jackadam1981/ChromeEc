/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __HID_DEVICE_H
#define __HID_DEVICE_H

#include <stdint.h>
#include <stddef.h>

#include "hooks.h"

#define HID_SUBSYS_MAX_PAYLOAD_SIZE			4954

#define HID_SUBSYS_ERR_TOO_MANY_HID_DEVICES		-1
#define HID_SUBSYS_ERR_INVALID_HANDLE			-2
#define HID_SUBSYS_ERR_TOO_BIG_REPORT_SIZE		-3
#define HID_SUBSYS_ERR_NOT_READY			-4
#define HID_SUBSYS_ERR_INVALID_ARGS			-5
#define HID_SUBSYS_ERR_INIT_FAIL			-6

struct hid_callbacks {
	/* function called during registration */
	int (*initialize)(int hid_handle);

	/* return size of data copied to buf */
	int (*get_hid_descriptor)(int hid_handle, uint8_t *buf,
				  size_t buf_size);
	/* return size of data copied  to buf */
	int (*get_report_descriptor)(int hid_handle, uint8_t *buf,
				     size_t buf_size);
	/* return size of data copied to buf */
	int (*get_feature_report)(int hid_handle, uint8_t report_id,
				  uint8_t *buf, uint32_t buf_size);
	/* return tranferred data size */
	int (*set_feature_report)(int hid_handle, uint8_t report_id,
				  const uint8_t *data, size_t data_size);
	/* return size of data copied to buf */
	int (*get_input_report)(int hid_handle, uint8_t report_id,
				uint8_t *buf, size_t buf_size);

	/* suspend/resume */
	int (*resume)(int hid_handle);
	int (*suspend)(int hid_handle);
};

struct hid_device {
	uint8_t dev_class;
	uint16_t pid;
	uint16_t vid;

	const struct hid_callbacks *cbs;
};

/*
 * Do not call this function directly.
 * The function should be called only by HID_DEVICE_ENTRY()
 */
int hid_subsys_register_device(struct hid_device *dev_info);
/* send HID input report */
int hid_subsys_send_input_report(int hid_handle, uint8_t *buf, size_t buf_size);
/* store HID device specific data */
int hid_subsys_set_device_data(int hid_handle, void *data);
/* retrieve HID device specific data */
void *hid_subsys_get_device_data(int hid_handle);

#define HID_DEVICE_ENTRY(hid_dev) \
	void _hid_dev_entry_##hid_dev(void) \
	{ \
		hid_subsys_register_device(&(hid_dev)); \
	} \
	DECLARE_HOOK(HOOK_INIT, _hid_dev_entry_##hid_dev, HOOK_PRIO_LAST - 2)

#endif /* __HID_DEVICE_H */
