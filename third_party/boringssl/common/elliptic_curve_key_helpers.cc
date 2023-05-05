/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "openssl/ec_key.h"
#include "openssl/mem.h"
#include "openssl/obj_mac.h"

extern "C" void *OPENSSL_memory_alloc(size_t size)
{
	ccprints("OPENSSL_memory_alloc %d\n", size);
	uint8_t *result = (uint8_t *)malloc(size + 8);
	if (result == nullptr)
		return nullptr;
	ccprints("%p\n", result + 8);
	*((int *)((void *)result)) = size;
	return result + 8;
}

extern "C" void OPENSSL_memory_free(void *ptr)
{
	ccprints("OPENSSL_memory_free %p\n", ptr);
	if (ptr == nullptr)
		return;
	uint8_t *tmp = (uint8_t *)ptr;
	tmp -= 8;
	return free(tmp);
}
extern "C" size_t OPENSSL_memory_get_size(void *ptr)
{
	ccprints("OPENSSL_memory_get_size %p\n", ptr);
	if (ptr == nullptr)
		return 0;
	uint8_t *tmp = (uint8_t *)ptr;
	tmp -= 8;
	int value = *((int *)((void *)tmp));
	ccprints("%d\n", value);
	return value;
}

bssl::UniquePtr<EC_KEY> generate_elliptic_curve_key()
{
	using VoidPtr = void *;
	VoidPtr arr[11] = {
		OPENSSL_malloc(32), OPENSSL_malloc(24),	 OPENSSL_malloc(20),
		OPENSSL_malloc(32), OPENSSL_malloc(20),	 OPENSSL_malloc(32),
		OPENSSL_malloc(20), OPENSSL_malloc(32),	 OPENSSL_malloc(20),
		OPENSSL_malloc(32), OPENSSL_malloc(344),
	};
	for (int i = 0; i < 11; i++) {
		ccprints("arr[%d] = %p\n", i, arr[i]);
		OPENSSL_free(arr[i]);
		if (arr[i] == nullptr) {
			break;
		}
	}
	bssl::UniquePtr<EC_KEY> key(
		EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
	if (key == nullptr) {
		ccprints("EC_KEY OOM\n");
		return nullptr;
	}

	if (EC_KEY_generate_key(key.get()) != 1) {
		return nullptr;
	}

	return key;
}
