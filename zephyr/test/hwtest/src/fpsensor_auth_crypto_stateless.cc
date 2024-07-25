/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "crypto/elliptic_curve_key.h"
#include "ec_commands.h"
#include "fpsensor/fpsensor_auth_crypto.h"
#include "openssl/aes.h"
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/obj_mac.h"
#include "rollback.h"

#include <assert.h>
#include <stdbool.h>

#include <zephyr/ztest.h>

ZTEST_SUITE(fpsensor_auth_crypto_stateless, NULL, NULL, NULL, NULL, NULL);

ZTEST(fpsensor_auth_crypto_stateless, test_fp_create_ec_key_from_pubkey)
{
	fp_elliptic_curve_public_key pubkey = {
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

	zassert_not_equal(key.get(), nullptr, "%p");
	zassert_equal(EC_KEY_check_key(key.get()), 1, "%d");
}

ZTEST(fpsensor_auth_crypto_stateless, test_fp_create_ec_key_from_pubkey_fail)
{
	fp_elliptic_curve_public_key pubkey = {
		.x = {},
		.y = {},
	};

	bssl::UniquePtr<EC_KEY> key = create_ec_key_from_pubkey(pubkey);

	zassert_equal(key.get(), nullptr, "%p");
}

ZTEST(fpsensor_auth_crypto_stateless, test_fp_create_pubkey_from_ec_key)
{
	fp_elliptic_curve_public_key pubkey = {
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

	zassert_not_equal(key.get(), nullptr, "%p");
	zassert_equal(EC_KEY_check_key(key.get()), 1, "%d");

	auto result = create_pubkey_from_ec_key(*key);
	zassert_true(result.has_value());

	zassert_mem_equal(result->x, pubkey.x, sizeof(pubkey.x));
	zassert_mem_equal(result->y, pubkey.y, sizeof(pubkey.y));
}

ZTEST(fpsensor_auth_crypto_stateless, test_fp_create_ec_key_from_privkey)
{
	std::array<uint8_t, 32> data = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
					 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
					 2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };

	bssl::UniquePtr<EC_KEY> key =
		create_ec_key_from_privkey(data.data(), data.size());

	zassert_not_equal(key.get(), nullptr, "%p");

	/* There is nothing to check for the private key. */
}

ZTEST(fpsensor_auth_crypto_stateless, test_fp_create_ec_key_from_privkey_fail)
{
	std::array<uint8_t, 1> data = {};

	bssl::UniquePtr<EC_KEY> key =
		create_ec_key_from_privkey(data.data(), data.size());

	zassert_equal(key.get(), nullptr, "%p");
}

ZTEST(fpsensor_auth_crypto_stateless, test_fp_generate_ecdh_shared_secret)
{
	struct fp_elliptic_curve_public_key pubkey = {
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

	bssl::UniquePtr<EC_KEY> public_key = create_ec_key_from_pubkey(pubkey);

	zassert_not_equal(public_key.get(), nullptr, "%p");

	std::array<uint8_t, 32> privkey = { 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
					    1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
					    2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };

	bssl::UniquePtr<EC_KEY> private_key =
		create_ec_key_from_privkey(privkey.data(), privkey.size());

	zassert_not_equal(private_key.get(), nullptr, "%p");

	std::array<uint8_t, 32> shared_secret;
	zassert_equal(generate_ecdh_shared_secret(*private_key, *public_key,
						  shared_secret.data(),
						  shared_secret.size()),
		      EC_SUCCESS, "%d");

	std::array<uint8_t, 32> expected_result = {
		0x46, 0x86, 0xca, 0x75, 0xce, 0xa1, 0xde, 0x23,
		0x48, 0xb3, 0x0b, 0xfc, 0xd7, 0xbe, 0x7a, 0xa0,
		0x33, 0x17, 0x6c, 0x97, 0xc6, 0xa7, 0x70, 0x7c,
		0xd4, 0x2c, 0xfd, 0xc0, 0xba, 0xc1, 0x47, 0x01,
	};

	zassert_mem_equal(shared_secret.data(), expected_result.data(),
			  shared_secret.size());
}

ZTEST(fpsensor_auth_crypto_stateless, test_fp_generate_gsc_session_key)
{
	std::array<uint8_t, 32> auth_nonce = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
					       1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
					       2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };
	std::array<uint8_t, 32> gsc_nonce = { 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
					      1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
					      2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };
	std::array<uint8_t, 32> pairing_key = { 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
						1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
						2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };

	std::array<uint8_t, 32> gsc_session_key;

	zassert_equal(generate_gsc_session_key(auth_nonce, gsc_nonce,
					       pairing_key, gsc_session_key),
		      EC_SUCCESS, "%d");

	std::array<uint8_t, 32> expected_gsc_session_key = {
		0X1A, 0X1A, 0X3C, 0X33, 0X7F, 0XAE, 0XF9, 0X3E,
		0XA8, 0X7C, 0XE4, 0XEC, 0XD9, 0XFF, 0X45, 0X8A,
		0XB6, 0X2F, 0X75, 0XD5, 0XEA, 0X25, 0X93, 0X36,
		0X60, 0XF1, 0XAB, 0XD2, 0XF4, 0X9F, 0X22, 0X89,
	};

	zassert_mem_equal(gsc_session_key.data(),
			  expected_gsc_session_key.data(),
			  gsc_session_key.size());
}

ZTEST(fpsensor_auth_crypto_stateless, test_fp_generate_gsc_session_key_fail)
{
	std::array<uint8_t, 32> auth_nonce = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
					       1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
					       2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };
	std::array<uint8_t, 32> gsc_nonce = { 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
					      1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
					      2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };
	std::array<uint8_t, 32> pairing_key = { 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
						1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
						2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };

	/* Wrong gsc_session_key size. */
	std::array<uint8_t, 30> gsc_session_key;

	zassert_not_equal(generate_gsc_session_key(auth_nonce, gsc_nonce,
						   pairing_key,
						   gsc_session_key),
			  EC_SUCCESS, "%d");
}

