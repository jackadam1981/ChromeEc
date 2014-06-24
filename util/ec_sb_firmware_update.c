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
#include <unistd.h>

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

static void ec_sb_fw_update_print_info(struct ec_sb_fw_update_info *p)
{
	printf("\ninfo state:0x%X fw_id:0x%X\n",
		p->hdr.state,
		p->hdr.fw_id);
	printf("maker_id:0x%X hw_id:0x%X fw_ver:0x%X d_ver:0x%X\n",
		p->info.maker_id,
		p->info.hardware_id,
		p->info.fw_version,
		p->info.data_version);
	return;
}

static void ec_sb_fw_update_print_status(struct ec_sb_fw_update_status *p)
{
	printf("\nstatus state:0x%X fw_id:0x%X\n",
		p->hdr.state,
		p->hdr.fw_id);
	printf("f_maker_id:%d f_hw_id:%d f_fw_ver:%d f_permnent:%d\n",
		p->status.v_fail_maker_id,
		p->status.v_fail_hw_id,
		p->status.v_fail_fw_version,
		p->status.v_fail_permanent);
	printf("permanent failure:%d abnormal:%d fw_update:%d\n",
		p->status.permanent_failure,
		p->status.abnormal_condition,
		p->status.fw_update_supported);
	printf("fw_update_mode:%d fw_corrupted:%d cmd_reject:%d\n",
		p->status.fw_update_mode,
		p->status.fw_corrupted,
		p->status.cmd_reject);
	printf("invliad data:%d fw_fatal_err:%d fec_err:%d busy:%d\n",
		p->status.invalid_data,
		p->status.fw_fatal_error,
		p->status.fec_error,
		p->status.busy);
	return;
}

int ec_sb_fw_get_status(struct ec_sb_fw_update_status *status)
{
	int rv = EC_RES_SUCCESS;
	struct ec_sb_fw_update_header *hdr =
		(struct ec_sb_fw_update_header *)ec_outbuf;

	hdr->state = EC_CMD_SB_FW_UPDATE_STATUS;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		hdr, sizeof(*hdr), status, sizeof(*status));
	if (rv < 0) {
		fprintf(stderr,
			"Firmware Update Get Status Error\n");
		return -EC_RES_ERROR;
	}
	printf("cmd:%X ok:%d\n", hdr->state, rv);
	ec_sb_fw_update_print_status(status);
	return EC_RES_SUCCESS;
}

int ec_sb_fw_get_info(struct ec_sb_fw_update_info *info)
{
	int rv = EC_RES_SUCCESS;
	struct ec_sb_fw_update_header *hdr =
		(struct ec_sb_fw_update_header *)ec_outbuf;

	hdr->state = EC_CMD_SB_FW_UPDATE_INFO;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		hdr, sizeof(*hdr), info, sizeof(*info));
	if (rv < 0) {
		fprintf(stderr,
			"Firmware Update Get Info Error\n");
		return -EC_RES_ERROR;
	}
	printf("cmd:%X ok:%d\n", hdr->state, rv);
	ec_sb_fw_update_print_info(info);
	return EC_RES_SUCCESS;
}

int ec_sb_fw_update_subcmd(int state)
{
	int rv = EC_RES_SUCCESS;
	struct ec_sb_fw_update_header *hdr =
		(struct ec_sb_fw_update_header *)ec_outbuf;

	hdr->state = state;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		hdr, sizeof(*hdr), NULL, 0);
	if (rv < 0) {
		fprintf(stderr,
			"Firmware Update State:%d Error\n", state);
		return -EC_RES_ERROR;
	}
	printf("cmd:%X ok:%d\n", hdr->state, rv);
	return EC_RES_SUCCESS;
}

int ec_sb_firmware_update(const char *fw_image_name)
{
	int rv = EC_RES_SUCCESS, i;
	int size;
	char *buf;
	struct smart_battery_fw_header *fw_img_hdr;
	struct ec_sb_fw_update_status status;
	struct ec_sb_fw_update_info info;
	int err_retry_cnt = SB_FW_UPDATE_ERROR_RETRY_CNT;
	int fec_err_retry_cnt = SB_FW_UPDATE_FEC_ERROR_RETRY_CNT;
	int busy_retry_cnt = SB_FW_UPDATE_BUSY_ERROR_RETRY_CNT;

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

step0:
	printf("step0: %d\n", busy_retry_cnt);
	busy_retry_cnt--;
	if (busy_retry_cnt == 0)
		goto error_return;

	/*Step 0: cmd.0x35 Read Word */
	rv = ec_sb_fw_get_status(&status);
	if (rv)
		goto error_return;
	if (!((status.status.abnormal_condition == 0)
		&& (status.status.fw_update_supported == 1))) {
		fprintf(stderr, "Firmware Udpate is not supported!\n");
		goto error_return;
	}
	if (status.status.busy)
		goto step0;

step1:
	printf("step1: %d\n", err_retry_cnt);
	err_retry_cnt--;
	if (err_retry_cnt == 0)
		goto error_return;

	/*Step 1: cmd.0x37 Read Info */
	rv = ec_sb_fw_get_info(&info);
	if (rv)
		goto error_return;

step2:
	/*Step 2, 3, 4: cmd.0x35 Write Word 0x1000 & 0xF000 */
	rv = ec_sb_fw_update_subcmd(EC_CMD_SB_FW_UPDATE_BEGIN);
	if (rv)
		goto error_return;

	/*Step 5: cmd.0x35 Read Status */
	sleep(1);
	rv = ec_sb_fw_get_status(&status);
	if (rv)
		goto error_return;
	if (status.status.fw_update_mode == 0)
		goto step2;

	/* Write data in chunks */
	printf("Write size 0x%X total_size:0x%X\n", step_size, size);
	for (i = 0; i < size; i += step_size) {
		bsize = MIN(size - i, step_size);
		memcpy(&write->data[0], buf + i, bsize);
		write->size = step_size;
		write->offset = i;
step6:
		printf("write:%p sz:0x%lX offset:0x%X retry:%d\n",
			write, sizeof(*write), i, fec_err_retry_cnt);
		fec_err_retry_cnt--;
		if (fec_err_retry_cnt == 0)
			goto error_return;

		/*Step 6: Write block data, 32 bytes */
		write->hdr.state = EC_CMD_SB_FW_UPDATE_WRITE;
		rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
			write, sizeof(*write), &status, sizeof(status));
		if (rv < 0) {
			fprintf(stderr,
			"Firmware Update Write Error offset@%d\n", i);
			goto error_return;
		}

		sleep(1);

		/*Step 7: Check Return Status */
		rv = ec_sb_fw_get_status(&status);
		ec_sb_fw_update_print_status(&status);
		if (rv)
			goto error_return;

		if (status.status.fec_error)
			goto step6;
		if (status.status.fw_fatal_error)
			goto step2;
		if (status.status.permanent_failure ||
			status.status.v_fail_permanent)
			goto step8;
		if (status.status.v_fail_maker_id ||
			status.status.v_fail_hw_id    ||
			status.status.v_fail_fw_version ||
			status.status.fw_corrupted   ||
			status.status.cmd_reject     ||
			status.status.invalid_data)
			goto step1;

		fec_err_retry_cnt = SB_FW_UPDATE_FEC_ERROR_RETRY_CNT;
	}

step8:
	rv = ec_sb_fw_update_subcmd(EC_CMD_SB_FW_UPDATE_END);
	if (rv)
		goto error_return;

#if 0
	rv = ec_sb_fw_update_subcmd(EC_CMD_SB_FW_UPDATE_PROTECT);
#endif
error_return:
	free(buf);
	return rv;
}
