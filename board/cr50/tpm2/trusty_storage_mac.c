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
#include "nvmem_vars.h"
#include "system.h"
#include "trusty_storage_mac.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

/* Mask used to get/set valid mac bit */
#define TS_FLAGS_MASK 0x80

/* Key names for Trusty storage mac nvmem_vars */
const char g_trusty_tdp_var_name[] = {'T', 'S', '-', 'T', 'D', 'P'};
const char g_trusty_td_var_name[]  = {'T', 'S', '-', 'T', 'D', '-'};
const size_t g_trusty_key_len = sizeof(g_trusty_tdp_var_name);
BUILD_ASSERT(sizeof(g_trusty_tdp_var_name) == sizeof(g_trusty_td_var_name));

/* Possible command requests */
enum trusty_storage_mac_command {
	trusty_storage_mac_command_read = 0,
	trusty_storage_mac_command_write = 1,
	trusty_storage_mac_command_delete = 2,
};

/* Supported mac value files */
enum trusty_storage_mac_file_index {
	trusty_storage_mac_file_index_tdp = 1,
	trusty_storage_mac_file_index_td = 2,
};

/* Storage data */
struct trusty_storage_mac_data {
	uint8_t mac[16];
	uint8_t flags;
} __packed;

/* Request message format */
struct trusty_storage_mac_request {
	enum trusty_storage_mac_command command;
	enum trusty_storage_mac_file_index index;
	struct trusty_storage_mac_data data;
} __packed;

/* Response message format */
struct trusty_storage_mac_response {
	struct trusty_storage_mac_data data;
} __packed;

static const uint8_t *trusty_storage_mac_key(
	enum trusty_storage_mac_file_index index)
{
	switch (index) {
	case trusty_storage_mac_file_index_tdp:
		return g_trusty_tdp_var_name;
	case trusty_storage_mac_file_index_td:
		return g_trusty_td_var_name;
	default:
		return NULL;
	}
}

/* Helper function to write the storage mac value. */
static int trusty_storage_mac_write(const uint8_t *key,
				    const struct trusty_storage_mac_data *data)
{
	enum ec_error_list error;

	error = setvar(key, g_trusty_key_len, (const uint8_t *)data,
		       sizeof(struct trusty_storage_mac_data));
	if (error)
		return error;

	return 0;
}

/* Helper function to read the storage mac value. */
static int trusty_storage_mac_read(const uint8_t *key,
				   struct trusty_storage_mac_data *data)
{
	const struct tuple *ptr;
	const uint8_t *value;
	struct trusty_storage_mac_data empty_data = { 0 };
	int ret = 0;

	ptr = getvar(key, g_trusty_key_len);
	if (!ptr) {
		*data = empty_data;
		return trusty_storage_mac_write(key, &empty_data);
	}

	value = tuple_val(ptr);
	memcpy(data, value, sizeof(struct trusty_storage_mac_data));
	freevar(ptr);

	return ret;
}

/* Helper function to delete a mac value by unsetting the flags mask and zeroing
 * the mac. The lower seven bits of the flags should be preserved.
 */
static int trusty_storage_mac_delete(const uint8_t *key)
{
	struct trusty_storage_mac_data data;
	int ret = 0;

	ret = trusty_storage_mac_read(key, &data);
	if (ret)
		return ret;

	/* Uninit all fields except for the lower 7 flag bits */
	memset(&data.mac, 0, 16);
	data.flags = data.flags & ~TS_FLAGS_MASK;

	return trusty_storage_mac_write(key, &data);
}

/* Helper function to check if a mac exists. */
static bool trusty_storage_mac_exists(const uint8_t *key)
{
	const struct tuple *ptr;
	bool exists = false;

	ptr = getvar(key, g_trusty_key_len);
	exists = !!ptr;
	freevar(ptr);
	return exists;

}

/* Delete the storage macs. */
int trusty_storage_mac_handle_owner_clear(void)
{
	const uint8_t *key;
	int ret_tdp = EC_SUCCESS;
	int ret_td = EC_SUCCESS;

	key = trusty_storage_mac_key(trusty_storage_mac_file_index_tdp);
	if (trusty_storage_mac_exists(key))
		ret_tdp = trusty_storage_mac_delete(key);
	key = trusty_storage_mac_key(trusty_storage_mac_file_index_td);
	if (trusty_storage_mac_exists(key))
		ret_td = trusty_storage_mac_delete(key);
	return ret_tdp != EC_SUCCESS ? ret_tdp : ret_td;
}

static enum vendor_cmd_rc process_trusty_storage_mac(enum vendor_cmd_cc code,
						     void *buf,
						     size_t input_size,
						     size_t *response_size)
{
	struct trusty_storage_mac_request *req = buf;
	struct trusty_storage_mac_response *rsp = buf;
	enum trusty_storage_mac_command command;
	const uint8_t *key;
	int ret;

	if (input_size != sizeof(struct trusty_storage_mac_request))
		return VENDOR_RC_BOGUS_ARGS;

	key = trusty_storage_mac_key(req->index);
	if (!key)
		return VENDOR_RC_BOGUS_ARGS;
	command = req->command;

	if (command == trusty_storage_mac_command_write) {
		req->data.flags |= TS_FLAGS_MASK;
		ret = trusty_storage_mac_write(key, &req->data);
		if (ret)
			return VENDOR_RC_WRITE_FLASH_FAIL;
	} else if (command == trusty_storage_mac_command_delete) {
		ret = trusty_storage_mac_delete(key);
		if (ret)
			return VENDOR_RC_INTERNAL_ERROR;
	}

	ret = trusty_storage_mac_read(key, &rsp->data);
	if (ret)
		return VENDOR_RC_READ_FLASH_FAIL;
	*response_size = sizeof(struct trusty_storage_mac_response);

	return VENDOR_RC_SUCCESS;
}

DECLARE_VENDOR_COMMAND(VENDOR_CC_TRUSTY_STORAGE_MAC,
		       process_trusty_storage_mac);
