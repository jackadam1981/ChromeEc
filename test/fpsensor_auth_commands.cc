/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_commands.h"
#include "fpsensor_auth_commands.h"
#include "fpsensor_state.h"
#include "mock/fpsensor_state_mock.h"
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/obj_mac.h"
#include "test_util.h"
#include "util.h"

#include <stdbool.h>

namespace
{

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

test_static enum ec_error_list
test_fp_command_establish_pairing_key_without_seed(void)
{
	enum ec_status rv;
	struct ec_response_fp_encryption_status resp = { 0 };
	struct ec_response_fp_establish_pairing_key_keygen keygen_response;

	/* Seed shouldn't have been set. */
	rv = test_send_host_command(EC_CMD_FP_ENC_STATUS, 0, NULL, 0, &resp,
				    sizeof(resp));

	TEST_EQ(check_seed_set_result(rv, 0, &resp), EC_SUCCESS, "%d");

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PAIRING_KEY_KEYGEN, 0,
				    NULL, 0, &keygen_response,
				    sizeof(keygen_response));

	TEST_NE(rv, EC_RES_SUCCESS, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list
test_fp_command_establish_pairing_key_keygen(void)
{
	enum ec_status rv;
	struct ec_response_fp_establish_pairing_key_keygen keygen_response;

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PAIRING_KEY_KEYGEN, 0,
				    NULL, 0, &keygen_response,
				    sizeof(keygen_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	bssl::UniquePtr<EC_KEY> pubkey =
		create_ec_key_from_pubkey(keygen_response.pubkey);

	TEST_NE(pubkey.get(), nullptr, "%p");
	TEST_EQ(EC_KEY_check_key(pubkey.get()), 1, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list
test_fp_command_establish_and_load_pairing_key(void)
{
	enum ec_status rv;
	struct ec_response_fp_establish_pairing_key_keygen keygen_response;
	struct ec_params_fp_establish_pairing_key_wrap wrap_params;
	struct ec_response_fp_establish_pairing_key_wrap wrap_response;
	struct ec_params_fp_load_pairing_key load_params;

	uint8_t privkey[FP_EC_PRIVATE_KEY_LEN];

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PAIRING_KEY_KEYGEN, 0,
				    NULL, 0, &keygen_response,
				    sizeof(keygen_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	memcpy(&wrap_params.encrypted_private_key.info,
	       &keygen_response.encrypted_private_key.info,
	       sizeof(keygen_response.encrypted_private_key.info));

	memcpy(wrap_params.encrypted_private_key.data,
	       keygen_response.encrypted_private_key.data,
	       sizeof(keygen_response.encrypted_private_key.data));

	memset(privkey, 'X', sizeof(privkey));

	bssl::UniquePtr<EC_GROUP> group(
		EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

	TEST_NE(group.get(), nullptr, "%p");

	bssl::UniquePtr<BIGNUM> n(
		BN_bin2bn(privkey, FP_EC_PRIVATE_KEY_LEN, nullptr));

	TEST_NE(n.get(), nullptr, "%p");

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	TEST_NE(public_point.get(), nullptr, "%p");

	TEST_EQ(EC_POINT_mul(group.get(), public_point.get(), n.get(), nullptr,
			     nullptr, nullptr),
		1, "%d");

	bssl::UniquePtr<BIGNUM> x_bn(BN_new());

	TEST_NE(x_bn.get(), nullptr, "%p");

	bssl::UniquePtr<BIGNUM> y_bn(BN_new());

	TEST_NE(y_bn.get(), nullptr, "%p");

	TEST_EQ(EC_POINT_get_affine_coordinates_GFp(
			group.get(), public_point.get(), x_bn.get(), y_bn.get(),
			nullptr),
		1, "%d");

	TEST_EQ(BN_bn2binpad(x_bn.get(), wrap_params.peers_pubkey.x,
			     FP_EC_PUBLIC_KEY_POINT_LEN),
		FP_EC_PUBLIC_KEY_POINT_LEN, "%d");

	TEST_EQ(BN_bn2binpad(y_bn.get(), wrap_params.peers_pubkey.y,
			     FP_EC_PUBLIC_KEY_POINT_LEN),
		FP_EC_PUBLIC_KEY_POINT_LEN, "%d");

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PAIRING_KEY_WRAP, 0,
				    &wrap_params, sizeof(wrap_params),
				    &wrap_response, sizeof(wrap_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	memcpy(&load_params.encrypted_pairing_key.info,
	       &wrap_response.encrypted_pairing_key.info,
	       sizeof(wrap_response.encrypted_pairing_key.info));

	memcpy(load_params.encrypted_pairing_key.data,
	       wrap_response.encrypted_pairing_key.data,
	       sizeof(wrap_response.encrypted_pairing_key.data));

	rv = test_send_host_command(EC_CMD_FP_LOAD_PAIRING_KEY, 0, &load_params,
				    sizeof(load_params), NULL, 0);

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list test_fp_command_establish_pairing_key_fail(void)
{
	enum ec_status rv;
	struct ec_response_fp_establish_pairing_key_keygen keygen_response;
	struct ec_params_fp_establish_pairing_key_wrap wrap_params;
	struct ec_response_fp_establish_pairing_key_wrap wrap_response;

	uint8_t privkey[FP_EC_PRIVATE_KEY_LEN];

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PAIRING_KEY_KEYGEN, 0,
				    NULL, 0, &keygen_response,
				    sizeof(keygen_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	/* No encryption info. */
	memset(&wrap_params.encrypted_private_key.info, 0,
	       sizeof(wrap_params.encrypted_private_key.info));

	memcpy(wrap_params.encrypted_private_key.data,
	       keygen_response.encrypted_private_key.data,
	       sizeof(keygen_response.encrypted_private_key.data));

	memset(privkey, 'X', sizeof(privkey));

	bssl::UniquePtr<EC_GROUP> group(
		EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

	TEST_NE(group.get(), nullptr, "%p");

	bssl::UniquePtr<BIGNUM> n(
		BN_bin2bn(privkey, FP_EC_PRIVATE_KEY_LEN, nullptr));

	TEST_NE(n.get(), nullptr, "%p");

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	TEST_NE(public_point.get(), nullptr, "%p");

	TEST_EQ(EC_POINT_mul(group.get(), public_point.get(), n.get(), nullptr,
			     nullptr, nullptr),
		1, "%d");

	bssl::UniquePtr<BIGNUM> x_bn(BN_new());

	TEST_NE(x_bn.get(), nullptr, "%p");

	bssl::UniquePtr<BIGNUM> y_bn(BN_new());

	TEST_NE(y_bn.get(), nullptr, "%p");

	TEST_EQ(EC_POINT_get_affine_coordinates_GFp(
			group.get(), public_point.get(), x_bn.get(), y_bn.get(),
			nullptr),
		1, "%d");

	TEST_EQ(BN_bn2binpad(x_bn.get(), wrap_params.peers_pubkey.x,
			     FP_EC_PUBLIC_KEY_POINT_LEN),
		FP_EC_PUBLIC_KEY_POINT_LEN, "%d");

	TEST_EQ(BN_bn2binpad(y_bn.get(), wrap_params.peers_pubkey.y,
			     FP_EC_PUBLIC_KEY_POINT_LEN),
		FP_EC_PUBLIC_KEY_POINT_LEN, "%d");

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PAIRING_KEY_WRAP, 0,
				    &wrap_params, sizeof(wrap_params),
				    &wrap_response, sizeof(wrap_response));

	TEST_NE(rv, EC_RES_SUCCESS, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list test_fp_command_load_pairing_key_fail(void)
{
	enum ec_status rv;
	struct ec_response_fp_establish_pairing_key_keygen keygen_response;
	struct ec_params_fp_establish_pairing_key_wrap wrap_params;
	struct ec_response_fp_establish_pairing_key_wrap wrap_response;
	struct ec_params_fp_load_pairing_key load_params;

	uint8_t privkey[FP_EC_PRIVATE_KEY_LEN];

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PAIRING_KEY_KEYGEN, 0,
				    NULL, 0, &keygen_response,
				    sizeof(keygen_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	memcpy(&wrap_params.encrypted_private_key.info,
	       &keygen_response.encrypted_private_key.info,
	       sizeof(keygen_response.encrypted_private_key.info));

	memcpy(wrap_params.encrypted_private_key.data,
	       keygen_response.encrypted_private_key.data,
	       sizeof(keygen_response.encrypted_private_key.data));

	memset(privkey, 'X', sizeof(privkey));

	bssl::UniquePtr<EC_GROUP> group(
		EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

	TEST_NE(group.get(), nullptr, "%p");

	bssl::UniquePtr<BIGNUM> n(
		BN_bin2bn(privkey, FP_EC_PRIVATE_KEY_LEN, nullptr));

	TEST_NE(n.get(), nullptr, "%p");

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	TEST_NE(public_point.get(), nullptr, "%p");

	TEST_EQ(EC_POINT_mul(group.get(), public_point.get(), n.get(), nullptr,
			     nullptr, nullptr),
		1, "%d");

	bssl::UniquePtr<BIGNUM> x_bn(BN_new());

	TEST_NE(x_bn.get(), nullptr, "%p");

	bssl::UniquePtr<BIGNUM> y_bn(BN_new());

	TEST_NE(y_bn.get(), nullptr, "%p");

	TEST_EQ(EC_POINT_get_affine_coordinates_GFp(
			group.get(), public_point.get(), x_bn.get(), y_bn.get(),
			nullptr),
		1, "%d");

	TEST_EQ(BN_bn2binpad(x_bn.get(), wrap_params.peers_pubkey.x,
			     FP_EC_PUBLIC_KEY_POINT_LEN),
		FP_EC_PUBLIC_KEY_POINT_LEN, "%d");

	TEST_EQ(BN_bn2binpad(y_bn.get(), wrap_params.peers_pubkey.y,
			     FP_EC_PUBLIC_KEY_POINT_LEN),
		FP_EC_PUBLIC_KEY_POINT_LEN, "%d");

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PAIRING_KEY_WRAP, 0,
				    &wrap_params, sizeof(wrap_params),
				    &wrap_response, sizeof(wrap_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	/* No encryption info. */
	memset(&load_params.encrypted_pairing_key.info, 0,
	       sizeof(load_params.encrypted_pairing_key.info));

	memcpy(load_params.encrypted_pairing_key.data,
	       wrap_response.encrypted_pairing_key.data,
	       sizeof(wrap_response.encrypted_pairing_key.data));

	rv = test_send_host_command(EC_CMD_FP_LOAD_PAIRING_KEY, 0, &load_params,
				    sizeof(load_params), NULL, 0);

	TEST_NE(rv, EC_RES_SUCCESS, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list test_fp_command_generate_nonce(void)
{
	enum ec_status rv;
	struct ec_response_fp_generate_nonce nonce_response;

	rv = test_send_host_command(EC_CMD_FP_GENERATE_NONCE, 0, NULL, 0,
				    &nonce_response, sizeof(nonce_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list test_fp_command_nonce_context(void)
{
	enum ec_status rv;
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

test_static enum ec_error_list test_fp_command_nonce_context_clear(void)
{
	enum ec_status rv;
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

test_static enum ec_error_list test_fp_command_nonce_context_limit(void)
{
	enum ec_status rv;
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

} // namespace

extern "C" void run_test(int argc, const char **argv)
{
	RUN_TEST(test_fp_command_establish_pairing_key_without_seed);
	RUN_TEST(test_set_fp_tpm_seed);
	RUN_TEST(test_fp_command_establish_pairing_key_keygen);
	RUN_TEST(test_fp_command_establish_pairing_key_fail);
	RUN_TEST(test_fp_command_establish_and_load_pairing_key);
	RUN_TEST(test_fp_command_load_pairing_key_fail);
	RUN_TEST(test_fp_command_generate_nonce);
	RUN_TEST(test_fp_command_nonce_context);
	RUN_TEST(test_fp_command_nonce_context_clear);
	RUN_TEST(test_fp_command_nonce_context_limit);
	test_print_result();
}
