/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_commands.h"
#include "fpsensor_state.h"
#include "mock/fpsensor_state_mock.h"
#include "openssl/aes.h"
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/obj_mac.h"
#include "test_util.h"
#include "util.h"

extern "C" {
#include "sha256.h"
#include "trng.h"
}

/* These must be included after the "openssl/aes.h" */
#include "crypto/fipsmodule/aes/internal.h"
#include "crypto/fipsmodule/modes/internal.h"

#include <stdbool.h>

static int
check_seed_set_result(const int rv, const uint32_t expected,
		      const struct ec_response_fp_encryption_status *resp)
{
	const uint32_t actual = resp->status & FP_ENC_STATUS_SEED_SET;

	if (rv != EC_RES_SUCCESS || expected != actual) {
		ccprintf("%s:%s(): rv = %d, seed is set: %d\n", __FILE__,
			 __func__, rv, actual);
		return -1;
	}

	return EC_SUCCESS;
}

test_static int test_set_fp_tpm_seed(void)
{
	int rv;
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
		return -1;
	}

	/* Now seed should have been set. */
	rv = test_send_host_command(EC_CMD_FP_ENC_STATUS, 0, NULL, 0, &resp,
				    sizeof(resp));

	return check_seed_set_result(rv, FP_ENC_STATUS_SEED_SET, &resp);
}

