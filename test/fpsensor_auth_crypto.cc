/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_commands.h"
#include "fpsensor_auth_crypto.h"
#include "fpsensor_state.h"
#include "mock/fpsensor_state_mock.h"
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/obj_mac.h"
#include "test_util.h"
#include "util.h"

#include <stdbool.h>

#include <array>

namespace
{

test_static enum ec_error_list
test_fp_auth_command_create_ec_key_from_pubkey(void)
{
	struct ec_fp_ec_public_key pubkey = {
		.x = {
			0x85, 0xAD, 0x35, 0x23, 0x05, 0x1E, 0x33, 0x3F,
			0xCA, 0xA7, 0xEA, 0xA5, 0x88, 0x33, 0x12, 0x95,
			0xA7, 0xB5, 0x98, 0x9F, 0x32, 0xEF, 0x7D, 0xE9,
			0xF8, 0x70, 0x14, 0x5E, 0x89, 0xCB, 0xDE, 0x1F,
		},
		.y = {
			0xD1, 0xDC, 0x91, 0xC6, 0xE6, 0x5B, 0x1E, 0x3C,
			0x01, 0x6C, 0xE6, 0x50, 0x25, 0x5D, 0x89, 0xCF,
			0xB7, 0x8D, 0x88, 0xB9, 0x0D, 0x09, 0x41, 0xF1,
			0x09, 0x4F, 0x61, 0x55, 0x6C, 0xC4, 0x96, 0x6B,
		},
	};

	bssl::UniquePtr<EC_KEY> key = create_ec_key_from_pubkey(pubkey);

	TEST_NE(key.get(), nullptr, "%p");
	TEST_EQ(EC_KEY_check_key(key.get()), 1, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list
test_fp_auth_command_create_ec_key_from_pubkey_fail(void)
{
	struct ec_fp_ec_public_key pubkey = {
		.x = {},
		.y = {},
	};

	bssl::UniquePtr<EC_KEY> key = create_ec_key_from_pubkey(pubkey);

	TEST_EQ(key.get(), nullptr, "%p");

	return EC_SUCCESS;
}

test_static enum ec_error_list test_fp_auth_command_fill_pubkey(void)
{
	struct ec_fp_ec_public_key pubkey = {
		.x = {
			0x85, 0xAD, 0x35, 0x23, 0x05, 0x1E, 0x33, 0x3F,
			0xCA, 0xA7, 0xEA, 0xA5, 0x88, 0x33, 0x12, 0x95,
			0xA7, 0xB5, 0x98, 0x9F, 0x32, 0xEF, 0x7D, 0xE9,
			0xF8, 0x70, 0x14, 0x5E, 0x89, 0xCB, 0xDE, 0x1F,
		},
		.y = {
			0xD1, 0xDC, 0x91, 0xC6, 0xE6, 0x5B, 0x1E, 0x3C,
			0x01, 0x6C, 0xE6, 0x50, 0x25, 0x5D, 0x89, 0xCF,
			0xB7, 0x8D, 0x88, 0xB9, 0x0D, 0x09, 0x41, 0xF1,
			0x09, 0x4F, 0x61, 0x55, 0x6C, 0xC4, 0x96, 0x6B,
		},
	};

	bssl::UniquePtr<EC_KEY> key = create_ec_key_from_pubkey(pubkey);

	TEST_NE(key.get(), nullptr, "%p");
	TEST_EQ(EC_KEY_check_key(key.get()), 1, "%d");

	struct ec_fp_ec_public_key result;

	TEST_EQ(fill_pubkey(*key, result), EC_SUCCESS, "%d");

	TEST_ASSERT_ARRAY_EQ(result.x, pubkey.x, sizeof(pubkey.x));
	TEST_ASSERT_ARRAY_EQ(result.y, pubkey.y, sizeof(pubkey.y));

	return EC_SUCCESS;
}

test_static enum ec_error_list test_fp_auth_command_encrypt_decrypt_data(void)
{
	struct ec_fp_auth_command_encryption_metadata info;
	const std::array<uint8_t, 32> input = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
						1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
						2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };
	uint16_t version = 1;
	std::array<uint8_t, 32> data;

	std::copy(input.begin(), input.end(), data.begin());

	TEST_EQ(encrypt_data_in_place(version, info, data.data(), data.size()),
		EC_SUCCESS, "%d");

	TEST_EQ(info.struct_version, version, "%d");

	/* The encrypted data should not be the same as the input. */
	TEST_ASSERT_ARRAY_NE(data, input, data.size());

	/* TODO(crrev/c/4511815): Decrypt the data, and check the result is the
	 * same. */

	return EC_SUCCESS;
}

test_static enum ec_error_list test_fp_auth_command_encrypt_decrypt_key(void)
{
	struct ec_fp_encrypted_private_key enc_key;
	uint16_t version = 1;
	std::array<uint8_t, 32> privkey = { 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
					    1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
					    2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };

	bssl::UniquePtr<EC_KEY> key(
		EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));

	TEST_NE(key.get(), nullptr, "%p");

	TEST_EQ(EC_KEY_oct2priv(key.get(), privkey.data(), privkey.size()), 1,
		"%d");

	TEST_EQ(fill_encrypted_private_key(*key, version, enc_key), EC_SUCCESS,
		"%d");

	TEST_EQ(enc_key.info.struct_version, version, "%d");

	/* TODO(crrev/c/4511815): Decrypt the data, and check the result is the
	 * same. */

	return EC_SUCCESS;
}

enum ec_error_list
check_seed_set_result(const enum ec_status rv, const uint32_t expected,
		      const struct ec_response_fp_encryption_status *resp)
{
	const uint32_t actual = resp->status & FP_ENC_STATUS_SEED_SET;

	if (rv != EC_RES_SUCCESS || expected != actual) {
		ccprintf("%s:%s(): rv = %d, seed is set: %d\n", __FILE__,
			 __func__, rv, actual);
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

test_static enum ec_error_list test_set_fp_tpm_seed(void)
{
	enum ec_status rv;
	struct ec_params_fp_seed params;
	struct ec_response_fp_encryption_status resp = { 0 };

	params.struct_version = FP_TEMPLATE_FORMAT_VERSION;
	memcpy(params.seed, default_fake_tpm_seed,
	       sizeof(default_fake_tpm_seed));

	rv = test_send_host_command(EC_CMD_FP_SEED, 0, &params, sizeof(params),
				    NULL, 0);
	if (rv != EC_RES_SUCCESS) {
		ccprintf("%s:%s(): rv = %d, set seed failed\n", __FILE__,
			 __func__, rv);
		return EC_ERROR_UNKNOWN;
	}

	/* Now seed should have been set. */
	rv = test_send_host_command(EC_CMD_FP_ENC_STATUS, 0, NULL, 0, &resp,
				    sizeof(resp));

	return check_seed_set_result(rv, FP_ENC_STATUS_SEED_SET, &resp);
}

} // namespace

extern "C" void run_test(int argc, const char **argv)
{
	RUN_TEST(test_fp_auth_command_create_ec_key_from_pubkey);
	RUN_TEST(test_fp_auth_command_create_ec_key_from_pubkey_fail);
	RUN_TEST(test_fp_auth_command_fill_pubkey);
	RUN_TEST(test_set_fp_tpm_seed);
	RUN_TEST(test_fp_auth_command_encrypt_decrypt_data);
	RUN_TEST(test_fp_auth_command_encrypt_decrypt_key);
	test_print_result();
}
