/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "crypto/elliptic_curve_key.h"

#ifdef CONFIG_BORINGSSL_CRYPTO
#include "openssl/ec_key.h"
#include "openssl/mem.h"
#include "openssl/obj_mac.h"
#else
#include <stdint.h>

#include <malloc.h>
#endif

#ifdef CONFIG_BORINGSSL_CRYPTO
bssl::UniquePtr<EC_KEY> generate_elliptic_curve_key()
{
	bssl::UniquePtr<EC_KEY> key(
		EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
	if (key == nullptr) {
		return nullptr;
	}

	if (EC_KEY_generate_key(key.get()) != 1) {
		return nullptr;
	}

	return key;
}
#else
uint32_t *generate_elliptic_curve_key()
{
	uint32_t const key_size = 128;
	uint32_t *key = (uint32_t *)(malloc(key_size));

	return key;
}
#endif
