/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/pd_driver.h"
#include "include/platform.h"
#include "tps6699x.h"

#include <stdio.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

struct tfu_initiate {
	uint16_t num_blocks;
	uint16_t data_block_size;
	uint16_t timeout_secs;
	uint16_t broadcast_address;
} __attribute__((__packed__));

struct tfu_download {
	uint16_t num_blocks;
	uint16_t data_block_size;
	uint16_t timeout_secs;
	uint16_t broadcast_address;
} __attribute__((__packed__));

/*
 * Complete uses custom values for switch/copy instead of true false.
 * Write these values to the register instead of true/false.
 */
#define DO_SWITCH 0xAC
#define DO_COPY 0xAC
struct tfu_complete {
	uint8_t do_switch;
	uint8_t do_copy;
} __attribute__((__packed__));

struct tfu_query {
	uint8_t bank;
	uint8_t cmd;
} __attribute__((__packed__));

struct tps6699x_tfu_query_output {
	uint8_t result;
	uint8_t tfu_state;
	uint8_t complete_image;
	uint16_t blocks_written;
	uint8_t header_block_status;
	uint8_t per_block_status[12];
	uint8_t num_header_bytes_written;
	uint8_t num_data_bytes_written;
	uint8_t num_appconfig_bytes_written;
} __attribute__((__packed__));

/* Largest chunk we want to read before writing. */
#define MAX_READ_CHUNK_SIZE 0x4000

/* Send metadata with TFUi */
#define METADATA_OFFSET 0x4
#define METADATA_LENGTH 0x8

/* Stream header with i2c_stream AFTER TFUi */
#define HEADER_BLOCK_OFFSET 0xC
#define HEADER_BLOCK_LENGTH 0x800

/* Size of fw not including appconfig and header block is at this offset. */
#define FW_SIZE_OFFSET 0x4F8

/* Stream data blocks after you write metadata with TFUd. */
#define DATA_REGION_OFFSET 0x80C
#define DATA_BLOCK_SIZE 0x4000
#define DATA_METADATA_LENGTH 0x8
#define DATA_METADATA_OFFSET_AT(block)                        \
	(((DATA_BLOCK_SIZE + DATA_METADATA_LENGTH) * block) + \
	 DATA_REGION_OFFSET)
#define DATA_AT(block) (DATA_METADATA_OFFSET_AT(block) + DATA_METADATA_LENGTH)

#define MAX_NUM_BLOCKS 12

int do_reset_pdc(struct tps6699x_device *dev)
{
	int ret;

	struct tps6699x_gaid_input gaid;
	gaid.switch_banks = GAID_SWITCH_BANK;
	gaid.copy_banks = 0;

	ret = tps6699x_4cc_run_task(dev, TI_DEFAULT_PORT, TPSCMD_GAID,
				    (uint8_t *)&gaid, sizeof(gaid), NULL, 0,
				    /*no_validation=*/true);

	if (ret < 0) {
		ELOG("Failed to run GAID: %d", ret);
	} else {
		/* Sleep for 1000ms. */
		platform_usleep(1000 * 1000);
	}

	return ret;
}

int tps6699x_reset_pdc(struct ucsi_pd_driver *pd)
{
	return do_reset_pdc((struct tps6699x_device *)pd->dev);
}

static int get_and_print_device_info(struct tps6699x_device *dev)
{
	struct tps6699x_device_info info = { 0 };
	int ret;

	ret = tps6699x_get_device_info(dev, &info);
	if (ret <= 0) {
		ELOG("Failed to get device info: %d", ret);
		return -1;
	}

	printf("Device info: %s\n", info.data);
	return 0;
}

