/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "compile_time_macros.h"
#include "console.h"
#include "heci_client.h"
#include "hid_device.h"
#include "util.h"

#ifdef HID_SUBSYS_DEBUG
#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_LPC, format, ## args)
#else
#define CPUTS(outstr)
#define CPRINTS(format, args...)
#define CPRINTF(format, args...)
#endif

#define __packed __attribute__((packed))

#define HECI_CLIENT_HID_GUID { 0x33AECD58, 0xB679, 0x4E54,\
			 { 0x9B, 0xD9, 0xA0, 0x4D, 0x34, 0xF0, 0xC2, 0x26 } }

#define HID_SUBSYS_MAX_HID_DEVICES			3

enum {
	HID_GET_HID_DESCRIPTOR = 0,
	HID_GET_REPORT_DESCRIPTOR,
	HID_GET_FEATURE_REPORT,
	HID_SET_FEATURE_REPORT,
	HID_GET_INPUT_REPORT,
	HID_PUBLISH_INPUT_REPORT,
	HID_PUBLISH_INPUT_REPORT_LIST, /* TODO: need to support batch report */

	HID_HID_CLIENT_READY_CMD = 30,
	HID_HID_COMMAND_MAX = 31,

	HID_DM_COMMAND_BASE,
	HID_DM_ENUM_DEVICES,
	HID_DM_ADD_DEVICE,
	HID_COMMAND_LAST
};

struct hid_device_info {
	uint32_t dev_id;
	uint8_t dev_class;
	uint16_t pid;
	uint16_t vid;
} __packed;

struct hid_enum_payload	{
	uint8_t num_of_hid_devices;
	struct hid_device_info dev_info[0];
} __packed;

struct hid_msg_hdr {
	uint8_t command     :7;
	uint8_t is_response :1;
	uint8_t device_id;
	uint8_t status;
	uint8_t flags;
	uint16_t size;
} __packed;

struct hid_msg {
	struct hid_msg_hdr hdr;
	uint8_t payload[HID_SUBSYS_MAX_PAYLOAD_SIZE];
} __packed;

struct hid_subsys_hid_device {
	struct hid_device_info info;
	const struct hid_callbacks *cbs;
	int can_send_hid_input;

	void *data;
};

struct hid_subsystem {
	uint8_t heci_handle;

	uint32_t num_of_hid_devices;
	struct hid_subsys_hid_device hid_devices[HID_SUBSYS_MAX_HID_DEVICES];
};

static struct hid_subsystem hid_subsys_ctx = {
	.heci_handle = HECI_INVALID_HANDLE,
};

#define HID_SUBSYS_DEV_IDX_TO_HANDLE(dev_idx)	((dev_idx) + 1)
#define HID_SUBSYS_DEV_HANDLE_TO_HID_DEV(dev_handle) \
				(&hid_subsys_ctx.hid_devices[(dev_handle) - 1])
#define HID_SUBSYS_IS_VALID_DEV_HANDLE(dev_handle) \
	((dev_handle) > 0 && (dev_handle) <= hid_subsys_ctx.num_of_hid_devices)
#define HID_HANDLE_TO_DEV_ID(dev_id)		(dev_id)
#define DEV_ID_TO_HID_HANDLE(dev_id)		((int)dev_id)

int hid_subsys_register_device(struct hid_device *dev_info)
{
	struct hid_subsys_hid_device *hid_device;
	int handle, ret;
	int hid_device_index;

	if (dev_info == NULL && dev_info->cbs == NULL)
		return HID_SUBSYS_ERR_INVALID_ARGS;

	if (hid_subsys_ctx.num_of_hid_devices == HID_SUBSYS_MAX_HID_DEVICES)
		return HID_SUBSYS_ERR_TOO_MANY_HID_DEVICES;

	hid_device_index = hid_subsys_ctx.num_of_hid_devices++;

	handle = HID_SUBSYS_DEV_IDX_TO_HANDLE(hid_device_index);

	hid_device = &hid_subsys_ctx.hid_devices[hid_device_index];

	hid_device->info.dev_class = dev_info->dev_class;
	hid_device->info.pid = dev_info->pid;
	hid_device->info.vid = dev_info->vid;
	hid_device->info.dev_id = HID_HANDLE_TO_DEV_ID(handle);

	hid_device->cbs = dev_info->cbs;

	if (dev_info->cbs->initialize) {
		ret = dev_info->cbs->initialize(handle);
		if (ret) {
			hid_subsys_ctx.num_of_hid_devices--;
			return HID_SUBSYS_ERR_INIT_FAIL;
		}
	}

	return handle;
}

