/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define _GNU_SOURCE /* for asprintf */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <dirent.h>
#include <stdint.h>
#include <limits.h>

#include "board_binary.h"
#include "battery.h"
#include "driver/battery/max17055.h"

#if 0
static int is_little_endian(void)
{
	uint32_t i = 0x01234567;

	/* return 0 for big endian, 1 for little endian. */
	return (*((uint8_t *)(&i))) == 0x67;
}
#endif

static int checksum(FILE *bb)
{
	uint32_t csum = 0;
	uint32_t value = 0;
	int ret;

	if (bb == NULL)
		return -1;

	do {
		ret = fread(&value, 4, 1, bb);
		if (ret != 1) {
			if (feof(bb))
				break;
			return -1;
		}
		csum += value;
	} while (ret > 0);

	return -csum;
}

static int print_blobs(FILE *bb)
{
	uint32_t next_blob;
	uint32_t blob_info;
	uint16_t size;
	int ret;
	int idx;
	int len = 0;
	uint8_t type;
	struct battery_info batt_info;
	struct max17055_batt_profile batt_profile;

	do {
		/* read next blob offset */
		ret = fread(&next_blob, 1, 4, bb);
		if (ret != 4)
			return -1;
		len += ret;

		/* read blob info */
		ret = fread(&blob_info, 1, 4, bb);
		if (ret != 4)
			return -1;

		len += ret;

		type = BLOB_TYPE(blob_info);
		size = BLOB_SIZE(blob_info);

		switch (type) {
		case BATTERY_INFO:
			idx = 0;
			while (size >= sizeof(struct battery_info)) {
				ret = fread(&batt_info, 1,
					sizeof(struct battery_info), bb);

				len += ret;
				size -= sizeof(struct battery_info);

				printf("const struct battery_info[%d]:\n",
						idx++);
				printf("\tvoltage_max:\t %d\n",
						batt_info.voltage_max);
				printf("\tvoltage_normal:\t %d\n",
						batt_info.voltage_normal);
				printf("\tvoltage_min:\t %d\n",
						batt_info.voltage_min);
				printf("\tprecharge_current:\t %d\n",
						batt_info.precharge_current);
				printf("\tstart_charging_min_c:\t %d\n",
						batt_info.start_charging_min_c);
				printf("\tstart_charging_max_c:\t %d\n",
						batt_info.start_charging_max_c);
				printf("\tcharging_min_c:\t %d\n",
						batt_info.charging_min_c);
				printf("\tcharging_max_c:\t %d\n",
						batt_info.charging_max_c);
				printf("\tdischarging_min_c:\t %d\n",
						batt_info.discharging_min_c);
				printf("\tdischarging_max_c:\t %d\n",
						batt_info.discharging_max_c);
				printf("\n");
			}
			break;
		case BATTERY_PROFILE:
			idx = 0;
			while (size >= sizeof(struct max17055_batt_profile)) {
				ret = fread(&batt_profile, 1,
				sizeof(struct max17055_batt_profile), bb);
				len += ret;
				size -= sizeof(struct battery_info);

				printf("const struct battery_profile[%d]:\n",
						idx++);
				printf("\tdesign_cap:\t %d\n",
						batt_profile.design_cap);
				printf("\tichg_term:\t %d\n",
						batt_profile.ichg_term);
				printf("\tv_empty_detect:\t %d\n",
						batt_profile.v_empty_detect);
				printf("\tdpacc:\t %d\n",
						batt_profile.dpacc);
				printf("\tlearn_cfg:\t %d\n",
						batt_profile.learn_cfg);
				printf("\trcomp0:\t %d\n",
						batt_profile.rcomp0);
				printf("\ttempco:\t %d\n",
						batt_profile.tempco);
				printf("\tqr_table00:\t %d\n",
						batt_profile.qr_table00);
				printf("\tqr_table10:\t %d\n",
						batt_profile.qr_table10);
				printf("\tqr_table20:\t %d\n",
						batt_profile.qr_table20);
				printf("\tqr_table30:\t %d\n",
						batt_profile.qr_table30);
				printf("\tis_ez_config:\t %d\n",
						batt_profile.is_ez_config);
				printf("\n");
			}
			break;
		case FAST_CHARGE_PARMS:
			break;
		default:
			printf("unknown structure type %x\n", type);
		}

		printf("\n");
	} while (next_blob);

	return len;
}

static int read_bb(FILE *bb)
{
	uint8_t magic[8];
	uint32_t size;
	uint32_t csum;
	uint32_t rlz;
	int ret;

	if (bb == NULL)
		return -1;

	/* read magic */
	ret = fread(magic, 1, 8, bb);
	if (ret != 8) {
		fprintf(stderr, "ERROR: Can't read from file\n");
		return -1;
	}

	if (memcmp(MAGIC_VER, magic, 8) != 0) {
		fprintf(stderr, "ERROR: Incorrect magic for board binary\n");
		return -1;
	}

	ret = fseek(bb, 0, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "ERROR: Can't read from file\n");
		return -1;
	}

	/* test checksum */
	if (checksum(bb) != 0) {
		fprintf(stderr, "ERROR: Checksum mismatch\n");
		return -1;
	}

	ret = fseek(bb, 8, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "ERROR: Can't read from file\n");
		return -1;
	}

	/* read board binary size */
	ret = fread(&size, 4, 1, bb);
	if (ret != 1) {
		fprintf(stderr, "ERROR: Can't read from file\n");
		return -1;
	}

	/* read board binary checksum */
	ret = fread(&csum, 4, 1, bb);
	if (ret != 1) {
		fprintf(stderr, "ERROR: Can't read from file\n");
		return -1;
	}

	/* read rlz_code */
	ret = fread(&rlz, 4, 1, bb);
	if (ret != 1) {
		fprintf(stderr, "ERROR: Can't read from file\n");
		return -1;
	}

	print_blobs(bb);

	return 1;
}

int main(int argc, char **argv)
{
	FILE *bb;
	int ret;

	if (argc < 2)
		return 1;

	/* Open bb */
	bb = fopen(argv[1], "rb");
	if (bb == NULL)
		return 1;

	ret = read_bb(bb);
	if (ret < 0)
		ret = 1;
	else
		ret = 0;

	fclose(bb);

	return ret;
}