int tps6699x_get_info(struct ucsi_pd_driver *pd)
{
	struct tps6699x_device *dev = (struct tps6699x_device *)pd->dev;
	struct tps6699x_boot_flags flags = { 0 };
	struct tps6699x_device_info info = { 0 };
	uint32_t version = 0;

	int ret = tps6699x_get_boot_flags(dev, &flags);
	if (ret <= 0) {
		ELOG("Failed to get boot flags: %d", ret);
		return -1;
	}

	ret = tps6699x_get_version(dev, &version);
	if (ret <= 0) {
		ELOG("Failed to get version: %d", ret);
		return -1;
	}

	ret = tps6699x_get_device_info(dev, &info);
	if (ret <= 0) {
		ELOG("Failed to get device info: %d", ret);
		return -1;
	}

	printf("Active bank: %u, Are Banks Valid: [%b; %b], Number of ports: %d\n",
	       ACTIVE_BANK_MASK(flags.bank_info), BANK0_VALID(flags.bank_info),
	       BANK1_VALID(flags.bank_info), NUM_PORTS_MASK(flags.port_info));
	printf("Bank 0 FW (0x%x), Bank 1 FW (0x%x)\n", flags.fw_version_bank0,
	       flags.fw_version_bank1);
	printf("Fw version: 0x%x\n", version);
	printf("Device info: %s\n", info.data);

	return 0;
}

/*
 * Read |len| bytes at |offset| in given file to |buf|.
 */
ssize_t read_file_offset(int fd, size_t offset, void *buf, size_t len)
{
	off_t seek_result;
	ssize_t read_result;

	DLOG("Reading 0x%x bytes at offset 0x%x", len, offset);

	seek_result = lseek(fd, offset, SEEK_SET);
	if (seek_result != offset) {
		ELOG("Failed to seek to %u. Result=%d", offset, seek_result);
		return -1;
	}

	DLOG("Seek to 0x%x complete and now reading.", seek_result);
	read_result = read(fd, buf, len);

	DLOG("Read 0x%x bytes", read_result);
	return read_result;
}

int get_appconfig_offsets(struct tps6699x_device *dev, int fd,
			  uint16_t num_data_blocks, size_t *metadata_offset,
			  size_t *data_block_offset)
{
	ssize_t bytes_read;
	uint32_t fw_size;

	bytes_read = read_file_offset(fd, FW_SIZE_OFFSET, (uint8_t *)&fw_size,
				      sizeof(fw_size));

	if (bytes_read < 0) {
		ELOG("Failed to read firmware size from binary: %d",
		     bytes_read);
		return -1;
	}

	// The Application Configuration is stored at the following offset
	// FirmwareImageSize (Which excludes Header and App Config) + 0x800
	// (Header Block Size)
	// + (8 (Meta Data for Each Block including Header block) * Number of
	// Data block + 1)
	// + 4 (File Identifier)
	*metadata_offset = fw_size + HEADER_BLOCK_LENGTH +
			   (DATA_METADATA_LENGTH * (num_data_blocks + 1)) +
			   METADATA_OFFSET;

	*data_block_offset = *metadata_offset + DATA_METADATA_LENGTH;

	return 0;
}

/* Download specified block to device. */
int tfud_block(struct tps6699x_device *dev, int fd, char *fbuf,
	       size_t metadata_offset, size_t data_block_offset)
{
	ssize_t bytes_read;
	int ret;
	struct tfu_download tfud;
	uint8_t rbuf[1];

	/* First read the block metadata. */
	bytes_read = read_file_offset(fd, metadata_offset, (uint8_t *)&tfud,
				      DATA_METADATA_LENGTH);

	if (bytes_read < 0 || bytes_read != DATA_METADATA_LENGTH) {
		ELOG("Failed to read block metadata. Wanted %d, got %d",
		     DATA_METADATA_LENGTH, bytes_read);
		return -1;
	}

	if (tfud.data_block_size > DATA_BLOCK_SIZE) {
		ELOG("TFUd block size too big: 0x%x (max is 0x%x)",
		     tfud.data_block_size, DATA_BLOCK_SIZE);
		return -1;
	}

	ret = tps6699x_4cc_run_task(dev, TI_DEFAULT_PORT, TPSCMD_TFUd,
				    (uint8_t *)&tfud,
				    sizeof(struct tfu_download), rbuf, 1,
				    /*no_validation=*/false);

	if (ret < 0 || rbuf[0] != 0) {
		ELOG("Failed to run TFUd. Ret=%d, rbuf[0] = %u", ret, rbuf[0]);
		return -1;
	}

	bytes_read = read_file_offset(fd, data_block_offset, fbuf,
				      tfud.data_block_size);

	if (bytes_read < 0 || bytes_read != tfud.data_block_size) {
		ELOG("Failed to read block. Wanted %d, got %d",
		     tfud.data_block_size, bytes_read);
		return -1;
	}

	ret = tps6699x_broadcast_stream(dev, tfud.broadcast_address, fbuf,
					tfud.data_block_size);

	if (ret < 0 || ret != tfud.data_block_size) {
		ELOG("Streaming data block failed. Expected to write %d but result was %d",
		     tfud.data_block_size, ret);
		return -1;
	}

	/* Wait 150ms after each data block. */
	platform_usleep(150 * 1000);

	return 0;
}

