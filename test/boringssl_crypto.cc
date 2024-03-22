/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "crypto/cleanse_wrapper.h"
#include "crypto/elliptic_curve_key.h"
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/mem.h"
#include "openssl/obj_mac.h"
#include "openssl/rand.h"
#include "test_util.h"
#include "util.h"

extern "C" {
#include "fpsensor/fpsensor_state_without_driver_info.h"
#include "sha256.h"
}

#include <array>
#include <memory>

test_static enum ec_error_list test_rand(void)
{
	constexpr uint8_t zero[256] = { 0 };
	uint8_t buf1[256];
	uint8_t buf2[256];

	RAND_bytes(buf1, sizeof(buf1));
	RAND_bytes(buf2, sizeof(buf2));

	TEST_ASSERT_ARRAY_NE(buf1, zero, sizeof(zero));
	TEST_ASSERT_ARRAY_NE(buf2, zero, sizeof(zero));
	TEST_ASSERT_ARRAY_NE(buf1, buf2, sizeof(buf1));

	return EC_SUCCESS;
}

test_static enum ec_error_list test_ecc_keygen(void)
{
	bssl::UniquePtr<EC_KEY> key1 = generate_elliptic_curve_key();

	TEST_NE(key1.get(), nullptr, "%p");

	/* The generated key should be valid.*/
	TEST_EQ(EC_KEY_check_key(key1.get()), 1, "%d");

	bssl::UniquePtr<EC_KEY> key2 = generate_elliptic_curve_key();

	TEST_NE(key2.get(), nullptr, "%p");

	/* The generated key should be valid. */
	TEST_EQ(EC_KEY_check_key(key2.get()), 1, "%d");

	const BIGNUM *priv1 = EC_KEY_get0_private_key(key1.get());
	const BIGNUM *priv2 = EC_KEY_get0_private_key(key2.get());

	/* The generated keys should not be the same. */
	TEST_NE(BN_cmp(priv1, priv2), 0, "%d");

	/* The generated keys should not be zero. */
	TEST_EQ(BN_is_zero(priv1), 0, "%d");
	TEST_EQ(BN_is_zero(priv2), 0, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list test_cleanse_wrapper_std_array(void)
{
	using TypeToCheck = std::array<uint8_t, 6>;
	using Wrapped = CleanseWrapper<TypeToCheck>;

	/* Preserve a space for it. */
	uint8_t *buffer = static_cast<uint8_t *>(malloc(sizeof(Wrapped)));
	Wrapped *data = reinterpret_cast<Wrapped *>(buffer);

	/* Call the constructor. */
	new (data) Wrapped({ 1, 1, 1, 1, 1, 1 });

	TEST_ASSERT_MEMSET(data->data(), 1, data->size());

	/* Call the destructor. */
	data->~Wrapped();

	TEST_ASSERT_MEMSET(buffer, 0, sizeof(Wrapped));

	/* Free the space. */
	free(buffer);

	return EC_SUCCESS;
}

test_static enum ec_error_list test_cleanse_wrapper_sha256(void)
{
	using TypeToCheck = struct sha256_ctx;
	using Wrapped = CleanseWrapper<TypeToCheck>;

	/* Preserve a space for it. */
	uint8_t *buffer = static_cast<uint8_t *>(malloc(sizeof(Wrapped)));
	Wrapped *data = reinterpret_cast<Wrapped *>(buffer);

	/* Call the constructor. */
	new (data) Wrapped();

	std::array<uint8_t, 5> data_to_sha = { 1, 2, 3, 4, 5 };
	SHA256_init(data);
	SHA256_update(data, data_to_sha.data(), data_to_sha.size());
	uint8_t *result = SHA256_final(data);

	std::array<uint8_t, 32> expected_result = {
		0X74, 0XF8, 0X1F, 0XE1, 0X67, 0XD9, 0X9B, 0X4C,
		0XB4, 0X1D, 0X6D, 0X0C, 0XCD, 0XA8, 0X22, 0X78,
		0XCA, 0XEE, 0X9F, 0X3E, 0X2F, 0X25, 0XD5, 0XE5,
		0XA3, 0X93, 0X6F, 0XF3, 0XDC, 0XEC, 0X60, 0XD0,
	};

	TEST_ASSERT_ARRAY_EQ(result, expected_result, expected_result.size());

	/* Call the destructor. */
	data->~Wrapped();

	TEST_ASSERT_MEMSET(buffer, 0, sizeof(Wrapped));

	/* Free the space. */
	free(buffer);

	return EC_SUCCESS;
}

test_static enum ec_error_list test_cleanse_wrapper_custom_struct(void)
{
	struct TestingStruct {
		bool used;
		std::array<uint32_t, 4> data;
	};

	using TypeToCheck = TestingStruct;
	using Wrapped = CleanseWrapper<TypeToCheck>;

	/* Preserve a space for it. */
	uint8_t *buffer = static_cast<uint8_t *>(malloc(sizeof(Wrapped)));
	Wrapped *data = reinterpret_cast<Wrapped *>(buffer);

	/* Call the constructor. */
	new (data) Wrapped({
		.used = true,
		.data = { 0x7fffffffu, 0x12345678u, 0x0u, 0x42u },
	});

	TEST_EQ(data->used, true, "%d");
	TEST_EQ(data->data[0], 0x7fffffffu, "%d");
	TEST_EQ(data->data[1], 0x12345678u, "%d");
	TEST_EQ(data->data[2], 0x0u, "%d");
	TEST_EQ(data->data[3], 0x42u, "%d");

	/* Call the destructor. */
	data->~Wrapped();

	TEST_ASSERT_MEMSET(buffer, 0, sizeof(Wrapped));

	/* Free the space. */
	free(buffer);

	return EC_SUCCESS;
}

test_static enum ec_error_list test_cleanse_wrapper_normal_usage(void)
{
	struct TestingStruct {
		bool used;
		std::array<uint32_t, 4> data;
	};

	CleanseWrapper<std::array<uint8_t, 6> > array({ 1, 1, 1, 1, 1, 1 });

	TEST_ASSERT_MEMSET(array.data(), 1, array.size());

	CleanseWrapper<TestingStruct> data({
		.used = true,
		.data = { 0x7fffffffu, 0x12345678u, 0x0u, 0x42u },
	});

	TEST_EQ(data.used, true, "%d");
	TEST_EQ(data.data[0], 0x7fffffffu, "%d");
	TEST_EQ(data.data[1], 0x12345678u, "%d");
	TEST_EQ(data.data[2], 0x0u, "%d");
	TEST_EQ(data.data[3], 0x42u, "%d");

	CleanseWrapper<struct sha256_ctx> ctx;

	std::array<uint8_t, 5> data_to_sha = { 1, 2, 3, 4, 5 };
	SHA256_init(&ctx);
	SHA256_update(&ctx, data_to_sha.data(), data_to_sha.size());
	uint8_t *result = SHA256_final(&ctx);

	std::array<uint8_t, 32> expected_result = {
		0X74, 0XF8, 0X1F, 0XE1, 0X67, 0XD9, 0X9B, 0X4C,
		0XB4, 0X1D, 0X6D, 0X0C, 0XCD, 0XA8, 0X22, 0X78,
		0XCA, 0XEE, 0X9F, 0X3E, 0X2F, 0X25, 0XD5, 0XE5,
		0XA3, 0X93, 0X6F, 0XF3, 0XDC, 0XEC, 0X60, 0XD0,
	};

	TEST_ASSERT_ARRAY_EQ(result, expected_result, expected_result.size());

	/* There is no way to check the context is cleared without undefined
	 * behavior. */

	return EC_SUCCESS;
}

static enum ec_error_list
hkdf_expand_one_step(uint8_t *out_key, size_t out_key_size, const uint8_t *prk,
		     size_t prk_size, const uint8_t *info, size_t info_size)
{
	uint8_t key_buf[SHA256_DIGEST_SIZE];
	uint8_t message_buf[SHA256_DIGEST_SIZE + 1];

	if (out_key_size > SHA256_DIGEST_SIZE) {
		ccprints("Deriving key material longer than SHA256_DIGEST_SIZE "
			 "requires more steps of HKDF expand.");
		return EC_ERROR_INVAL;
	}

	if (info_size > SHA256_DIGEST_SIZE) {
		ccprints("Info size too big for HKDF.");
		return EC_ERROR_INVAL;
	}

	memcpy(message_buf, info, info_size);
	/* 1 step, set the counter byte to 1. */
	message_buf[info_size] = 0x01;
	hmac_SHA256(key_buf, prk, prk_size, message_buf, info_size + 1);

	memcpy(out_key, key_buf, out_key_size);

	return EC_SUCCESS;
}

test_static enum ec_error_list test_hkdf(void)
{
	uint8_t prk[SHA256_DIGEST_SIZE];
	uint8_t out_key[SBP_ENC_KEY_LEN];

	static const uint8_t ikm[] = {
		0xcf, 0xe3, 0x23, 0x76, 0x35, 0x04, 0xc2, 0x0f, 0x0d, 0xb6,
		0x02, 0xa9, 0x68, 0xba, 0x2a, 0x61, 0x86, 0x2a, 0x85, 0xd1,
		0xca, 0x09, 0x54, 0x8a, 0x6b, 0xe2, 0xe3, 0x38, 0xde, 0x5d,
		0x59, 0x14, 0xd9, 0x71, 0xaf, 0xc4, 0xcd, 0x36, 0xe3, 0x60,
		0xf8, 0x5a, 0xa0, 0xa6, 0x2c, 0xb3, 0xf5, 0xe2, 0xeb, 0xb9,
		0xd8, 0x2f, 0xb5, 0x78, 0x5c, 0x79, 0x82, 0xce, 0x06, 0x3f,
		0xcc, 0x23, 0xb9, 0xe7, 0x46, 0x71, 0x32, 0x2d, 0x02, 0xe3,
		0x85, 0xc7, 0x6b, 0x78, 0xd4, 0x6e, 0x0d, 0x6c, 0xcc, 0x75,
		0x83, 0x62, 0x35, 0x3a, 0x53, 0xb7, 0x80, 0x10, 0x79, 0xfa,
		0x9a, 0xe4, 0xdb, 0x97, 0x96, 0x6d
	};

	static const uint8_t salt1[] = {
		0xd0, 0x88, 0x34, 0x15, 0xc0, 0xfa, 0x8e, 0x22,
		0x9f, 0xb4, 0xd5, 0xa9, 0xee, 0xd3, 0x15, 0x19,
	};

	static const uint32_t user_id1[] = {
		0x608b1b0b, 0xe10d3d24, 0x0bbbe4e6, 0x807b36d9,
		0x2a1f8abc, 0xea38104a, 0x562d9431, 0x64d721c5,
	};

	static const uint32_t user_id2[] = {
		0x2546a2ca, 0xf1891f7a, 0x44aad8b8, 0x0d6aac74,
		0x6a4ab846, 0x9c279796, 0x5a72eae1, 0x8276d2a3,
	};

	static const uint8_t salt2[] = {
		0x72, 0x6b, 0xc1, 0xe4, 0x64, 0xd4, 0xff, 0xa2,
		0x5a, 0xac, 0x5b, 0x0b, 0x06, 0x67, 0xe1, 0x53,
	};

	static const uint8_t key1[] = {
		0xf8, 0x7b, 0x12, 0x83, 0xc0, 0xee, 0x73, 0x36,
		0x20, 0xc8, 0xff, 0xf0, 0xef, 0xa1, 0xc9, 0x3b,
	};

	static const uint8_t key2[] = { 0xa3, 0x38, 0x1e, 0x4e, 0x60, 0xf1,
					0xd4, 0xd3, 0xf5, 0x44, 0xbc, 0xe0,
					0xfb, 0x4c, 0x87, 0x0a };

	hmac_SHA256(prk, salt1, sizeof(salt1), ikm, sizeof(ikm));
	TEST_ASSERT(hkdf_expand_one_step(out_key, SBP_ENC_KEY_LEN, prk,
					 sizeof(prk), (uint8_t *)user_id1,
					 sizeof(user_id1)) == EC_SUCCESS);
	TEST_ASSERT_ARRAY_EQ(out_key, key1, sizeof(key1));

	hmac_SHA256(prk, salt2, sizeof(salt2), ikm, sizeof(ikm));
	TEST_ASSERT(hkdf_expand_one_step(out_key, SBP_ENC_KEY_LEN, prk,
					 sizeof(prk), (uint8_t *)user_id2,
					 sizeof(user_id2)) == EC_SUCCESS);
	TEST_ASSERT_ARRAY_EQ(out_key, key2, sizeof(key2));

	return EC_SUCCESS;
}

extern "C" void run_test(int argc, const char **argv)
{
	RUN_TEST(test_rand);
	RUN_TEST(test_ecc_keygen);
	RUN_TEST(test_cleanse_wrapper_std_array);
	RUN_TEST(test_cleanse_wrapper_sha256);
	RUN_TEST(test_cleanse_wrapper_custom_struct);
	RUN_TEST(test_cleanse_wrapper_normal_usage);
	RUN_TEST(test_hkdf);
	test_print_result();
}
