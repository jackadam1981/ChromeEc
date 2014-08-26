/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lock/gec_lock.h"
#include "comm-host.h"
#include "misc_util.h"
#include "ec_sb_firmware_update.h"
#include "ec_commands.h"
#include <unistd.h>

#define DPRINTF(fmt, ...) \
	do {\
		if (debug)\
			printf("SBFW: " fmt, ## __VA_ARGS__);\
	} while (0)

/* Debug EC Smart Battery Firmwarwe Update */
static int debug;

/** Simplo Battery: Required 10 seconds delay for 1st 10 block write
 * unit in seconds
 */
static int delay_x_us = 9000000;

/** Simplo Battery: Additional delays are required after each 32-byte write
 *  unit in useconds
 */
static int delay_y_us = 40000;

static void print_battery_firmware_image_hdr(
	struct sb_fw_header *hdr)
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

static void print_info(struct sb_fw_update_info *info)
{
	printf("maker_id:0x%X hw_id:0x%X fw_ver:0x%X d_ver:0x%X\n",
		info->maker_id,
		info->hardware_id,
		info->fw_version,
		info->data_version);
	return;
}

static void print_status(struct sb_fw_update_status *sts)
{
	printf("f_maker_id:%d f_hw_id:%d f_fw_ver:%d f_permnent:%d\n",
		sts->v_fail_maker_id,
		sts->v_fail_hw_id,
		sts->v_fail_fw_version,
		sts->v_fail_permanent);
	printf("permanent failure:%d abnormal:%d fw_update:%d\n",
		sts->permanent_failure,
		sts->abnormal_condition,
		sts->fw_update_supported);
	printf("fw_update_mode:%d fw_corrupted:%d cmd_reject:%d\n",
		sts->fw_update_mode,
		sts->fw_corrupted,
		sts->cmd_reject);
	printf("invliad data:%d fw_fatal_err:%d fec_err:%d busy:%d\n",
		sts->invalid_data,
		sts->fw_fatal_error,
		sts->fec_error,
		sts->busy);
	printf("\n");
	return;
}

/* @return 1 (True) if img signature is valid */
static int check_battery_firmware_image_signature(
	struct sb_fw_header *hdr)
{
	return (hdr->signature[0] == 'B') &&
		(hdr->signature[1] == 'T') &&
		(hdr->signature[2] == 'F') &&
		(hdr->signature[3] == 'W');
}

/* @return 1 (True) if img checksum is valid. */
static int check_battery_firmware_image_checksum(
	struct sb_fw_header *hdr)
{
	int i;
	uint8_t sum = 0;
	uint8_t *img = (uint8_t *)hdr;
	img += hdr->fw_binary_offset;
	for (i = 0; i < hdr->fw_binary_size; i++)
		sum += img[i];
	sum += hdr->checksum;
	return sum == 0;
}

/* @return 1 (True) if img versions are ok to update. */
static int check_battery_firmware_image_version(
	struct sb_fw_header *hdr,
	struct sb_fw_update_info *p)
{
	return (((hdr->fw_version == 0xFFFF)
			|| (hdr->fw_version > p->fw_version)) &&
		((hdr->data_table_version == 0xFFFF)
			|| (hdr->data_table_version > p->data_version)));
}


static int check_battery_firmware_ids(
	struct sb_fw_header *hdr,
	struct sb_fw_update_info *p)
{
	return ((hdr->vendor_id == p->maker_id) &&
		(hdr->battery_type == p->hardware_id));
}

/* check_if_need_update_fw
 * @return 1 (true) if need; 0 (false) if not.
 */
static int check_if_need_update_fw(
		struct sb_fw_header *hdr,
		struct sb_fw_update_info *info)
{
	return check_battery_firmware_image_signature(hdr)

	&& check_battery_firmware_ids(hdr, info)

	&& check_battery_firmware_image_version(hdr, info)

	&& check_battery_firmware_image_checksum(hdr);
}

static int get_status(struct sb_fw_update_status *status)
{
	int rv = EC_RES_SUCCESS;
	int i = 0;
	struct ec_params_sb_fw_update *param =
		(struct ec_params_sb_fw_update *)ec_outbuf;

	struct ec_response_sb_fw_update *resp =
		(struct ec_response_sb_fw_update *)ec_inbuf;

	param->hdr.subcmd = EC_SB_FW_UPDATE_STATUS;
	do {
		rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
			param, sizeof(struct ec_sb_fw_update_header),
			resp, SB_FW_UPDATE_CMD_STATUS_SIZE);
	} while ((rv < 0) && (i++ < 3));

	if (rv < 0) {
		fprintf(stderr,
			"Firmware Update Get Status Error\n");
		return -EC_RES_ERROR;
	}
	memcpy(status, resp->status.data, SB_FW_UPDATE_CMD_STATUS_SIZE);
	return EC_RES_SUCCESS;
}

