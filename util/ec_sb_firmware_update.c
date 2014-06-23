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

void ec_sb_firmware_status_print(struct ec_sb_fw_update_status *status)
{
	printf("id:%X state:%X size:%X\n",
		status->hdr.fw_id,
		status->hdr.state,
		status->fw.size);
	printf("	hash_code:%X nwrite:%X\n",
		status->fw.hash_code,
		status->fw.nwrite);
}

int ec_sb_firmware_update(const char *fw_image_name)
{
	int rv = EC_RES_SUCCESS, i;
	int size;
	char *buf;
	struct ec_sb_fw_update_status *status =
		(struct ec_sb_fw_update_status *)ec_outbuf;
	struct ec_sb_fw_update_write *write =
		(struct ec_sb_fw_update_write *)ec_outbuf;
	int bsize, step_size = 2;

	/* Read the input file */
	buf = read_file(fw_image_name, &size);
	if (!buf) {
		fprintf(stderr,
			"Firmware Update: Load Firmware Image[%s] Error\n",
			fw_image_name);
		return -1;
	}

	status->hdr.state = EC_CMD_SB_FW_UPDATE_BEGIN;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
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
		write->hdr.state = EC_CMD_SB_FW_UPDATE_WRITE;
		write->size = step_size;
		write->offset = i;
		rv = ec_command(EC_CMD_SB_FW_UPDATE_WRITE, 0,
			write, sizeof(*write)+step_size, NULL, 0);
		if (rv < 0) {
			fprintf(stderr,
			"Firmware Update Write Error offset@%d\n", i);
			goto error_return;
		}
	}
	status->hdr.state = EC_CMD_SB_FW_UPDATE_END;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		status, sizeof(*status), status, sizeof(*status));
	if (rv) {
		fprintf(stderr,
			"Firmware Update End Error\n");
		goto error_return;
	}

	status->hdr.state = EC_CMD_SB_FW_UPDATE_PROTECT;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		status, sizeof(*status), status, sizeof(*status));
	if (rv) {
		fprintf(stderr,
			"Firmware Update Switch to Protection Error\n");
	}

error_return:
	free(buf);
	return rv;
}
