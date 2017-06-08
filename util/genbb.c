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

#include "struct.h"
#include "board_binary.h"
#include "battery.h"

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

static int bb_write(FILE *bb, const void *value, uint8_t len, uint32_t *csum)
{
	int i;
	int ret;

	if (bb == NULL || value == NULL || csum == NULL)
		return -1;

	ret = fwrite(value, 4, len, bb);
	if (ret != len)
		return -1;

	for (i = 0; i < len; i++)
		*csum += *((uint32_t *)value + i);

	return len * 4;
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
	ret = bb_write(bb, &offset, 1, csum);
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

	if (!is_last)
		ret = update_blob_offset(bb, len, len - 4, csum);
	else
		ret = update_blob_offset(bb, len, 0, csum);
	if (ret < 0)
		return -1;

	return len;
}

static int battery_info_blob(FILE *bb, uint32_t *csum)
{
	int len = 0;
	int size;
	int ret;
	uint32_t blob_info = BATTERY_INFO;

	if (ptr_info == NULL)
		/* battery_info not defined. So just ignore */
		return 0;

	size = sizeof(struct battery_info);

	ret = bb_write(bb, &blob_info, 1, csum);
	if (ret < 0)
		return -1;
	len += ret;

	ret = bb_write(bb, &size, 1, csum);
	if (ret < 0)
		return -1;
	len += ret;

	ret = bb_write(bb, (void *)&info, size / 4, csum);
	if (ret < 0)
		return -1;

	len += ret;

	return len;
}

static int gen_bb(FILE *bb, const char *board, int bl)
{
	int i;
	int len = 0;
	uint8_t *magic = "-chrome-";
	uint32_t csum = 0;
	int ret;

	if (bb == NULL || board == NULL)
		return -1;

	/* write magic */
	ret = bb_write(bb, magic, 2, &csum);
	if (ret < 0)
		return -1;

	/* skip size place holder */
	ret = fseek(bb, 4, SEEK_CUR);
	if (ret < 0)
		return -1;

	/* skip checksum place holder */
	ret = fseek(bb, 4, SEEK_CUR);
	if (ret < 0)
		return -1;

	/* write board name */
	for (i = 0; i < bl; i++) {
		ret = bb_write(bb, (uint32_t *)board + i, 1, &csum);
		if (ret < 0)
			return -1;
		len += ret;
	}

	/* *** Start writing blobs */

	/* write battery info blob */
	ret = write_blob(bb, battery_info_blob, &csum, LAST_BLOB);
	if (ret < 0)
		return -1;
	len += ret;

	/* *** Done writing blobs */

	/* update size */
	ret = fseek(bb, 8, SEEK_SET);
	if (ret < 0)
		return -1;

	/* add magic, size, and checksum lengths */
	len += 8; /* magic length */
	len += 4; /* size length */
	len += 4; /* checksum length */

	ret = bb_write(bb, &len, 1, &csum);
	if (ret < 0)
		return -1;

	/* 2s complement */
	csum = -csum;

	/* update checksum */
	ret = bb_write(bb, &csum, 1, &csum);
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