static int get_info(struct sb_fw_update_info *info)
{
	int rv = EC_RES_SUCCESS;

	struct ec_params_sb_fw_update *param =
		(struct ec_params_sb_fw_update *)ec_outbuf;

	struct ec_response_sb_fw_update *resp =
		(struct ec_response_sb_fw_update *)ec_inbuf;

	param->hdr.subcmd = EC_SB_FW_UPDATE_INFO;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		param, sizeof(struct ec_sb_fw_update_header),
		resp, SB_FW_UPDATE_CMD_INFO_SIZE);
	if (rv < 0) {
		fprintf(stderr,
			"Firmware Update Get Info Error\n");
		return -EC_RES_ERROR;
	}
	memcpy(info, resp->info.data, SB_FW_UPDATE_CMD_INFO_SIZE);
	return EC_RES_SUCCESS;
}

static int send_subcmd(int subcmd)
{
	int rv = EC_RES_SUCCESS;
	struct ec_params_sb_fw_update *param =
		(struct ec_params_sb_fw_update *)ec_outbuf;

	param->hdr.subcmd = subcmd;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		param, sizeof(struct ec_sb_fw_update_header), NULL, 0);
	if (rv < 0) {
		fprintf(stderr,
			"Firmware Update State:%d Error\n", subcmd);
		return -EC_RES_ERROR;
	}
	return EC_RES_SUCCESS;
}

static int write_block(const uint8_t *ptr, int bsize)
{
	int rv;
	struct ec_params_sb_fw_update *param =
		(struct ec_params_sb_fw_update *)ec_outbuf;

	memcpy(param->write.data, ptr, bsize);

	param->hdr.subcmd = EC_SB_FW_UPDATE_WRITE;
	rv = ec_command(EC_CMD_SB_FW_UPDATE, 0,
		param, sizeof(struct ec_params_sb_fw_update), NULL, 0);
	if (rv < 0) {
		fprintf(stderr,
		"Firmware Update Write Error offset@%p\n", ptr);
		return -EC_RES_ERROR;
	}
	return EC_RES_SUCCESS;
}

static void dump_data(uint8_t *data, int size)
{
	int i = 0;
	for (i = 0; i < size; i++) {
		if ((i%16) == 0)
			printf("\n");
		printf("%02X ", data[i]);
	}
	printf("\n");
}

