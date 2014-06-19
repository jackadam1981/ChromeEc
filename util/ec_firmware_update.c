/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comm-host.h"
#include "misc_util.h"

void ec_firmware_status_print(uint32_t fw_id,
		struct ec_firmware_update_status *status)
{
	printf("id:%X,%X state:%X intf:%X size:%X\n",
		fw_id,
		status->hdr.fw_id,
		status->hdr.state,
		status->fw.intf,
		status->fw.size);
	printf("	ver:%X hash_code:%X nwrite:%X\n",
		status->fw.version,
		status->fw.hash_code,
		status->fw.nwrite);
}

int ec_firmware_list(void)
{
	int rv = EC_RES_SUCCESS, i;
	struct ec_firmware_update_list *list =
		(struct ec_firmware_update_list *)ec_outbuf;

	rv = ec_command(EC_CMD_FIRMWARE_UPDATE_LIST, 0,
		list, sizeof(*list), list, sizeof(*list));

	for (i = 0; i < list->num; i++)
		ec_firmware_status_print(i, &list->fw_status[i]);
	return rv;
}

int ec_firmware_update(uint32_t fw_id, const char *fw_image_name)
{
	int rv = EC_RES_SUCCESS, i;
	int size;
	int intf;
	char *buf;
	struct ec_firmware_update_status *status =
		(struct ec_firmware_update_status *)ec_outbuf;
	struct ec_firmware_update_write *write =
		(struct ec_firmware_update_write *)ec_outbuf;
	int bsize, step_size = 2;

	/* Read the input file */
	buf = read_file(fw_image_name, &size);
	if (!buf) {
		fprintf(stderr,
			"Firmware Update: Load Firmware Image[%s] Error\n",
			fw_image_name);
		return -1;
	}

	status->hdr.state  = EC_CMD_FIRMWARE_UPDATE_STATUS;
	status->hdr.fw_id = fw_id;
	rv = ec_command(EC_CMD_FIRMWARE_UPDATE, 0,
		status, sizeof(*status), status, sizeof(*status));
	if (rv) {
		fprintf(stderr,
			"Firmware Update Query Status Error\n");
		goto error_return;
	}

	intf = status->fw.intf;
	status->hdr.state =  EC_CMD_FIRMWARE_UPDATE_BEGIN;
	status->hdr.fw_id = fw_id;
	rv = ec_command(EC_CMD_FIRMWARE_UPDATE, 0,
		status, sizeof(*status), status, sizeof(*status));
	if (rv) {
		fprintf(stderr,
			"Firmware Update Start Error\n");
		goto error_return;
	}

	/* Write data in chunks */
	printf("Write size %d...\n", step_size);
	for (i = 0; i < size; i += step_size) {
		bsize = MIN(size - i, step_size);
		memcpy(&write->data[0], buf + i, bsize);
		write->hdr.state = EC_CMD_FIRMWARE_UPDATE_WRITE;
		write->hdr.fw_id = fw_id;
		write->size = step_size;
		write->offset = i;
		write->intf = intf;
		rv = ec_command(EC_CMD_FIRMWARE_UPDATE_WRITE, 0,
			write, sizeof(*write)+step_size, NULL, 0);
		if (rv < 0) {
			fprintf(stderr,
			"Firmware Update Write Error offset@%d\n", i);
			goto error_return;
		}
	}
	status->hdr.state = EC_CMD_FIRMWARE_UPDATE_END;
	rv = ec_command(EC_CMD_FIRMWARE_UPDATE, 0,
		status, sizeof(*status), status, sizeof(*status));
	if (rv) {
		fprintf(stderr,
			"Firmware Update End Error\n");
		goto error_return;
	}

	status->hdr.state = EC_CMD_FIRMWARE_UPDATE_PROTECT;
	rv = ec_command(EC_CMD_FIRMWARE_UPDATE, 0,
		status, sizeof(*status), status, sizeof(*status));
	if (rv) {
		fprintf(stderr,
			"Firmware Update Switch to Protection Error\n");
	}

error_return:
	free(buf);
	return rv;
}
