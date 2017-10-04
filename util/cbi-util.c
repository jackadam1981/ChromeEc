/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common boot flow utility
 */

#include <errno.h>
#include <dirent.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "cbi.h"
#include "crc8.h"

/* Command line options */
enum {
	/* mode options */
	OPT_MODE_NONE,
	OPT_MODE_CREATE,
	OPT_MODE_SHOW,
	OPT_BOARD_VERSION,
	OPT_OEM_ID,
	OPT_SKU_ID,
	OPT_SIZE,
	OPT_ERASE_BYTE,
	OPT_SHOW_ALL,
	OPT_HELP,
};

static const struct option long_opts[] = {
	{"create", 1, 0, OPT_MODE_CREATE},
	{"show", 1, 0, OPT_MODE_SHOW},
	{"board_version", 1, 0, OPT_BOARD_VERSION},
	{"oem_id", 1, 0, OPT_OEM_ID},
	{"sku_id", 1, 0, OPT_SKU_ID},
	{"size", 1, 0, OPT_SIZE},
	{"erase_byte", 1, 0, OPT_ERASE_BYTE},
	{"all", 0, 0, OPT_SHOW_ALL},
	{"help", 0, 0, OPT_HELP},
	{NULL, 0, 0, 0}
};

static int write_file(const char *filename, const char *buf, int size)
{
	FILE *f;
	int i;

	/* Write to file */
	f = fopen(filename, "wb");
	if (!f) {
		perror("Error opening output file");
		return -1;
	}
	i = fwrite(buf, 1, size, f);
	fclose(f);
	if (i != size) {
		perror("Error writing to file");
		return -1;
	}

	return 0;
}

