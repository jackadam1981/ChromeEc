/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "host_command.h"
#include "sha1.h"
#include "sha256.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

static void checksum_sum(void *address, size_t len, void *hash) {
	uint32_t sum = 0;
	uint32_t *dptr = (uint32_t *)address;
	size_t i;

	for (i = 0; i < len; i += 4) {
		sum += *(dptr++);
	}
	*(uint32_t *)hash = sum;
}

#ifdef CONFIG_SHA1
static void checksum_sha1sum(void *address, size_t len, void *hash) {
	struct sha1_ctx ctx;

	sha1_init(&ctx);
	sha1_update(&ctx, address, len);
	memcpy(hash, sha1_final(&ctx), SHA1_DIGEST_SIZE);
}
#endif /* CONFIG_SHA1 */

#ifdef CONFIG_SHA256
static void checksum_sha256sum(void *address, size_t len, void *hash) {
	struct sha256_ctx ctx;

	SHA256_init(&ctx);
	SHA256_update(&ctx, address, len);
	memcpy(hash, SHA256_final(&ctx), SHA1_DIGEST_SIZE);
}
#endif /* CONFIG_SHA256 */

static int command_sum(int argc, char **argv)
{
	uint32_t address, num = 1;
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

	ccprintf("address = 0x%x | count = 0x%x\n", address, num);

	t0 = get_time();
	checksum_sum((void *)address, (size_t)num, &sum);
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
	char hash[SHA1_DIGEST_SIZE];
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

	ccprintf("address = 0x%x | count = 0x%x\n", address, num);

	t0 = get_time();
	/* SHA-1 Hash of the range */
	checksum_sha1sum((void *)address, num, hash);
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
	char hash[SHA256_DIGEST_SIZE];
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

	ccprintf("address = 0x%x | count = 0x%x\n", address, num);

	t0 = get_time();
	/* SHA-256 Hash of the range */
	checksum_sha256sum((void *)address, num, hash);
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


struct ec_params_get_checksum
{
	int algo;
	void *address;
	size_t len;
} __ec_align4;

// struct ec_response_get_checksum
// {
// 	char buf[];
// } __ec_align4;

static enum ec_status
host_command_get_checksum(struct host_cmd_handler_args *args)
{
	const struct ec_params_get_checksum *p = args->params;

	if (args->params_size != sizeof(struct ec_params_get_checksum)) {
		return EC_RES_INVALID_PARAM;
	}

	ccprintf("HASH(%d): addr=0x%pP len=0x%x!\n", p->algo,  p->address, p->len);
	cflush();

	switch (p->algo)
	{
	case 0:
		checksum_sum(p->address, p->len, args->response);
		args->response_size = sizeof(uint32_t);
		break;
#ifdef CONFIG_SHA1
	case 1:
		checksum_sha1sum(p->address, p->len, args->response);
		args->response_size = SHA1_DIGEST_SIZE;
		break;
#endif
#ifdef CONFIG_SHA256
	case 2:
		checksum_sha256sum(p->address, p->len, args->response);
		args->response_size = SHA256_DIGEST_SIZE;
		break;
#endif
	default:
		return EC_RES_INVALID_PARAM;
	}

	ccprintf("Hash result: %ph\n", HEX_BUF(args->response, args->response_size));
	cflush();

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(0x00CC,
		     host_command_get_checksum,
		     EC_VER_MASK(0));