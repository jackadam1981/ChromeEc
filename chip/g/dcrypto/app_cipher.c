/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "common.h"

#include "console.h"
#include "dcrypto.h"
#include "flash.h"
#include "hooks.h"
#include "shared_mem.h"
#include "timer.h"


int DCRYPTO_app_cipher(void *salt, void *out, const void *in, size_t len)
{
	struct APPKEY_CTX ctx;
	uint32_t iv[4] = {};

	memcpy(iv + 2, salt, sizeof(*iv));
	if (!DCRYPTO_appkey_init(NVMEM, &ctx))
		return 0;

	if (!DCRYPTO_aes_ctr(out, ctx.key, 128, (uint8_t*) iv, in, len))
		return 0;

	DCRYPTO_appkey_finish(&ctx);
	return 1;
}

/*
 * Let's use some odd size to make sure unaligned buffers are handled
 * properly.
 */
#define TEST_BLOB_SIZE 14381

static uint8_t result;
static void run_cipher_cmd(void)
{
	int rv;
	char *p;
	uint8_t sha[SHA_DIGEST_SIZE];
	uint8_t sha_after[SHA_DIGEST_SIZE];
	int match;

	rv = shared_mem_acquire(TEST_BLOB_SIZE, (char **)&p);

	if (rv != EC_SUCCESS) {
	        result = rv + 1;
	        return;
	}

	ccprintf("original data           %.16h\n", p);

	DCRYPTO_SHA1_hash((uint8_t *)p, TEST_BLOB_SIZE, sha);

	rv = DCRYPTO_app_cipher(&sha, p, p, TEST_BLOB_SIZE);
	ccprintf("rv 0x%02x, out data       %.16h\n", rv, p);

	if (rv == 1) {
	        rv = DCRYPTO_app_cipher(&sha, p, p, TEST_BLOB_SIZE);
	        ccprintf("rv 0x%02x, orig. data     %.16h\n", rv, p);
	}

	DCRYPTO_SHA1_hash((uint8_t *)p, TEST_BLOB_SIZE, sha_after);

	match = !memcmp(sha, sha_after, sizeof(sha));
	ccprintf("sha1 before and after %smatch!\n",
		 match ? "" : "MIS");

	shared_mem_release(p);

	if ((rv == 1) && match)
	        result = EC_SUCCESS;
	else
	        result = EC_ERROR_UNKNOWN;
}
DECLARE_DEFERRED(run_cipher_cmd);

static int cmd_cipher(int argc, char **argv)
{
	int i = 0;

	result = ~0;
	hook_call_deferred(&run_cipher_cmd_data, 0);

	while(i++ < 100) {  /* 100 ms is more than enough. */
	        msleep(100);
	        if (result != (uint8_t)~0) {
	                ccprintf("Done in %d ms\n", i * 10);
	                return result;
	        }
	}
	ccprintf("Timed out, you might want to reboot...\n");
	return EC_ERROR_TIMEOUT;
}
DECLARE_SAFE_CONSOLE_COMMAND(cipher, cmd_cipher, NULL, NULL);
