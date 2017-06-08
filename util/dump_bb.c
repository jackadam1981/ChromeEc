/* Copyright 2017 The Chromium OS Authors. All rights reserved.
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
	uint32_t info;
	uint32_t size;
	int ret;
	int len = 0;
	uint8_t type;
	struct battery_info batt_info;

	do {
		/* read next blob offset */
		ret = fread(&next_blob, 1, 4, bb);
		if (ret != 4)
			return -1;
		len += ret;

		/* read info */
		ret = fread(&info, 1, 4, bb);
		if (ret != 4)
			return -1;
		len += ret;

		/* read size */
		ret = fread(&size, 1, 4, bb);
		if (ret != 4)
			return -1;
		len += ret;

		type = BLOB_TYPE(info);
		switch (type) {
		case BATTERY_INFO:
			ret = fread(&batt_info, 1,
				sizeof(struct battery_info), bb);
			if (ret != size)
				return -1;
			len += ret;

			printf("const struct battery_info:\n");
			printf("\tvoltage_max:          %d\n",
					batt_info.voltage_max);
			printf("\tvoltage_normal:       %d\n",
					batt_info.voltage_normal);
			printf("\tvoltage_min:          %d\n",
					batt_info.voltage_min);
			printf("\tprecharge_current:    %d\n",
					batt_info.precharge_current);
			printf("\tstart_charging_min_c: %d\n",
					batt_info.start_charging_min_c);
			printf("\tstart_charging_max_c: %d\n",
					batt_info.start_charging_max_c);
			printf("\tcharging_min_c:       %d\n",
					batt_info.charging_min_c);
			printf("\tcharging_max_c:       %d\n",
					batt_info.charging_max_c);
			printf("\tdischarging_min_c:    %d\n",
					batt_info.discharging_min_c);
			printf("\tdischarging_max_c:    %d\n",
					batt_info.discharging_max_c);
			printf("\n");
			break;
		case FAST_CHARGE_PARMS:
			break;
		default:
			printf("unknown structure type %x\n", type);
		}
	} while (next_blob);

	return len;
}

static int read_bb(FILE *bb)
{
	char magic[9];
	uint32_t size;
	uint32_t csum;
	char temp;
	char *name;
	int name_len;
	int ret;

	if (bb == NULL)
		return -1;

	/* read magic */
	ret = fread(magic, 1, 8, bb);
	if (ret != 8) {
		fprintf(stderr, "ERROR: Can't read from file\n");
		return -1;
	}

	magic[8] = 0;

	if (strcmp("-chrome-", magic) != 0) {
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

	name_len = 0;
	while ((temp = fgetc(bb)) != EOF) {
		if (temp == 0)
			break;
		name_len++;
	}

	ret = fseek(bb, 16, SEEK_SET);
	if (ret < 0) {
		fprintf(stderr, "ERROR: Can't read from file\n");
		return -1;
	}

	name_len = (name_len + 3) & ~4;
	name = (char *)malloc(name_len);
	if (name == NULL) {
		fprintf(stderr, "ERROR: Out of memory\n");
		return -1;
	}

	ret = fread(name, 1, name_len, bb);
	if (ret != name_len) {
		fprintf(stderr, "ERROR: Can't read from file\n");
		return -1;
	}

	printf("Board: %s\n", name);
	printf("Size:  %d\n", size);
	printf("Csum:  0x%08x\n", csum);

	free(name);

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
