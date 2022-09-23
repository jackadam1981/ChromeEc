/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_commands.h"
#include "fpsensor_state.h"
#include "mock/fpsensor_state_mock.h"
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/obj_mac.h"
#include "test_util.h"
#include "util.h"

extern "C" {
#include "trng.h"
}

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

	if (group.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> n(
		BN_bin2bn(privkey, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (n.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	if (public_point.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_mul(group.get(), public_point.get(), n.get(), nullptr,
			 nullptr, nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> x_bn(BN_new());

	if (x_bn.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> y_bn(BN_new());

	if (y_bn.get() == nullptr) {
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
	test_print_result();
}
