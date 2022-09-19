/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "compile_time_macros.h"

/* Boringssl headers need to be included before extern "C" section. */
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/mem.h"
#include "openssl/obj_mac.h"

extern "C" {
#include "common.h"
#include "ec_commands.h"
#include "host_command.h"
#include "sha256.h"
#include "system.h"
#include "task.h"
#include "trng.h"
#include "util.h"
}

#include "fpsensor.h"
#include "fpsensor_crypto.h"
#include "fpsensor_state.h"
#include "fpsensor_utils.h"
#include "scoped_fast_cpu.h"

BUILD_ASSERT(FP_PK_LEN == SHA256_DIGEST_SIZE);
BUILD_ASSERT(FP_PK_EC_PUBLIC_KEY_LEN == 32);
BUILD_ASSERT(FP_PK_EC_PRIVATE_KEY_LEN == 32);

static enum ec_status
fp_command_establish_pk_keygen(struct host_cmd_handler_args *args)
{
	auto *r = static_cast<ec_response_fp_establish_pk_keygen *>(
		args->response);

	ScopedFastCpu fast_cpu;

	r->enc_privkey_info.struct_version = FP_PK_ENC_METADATA_VERSION;
	trng_init();
	trng_rand_bytes(r->enc_privkey, FP_PK_EC_PRIVATE_KEY_LEN);
	trng_rand_bytes(r->enc_privkey_info.nonce, FP_PK_NONCE_BYTES);
	trng_rand_bytes(r->enc_privkey_info.encryption_salt,
			FP_PK_ENCRYPTION_SALT_BYTES);
	trng_exit();

	bssl::UniquePtr<EC_GROUP> group(
		EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

	if (group.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> secret(
		BN_bin2bn(r->enc_privkey, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (secret.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	if (public_point.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_mul(group.get(), public_point.get(), secret.get(), nullptr,
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

	if (BN_bn2binpad(x_bn.get(), r->pubkey_x, FP_PK_EC_PUBLIC_KEY_LEN) !=
	    FP_PK_EC_PUBLIC_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(y_bn.get(), r->pubkey_y, FP_PK_EC_PUBLIC_KEY_LEN) !=
	    FP_PK_EC_PUBLIC_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	uint8_t key[SBP_ENC_KEY_LEN];
	int ret =
		derive_encryption_key(key, r->enc_privkey_info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_keygen: Failed to derive key");
		return EC_RES_UNAVAILABLE;
	}

	/* Encrypt the secret blob in-place. */
	ret = aes_gcm_encrypt(key, SBP_ENC_KEY_LEN, r->enc_privkey,
			      r->enc_privkey, FP_PK_EC_PRIVATE_KEY_LEN,
			      r->enc_privkey_info.nonce, FP_PK_NONCE_BYTES,
			      r->enc_privkey_info.tag, FP_PK_TAG_BYTES);
	OPENSSL_cleanse(key, sizeof(key));
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_keygen: Failed to encrypt template");
		return EC_RES_UNAVAILABLE;
	}

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_ESTABLISH_PK_KEYGEN,
		     fp_command_establish_pk_keygen, EC_VER_MASK(0));
