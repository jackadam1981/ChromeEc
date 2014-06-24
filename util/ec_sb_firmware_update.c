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
#include "ec_sb_firmware_update.h"

void print_battery_firmware_image_hdr(
	struct smart_battery_fw_header *hdr)
{
	printf("%c%c%c%c hdr_ver:%04X major_minor:%04X\n",
		hdr->signature[0],
		hdr->signature[1],
		hdr->signature[2],
		hdr->signature[3],
		hdr->hdr_version, hdr->pkg_version_major_minor);

	printf("vendor_id:%04X battery_type:%04X fw_ver:%04X tbl_ver:%04X\n",
		hdr->vendor_id, hdr->battery_type, hdr->fw_version,
		hdr->data_table_version);

	printf("bin off:%08X size:%08X chk_sum:%02X\n",
		hdr->fw_binary_offset, hdr->fw_binary_size, hdr->checksum);
}

int ec_sb_firmware_update(const char *fw_image_name)
{
	int rv = EC_RES_SUCCESS, i;
	int size;
	char *buf;
	struct smart_battery_fw_header *fw_img_hdr;

	struct ec_sb_fw_update_header *hdr =
		(struct ec_sb_fw_update_header *)ec_outbuf;

	struct ec_sb_fw_update_info *info =
		(struct ec_sb_fw_update_info *)ec_outbuf;

	struct ec_sb_fw_update_status *status =
		(struct ec_sb_fw_update_status *)ec_outbuf;

	struct ec_sb_fw_update_write *write =
		(struct ec_sb_fw_update_write *)ec_outbuf;

	int bsize, step_size = SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE;

	/* Read the input file */
	printf("\n==> Read File:%s\n", fw_image_name);
	buf = read_file(fw_image_name, &size);
	if (!buf) {
		fprintf(stderr,
			"Firmware Update: Load Firmware Image[%s] Error\n",
			fw_image_name);
		return -1;
	}

	fw_img_hdr = (struct smart_battery_fw_header *)buf;
	print_battery_firmware_image_hdr(fw_img_hdr);

	hdr->state = EC_CMD_SB_FW_UPDATE_INFO;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		hdr, sizeof(*hdr), info, sizeof(*info));
	if (rv < 0) {
		fprintf(stderr,
			"Firmware Update Get Info Error\n");
		goto error_return;
	}
	printf("cmd:%X ok:%d\n", hdr->state, rv);

	hdr->state = EC_CMD_SB_FW_UPDATE_STATUS;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		hdr, sizeof(*hdr), status, sizeof(*status));
	if (rv < 0) {
		fprintf(stderr,
			"Firmware Update Get Status Error\n");
		goto error_return;
	}
	printf("cmd:%X ok:%d\n", hdr->state, rv);

	/* Remove me to debug firmware update interface */
	printf("Please remove me %s:%d to debug firmware update\n",
		__FILE__, __LINE__);
goto error_return;

	hdr->state = EC_CMD_SB_FW_UPDATE_BEGIN;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		hdr, sizeof(*hdr), NULL, 0);
	if (rv < 0) {
		fprintf(stderr,
			"Firmware Update Start Error\n");
		goto error_return;
	}
	printf("cmd:%X ok:%d\n", hdr->state, rv);

	hdr->state = EC_CMD_SB_FW_UPDATE_STATUS;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		hdr, sizeof(*hdr), status, sizeof(*status));
	if (rv < 0) {
		fprintf(stderr,
			"Firmware Update Get Status Error\n");
		goto error_return;
	}
	printf("cmd:%X ok:%d\n", hdr->state, rv);

	/* Write data in chunks */
	printf("Write size 0x%X total_size:0x%X\n", step_size, size);
	for (i = 0; i < size; i += step_size) {
		bsize = MIN(size - i, step_size);
		memcpy(&write->data[0], buf + i, bsize);
		write->hdr.state = EC_CMD_SB_FW_UPDATE_WRITE;
		write->size = step_size;
		write->offset = i;
		printf("write:%p sz:0x%lX\n", write, sizeof(*write));
		rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
			write, sizeof(*write), NULL, 0);
		if (rv < 0) {
			fprintf(stderr,
			"Firmware Update Write Error offset@%d\n", i);
			goto error_return;
		}
		printf("cmd:%X ok:%d\n", write->hdr.state, rv);

		hdr->state = EC_CMD_SB_FW_UPDATE_STATUS;
		rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
			hdr, sizeof(*hdr), status, sizeof(*status));
		if (rv < 0) {
			fprintf(stderr,
				"Fw Update Get Status Error off@%d\n", i);
			goto error_return;
		}
		printf("cmd:%X ok:%d\n", hdr->state, rv);
	}

	hdr->state = EC_CMD_SB_FW_UPDATE_END;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		hdr, sizeof(*hdr), NULL, 0);
	if (rv < 0) {
		fprintf(stderr,
			"Firmware Update End Error\n");
		goto error_return;
	}
	printf("cmd:%X ok:%d\n", hdr->state, rv);

	hdr->state = EC_CMD_SB_FW_UPDATE_PROTECT;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		hdr, sizeof(*hdr), NULL, 0);
	if (rv < 0) {
		fprintf(stderr,
			"Firmware Update Switch to Protection Error\n");
	}
	printf("cmd:%X ok:%d\n", hdr->state, rv);

error_return:
	free(buf);
	return rv;
}
