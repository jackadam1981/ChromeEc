/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * gcc -Wall data2h.c -o data2h && ./data2h > data.h
 */

#include <stdint.h>
#include <stdio.h>

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

int main1(void)
{
	uint16_t crc16;

	crc16 = crc16_arg(0x12, 0);
	crc16 = crc16_arg(0x34, crc16);
	crc16 = crc16_arg(0x56, crc16);
	crc16 = crc16_arg(0x78, crc16);
	crc16 = crc16_arg(0x90, crc16);
	printf("%04x\n", crc16);

	return 0;
}

int main(void)
{
	uint8_t data[512];
	int blk, j;
	uint16_t crc16;

	FILE *in = fopen("preloader.img", "r");

	printf("static const uint8_t raw_data[] __aligned(4) =\n"
		"{\n"
		"\t0xff, 0x97, /* Acknowledge boot mode: 1 S=0 010 E=1 11 */\n"
		"\t0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,\n");

	for (blk = 0; blk < 3; blk++) {
		crc16 = 0;
		fread(data, 1, 512, in);

		printf("\t/* Block %d */\n", blk);
		printf("\t0xff, 0xfe, /* idle, start bit. */");
		for (j = 0; j < sizeof(data); j++) {
			printf("%s0x%02x,",
				(j % 8) == 0 ? "\n\t" : " ", data[j]);
			crc16 = crc16_arg(data[j], crc16);
		}
		printf("\n");

		printf("\t0x%02x, 0x%02x, 0xff, /* CRC, end bit, idle */\n",
			crc16 >> 8, crc16 & 0xff);
	}

	printf("\t/* Last block: idle */\n");
	printf("\t0xff, 0xff, 0xff, 0xff\n");
	printf("};");

	return 0;
}