int hid_subsys_send_input_report(int hid_handle, uint8_t *buf, size_t buf_size)
{
	struct hid_subsys_hid_device *hid_device;
	struct hid_msg_hdr hid_msg_hdr;
	struct heci_msg_item msg_item[2];
	struct heci_msg_list msg_list;

	if (!HID_SUBSYS_IS_VALID_DEV_HANDLE(hid_handle))
		return HID_SUBSYS_ERR_INVALID_HANDLE;

	if (buf_size > HID_SUBSYS_MAX_PAYLOAD_SIZE)
		return HID_SUBSYS_ERR_TOO_BIG_REPORT_SIZE;

	if (hid_subsys_ctx.heci_handle == HECI_INVALID_HANDLE)
		return HID_SUBSYS_ERR_NOT_READY;

	hid_device = HID_SUBSYS_DEV_HANDLE_TO_HID_DEV(hid_handle);
	if (!hid_device->can_send_hid_input)
		return HID_SUBSYS_ERR_NOT_READY;

	memset(&hid_msg_hdr, 0, sizeof(hid_msg_hdr));

	hid_msg_hdr.command = HID_PUBLISH_INPUT_REPORT;
	hid_msg_hdr.device_id = hid_device->info.dev_id;
	hid_msg_hdr.size = buf_size;

	msg_item[0].size = sizeof(hid_msg_hdr);
	msg_item[0].buf = (uint8_t *)&hid_msg_hdr;

	msg_item[1].size = buf_size;
	msg_item[1].buf = buf;

	msg_list.num_of_items = 2;
	msg_list.items[0] = &msg_item[0];
	msg_list.items[1] = &msg_item[1];

	heci_send_msgs(hid_subsys_ctx.heci_handle, &msg_list);

	return 0;
}

int hid_subsys_set_device_data(int hid_handle, void *data)
{
	struct hid_subsys_hid_device *hid_device;

	if (!HID_SUBSYS_IS_VALID_DEV_HANDLE(hid_handle))
		return HID_SUBSYS_ERR_INVALID_HANDLE;

	hid_device = HID_SUBSYS_DEV_HANDLE_TO_HID_DEV(hid_handle);
	hid_device->data = data;

	return 0;
}

void *hid_subsys_get_device_data(int hid_handle)
{
	struct hid_subsys_hid_device *hid_device;

	if (!HID_SUBSYS_IS_VALID_DEV_HANDLE(hid_handle))
		return NULL;

	hid_device = HID_SUBSYS_DEV_HANDLE_TO_HID_DEV(hid_handle);
	return hid_device->data;
}

static int handle_hid_device_msg(struct hid_msg *hid_msg)
{
	int size = 0, payload_size, buf_size, size_written = 0;
	uint8_t *payload;
	struct hid_subsys_hid_device *hid_dev;
	const struct hid_callbacks *cbs;
	int hid_handle;

	if (!HID_SUBSYS_IS_VALID_DEV_HANDLE(hid_msg->hdr.device_id))
		return -1;

	hid_dev = HID_SUBSYS_DEV_HANDLE_TO_HID_DEV(hid_msg->hdr.device_id);
	cbs = hid_dev->cbs;

	hid_handle = DEV_ID_TO_HID_HANDLE(hid_msg->hdr.device_id);
	payload = hid_msg->payload;
	payload_size = hid_msg->hdr.size; /* input data */
	buf_size = sizeof(hid_msg->payload); /* buffer to be written by cb */

	switch (hid_msg->hdr.command) {
	case  HID_GET_HID_DESCRIPTOR:
		if (cbs->get_hid_descriptor) {
			size = cbs->get_hid_descriptor(hid_handle, payload,
						       buf_size);
		}

		break;
	case HID_GET_REPORT_DESCRIPTOR:
		if (cbs->get_report_descriptor) {
			size = cbs->get_report_descriptor(hid_handle, payload,
							  buf_size);
		}

		hid_dev->can_send_hid_input = 1;

		break;

	case HID_GET_FEATURE_REPORT:
		if (cbs->get_feature_report) {
			size = cbs->get_feature_report(hid_handle, payload[0],
						       payload, buf_size);

		}

		break;

	case HID_SET_FEATURE_REPORT:
		if (cbs->set_feature_report) {
			size_written = cbs->set_feature_report(hid_handle,
							       payload[0],
							       payload,
							       payload_size);
			/* if no error, reply with the report id */
			/* [TODO] do i need to compare with payload_size ? */
			if (size_written >= 0)
				size = sizeof(uint8_t);
			else /* make "size" to indicate error */
				size = size_written;
		}

		break;
	case HID_GET_INPUT_REPORT:
		if (cbs->get_input_report) {
			size = cbs->get_input_report(hid_handle, payload[0],
						     payload, buf_size);
		}

		break;

	default:
		CPRINTF("invalid hid command %d, ignoring request\n",
			hid_msg->hdr.command);
		return -1;
	}

	if (size > 0) {
		hid_msg->hdr.size = size;
		hid_msg->hdr.status = 0;
	} else { /* error in callback */
		/* TODO: doesn't make sense asking hid device to set error
		 * need to check HID protocol implementation in host.
		 */
		hid_msg->hdr.size = 0;
		hid_msg->hdr.status = -size;
	}

	hid_msg->hdr.is_response = 1;
	hid_msg->hdr.flags = 0;
	hid_msg->hdr.status = 0; /* what to do ? */

	heci_send_msg(hid_subsys_ctx.heci_handle, (uint8_t *)hid_msg,
		      sizeof(hid_msg->hdr) + hid_msg->hdr.size);

	return 0;
}

