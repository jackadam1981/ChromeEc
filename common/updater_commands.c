/* Copyright 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* System module for Chrome EC */

#include "console.h"
#include "sha1.h"
#include "sha256.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

static int command_sum(int argc, char **argv)
{
	uint32_t address, i, num = 1;
	char *e;
	timestamp_t t0;
	uint32_t overall_us;
	uint32_t sum = 0;

	if (argc < 2 || argc > 3)
		return EC_ERROR_PARAM_COUNT;

	address = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	if (argc == 3)
		num = strtoi(argv[2], &e, 0);
	if (*e || num < 1)
		return EC_ERROR_PARAM2;

	t0 = get_time();
	for (i = 0; i < num; i += 4) {
		sum += ((uint32_t *)address)[i];
	}
	overall_us = time_since32(t0);

	ccprintf("%08x\n", sum);
	ccprintf("This hashing took %uus.\n", overall_us);
	cflush();
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND_FLAGS
	(sum, command_sum,
	 "addr [count]",
	 "sum hash ranges of memory",
	 CMD_FLAG_RESTRICTED);

#ifdef CONFIG_SHA1
static int command_sha1sum(int argc, char **argv)
{
	uint32_t address, num = 1;
	char *e;
	uint8_t *hash;
	struct sha1_ctx ctx;
	timestamp_t t0;
	uint32_t overall_us;

	if (argc < 2 || argc > 3)
		return EC_ERROR_PARAM_COUNT;

	address = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	if (argc == 3)
		num = strtoi(argv[2], &e, 0);
	if (*e || num < 1)
		return EC_ERROR_PARAM2;

	t0 = get_time();
	/* SHA-1 Hash of the range */
	sha1_init(&ctx);
	sha1_update(&ctx, (void *)address, num);
	hash = sha1_final(&ctx);
	overall_us = time_since32(t0);

	ccprintf("%ph\n", HEX_BUF(hash, SHA1_DIGEST_SIZE));
	ccprintf("This hashing took %uus.\n", overall_us);
	cflush();
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND_FLAGS
	(sha1sum, command_sha1sum,
	 "addr [count]",
	 "sha1 hash ranges of memory",
	 CMD_FLAG_RESTRICTED);
#endif /* CONFIG_SHA1 */

#ifdef CONFIG_SHA256
static int command_sha256sum(int argc, char **argv)
{
	uint32_t address, num = 1;
	char *e;
	uint8_t *hash;
	struct sha256_ctx ctx;
	timestamp_t t0;
	uint32_t overall_us;

	if (argc < 2 || argc > 3)
		return EC_ERROR_PARAM_COUNT;

	address = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	if (argc == 3)
		num = strtoi(argv[2], &e, 0);
	if (*e || num < 1)
		return EC_ERROR_PARAM2;

	t0 = get_time();
	/* SHA-256 Hash of the range */
	SHA256_init(&ctx);
	SHA256_update(&ctx, (void *)address, num);
	hash = SHA256_final(&ctx);
	overall_us = time_since32(t0);

	ccprintf("%ph\n", HEX_BUF(hash, SHA256_DIGEST_SIZE));
	ccprintf("This hashing took %uus.\n", overall_us);
	cflush();
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND_FLAGS
	(sha256sum, command_sha256sum,
	 "addr [count]",
	 "sha256 hash ranges of memory",
	 CMD_FLAG_RESTRICTED);
#endif /* CONFIG_SHA256 */