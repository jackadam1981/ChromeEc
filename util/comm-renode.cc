/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "comm-host.h"
#include "ec_commands.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static FILE *renode_device;

static int sum_bytes(const void *data, int length)
{
	const uint8_t *bytes = (const uint8_t *)data;
	int sum = 0;
	int i;

	for (i = 0; i < length; i++)
		sum += bytes[i];
	return sum;
}

static int ec_command_renode(int command, int version, const void *outdata,
			     int outsize, void *indata, int insize)
{
	size_t buffer_len = sizeof(struct ec_host_request) + outsize;
	uint8_t *buffer =
		(uint8_t *)calloc(1, sizeof(struct ec_host_request) + outsize);

	struct ec_host_request *header = (struct ec_host_request *)buffer;
	*header = {
		.struct_version = EC_HOST_REQUEST_VERSION,
		.checksum = 0,
		.command = (uint16_t)command,
		.command_version = (uint8_t)version,
		.reserved = 0,
		.data_len = (uint16_t)outsize,
	};
	memcpy(buffer + sizeof(struct ec_host_request), outdata, outsize);

	header->checksum = (uint8_t)(-sum_bytes(buffer, buffer_len));

	if (fwrite(buffer, 1, buffer_len, renode_device) != buffer_len) {
		fprintf(stderr, "Command write failed (%s)\n", strerror(errno));
		return 1;
	}
	free(buffer);

	struct ec_host_response response;
	size_t result_size = fread(
		&response, 1, sizeof(struct ec_host_response), renode_device);
	if (result_size != sizeof(sizeof(struct ec_host_response))) {
		fprintf(stderr,
			"Invalid header size. Expected: %zu, got: %zu\n",
			sizeof(struct ec_host_response), result_size);
		return -EC_RES_INVALID_RESPONSE;
	}

	if (response.data_len != insize) {
		fprintf(stderr,
			"Data lengths don't match. Expected: %d, got: %d\n",
			insize, response.data_len);
		return -EC_RES_INVALID_RESPONSE;
	}

	result_size = fread(indata, 1, insize, renode_device);
	if (result_size != response.data_len) {
		fprintf(stderr, "Invalid data length. Expected: %d, got: %zu",
			response.data_len, result_size);
		return -EC_RES_INVALID_RESPONSE;
	}

	return EC_RES_SUCCESS;
}

int comm_init_renode(const char *file_name)
{
	FILE *f = fopen(file_name, "r+");
	if (f == NULL) {
		fprintf(stderr,
			"Renode PTY file '%s' could not be opened! (%s)\n",
			file_name, strerror(errno));
		return errno;
	}

	renode_device = f;
	ec_command_proto = ec_command_renode;
	return 0;
}
