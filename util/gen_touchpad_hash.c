/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdint.h>
#include <openssl/sha.h>

#include "config.h"

/* Output blank hashes */
static int hash_fw_blank(FILE* hashes) {
	uint8_t digest[SHA256_DIGEST_LENGTH] = { 0 };
	int len, wb;

	for (len = 0; len < CONFIG_TOUCHPAD_VIRTUAL_SIZE;
	     				len += CONFIG_UPDATE_PDU_SIZE) {
		wb = fwrite(digest, sizeof(digest), 1, hashes);

		if (wb != 1) {
			fprintf(stderr, "Error writing hashes.\n");
			return 1;
		}
	}

	return 0;
}

static int hash_fw(FILE* tp_fw, FILE* hashes) {
	char buffer[CONFIG_UPDATE_PDU_SIZE];
	int len = 0;
	int rb, wb;
	SHA256_CTX ctx;
	uint8_t digest[SHA256_DIGEST_LENGTH];

	while (1) {
		rb = fread(buffer, 1, sizeof(buffer), tp_fw);
		len += rb;

		if (rb == 0)
			break;

                /* Calculate hash for the block. */
                SHA256_Init(&ctx);
                SHA256_Update(&ctx, buffer, rb);
                SHA256_Final(digest, &ctx);

		wb = fwrite(digest, sizeof(digest), 1, hashes);

		if (wb != 1) {
			fprintf(stderr, "Error writing hashes.\n");
			return 1;
		}

		if (rb < sizeof(buffer))
			break;
	}

	if (!feof(tp_fw) || ferror(tp_fw)) {
		fprintf(stderr, "Error reading file.\n");
		return 1;
	}

	if (len != CONFIG_TOUCHPAD_VIRTUAL_SIZE) {
		fprintf(stderr, "Incorrect TP FW size (%d vs %d).\n",
			len, CONFIG_TOUCHPAD_VIRTUAL_SIZE);
		return 1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int nopt;
	int ret;
	const char *out = NULL;
	const char *board = NULL;
	char *tp_fw_name;
	FILE* tp_fw = NULL;
	FILE* hashes;
	const char * const short_opt = "hb:f:o:";
	const struct option long_opts[] = {
		{ "help", 0, NULL, 'h' },
		{ "board", 1, NULL, 'b' },
		{ "fw", 1, NULL, 'f' },
		{ "out", 1, NULL, 'o' },
		{ NULL }
	};

	do {
		nopt = getopt_long(argc, argv, short_opt, long_opts, NULL);
		switch (nopt) {
		case 'h': /* -h or --help */
			printf("USAGE: %s -b <board name> -f <touchpad FW> -o <output file>\n",
					argv[0]);
			return 1;

		case 'b': /* -b or --board */
			board = optarg;
			break;

		case 'f': /* -f or --firmware */
			tp_fw_name = optarg;
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

	hashes = fopen(out, "w");

	if (!hashes) {
		fprintf(stderr, "Cannot open output file.\n");
		return 1;
	}

	if (tp_fw_name) {
		tp_fw = fopen(tp_fw_name, "r");

		if (!tp_fw) {
			fprintf(stderr, "Cannot open firmware.\n");
			return 1;
		}
		ret = hash_fw(tp_fw, hashes);

		fclose(tp_fw);
	} else {
		printf("No touchpad firmware provided, outputting blank hashes.\n");
		ret = hash_fw_blank(hashes);
	}

	fclose(hashes);

	return ret;
}