int tfuq_run(struct tps6699x_device *dev,
	     struct tps6699x_tfu_query_output *output)
{
	struct tfu_query tfuq;
	tfuq.bank = 0;
	tfuq.cmd = 0;

	return tps6699x_4cc_run_task(dev, TI_DEFAULT_PORT, TPSCMD_TFUq,
				     (uint8_t *)&tfuq, sizeof(struct tfu_query),
				     (uint8_t *)output,
				     sizeof(struct tps6699x_tfu_query_output),
				     /*no_validation=*/false);
};

int tps6699x_do_firmware_update(struct ucsi_pd_driver *pd, const char *filepath,
				int dry_run)
{
	struct tps6699x_device *dev = (struct tps6699x_device *)pd->dev;
	int fd;
	char fbuf[MAX_READ_CHUNK_SIZE];
	uint8_t rbuf[1 + sizeof(struct tps6699x_tfu_query_output)];
	ssize_t bytes_read = 0;
	int ret = 0;
	struct tfu_initiate tfui;
	struct tps6699x_tfu_query_output tfuq_out;
	size_t appconfig_metadata_offset, appconfig_data_offset;

	if (!filepath) {
		ELOG("Filepath was empty.");
		return -1;
	}

	DLOG("Fwupdate: File path is %s", filepath);
	/* Open the file descriptor */
	fd = open(filepath, O_RDONLY);
	if (fd < 0) {
		ELOG("Could not open file at %s", filepath);
		return -1;
	}

	/*
	 * Flow of operations for firmware update:
	 *   - TFUs: Start TFU process (puts device into bootloader mode)
	 *   - TFUi: Initiate firmware update. This also validates header.
	 *   - TFUd - Loop to download firmware.
	 *   - TFUc - Complete firmware update.
	 *
	 * To cancel or query current status, you can also do the following:
	 *   - TFUq: Query the TFU process
	 *   - TFUe: Cancel back to initial download state.
	 */

	/* Start TFU process. Return should be 0 in rbuf[0]. */
	ret = tps6699x_4cc_run_task(dev, TI_DEFAULT_PORT, TPSCMD_TFUs, NULL, 0,
				    NULL, 0, /*no_validation=*/true);
	if (ret < 0) {
		ELOG("Failed to run TFUs. Ret=%d", ret);
		goto reset_pdc;
	}

	/*
	 * TFUs unconditionally succeeds but needs 200ms to get into bootloader
	 * mode.
	 */
	platform_usleep(200 * 1000);

	DLOG("TFUs complete.");

	/* Read metadata header. */
	bytes_read = read_file_offset(fd, METADATA_OFFSET, (uint8_t *)&tfui,
				      METADATA_LENGTH);
	if (bytes_read < 0) {
		ELOG("Failed to read metadata. Wanted %d, got %d",
		     METADATA_LENGTH, bytes_read);
		goto cleanup;
	}

	DLOG("Sending TFUi.");

	/* Write TFUi with header. */
	ret = tps6699x_4cc_run_task(dev, TI_DEFAULT_PORT, TPSCMD_TFUi,
				    (uint8_t *)&tfui,
				    sizeof(struct tfu_initiate), rbuf, 1,
				    /*no_validation=*/false);

	if (ret < 0 || rbuf[0] != 0) {
		ELOG("Failed to run TFUi. Ret=%d, rbuf[0]=%u", ret, rbuf[0]);
		goto cleanup;
	}

	/* Read metadata buffer and stream at address given. */
	bytes_read = read_file_offset(fd, HEADER_BLOCK_OFFSET, fbuf,
				      HEADER_BLOCK_LENGTH);
	if (bytes_read < 0 || bytes_read != HEADER_BLOCK_LENGTH) {
		ELOG("Failed to read header stream. Wanted %d but got %d",
		     HEADER_BLOCK_LENGTH, bytes_read);
		goto cleanup;
	}

	DLOG("Streaming header.");

	ret = tps6699x_broadcast_stream(dev, tfui.broadcast_address, fbuf,
					HEADER_BLOCK_LENGTH);
	if (ret < 0 || ret != HEADER_BLOCK_LENGTH) {
		ELOG("Streaming header failed. Expected to write %d but result was %d",
		     HEADER_BLOCK_LENGTH, ret);
		goto cleanup;
	}

	DLOG("TFUi complete and header streamed.");

	/* Wait 200ms after streaming header to do data block. */
	platform_usleep(200 * 1000);

	/* Iterate through all image blocks. */
	for (int block = 0; block < tfui.num_blocks; ++block) {
		DLOG("Flashing block %d", block);
		tfud_block(dev, fd, fbuf, DATA_METADATA_OFFSET_AT(block),
			   DATA_AT(block));

		DLOG("Finished flashing block. Do we need to retry?");
		platform_memset(&tfuq_out, 0, sizeof(tfuq_out));
		ret = tfuq_run(dev, &tfuq_out);
		if (ret >= 0) {
			DLOG("TFUq says current block was written=%d, status = 0x%02x",
			     (tfuq_out.blocks_written & (1 << block)) ? 1 : 0,
			     tfuq_out.per_block_status[block]);
		}
	}

	DLOG("Flashing appconfig to block %d", tfui.num_blocks);
	if (get_appconfig_offsets(dev, fd, tfui.num_blocks,
				  &appconfig_metadata_offset,
				  &appconfig_data_offset) < 0) {
		ELOG("Failed to get appconfig offsets!");
		goto cleanup;
	}

	tfud_block(dev, fd, fbuf, appconfig_metadata_offset,
		   appconfig_data_offset);

	DLOG("All data blocks flashed.");

	/* Only commit changes if not dry run */
	if (!dry_run) {
		/* Finish update with a TFU copy. */
		struct tfu_complete tfuc;
		tfuc.do_switch = 0;
		tfuc.do_copy = DO_COPY;

		DLOG("Running TFUc [Switch: 0x%02x, Copy: 0x%02x]",
		     tfuc.do_switch, tfuc.do_copy);
		ret = tps6699x_4cc_run_task(
			dev, TI_DEFAULT_PORT, TPSCMD_TFUc, (uint8_t *)&tfuc,
			sizeof(tfuc), rbuf,
			1 + sizeof(struct tps6699x_tfu_query_output),
			/*no_validation=*/false);

		if (ret < 0 || rbuf[0] != 0) {
			ELOG("Failed 4cc task with result %d, rbuf[0] = %d",
			     ret, rbuf[0]);
			goto cleanup;
		}

		DLOG("TFUq bytes [Success: 0x%02x, State: 0x%02x, Complete: 0x%02x]",
		     rbuf[1], rbuf[2], rbuf[3]);

		/* Wait 1600ms for reset to complete. */
		platform_usleep(1600 * 1000);

		/* Confirm we're on the new firmware now. */
		get_and_print_device_info(dev);
	} else {
		DLOG("Exiting dry run with TFUe");
		ret = tps6699x_4cc_run_task(dev, TI_DEFAULT_PORT, TPSCMD_TFUe,
					    NULL, 0, rbuf, 1,
					    /*no_validation=*/false);
		if (ret < 0 || rbuf[0] != 0) {
			ELOG("Cleaning up resulted in ret=%d and result byte=0x%02x",
			     ret, rbuf[0]);
		}

		do_reset_pdc(dev);
		get_and_print_device_info(dev);
	}

	return 0;

cleanup:
	ret = tps6699x_4cc_run_task(dev, TI_DEFAULT_PORT, TPSCMD_TFUe, NULL, 0,
				    rbuf, 1, /*no_validation=*/false);
	ELOG("Cleaning up resulted in ret=%d and result byte=0x%02x", ret,
	     rbuf[0]);

reset_pdc:
	/* Reset and confirm we restored original firmware. */
	do_reset_pdc(dev);
	get_and_print_device_info(dev);

	return -1;
}
