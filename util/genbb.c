/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
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

#include "struct.h"
#include "board_binary.h"
#include "battery.h"
#include "driver/battery/max17055.h"

#define MAX_BLOB_SIZE 0xffff

#define NEXT_BLOB	0
#define LAST_BLOB	1

#if 0
/* Might be needed if the structures must be converted to little endian. */
static int is_little_endian(void)
{
	uint32_t i = 0x01234567;

	/* return 0 for big endian, 1 for little endian.*/
	return (*((uint8_t *)(&i))) == 0x67;
}
#endif

static int bb_write(FILE *bb, const void *value, uint8_t length, uint32_t *csum)
{
	int i;
	int ret;
	int len = length / 4;

	if (bb == NULL || value == NULL || csum == NULL)
		return -1;

	ret = fwrite(value, 4, len, bb);
	if (ret != len)
		return -1;

	for (i = 0; i < len; i++)
		*csum += *((uint32_t *)value + i);

	return length;
}

static int update_blob_offset(FILE *bb, int32_t backup, int32_t offset,
								uint32_t *csum)
{
	int ret;

	if (bb == NULL || csum == NULL)
		return -1;

	/* update "next blob offset* */
	ret = fseek(bb, -backup, SEEK_END);
	if (ret < 0)
		return -1;

	/* write offset */
	ret = bb_write(bb, &offset, 4, csum);
	if (ret < 0)
		return -1;

	/* return to end of file */
	ret = fseek(bb, backup, SEEK_CUR);
	if (ret < 0)
		return -1;

	return 0;
}

static int write_blob(FILE *bb, int (*blob)(FILE *bb, uint32_t *csum),
						uint32_t *csum, int is_last)
{
	int32_t len = 0;
	int ret;

	if (bb == NULL || blob == NULL || csum == NULL)
		return -1;

	/* skip over "next blob offset" */
	ret = fseek(bb, 4, SEEK_END);
	if (ret < 0)
		return -1;
	len += 4;

	ret = blob(bb, csum);
	if (ret < 0)
		return -1;
	len += ret;

	if (is_last)
		ret = update_blob_offset(bb, len, 0, csum);
	else
		ret = update_blob_offset(bb, len, len, csum);
	if (ret < 0)
		return -1;

	return len;
}

static int battery_info_blob(FILE *bb, uint32_t *csum)
{
	int len = 0;
	int size = battery_info_size;
	int ret;
	uint32_t blob_info;

	if (ptr_battery_info == NULL)
		/* battery_info not defined. So just ignore */
		return 0;

	if (size > MAX_BLOB_SIZE)
		return -1;

	/* NOTE: byte 2 is unused at this time */
	blob_info = (BATTERY_INFO << 24) | size;

	ret = bb_write(bb, &blob_info, 4, csum);
	if (ret < 0)
		return -1;

	len += ret;

	ret = bb_write(bb, (void *)ptr_battery_info, size, csum);
	if (ret < 0)
		return -1;

	len += ret;

	return len;
}

static int battery_profile_blob(FILE *bb, uint32_t *csum)
{
	int len = 0;
	int ret;
	int size = batt_profile_size;
	uint32_t blob_info;

	if (ptr_batt_profile == NULL)
		/* battery_profile not defined. So just ignore */
		return 0;

	if (size > MAX_BLOB_SIZE)
		return -1;

	/* NOTE: byte 2 is unused at this time */
	blob_info = (BATTERY_PROFILE << 24) | size;

	ret = bb_write(bb, &blob_info, 4, csum);
	if (ret < 0)
		return -1;

	len += ret;

	ret = bb_write(bb, (void *)ptr_batt_profile, size, csum);
	if (ret < 0)
		return -1;

	len += ret;

	return len;
}

static int gen_bb(FILE *bb, const char *board, int bl)
{
	int len = 0;
	uint8_t *magic = MAGIC_VER;
	uint32_t csum = 0;
	int ret;
	uint32_t rlz_code = 0xaabbccdd;

	if (bb == NULL || board == NULL)
		return -1;

	/* write magic */
	ret = bb_write(bb, magic, 8, &csum);
	if (ret < 0)
		return -1;

	/* skip full size place holder */
	ret = fseek(bb, 4, SEEK_CUR);
	if (ret < 0)
		return -1;

	/* skip checksum place holder */
	ret = fseek(bb, 4, SEEK_CUR);
	if (ret < 0)
		return -1;

	/* write RLZ_CODE */
	ret = bb_write(bb, &rlz_code, 4, &csum);
	if (ret < 0)
		return -1;
	len += ret;

	/* ***** Start writing blobs */

	/* write battery info blob */
	ret = write_blob(bb, battery_info_blob, &csum, NEXT_BLOB);
	if (ret < 0)
		return -1;
	len += ret;

	/* write battery prifile blob */
	ret = write_blob(bb, battery_profile_blob, &csum, LAST_BLOB);
	if (ret < 0)
		return -1;
	len += ret;

	/* ***** Done writing blobs */

	/* update size */
	ret = fseek(bb, 8, SEEK_SET);
	if (ret < 0)
		return -1;

	/* add magic, size, and checksum lengths */
	len += 8; /* magic length */
	len += 4; /* size length */
	len += 4; /* checksum length */

	ret = bb_write(bb, &len, 4, &csum);
	if (ret < 0)
		return -1;

	/* 2s complement */
	csum = -csum;

	/* update checksum */
	ret = bb_write(bb, &csum, 4, &csum);
	if (ret < 0)
		return -1;

	return len;
}


int main(int argc, char **argv)
{
	FILE *bb;
	int nopt;
	int ret;
	const char *out;
	const char *board;
	char *board_pad;
	DIR *bbdir;
	char *name;
	int name_size;
	int board_size;
	const char * const short_opt = "hb:o:";
	const struct option long_opts[] = {
		{ "help", 0, NULL, 'h' },
		{ "board", 1, NULL, 'b' },
		{ "out", 1, NULL, 'o' },
		{ NULL }
	};

	do {
		nopt = getopt_long(argc, argv, short_opt, long_opts, NULL);
		switch (nopt) {
		case 'h': /* -h or --help */
			printf("USAGE: %s -b <board name> -o <out directory>\n",
					argv[0]);
			return 1;

		case 'b': /* -b or --board */
			board = optarg;
			break;

		case 'o': /* -o or --out */
			out = optarg;
			break;

		case -1:
			break;

		default:
			abort();
		}
	} while (nopt != -1);

	if (out == NULL || board == NULL)
		return 1;

	/* Make sure BSON directory exists */
	bbdir = opendir(out);
	if (bbdir == NULL) {
		fprintf(stderr, "ERROR: %s directory does not exist.\n", out);
		return 1;
	}
	closedir(bbdir);

	name_size = asprintf(&name, "%s/%s.bb", out, board);
	if (name_size < 0) {
		fprintf(stderr, "ERROR: Out of memory.\n");
		return 1;
	}

	board_size = ((strlen(board) + 3) & ~4);
	board_pad = (uint8_t *)malloc(board_size);
	if (board_pad == NULL) {
		fprintf(stderr, "ERROR, Out of memory.\n");
		return 1;
	}

	sprintf(board_pad, "%s", board);

	/* Create bson */
	bb = fopen(name, "wb");
	if (bb == NULL) {
		free(name);
		return 1;
	}

	ret = gen_bb(bb, board_pad, board_size / 4);
	if (ret < 0)
		ret = 1;
	else
		ret = 0;

	fclose(bb);
	free(name);
	free(board_pad);

	return ret;
}