ZTEST(fpsensor_auth_crypto_stateless,
      test_fp_decrypt_data_with_gsc_session_key_in_place)
{
	std::array<uint8_t, 32> gsc_session_key = {
		0X1A, 0X1A, 0X3C, 0X33, 0X7F, 0XAE, 0XF9, 0X3E,
		0XA8, 0X7C, 0XE4, 0XEC, 0XD9, 0XFF, 0X45, 0X8A,
		0XB6, 0X2F, 0X75, 0XD5, 0XEA, 0X25, 0X93, 0X36,
		0X60, 0XF1, 0XAB, 0XD2, 0XF4, 0X9F, 0X22, 0X89,
	};

	std::array<uint8_t, 16> iv = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5,
	};

	std::array<uint8_t, 32> data = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
					 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
					 2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };

	zassert_equal(decrypt_data_with_gsc_session_key_in_place(
			      gsc_session_key, iv, data),
		      EC_SUCCESS, "%d");

	std::array<uint8_t, 32> expected_data = {
		0X6D, 0XED, 0XAD, 0X04, 0XF8, 0XDB, 0XAE, 0X51,
		0XF8, 0XEE, 0X94, 0X7E, 0XDB, 0X12, 0X14, 0X22,
		0X38, 0X32, 0X27, 0XC5, 0X19, 0X72, 0XA3, 0X60,
		0X67, 0X71, 0X25, 0XE8, 0X27, 0X56, 0XC6, 0X35,
	};

	zassert_mem_equal(data.data(), expected_data.data(), data.size());
}

ZTEST(fpsensor_auth_crypto_stateless,
      test_fp_decrypt_data_with_gsc_session_key_in_place_fail)
{
	std::array<uint8_t, 32> gsc_session_key = {
		0X1A, 0X1A, 0X3C, 0X33, 0X7F, 0XAE, 0XF9, 0X3E,
		0XA8, 0X7C, 0XE4, 0XEC, 0XD9, 0XFF, 0X45, 0X8A,
		0XB6, 0X2F, 0X75, 0XD5, 0XEA, 0X25, 0X93, 0X36,
		0X60, 0XF1, 0XAB, 0XD2, 0XF4, 0X9F, 0X22, 0X89,
	};

	/* Wrong IV size. */
	std::array<uint8_t, 32> iv = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5,
	};

	std::array<uint8_t, 32> data = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
					 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1,
					 2, 3, 4, 5, 6, 7, 8, 9, 1, 2 };

	zassert_not_equal(decrypt_data_with_gsc_session_key_in_place(
				  gsc_session_key, iv, data),
			  EC_SUCCESS, "%d");
}

ZTEST(fpsensor_auth_crypto_stateless,
      test_fp_encrypt_data_with_ecdh_key_in_place)
{
	constexpr std::array<uint8_t, FP_ELLIPTIC_CURVE_PUBLIC_KEY_IV_LEN>
		zero_iv = { 0 };

	bssl::UniquePtr<EC_KEY> ecdh_key = generate_elliptic_curve_key();

	std::optional<fp_elliptic_curve_public_key> pubkey =
		create_pubkey_from_ec_key(*ecdh_key);

	zassert_true(pubkey.has_value());

	zassert_not_equal(ecdh_key.get(), nullptr, "%p");

	struct fp_elliptic_curve_public_key response_pubkey;

	std::array<uint8_t, FP_POSITIVE_MATCH_SECRET_BYTES> secret = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5,
		6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 2
	};

	std::array<uint8_t, FP_POSITIVE_MATCH_SECRET_BYTES> enc_secret = secret;

	std::array<uint8_t, FP_ELLIPTIC_CURVE_PUBLIC_KEY_IV_LEN> iv = {};

	zassert_mem_equal(iv.data(), zero_iv.data(), iv.size());

	zassert_equal(encrypt_data_with_ecdh_key_in_place(*pubkey, enc_secret,
							  iv, response_pubkey),
		      EC_SUCCESS, "%d");

	/* The encrypted data should not be the same as the input. */
	zassert_true(memcmp(enc_secret.data(), secret.data(), secret.size()) !=
		     0);

	/* The IV should not be zero. */
	zassert_true(memcmp(iv.data(), zero_iv.data(), iv.size()) != 0);

	bssl::UniquePtr<EC_KEY> output_key =
		create_ec_key_from_pubkey(response_pubkey);

	zassert_not_equal(output_key.get(), nullptr, "%p");

	std::array<uint8_t, 32> share_secret;
	zassert_equal(generate_ecdh_shared_secret(*ecdh_key, *output_key,
						  share_secret.data(),
						  share_secret.size()),
		      EC_SUCCESS, "%d");

	AES_KEY aes_key;
	zassert_equal(AES_set_encrypt_key(share_secret.data(), 256, &aes_key),
		      0, "%d");

	unsigned int block_num = 0;
	std::array<uint8_t, AES_BLOCK_SIZE> ecount_buf;

	/* The AES CTR uses the same function for encryption & decryption. */
	AES_ctr128_encrypt(enc_secret.data(), enc_secret.data(),
			   enc_secret.size(), &aes_key, iv.data(),
			   ecount_buf.data(), &block_num);

	/* The secret should be the same after decrypt. */
	zassert_mem_equal(enc_secret.data(), secret.data(), secret.size());
}
