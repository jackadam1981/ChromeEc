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
#include "task.h"
#include "timer.h"


int DCRYPTO_app_cipher(const void *salt, void *out, const void *in, size_t len)
{
	struct APPKEY_CTX ctx;
	uint32_t iv[4];

	memcpy(iv, salt, sizeof(iv));
	if (!DCRYPTO_appkey_init(NVMEM, &ctx))
		return 0;

	if (!DCRYPTO_aes_ctr(out, ctx.key, 128, (uint8_t *) iv, in, len))
		return 0;

	DCRYPTO_appkey_finish(&ctx);
	return 1;
}

#define HEAP_HEAD_ROOM 0x400

static uint8_t result;
static void run_cipher_cmd(void)
{
	int rv;
	char *p;
	uint8_t sha[SHA_DIGEST_SIZE];
	uint8_t sha_after[SHA_DIGEST_SIZE];
	int match;
	uint32_t tstamp;
	size_t test_blob_size;

	test_blob_size = shared_mem_size();
	/*
	 * Leave some room to crypto functions, just in case.
	 */
	if (test_blob_size < HEAP_HEAD_ROOM) {
		ccprintf("Not enough memory to run the test\n");
		result = EC_ERROR_OVERFLOW;
		task_set_event(TASK_ID_CONSOLE, TASK_EVENT_CUSTOM(1), 0);
		return;
	}

	/*
	 * Let's use some odd size to make sure unaligned buffers are
	 * handled properly.
	 */
	test_blob_size = (test_blob_size - HEAP_HEAD_ROOM) & ~0xf;
	test_blob_size |= 7;

	rv = shared_mem_acquire(test_blob_size, (char **)&p);
	if (rv != EC_SUCCESS) {
		ccprintf("Failed to allocate %d bytes\n", test_blob_size);
		result = EC_ERROR_OVERFLOW;
		task_set_event(TASK_ID_CONSOLE, TASK_EVENT_CUSTOM(1), 0);
		return;
	}

	ccprintf("blob size %d at %p\n", test_blob_size, p);
	ccprintf("original data           %.16h\n", p);

	DCRYPTO_SHA1_hash((uint8_t *)p, test_blob_size, sha);

	tstamp = get_time().val;
	rv = DCRYPTO_app_cipher(&sha, p, p, test_blob_size);
	tstamp = get_time().val - tstamp;
	ccprintf("out data                %.16h, time %d us\n", p, tstamp);
	if (!rv) {
		ccprintf("encryption failed\n");
		result = EC_ERROR_UNKNOWN;
		shared_mem_release(p);
		task_set_event(TASK_ID_CONSOLE, TASK_EVENT_CUSTOM(1), 0);
		return;
	}


	tstamp = get_time().val;
	rv = DCRYPTO_app_cipher(&sha, p, p, test_blob_size);
	tstamp = get_time().val - tstamp;
	ccprintf("decrypted data          %.16h, time %d us\n", p, tstamp);
	if (!rv) {
		ccprintf("decryption failed\n");
		result = EC_ERROR_UNKNOWN;
		shared_mem_release(p);
		task_set_event(TASK_ID_CONSOLE, TASK_EVENT_CUSTOM(1), 0);
		return;
	}

	DCRYPTO_SHA1_hash((uint8_t *)p, test_blob_size, sha_after);
	match = !memcmp(sha, sha_after, sizeof(sha));
	ccprintf("sha1 before and after %smatch!\n",
		 match ? "" : "MIS");

	shared_mem_release(p);

	if ((rv == 1) && match)
		result = EC_SUCCESS;
	else
		result = EC_ERROR_UNKNOWN;

	task_set_event(TASK_ID_CONSOLE, TASK_EVENT_CUSTOM(1), 0);
}
DECLARE_DEFERRED(run_cipher_cmd);

static int cmd_cipher(int argc, char **argv)
{
	uint32_t events;

	hook_call_deferred(&run_cipher_cmd_data, 0);

	/* Should be done much sooner than in 1 second. */
	events = task_wait_event_mask(TASK_EVENT_CUSTOM(1), 1 * SECOND);
	if (!(events & TASK_EVENT_CUSTOM(1))) {
		ccprintf("Timed out, you might want to reboot...\n");
		return EC_ERROR_TIMEOUT;
	}

	return result;
}
DECLARE_SAFE_CONSOLE_COMMAND(cipher, cmd_cipher, NULL, NULL);
