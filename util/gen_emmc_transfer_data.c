/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Generate transferring data from a file. The transferring data emulates the
 * eMMC protocol.
 */

#include <err.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <compile_time_macros.h>

/* eMMC transfer block size */
#define BLOCK_SIZE		512

uint16_t crc16_arg(uint8_t data, uint16_t previous_crc)
{
	unsigned int crc = previous_crc << 8;
	int i;

	crc ^= (data << 16);
	for (i = 8; i; i--) {
		if (crc & 0x800000)
			crc ^= (0x11021 << 7);
		crc <<= 1;
	}

	return (uint16_t)(crc >> 8);
}

void binary_format(FILE *fin, FILE *fout)
{
	uint8_t data[BLOCK_SIZE];
	int blk, j;
	uint16_t crc16;
	size_t cnt;

	/* eMMC emulating bytes */
	uint8_t ack_boot_mode[] = {
		0xff, 0x97, /* Acknowledge boot mode: 1 S=0 010 E=1 11 */
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	uint8_t start[] = { 0xff, 0xfe /* idle, star bit. */ };
	uint8_t crc_end[] = { 0xff, 0xff, 0xff }; /* CRC, end bit, idle */
	uint8_t idle[] = { 0xff, 0xff, 0xff, 0xff }; /* last block: idle */

	fwrite(ack_boot_mode, sizeof(uint8_t), ARRAY_SIZE(ack_boot_mode), fout);

	for (blk = 0;; blk++) {
		crc16 = 0;
		cnt = fread(data, 1, BLOCK_SIZE, fin);

		if (cnt == 0)
			break;
		else if (cnt < BLOCK_SIZE)
			memset(&data[cnt], 0xff, BLOCK_SIZE - cnt);

		fwrite(start, sizeof(uint8_t), ARRAY_SIZE(start), fout);
		for (j = 0; j < sizeof(data); j++) {
			fwrite(&data[j], sizeof(uint8_t), 1, fout);
			crc16 = crc16_arg(data[j], crc16);
		}

		crc_end[0] = crc16 >> 8; /* CRC */
		crc_end[1] = crc16 & 0xff; /* end bit */
		fwrite(crc_end, sizeof(uint8_t), ARRAY_SIZE(crc_end), fout);
	}

	/* Last block: idle */
	fwrite(idle, sizeof(uint8_t), ARRAY_SIZE(idle), fout);
}

int main(int argc, char **argv)
{
	int nopt;
	const char *output_name = NULL;
	char *input_name = NULL;
	FILE *fin = NULL;
	FILE *fout = NULL;

	const char short_opts[] = "i:ho:";
	const struct option long_opts[] = {
		{ "input", 1, NULL, 'i' },
		{ "help", 0, NULL, 'h' },
		{ "out", 1, NULL, 'o' },
		{ NULL }
	};
	const char usage[] = "USAGE: %s [-i <input>] -o <output>\n";

	while ((nopt = getopt_long(argc, argv, short_opts, long_opts,
								NULL)) != -1) {
		switch (nopt) {
		case 'i': /* -i or --input*/
			input_name = optarg;
			break;
		case 'h': /* -h or --help */
			fprintf(stdout, usage, argv[0]);
			return 0;
		case 'o': /* -o or --out */
			output_name = optarg;
			break;
		default: /* Invalid parameter. */
			fprintf(stderr, usage, argv[0]);
			return 1;
		}
	}

	if (output_name == NULL) {
		fprintf(stderr, usage, argv[0]);
		return 1;
	}

	fout = fopen(output_name, "wb");
	if (!fout)
		err(1, "Cannot open output file");

	if (input_name == NULL) {
		printf("No bootblock provided, outputting empty file.\n");
		goto out_close_fout;
	}

	fin = fopen(input_name, "r");

	if (!fin)
		err(1, "Cannot open input file");


	binary_format(fin, fout);

	fclose(fin);

out_close_fout:
	fclose(fout);

	return 0;
}
