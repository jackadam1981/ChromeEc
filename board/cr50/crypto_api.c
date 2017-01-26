/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "crypto_api.h"
#include "dcrypto.h"

void app_compute_hash(uint8_t *p_buf, size_t num_bytes,
		  uint8_t *p_hash, size_t hash_len)
{
	uint8_t sha1_digest[SHA_DIGEST_SIZE];

	/*
	 * Use the built in dcrypto engine to generate the sha1 hash of the
	 * buffer.
	 */
	DCRYPTO_SHA1_hash((uint8_t *)p_buf, num_bytes, sha1_digest);

	memcpy(p_hash, sha1_digest, MIN(hash_len, sizeof(sha1_digest)));

	if (hash_len > sizeof(sha1_digest))
		memset(p_hash + sizeof(sha1_digest), 0,
		       hash_len - sizeof(sha1_digest));
}

int app_cipher(const void *salt, void *out, const void *in, size_t size)
{
	return DCRYPTO_app_cipher(NVMEM, salt, out, in, size);
}

#ifdef CRYPTO_TEST_SETUP

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "shared_mem.h"
#include "task.h"
#include "timer.h"

/*
 * Let's use some odd size to make sure unaligned buffers are handled
 * properly.
 */
#define TEST_BLOB_SIZE 16387

static uint8_t result;
static void run_cipher_cmd(void)
{
	int rv;
	char *p;
	uint8_t sha[SHA_DIGEST_SIZE];
	uint8_t sha_after[SHA_DIGEST_SIZE];
	int match;
	uint32_t tstamp1, tstamp2;

	rv = shared_mem_acquire(TEST_BLOB_SIZE, (char **)&p);

	if (rv != EC_SUCCESS) {
		result = rv;
		return;
	}

	ccprintf("original data           %.16h\n", p);

	DCRYPTO_SHA1_hash((uint8_t *)p, TEST_BLOB_SIZE, sha);

	tstamp1 = get_time().val;
	rv = DCRYPTO_app_cipher(NVMEM, &sha, p, p, TEST_BLOB_SIZE);
	tstamp1 = get_time().val - tstamp1;

	if (rv == 1) {
		tstamp2 = get_time().val;
		rv = DCRYPTO_app_cipher(NVMEM, &sha, p, p, TEST_BLOB_SIZE);
		tstamp2 = get_time().val - tstamp2;
		ccprintf("rv 0x%02x, out data       %.16h, time %d us\n",
			rv, p, tstamp1);
		ccprintf("rv 0x%02x, orig. data     %.16h, time %d us\n",
			 rv, p, tstamp2);
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
#endif