int ec_sb_firmware_update(const char *fw_image_name)
{
	int rv = EC_RES_SUCCESS, i;
	int size;
	char *buf, *ptr;
	struct sb_fw_header *fw_img_hdr;
	struct sb_fw_update_status status;
	struct sb_fw_update_info info;
	int err_retry_cnt = SB_FW_UPDATE_ERROR_RETRY_CNT;
	int fec_err_retry_cnt = SB_FW_UPDATE_FEC_ERROR_RETRY_CNT;
	int busy_retry_cnt = SB_FW_UPDATE_BUSY_ERROR_RETRY_CNT;

	int bsize, step_size = SB_FW_UPDATE_CMD_WRITE_BLOCK_SIZE;

	/* Read the input file */
	DPRINTF("\n\n==> Read File:%s\n", fw_image_name);

	buf = read_file(fw_image_name, &size);
	if (!buf) {
		fprintf(stderr,
			"Firmware Update: Load Firmware Image[%s] Error\n",
			fw_image_name);
		return -1;
	}
	ptr = buf;
	fw_img_hdr = (struct sb_fw_header *)ptr;
	if (debug)
		print_battery_firmware_image_hdr(fw_img_hdr);

	if (fw_img_hdr->fw_binary_offset >= size || size < 256) {
		fprintf(stderr,
			"Firmware Update: Load Firmware Image[%s] format Error\n",
			fw_image_name);
		return -1;
	}
step0:
	if (busy_retry_cnt == 0) {
		rv = -1;
		goto error_return;
	}

	busy_retry_cnt--;

	rv = get_status(&status);
	if (rv) {
		rv = -1;
		goto error_return;
	}

	if (debug)
		print_status(&status);

	if (!((status.abnormal_condition == 0)
		&& (status.fw_update_supported == 1))) {
		fprintf(stderr, "Firmware Udpate is not supported!\n");
		goto error_return;
	}
	if (status.busy)
		goto step0;

step1:
	if (err_retry_cnt == 0)
		goto error_return;
	err_retry_cnt--;

	/*Step 1: cmd.0x37 Read Info */
	rv = get_info(&info);
	if (rv) {
		rv = -1;
		goto error_return;
	}

	if (debug)
		print_info(&info);

	rv = get_status(&status);
	if (rv) {
		rv = -1;
		goto error_return;
	}

	if (debug) {
		if (check_battery_firmware_image_signature(fw_img_hdr))
			printf("img sig ok\n");

		if (check_battery_firmware_ids(fw_img_hdr, &info))
			printf("img IDs ok\n");

		if (check_battery_firmware_image_version(fw_img_hdr, &info))
			printf("img ver ok\n");

		if (check_battery_firmware_image_checksum(fw_img_hdr))
			printf("img chk sum ok\n");
	}

	rv = check_if_need_update_fw(fw_img_hdr, &info);
	if (rv == 0) {
		printf("ERROR:Battery firmware is not valid to update!\n");
		print_info(&info);
		print_battery_firmware_image_hdr(fw_img_hdr);
		rv = EC_RES_INVALID_PARAM;
		goto error_return;
	}
step2:
	/*Step 2: cmd.0x35 write word 0x1000 */
	DPRINTF("cmd.0x35 write word 0x1000\n");
	rv = send_subcmd(EC_SB_FW_UPDATE_PREPARE);
	if (rv)
		goto error_return;

	rv = get_status(&status);
	if (rv)
		goto error_return;

	/*Step 4: cmd.0x35 Write Word 0xF000 */
	DPRINTF("cmd.0x35 write word 0xF000\n");
	rv = send_subcmd(EC_SB_FW_UPDATE_BEGIN);
	if (rv)
		goto error_return;

	usleep(500000);

	/*Step 5: cmd.0x35 Read Status */
	rv = get_status(&status);
	if (rv)
		goto error_return;
	if (status.fw_update_mode == 0)
		goto step2;

	/* Write data in chunks */
	ptr += fw_img_hdr->fw_binary_offset;
	size -= fw_img_hdr->fw_binary_offset;
	DPRINTF("Write size 0x%X total_size:0x%X\n", step_size, size);
	for (i = 0; i < size; i += step_size) {
		bsize = MIN(size - i, step_size);
step6:
		if ((i & 0x1FFF) == 0x000)
			printf("\n%X\n", i);
		else
			printf(".");

		if (fec_err_retry_cnt == 0)
			goto error_return;
		fec_err_retry_cnt--;

		/*Step 6: Write block data, 32 bytes */
		rv = write_block(ptr+i, bsize);
		if (rv)
			goto error_return;

		if (delay_x_us || delay_y_us) {
			if (i <= step_size * 10)
				usleep(delay_x_us);
			else
				usleep(delay_y_us);
		}

		/*Step 7: Check Return Status */
		do {
			rv = get_status(&status);
			if (rv) {
				printf("Offset:%X smbus error:%X\n", i, rv);
				dump_data(ptr+i, bsize);
				print_status(&status);
				goto error_return;
			}
		} while (status.busy);

		if (status.fec_error) {
			printf("Offset:%X\n", i);
			dump_data(ptr+i, bsize);
			print_status(&status);
			rv = EC_RES_ERROR;
			goto step6;
		}
		if (status.fw_fatal_error) {
			printf("Offset:%X\n", i);
			dump_data(ptr+i, bsize);
			print_status(&status);
			rv = EC_RES_ERROR;
			goto step2;
		}
		if (status.permanent_failure ||
			status.v_fail_permanent) {
			printf("Offset:%X\n", i);
			dump_data(ptr+i, bsize);
			print_status(&status);
			rv = EC_RES_ERROR;
			goto step8;
		}
		if (status.v_fail_maker_id ||
			status.v_fail_hw_id    ||
			status.v_fail_fw_version ||
			status.fw_corrupted   ||
			status.cmd_reject ||
			status.invalid_data) {
			printf("Offset:%X\n", i);
			dump_data(ptr+i, bsize);
			print_status(&status);
			rv = EC_RES_ERROR;
			goto step1;
		}

		fec_err_retry_cnt = SB_FW_UPDATE_FEC_ERROR_RETRY_CNT;
	}

step8:
	rv = send_subcmd(EC_SB_FW_UPDATE_END);
	if (rv) {
		printf("SB FW Update End Error\n");
		goto error_return;
	}

	/* Note: Sleep is required! */
	usleep(500000);

/* Pull for completion */
step9:
	rv = get_status(&status);
	if (rv) {
		printf("SB FW Update End get status Error: rv:%d\n", rv);
		goto error_return;
	}
	if ((status.fw_update_mode == 1)
		|| (status.busy == 1))
		goto step9;

error_return:

	free(buf);

	if (rv)
		printf("\n\n==> Firmware:%s Update Failed:%d\n",
			fw_image_name, rv);
	else
		printf("\n\n==> Firmware:%s Update Complete.\n",
			fw_image_name);

	return rv;
}

#define GEC_LOCK_TIMEOUT_SECS   30  /* 30 secs */

int main(int argc, char *argv[])
{
	int rv = 0;
	const char *test = "normal";
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <fw_filename> <test>\n", argv[0]);
		return -1;
	}

	if (argc >= 3)
		test = argv[2];

	if (argc >= 4)
		delay_x_us = atoi(argv[3]);

	if (argc >= 5)
		delay_y_us = atoi(argv[4]);

	if (argc >= 6)
		debug = atoi(argv[5]);

	if (acquire_gec_lock(GEC_LOCK_TIMEOUT_SECS) < 0) {
		fprintf(stderr, "Could not acquire GEC lock.\n");
		exit(1);
	}

	if (comm_init()) {
		fprintf(stderr, "Couldn't find EC\n");
		goto out;
	}

	DPRINTF("fw_filename:%s\n", argv[1]);
	rv = ec_sb_firmware_update(argv[1]);

	/* set to protect mode if not running a fw update test */
	if (strcmp(test, "test"))
		rv |= send_subcmd(EC_SB_FW_UPDATE_PROTECT);
out:
	release_gec_lock();
	return rv;
}
