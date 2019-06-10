/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_commands.h"
#include "fpsensor_crypto.h"
#include "fpsensor_state.h"
#include "host_command.h"
#include "test_util.h"
#include "util.h"

static const uint8_t fake_rollback_secret[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t fake_tpm_seed[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
};

/* Mock the rollback for unit test. */
int rollback_get_secret(uint8_t *secret)
{
	memcpy(secret, fake_rollback_secret, sizeof(fake_rollback_secret));
	return EC_SUCCESS;
}

static int check_fp_enc_status_valid_flags(const uint32_t expected)
{
	int rv;
	struct ec_response_fp_encryption_status resp = { 0 };

	rv = test_send_host_command(EC_CMD_FP_ENC_STATUS, 0,
				    NULL, 0,
				    &resp, sizeof(resp));
	if (rv != EC_RES_SUCCESS) {
		ccprintf("%s:%s(): failed to get encryption status. rv = %d\n",
			 __FILE__, __func__, rv);
		return -1;
	}

	if (resp.valid_flags != expected) {
		ccprintf("%s:%s(): expected valid flags 0x%08x, got 0x%08x\n",
			 __FILE__, __func__, expected, resp.valid_flags);
		return -1;
	}

	return EC_RES_SUCCESS;
}

static int test_derive_encryption_key_raw(const uint32_t *user_id_,
					  const uint8_t *salt,
					  const uint8_t *expected_key)
{
	uint8_t key[SBP_ENC_KEY_LEN];
	int rv;

	memcpy(user_id, user_id_, sizeof(user_id));
	rv = derive_encryption_key(key, salt);

	TEST_ASSERT(rv == EC_RES_SUCCESS);
	TEST_ASSERT_ARRAY_EQ(key, expected_key, sizeof(key));

	return EC_SUCCESS;
}

test_static int test_derive_encryption_key(void)
{
	/*
	 * These vectors are obtained by choosing the salt and the user_id
	 * (used as "info" in HKDF), and running boringSSL's HKDF
	 * (https://boringssl.googlesource.com/boringssl/+/c0b4c72b6d4c6f4828a373ec454bd646390017d4/crypto/hkdf/)
	 * locally to get the output key. The IKM used in the run is the
	 * concatenation of |fake_rollback_secret| and |fake_tpm_seed|.
	 */
	static const uint32_t user_id1[] = {
		0x00000000, 0x00000001, 0x00000002, 0x00000003,
		0x00000004, 0x00000005, 0x00000006, 0x00000007,
	};

	static const uint8_t salt1[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};

	static const uint8_t key1[] = {
		0xf9, 0xb8, 0x69, 0x26, 0x16, 0xa9, 0x5a, 0x31,
		0xba, 0x76, 0xe5, 0x67, 0x1c, 0xa4, 0x00, 0xd9,
	};

	static const uint32_t user_id2[] = {
		0x00000000, 0x10000000, 0x20000000, 0x30000000,
		0x40000000, 0x50000000, 0x60000000, 0x70000000,
	};

	static const uint8_t salt2[] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	};

	static const uint8_t key2[] = {
		0x0d, 0xc5, 0xb2, 0x8e, 0xd1, 0x8f, 0xca, 0x65,
		0x12, 0x70, 0x17, 0x91, 0x5a, 0xc2, 0xdc, 0xc8,
	};

	TEST_ASSERT(test_derive_encryption_key_raw(user_id1, salt1, key1) ==
		EC_SUCCESS);

	TEST_ASSERT(test_derive_encryption_key_raw(user_id2, salt2, key2) ==
		EC_SUCCESS);

	return EC_SUCCESS;
}

static int check_fp_tpm_seed_not_set(void)
{
	int rv;
	struct ec_response_fp_encryption_status resp = { 0 };

	/* Initially the seed should not have been set. */
	rv = test_send_host_command(EC_CMD_FP_ENC_STATUS, 0,
				    NULL, 0,
				    &resp, sizeof(resp));
	if (rv != EC_RES_SUCCESS || resp.status & FP_ENC_STATUS_SEED_SET) {
		ccprintf("%s:%s(): rv = %d, seed is set: %d\n", __FILE__,
			 __func__, rv, resp.status & FP_ENC_STATUS_SEED_SET);
		return -1;
	}

	return EC_RES_SUCCESS;
}

static int set_fp_tpm_seed(void)
{
	/*
	 * TODO(yichengli): test setting the seed twice:
	 * the second time fails;
	 * the seed is still set.
	 */
	int rv;
	struct ec_params_fp_seed params;
	struct ec_response_fp_encryption_status resp = { 0 };

	params.struct_version = FP_TEMPLATE_FORMAT_VERSION;
	memcpy(params.seed, fake_tpm_seed, sizeof(fake_tpm_seed));

	rv = test_send_host_command(EC_CMD_FP_SEED, 0,
				    &params, sizeof(params),
				    NULL, 0);
	if (rv != EC_RES_SUCCESS) {
		ccprintf("%s:%s(): rv = %d, set seed failed\n",
			 __FILE__, __func__, rv);
		return -1;
	}

	/* Now seed should have been set. */
	rv = test_send_host_command(EC_CMD_FP_ENC_STATUS, 0,
				    NULL, 0,
				    &resp, sizeof(resp));
	if (rv != EC_RES_SUCCESS || !(resp.status & FP_ENC_STATUS_SEED_SET)) {
		ccprintf("%s:%s(): rv = %d, seed is set: %d\n", __FILE__,
			 __func__, rv, resp.status & FP_ENC_STATUS_SEED_SET);
		return -1;
	}

	return EC_RES_SUCCESS;
}

test_static int test_fpsensor(void)
{
	TEST_ASSERT(check_fp_enc_status_valid_flags(FP_ENC_STATUS_SEED_SET) ==
		    EC_RES_SUCCESS);
	TEST_ASSERT(check_fp_tpm_seed_not_set() == EC_RES_SUCCESS);
	TEST_ASSERT(set_fp_tpm_seed() == EC_RES_SUCCESS);

	return EC_SUCCESS;
}

void run_test(void)
{
	RUN_TEST(test_fpsensor);
	RUN_TEST(test_derive_encryption_key);

	test_print_result();
}
