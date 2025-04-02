/*
 * Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"

#include "Global.h"
#include "board.h"
#include "console.h"
#include "endian.h"
#include "extension.h"
#include "system.h"
#include "util.h"

#include "nvmem_vars.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/* Mask used to get/set valid mac bit */
#define TS_FLAGS_MASK 0x80

/* Key names for Trusty storage mac nvmem_vars */
#define TS_KEY_TDP "ts-tdp"
#define TS_KEY_TD  "ts-td-"
#define TS_KEY_LEN (sizeof(TS_KEY_TDP) - 1)

enum trusty_storage_mac_command {
	trusty_storage_mac_command_read = 0,
	trusty_storage_mac_command_write = 1,
	trusty_storage_mac_command_delete = 2,
};

enum trusty_storage_mac_file_index {
	trusty_storage_mac_file_index_tdp = 1,
	trusty_storage_mac_file_index_td = 2,
};

struct trusty_storage_mac_data {
	uint8_t mac[16];
	uint8_t flags;
} __packed;

struct trusty_storage_mac_request {
	enum trusty_storage_mac_command command;
	enum trusty_storage_mac_file_index index;
	struct trusty_storage_mac_data data;
} __packed;

struct trusty_storage_mac_response {
	struct trusty_storage_mac_data data;
} __packed;

static const uint8_t* trusty_storage_mac_key(enum trusty_storage_mac_file_index index) {
	switch (index) {
		case trusty_storage_mac_file_index_tdp:
			return TS_KEY_TDP;
		case trusty_storage_mac_file_index_td:
			return TS_KEY_TD;
		default:
			return NULL;
	}
}

static int trusty_storage_mac_write(const uint8_t *key, const struct trusty_storage_mac_data *data) {
	enum ec_error_list error;

	error = setvar(key, TS_KEY_LEN, (const uint8_t*) data, sizeof(struct trusty_storage_mac_data));
	if (error) {
		return error;
	}

	return 0;
}

static int trusty_storage_mac_read(const uint8_t *key, struct trusty_storage_mac_data *data) {
	const struct tuple *ptr;
	const uint8_t *value;
	struct trusty_storage_mac_data empty_data = {0};
	int ret = 0;

	ptr = getvar(key, TS_KEY_LEN);
	if (!ptr) {
		CPRINTS("%s: init trusty storage mac", __func__);
		*data = empty_data;
		return trusty_storage_mac_write(key, &empty_data);
	}

	value = tuple_val(ptr);
	memcpy(data, value, sizeof(struct trusty_storage_mac_data));
	freevar(ptr);

	return ret;
}

static int trusty_storage_mac_delete(const uint8_t *key) {
	struct trusty_storage_mac_data data;
	int ret = 0;

	ret = trusty_storage_mac_read(key, &data);
	if (ret) {
		return ret;
	}

	/* Uninit all fields except for the lower 7 flag bits */
	memset(&data.mac, 0, 16);
	data.flags = data.flags & !TS_FLAGS_MASK;

	return trusty_storage_mac_write(key, &data);
}

static enum vendor_cmd_rc process_trusty_storage_mac(enum vendor_cmd_cc code,
					   void *buf,
					   size_t input_size,
					   size_t *response_size)
{
	struct trusty_storage_mac_request *req = buf;
	struct trusty_storage_mac_response *rsp = buf;
	enum trusty_storage_mac_command command;
	const uint8_t* key;
	int ret;

	CPRINTS("%s", __func__);

	if (input_size != sizeof(struct trusty_storage_mac_request)) {
		return VENDOR_RC_INTERNAL_ERROR;
	}

	key = trusty_storage_mac_key(req->index);
	if (!key) {
		return VENDOR_RC_INTERNAL_ERROR;
	}
	command = req->command;

	if (command == trusty_storage_mac_command_write) {
		req->data.flags |= TS_FLAGS_MASK;
		ret = trusty_storage_mac_write(key, &req->data);
		if (ret) {
			return VENDOR_RC_WRITE_FLASH_FAIL;
		}
	} else if (command == trusty_storage_mac_command_delete) {
		ret = trusty_storage_mac_delete(key);
		if (ret) {
			return VENDOR_RC_INTERNAL_ERROR;
		}
	}

	ret = trusty_storage_mac_read(key, &rsp->data);
	if (ret) {
		return VENDOR_RC_READ_FLASH_FAIL;
	}
	*response_size = sizeof(struct trusty_storage_mac_response);

	return VENDOR_RC_SUCCESS;
}

DECLARE_VENDOR_COMMAND(VENDOR_CC_TRUSTY_STORAGE_MAC, process_trusty_storage_mac);