test_static int test_fp_command_establish_and_load_pk(void)
{
	int rv;
	struct ec_response_fp_establish_pk_keygen keygen_response;
	struct ec_params_fp_establish_pk_wrap wrap_params;
	struct ec_response_fp_establish_pk_wrap wrap_response;
	struct ec_params_fp_load_pk load_params;

	uint8_t privkey[FP_PK_EC_PRIVATE_KEY_LEN];

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PK_KEYGEN, 0, NULL, 0,
				    &keygen_response, sizeof(keygen_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	memcpy(&wrap_params.enc_privkey_info, &keygen_response.enc_privkey_info,
	       sizeof(keygen_response.enc_privkey_info));

	memcpy(wrap_params.enc_privkey, keygen_response.enc_privkey,
	       sizeof(keygen_response.enc_privkey));

	trng_init();
	trng_rand_bytes(privkey, FP_PK_EC_PRIVATE_KEY_LEN);
	trng_exit();

	bssl::UniquePtr<EC_GROUP> group(
		EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

	if (group == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> n(
		BN_bin2bn(privkey, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (n == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	if (public_point == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_mul(group.get(), public_point.get(), n.get(), nullptr,
			 nullptr, nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> x_bn(BN_new());

	if (x_bn == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> y_bn(BN_new());

	if (y_bn == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_get_affine_coordinates_GFp(group.get(), public_point.get(),
						x_bn.get(), y_bn.get(),
						nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(x_bn.get(), wrap_params.peers_pubkey_x,
			 FP_PK_EC_PUBLIC_KEY_LEN) != FP_PK_EC_PUBLIC_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(y_bn.get(), wrap_params.peers_pubkey_y,
			 FP_PK_EC_PUBLIC_KEY_LEN) != FP_PK_EC_PUBLIC_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PK_WRAP, 0,
				    &wrap_params, sizeof(wrap_params),
				    &wrap_response, sizeof(wrap_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	memcpy(&load_params.enc_pk_info, &wrap_response.enc_pk_info,
	       sizeof(wrap_response.enc_pk_info));

	memcpy(load_params.enc_pk, wrap_response.enc_pk,
	       sizeof(wrap_response.enc_pk));

	rv = test_send_host_command(EC_CMD_FP_LOAD_PK, 0, &load_params,
				    sizeof(load_params), NULL, 0);

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	return EC_SUCCESS;
}

test_static int test_fp_command_establish_pk_fail(void)
{
	int rv;
	struct ec_response_fp_establish_pk_keygen keygen_response;
	struct ec_params_fp_establish_pk_wrap wrap_params;
	struct ec_response_fp_establish_pk_wrap wrap_response;

	uint8_t privkey[FP_PK_EC_PRIVATE_KEY_LEN];

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PK_KEYGEN, 0, NULL, 0,
				    &keygen_response, sizeof(keygen_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	/* No encryption info. */
	memset(&wrap_params.enc_privkey_info, 0,
	       sizeof(wrap_params.enc_privkey_info));

	memcpy(wrap_params.enc_privkey, keygen_response.enc_privkey,
	       sizeof(keygen_response.enc_privkey));

	trng_init();
	trng_rand_bytes(privkey, FP_PK_EC_PRIVATE_KEY_LEN);
	trng_exit();

	bssl::UniquePtr<EC_GROUP> group(
		EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

	if (group == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> n(
		BN_bin2bn(privkey, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (n == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	if (public_point == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_mul(group.get(), public_point.get(), n.get(), nullptr,
			 nullptr, nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> x_bn(BN_new());

	if (x_bn == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> y_bn(BN_new());

	if (y_bn == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_get_affine_coordinates_GFp(group.get(), public_point.get(),
						x_bn.get(), y_bn.get(),
						nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(x_bn.get(), wrap_params.peers_pubkey_x,
			 FP_PK_EC_PUBLIC_KEY_LEN) != FP_PK_EC_PUBLIC_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(y_bn.get(), wrap_params.peers_pubkey_y,
			 FP_PK_EC_PUBLIC_KEY_LEN) != FP_PK_EC_PUBLIC_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PK_WRAP, 0,
				    &wrap_params, sizeof(wrap_params),
				    &wrap_response, sizeof(wrap_response));

	TEST_NE(rv, EC_RES_SUCCESS, "%d");

	return EC_SUCCESS;
}

test_static int test_fp_command_load_pk_fail(void)
{
	int rv;
	struct ec_response_fp_establish_pk_keygen keygen_response;
	struct ec_params_fp_establish_pk_wrap wrap_params;
	struct ec_response_fp_establish_pk_wrap wrap_response;
	struct ec_params_fp_load_pk load_params;

	uint8_t privkey[FP_PK_EC_PRIVATE_KEY_LEN];

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PK_KEYGEN, 0, NULL, 0,
				    &keygen_response, sizeof(keygen_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	memcpy(&wrap_params.enc_privkey_info, &keygen_response.enc_privkey_info,
	       sizeof(keygen_response.enc_privkey_info));

	memcpy(wrap_params.enc_privkey, keygen_response.enc_privkey,
	       sizeof(keygen_response.enc_privkey));

	trng_init();
	trng_rand_bytes(privkey, FP_PK_EC_PRIVATE_KEY_LEN);
	trng_exit();

	bssl::UniquePtr<EC_GROUP> group(
		EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

	if (group == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> n(
		BN_bin2bn(privkey, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (n == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	if (public_point == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_mul(group.get(), public_point.get(), n.get(), nullptr,
			 nullptr, nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> x_bn(BN_new());

	if (x_bn == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> y_bn(BN_new());

	if (y_bn == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_get_affine_coordinates_GFp(group.get(), public_point.get(),
						x_bn.get(), y_bn.get(),
						nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(x_bn.get(), wrap_params.peers_pubkey_x,
			 FP_PK_EC_PUBLIC_KEY_LEN) != FP_PK_EC_PUBLIC_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(y_bn.get(), wrap_params.peers_pubkey_y,
			 FP_PK_EC_PUBLIC_KEY_LEN) != FP_PK_EC_PUBLIC_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PK_WRAP, 0,
				    &wrap_params, sizeof(wrap_params),
				    &wrap_response, sizeof(wrap_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	/* No encryption info. */
	memset(&load_params.enc_pk_info, 0, sizeof(load_params.enc_pk_info));

	memcpy(load_params.enc_pk, wrap_response.enc_pk,
	       sizeof(wrap_response.enc_pk));

	rv = test_send_host_command(EC_CMD_FP_LOAD_PK, 0, &load_params,
				    sizeof(load_params), NULL, 0);

	TEST_NE(rv, EC_RES_SUCCESS, "%d");

	return EC_SUCCESS;
}

test_static int test_fp_command_establish_pk_without_seed(void)
{
	int rv;
	struct ec_response_fp_establish_pk_keygen keygen_response;

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PK_KEYGEN, 0, NULL, 0,
				    &keygen_response, sizeof(keygen_response));

	TEST_NE(rv, EC_RES_SUCCESS, "%d");

	return EC_SUCCESS;
}

test_static int test_fp_command_generate_nonce(void)
{
	int rv;
	struct ec_response_fp_generate_nonce nonce_response;

	rv = test_send_host_command(EC_CMD_FP_GENERATE_NONCE, 0, NULL, 0,
				    &nonce_response, sizeof(nonce_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	return EC_SUCCESS;
}

test_static int test_fp_command_nonce_context(void)
{
	int rv;
	struct ec_response_fp_generate_nonce nonce_response;
	struct ec_params_fp_nonce_context nonce_params = {};

	templ_valid = 1;

	rv = test_send_host_command(EC_CMD_FP_GENERATE_NONCE, 0, NULL, 0,
				    &nonce_response, sizeof(nonce_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	rv = test_send_host_command(EC_CMD_FP_NONCE_CONTEXT, 0, &nonce_params,
				    sizeof(nonce_params), NULL, 0);

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	TEST_EQ(templ_valid, 1u, "%d");

	return EC_SUCCESS;
}

test_static int test_fp_command_nonce_context_clear(void)
{
	int rv;
	struct ec_response_fp_generate_nonce nonce_response;
	struct ec_params_fp_nonce_context nonce_params = {
		.clear_context = 1,
	};

	templ_valid = 1;

	rv = test_send_host_command(EC_CMD_FP_GENERATE_NONCE, 0, NULL, 0,
				    &nonce_response, sizeof(nonce_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	rv = test_send_host_command(EC_CMD_FP_NONCE_CONTEXT, 0, &nonce_params,
				    sizeof(nonce_params), NULL, 0);

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	TEST_EQ(templ_valid, 0u, "%d");

	return EC_SUCCESS;
}

test_static int test_fp_command_nonce_context_limit(void)
{
	int rv;
	struct ec_params_fp_context_v1 ctx_params = {
		.action = FP_CONTEXT_GET_RESULT,
	};
	struct ec_response_fp_generate_nonce nonce_response;
	struct ec_params_fp_nonce_context nonce_params = {
		.clear_context = 1,
	};

	rv = test_send_host_command(EC_CMD_FP_CONTEXT, 1, &ctx_params,
				    sizeof(ctx_params), NULL, 0);

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	/* Call nonce context without generated nonce should fail. */
	rv = test_send_host_command(EC_CMD_FP_NONCE_CONTEXT, 0, &nonce_params,
				    sizeof(nonce_params), NULL, 0);

	TEST_EQ(rv, EC_RES_ACCESS_DENIED, "%d");

	rv = test_send_host_command(EC_CMD_FP_GENERATE_NONCE, 0, NULL, 0,
				    &nonce_response, sizeof(nonce_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	/* Normal context should not clear the generated nonce. */
	/* This will be used for the migration path. */
	rv = test_send_host_command(EC_CMD_FP_CONTEXT, 1, &ctx_params,
				    sizeof(ctx_params), NULL, 0);

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	/* Call nonce context with generated nonce should success. */
	rv = test_send_host_command(EC_CMD_FP_NONCE_CONTEXT, 0, &nonce_params,
				    sizeof(nonce_params), NULL, 0);

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	rv = test_send_host_command(EC_CMD_FP_GENERATE_NONCE, 0, NULL, 0,
				    &nonce_response, sizeof(nonce_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	rv = test_send_host_command(EC_CMD_FP_NONCE_CONTEXT, 0, &nonce_params,
				    sizeof(nonce_params), NULL, 0);

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	/* Call nonce context twice should fail. */
	rv = test_send_host_command(EC_CMD_FP_NONCE_CONTEXT, 0, &nonce_params,
				    sizeof(nonce_params), NULL, 0);

	TEST_EQ(rv, EC_RES_ACCESS_DENIED, "%d");

	rv = test_send_host_command(EC_CMD_FP_GENERATE_NONCE, 0, NULL, 0,
				    &nonce_response, sizeof(nonce_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	rv = test_send_host_command(EC_CMD_FP_GENERATE_NONCE, 0, NULL, 0,
				    &nonce_response, sizeof(nonce_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	rv = test_send_host_command(EC_CMD_FP_NONCE_CONTEXT, 0, &nonce_params,
				    sizeof(nonce_params), NULL, 0);

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	/* Call nonce context twice should fail even we generated two nonces. */
	rv = test_send_host_command(EC_CMD_FP_NONCE_CONTEXT, 0, &nonce_params,
				    sizeof(nonce_params), NULL, 0);

	TEST_EQ(rv, EC_RES_ACCESS_DENIED, "%d");

	return EC_SUCCESS;
}

test_static int test_fp_command_read_match_secret_with_pubkey_succeed(void)
{
	struct ec_response_fp_read_match_secret_with_pubkey response = {};
	/* Create valid param with 0 <= fgr < 5 */
	uint16_t matched_fgr = 1;
	struct ec_params_fp_read_match_secret_with_pubkey params = {
		.fgr = matched_fgr,
	};

	/* Expected positive_match_secret same as  in test/fpsensor_crypto.c*/
	static const uint8_t
		expected_positive_match_secret_for_empty_user_id[] = {
			0x8d, 0xc4, 0x5b, 0xdf, 0x55, 0x1e, 0xa8, 0x72,
			0xd6, 0xdd, 0xa1, 0x4c, 0xb8, 0xa1, 0x76, 0x2b,
			0xde, 0x38, 0xd5, 0x03, 0xce, 0xe4, 0x74, 0x51,
			0x63, 0x6c, 0x6a, 0x26, 0xa9, 0xb7, 0xfa, 0x68,
		};

	/* Create positive secret match state with valid deadline value,
	 * readable state, and correct template matched
	 */
	struct positive_match_secret_state test_state_1 = {
		.template_matched = matched_fgr,
		.readable = true,
		.deadline = { .val = 5000000 },
	};

	uint8_t privkey[FP_PK_EC_PRIVATE_KEY_LEN];
	uint8_t share_secret[FP_POSITIVE_MATCH_SECRET_BYTES];
	struct sha256_ctx ctx;
	AES_KEY aes_key;
	uint8_t aes_iv[FP_CONTEXT_USERID_IV_LEN];
	uint8_t ecount_buf[16];
	unsigned int block_num = 0;

	trng_init();
	trng_rand_bytes(privkey, FP_PK_EC_PRIVATE_KEY_LEN);
	trng_exit();

	bssl::UniquePtr<EC_GROUP> group(
		EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

	if (group == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> n(
		BN_bin2bn(privkey, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (n == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	if (public_point == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_mul(group.get(), public_point.get(), n.get(), nullptr,
			 nullptr, nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> x_bn(BN_new());

	if (x_bn == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> y_bn(BN_new());

	if (y_bn == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_get_affine_coordinates_GFp(group.get(), public_point.get(),
						x_bn.get(), y_bn.get(),
						nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(x_bn.get(), params.pubkey_x,
			 FP_PK_EC_PUBLIC_KEY_LEN) != FP_PK_EC_PUBLIC_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(y_bn.get(), params.pubkey_y,
			 FP_PK_EC_PUBLIC_KEY_LEN) != FP_PK_EC_PUBLIC_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	positive_match_secret_state = test_state_1;
	/* Set fp_positive_match_salt to the trivial value */
	memcpy(fp_positive_match_salt, default_fake_fp_positive_match_salt,
	       sizeof(default_fake_fp_positive_match_salt));

	TEST_ASSERT_ARRAY_EQ(
		(uint8_t const *)fp_positive_match_salt,
		(uint8_t const *)default_fake_fp_positive_match_salt,
		static_cast<int>(sizeof(default_fake_fp_positive_match_salt)));

	/* Initialize an empty user_id to compare positive_match_secret */
	memset(user_id, 0, sizeof(user_id));

	TEST_ASSERT(fp_tpm_seed_is_set());
	/* Test with the correct matched finger state and the default fake
	 * fp_positive_match_salt
	 */
	TEST_ASSERT(
		test_send_host_command(EC_CMD_FP_READ_MATCH_SECRET_WITH_PUBKEY,
				       0, &params, sizeof(params), &response,
				       sizeof(response)) == EC_SUCCESS);

	x_bn = bssl::UniquePtr<BIGNUM>(
		BN_bin2bn(response.pubkey_x, FP_PK_EC_PUBLIC_KEY_LEN, nullptr));
	if (x_bn == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	y_bn = bssl::UniquePtr<BIGNUM>(
		BN_bin2bn(response.pubkey_y, FP_PK_EC_PUBLIC_KEY_LEN, nullptr));
	if (y_bn == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_set_affine_coordinates_GFp(group.get(), public_point.get(),
						x_bn.get(), y_bn.get(),
						nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_mul(group.get(), public_point.get(), nullptr,
			 public_point.get(), n.get(), nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_get_affine_coordinates_GFp(group.get(), public_point.get(),
						x_bn.get(), y_bn.get(),
						nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(x_bn.get(), share_secret,
			 FP_POSITIVE_MATCH_SECRET_BYTES) != FP_PK_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	SHA256_init(&ctx);
	SHA256_update(&ctx, share_secret, FP_POSITIVE_MATCH_SECRET_BYTES);

	TEST_EQ(AES_set_encrypt_key(SHA256_final(&ctx), 256, &aes_key), 0,
		"%d");

	memcpy(aes_iv, response.iv, FP_EC_PUBLIC_KEY_IV_LEN);

	CRYPTO_ctr128_encrypt(response.enc_secret, response.enc_secret,
			      FP_CONTEXT_USERID_LEN, &aes_key, aes_iv,
			      ecount_buf, &block_num, (block128_f)AES_encrypt);
	TEST_ASSERT_ARRAY_EQ(
		response.enc_secret,
		expected_positive_match_secret_for_empty_user_id,
		static_cast<int>(sizeof(
			expected_positive_match_secret_for_empty_user_id)));

	return EC_SUCCESS;
}

extern "C" void run_test(int argc, const char **argv)
{
	RUN_TEST(test_fp_command_establish_pk_without_seed);
	RUN_TEST(test_set_fp_tpm_seed);
	RUN_TEST(test_fp_command_establish_pk_fail);
	RUN_TEST(test_fp_command_establish_and_load_pk);
	RUN_TEST(test_fp_command_load_pk_fail);
	RUN_TEST(test_fp_command_generate_nonce);
	RUN_TEST(test_fp_command_nonce_context);
	RUN_TEST(test_fp_command_nonce_context_clear);
	RUN_TEST(test_fp_command_nonce_context_limit);
	RUN_TEST(test_fp_command_read_match_secret_with_pubkey_succeed);
	test_print_result();
}
