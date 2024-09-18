/*
 * Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

#include "dice.h"
#include "platform.h"
#include "cbor_dice.h"

#define DBGDUMP(name)                        \
	do {                                 \
		printf("%s: ", #name);       \
		hexdump(name, sizeof(name)); \
		printf("\n");                \
	} while (0)

static void hexdump(const uint8_t *buf, size_t size)
{
	size_t i;

	for (i = 0; i < size; i++)
		printf("%02x ", buf[i]);
}

int main(void)
{
	uint8_t cbor_hdr[] = CFG_DESCR_LABEL_RESETTABLE;

	DBGDUMP(cbor_hdr);

	printf("kDiceHandoverSize = %zu\n", kDiceHandoverSize);

	return 0;
}