static int handle_hid_subsys_msg(struct hid_msg *hid_msg)
{
	int size  = 0, i;
	struct hid_enum_payload *enum_payload;

	switch (hid_msg->hdr.command) {
	case  HID_DM_ENUM_DEVICES:
		enum_payload = (struct hid_enum_payload *)hid_msg->payload;

		for (i = 0; i < hid_subsys_ctx.num_of_hid_devices; i++) {
			enum_payload->dev_info[i] =
					hid_subsys_ctx.hid_devices[i].info;
		}

		enum_payload->num_of_hid_devices =
					hid_subsys_ctx.num_of_hid_devices;

		/* reply payload size */
		size = sizeof(enum_payload->num_of_hid_devices);
		size += enum_payload->num_of_hid_devices *
					sizeof(enum_payload->dev_info[0]);

		break;

	default:
		CPRINTF("invalid hid command %d, ignoring request\n",
			hid_msg->hdr.command);
		return -1;
	}

	if (size > 0) {
		hid_msg->hdr.size = size;
		hid_msg->hdr.status = 0;
	} else { /* error in callback */
		hid_msg->hdr.size = 0;
		hid_msg->hdr.status = -size;
	}

	hid_msg->hdr.is_response = 1;
	hid_msg->hdr.flags = 0;
	hid_msg->hdr.status = 0; /* what to do ? */

	heci_send_msg(hid_subsys_ctx.heci_handle, (uint8_t *)hid_msg,
		      sizeof(hid_msg->hdr) + hid_msg->hdr.size);

	return 0;
}

static void hid_subsys_new_msg_received(uint8_t heci_handle, uint8_t *msg,
				 uint32_t msg_size)
{
	struct hid_msg *hid_msg = (struct hid_msg *)msg;

	/* workaround, since Host driver doesn't set size properly */
	if (hid_msg->hdr.size == 0 && msg_size > sizeof(hid_msg->hdr))
		hid_msg->hdr.size = msg_size - sizeof(hid_msg->hdr);

	if (hid_msg->hdr.size > HID_SUBSYS_MAX_PAYLOAD_SIZE)
		return; /* invalid hdr. discard */

	if (hid_msg->hdr.device_id)
		handle_hid_device_msg(hid_msg);
	else
		handle_hid_subsys_msg(hid_msg);
}

static int hid_subsys_initialize(uint8_t heci_handle)
{
	hid_subsys_ctx.heci_handle = heci_handle;

	return 0;
}

static int hid_subsys_resume(uint8_t heci_handle)
{
	int i, ret = 0;

	for (i = 0; i < hid_subsys_ctx.num_of_hid_devices; i++) {
		if (hid_subsys_ctx.hid_devices[i].cbs->resume)
			ret |= hid_subsys_ctx.hid_devices[i].cbs->resume(
					HID_SUBSYS_DEV_IDX_TO_HANDLE(i));
	}

	return ret;
}

static int hid_subsys_suspend(uint8_t heci_handle)
{
	int i, ret = 0;

	for (i = hid_subsys_ctx.num_of_hid_devices - 1; i >= 0; i--) {
		if (hid_subsys_ctx.hid_devices[i].cbs->suspend)
			ret |= hid_subsys_ctx.hid_devices[i].cbs->suspend(
					HID_SUBSYS_DEV_IDX_TO_HANDLE(i));
	}

	return ret;
}

static const struct heci_client_callbacks hid_subsys_heci_cbs = {
	.initialize = hid_subsys_initialize,
	.new_msg_received = hid_subsys_new_msg_received,
	.suspend = hid_subsys_suspend,
	.resume = hid_subsys_resume,
};

static const struct heci_client hid_subsys_heci_client = {
	.protocol_id = HECI_CLIENT_HID_GUID,
	.max_msg_size = HECI_MAX_MSG_SIZE,
	.protocol_ver = 1,
	.max_n_of_connections = 1,

	.cbs = &hid_subsys_heci_cbs,
};

HECI_CLIENT_ENTRY(hid_subsys_heci_client);