static uint8_t *read_file(const char *filename, uint32_t *size_ptr)
{
	FILE *f;
	uint8_t *buf;
	long size;

	*size_ptr = 0;

	f = fopen(filename, "rb");
	if (!f) {
		fprintf(stderr, "Unable to open file %s\n", filename);
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	rewind(f);

	if (size < 0 || size > UINT32_MAX) {
		fclose(f);
		return NULL;
	}

	buf = malloc(size);
	if (!buf) {
		fclose(f);
		return NULL;
	}

	if (1 != fread(buf, size, 1, f)) {
		fprintf(stderr, "Unable to read file %s\n", filename);
		fclose(f);
		free(buf);
		return NULL;
	}

	fclose(f);

	*size_ptr = size;
	return buf;
}

static int cbi_crc8(const struct board_info *bi)
{
	return crc8((uint8_t *)&bi->head.crc + 1, bi->head.total_size - 4);
}

/**
 * Create a new BDB
 *
 * This creates a new BDB using a pair of BDB keys and a pair of data keys.
 * A private data key is needed even with no hash entries.
 */
static int do_create(const char *cbi_filename, uint32_t size, uint8_t erase,
		     uint32_t board_version, uint32_t oem_id, uint32_t sku_id)
{
	struct board_info bi;
	int rv;
	uint8_t *buf;

	/* Check arguments.
	 * TODO: Be cleverer about supporting field size changes. */
	if (!cbi_filename || board_version > USHRT_MAX ||
			oem_id > UCHAR_MAX || sku_id > UCHAR_MAX ||
			size > USHRT_MAX) {
		fprintf(stderr, "Missing arguments\n");
		return -1;
	}

	buf = malloc(size);
	if (!buf) {
		fprintf(stderr, "Failed to allocate memory\n");
		return -1;
	}
	memset(buf, erase, size);
	memset(&bi, 0, sizeof(bi));

	memcpy(bi.head.magic, cbi_magic, sizeof(bi.head.magic));
	bi.head.major = CBI_VERSION_MAJOR;
	bi.head.minor = CBI_VERSION_MINOR;
	bi.head.total_size = sizeof(bi);
	bi.version = board_version;
	bi.oem_id = oem_id;
	bi.sku_id = sku_id;

	bi.head.crc = crc8((uint8_t *)&bi.head.version, bi.head.total_size - 4);
	memcpy(buf, &bi, sizeof(bi));

	/* Output blob */
	rv = write_file(cbi_filename, buf, size);
	if (rv) {
		fprintf(stderr, "Unable to write CBI blob\n");
		return rv;
	}

	fprintf(stderr, "CBI blob is created successfully\n");

	return 0;
}

static int do_show(const char *cbi_filename, int show_all)
{
	uint8_t *buf;
	uint32_t size;
	struct board_info *bi;

	if (!cbi_filename) {
		fprintf(stderr, "Missing arguments\n");
		return -1;
	}

	buf = read_file(cbi_filename, &size);
	if (!buf) {
		fprintf(stderr, "Unable to read CBI blob\n");
		return -1;
	}

	bi = (struct board_info *)buf;
	printf("CBI blob: %s\n", cbi_filename);
	printf("  BOARD_VERSION: %d.%d (0x%02x.0x%02x)\n",
	       bi->major, bi->minor, bi->major, bi->minor);
	printf("  OEM_ID: %d (0x%02x)\n", bi->oem_id, bi->oem_id);
	printf("  SKU_ID: %d (0x%02x)\n", bi->sku_id, bi->sku_id);

	if (memcmp(bi->head.magic, cbi_magic, sizeof(cbi_magic))) {
		fprintf(stderr, "Invalid Magic\n");
		return -1;
	}

	if (cbi_crc8(bi) != bi->head.crc) {
		fprintf(stderr, "Invalid CRC\n");
		return -1;
	}

	printf("Data validated successfully\n");
	return 0;
}

/* Print help and return error */
static void print_help(int argc, char *argv[])
{
	printf("\nUsage: cbi %s <--create|--show>\n"
	       "\n"
	       "Utility for managing Cros Board Info (CBIs).\n"
	       "\n"
	       "For '--create <cbi_file> [OPTIONS]', required OPTIONS are:\n"
	       "  --board_version <uint16>    Board version\n"
	       "  --oem_id <uint8>            OEM ID\n"
	       "  --sku_id <uint8>            SKU ID\n"
	       "  --size <uint16>             Size of output file\n"
	       "Optional OPTIONS are:\n"
	       "  --erase_byte <uint8>        Byte used for empty space\n"
	       "  --format_version <uint16>   Data format version\n"
	       "\n"
	       "For '--show <cbi_file> [OPTIONS]', OPTIONS are:\n"
	       "  --all                       Dump all information\n"
	       "  It also validates the contents against the checksum and\n"
	       "  returns non-zero if validation fails.\n"
	       "\n",
	       argv[0]);
}

int main(int argc, char **argv)
{
	int mode = OPT_MODE_NONE;
	const char *cbi_filename = NULL;
	uint32_t board_version = -1;
	uint32_t oem_id = -1;
	uint32_t sku_id = -1;
	uint32_t size = -1;
	uint8_t erase = 0xff;
	int show_all = 0;
	int parse_error = 0;
	char *e;
	int i;

	while ((i = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
		switch (i) {
		case '?':
			/* Unhandled option */
			fprintf(stderr, "Unknown option or missing value\n");
			parse_error = 1;
			break;
		case OPT_HELP:
			print_help(argc, argv);
			return !!parse_error;
		case OPT_MODE_CREATE:
			mode = i;
			cbi_filename = optarg;
			break;
		case OPT_MODE_SHOW:
			mode = i;
			cbi_filename = optarg;
			break;
		case OPT_BOARD_VERSION:
			board_version = strtoul(optarg, &e, 0);
			if (!*optarg || (e && *e)) {
				fprintf(stderr, "Invalid --board_version\n");
				parse_error = 1;
			}
			break;
		case OPT_OEM_ID:
			oem_id = strtoul(optarg, &e, 0);
			if (!*optarg || (e && *e)) {
				fprintf(stderr, "Invalid --oem_id\n");
				parse_error = 1;
			}
			break;
		case OPT_SKU_ID:
			sku_id = strtoul(optarg, &e, 0);
			if (!*optarg || (e && *e)) {
				fprintf(stderr, "Invalid --sku_id\n");
				parse_error = 1;
			}
			break;
		case OPT_SIZE:
			size = strtoul(optarg, &e, 0);
			if (!*optarg || (e && *e)) {
				fprintf(stderr, "Invalid --size\n");
				parse_error = 1;
			}
			break;
		case OPT_ERASE_BYTE:
			erase = strtoul(optarg, &e, 0);
			if (!*optarg || (e && *e)) {
				fprintf(stderr, "Invalid --erase_byte\n");
				parse_error = 1;
			}
			break;
		case OPT_SHOW_ALL:
			show_all = 1;
			break;
		}
	}

	if (parse_error) {
		print_help(argc, argv);
		return 1;
	}

	switch (mode) {
	case OPT_MODE_CREATE:
		return do_create(cbi_filename, size, erase,
				 board_version, oem_id, sku_id);
	case OPT_MODE_SHOW:
		return do_show(cbi_filename, show_all);
	case OPT_MODE_NONE:
	default:
		fprintf(stderr, "Must specify a mode.\n");
		print_help(argc, argv);
		return 1;
	}
}
