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
	uint8_t x[32] = {
		0x85, 0xAD, 0x35, 0x23, 0x05, 0x1E, 0x33, 0x3F,
		0xCA, 0xA7, 0xEA, 0xA5, 0x88, 0x33, 0x12, 0x95,
		0xA7, 0xB5, 0x98, 0x9F, 0x32, 0xEF, 0x7D, 0xE9,
		0xF8, 0x70, 0x14, 0x5E, 0x89, 0xCB, 0xDE, 0x1F,
	};
	uint8_t y[32] = {
		0xD1, 0xDC, 0x91, 0xC6, 0xE6, 0x5B, 0x1E, 0x3C,
		0x01, 0x6C, 0xE6, 0x50, 0x25, 0x5D, 0x89, 0xCF,
		0xB7, 0x8D, 0x88, 0xB9, 0x0D, 0x09, 0x41, 0xF1,
		0x09, 0x4F, 0x61, 0x55, 0x6C, 0xC4, 0x96, 0x6B,
	};
	bssl::UniquePtr<BIGNUM> x_bn(BN_bin2bn(x, sizeof(x), nullptr));
	TEST_NE(x_bn.get(), nullptr, "%p");

	bssl::UniquePtr<BIGNUM> y_bn(BN_bin2bn(y, sizeof(y), nullptr));
	TEST_NE(y_bn.get(), nullptr, "%p");

	bssl::UniquePtr<EC_KEY> key(
		EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));

	TEST_EQ(EC_KEY_set_public_key_affine_coordinates(key.get(), x_bn.get(),
							 y_bn.get()),
		1, "%d");

	struct ec_fp_ec_public_key pubkey;

	TEST_EQ(fill_pubkey(*key, pubkey), EC_SUCCESS, "%d");

	TEST_ASSERT_ARRAY_EQ(pubkey.x, x, sizeof(x));
	TEST_ASSERT_ARRAY_EQ(pubkey.y, y, sizeof(y));

	return EC_SUCCESS;
}

test_static enum ec_error_list test_fp_auth_command_encrypt_decrypt_data(void)
{
	struct ec_fp_auth_command_encryption_metadata info;
	uint8_t input[32] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5,
			      6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };
	uint16_t version = 1234;
	uint8_t data[32];

	memcpy(data, input, sizeof(data));

	TEST_EQ(encrypt_data_in_place(version, info, data, sizeof(data)),
		EC_SUCCESS, "%d");

	TEST_EQ(info.struct_version, version, "%d");

	/* The encrypted data should not be the same as the input. */
	TEST_ASSERT_ARRAY_NE(data, input, sizeof(data));

	uint8_t output[32];
	TEST_EQ(decrypt_data(info, data, sizeof(data), output, sizeof(output)),
		EC_SUCCESS, "%d");

	TEST_ASSERT_ARRAY_EQ(input, output, sizeof(input));

	return EC_SUCCESS;
}

test_static enum ec_error_list test_fp_auth_command_encrypt_decrypt_key(void)
{
	struct ec_fp_encrypted_private_key enc_key;
	uint16_t version = 1234;
	uint8_t privkey[32] = { 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5,
				6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };

	bssl::UniquePtr<EC_KEY> key(
		EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));

	TEST_NE(key.get(), nullptr, "%p");

	TEST_EQ(EC_KEY_oct2priv(key.get(), privkey, sizeof(privkey)), 1, "%d");

	TEST_EQ(fill_encrypted_private_key(*key, version, enc_key), EC_SUCCESS,
		"%d");

	TEST_EQ(enc_key.info.struct_version, version, "%d");

	bssl::UniquePtr<EC_KEY> out_key = decrypt_private_key(enc_key);

	TEST_NE(key.get(), nullptr, "%p");

	uint8_t output_privkey[32];
	TEST_EQ(EC_KEY_priv2oct(out_key.get(), output_privkey,
				sizeof(output_privkey)),
		sizeof(output_privkey), "%lu");

	TEST_ASSERT_ARRAY_EQ(privkey, output_privkey, sizeof(privkey));

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

} // namespace

extern "C" void run_test(int argc, const char **argv)
{
	RUN_TEST(test_fp_auth_command_create_ec_key_from_pubkey);
	RUN_TEST(test_fp_auth_command_create_ec_key_from_pubkey_fail);
	RUN_TEST(test_fp_auth_command_fill_pubkey);
	RUN_TEST(test_fp_command_establish_pairing_key_without_seed);
	RUN_TEST(test_set_fp_tpm_seed);
	RUN_TEST(test_fp_auth_command_encrypt_decrypt_data);
	RUN_TEST(test_fp_auth_command_encrypt_decrypt_key);
	RUN_TEST(test_fp_command_establish_pairing_key_keygen);
	RUN_TEST(test_fp_command_establish_pairing_key_fail);
	RUN_TEST(test_fp_command_establish_and_load_pairing_key);
	RUN_TEST(test_fp_command_load_pairing_key_fail);
	test_print_result();
}
