/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_SCOPED_OPENSSL_TYPES_HPP
#define __CROS_EC_SCOPED_OPENSSL_TYPES_HPP

#include <stdint.h>

#include <memory>
#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/mem.h>

/* Simplistic helper that wraps a call to a deleter function. In a C++11 world,
 * this would be std::function<>. An alternative would be to re-use
 * base::internal::RunnableAdapter<>, but that's far too heavy weight.
 */
template <typename Type, void (*Destroyer)(Type *)> struct OpenSSLDestroyer {
	void operator()(Type *ptr) const
	{
		Destroyer(ptr);
	}
};

template <typename PointerType, void (*Destroyer)(PointerType *)>
using ScopedOpenSSL =
	std::unique_ptr<PointerType, OpenSSLDestroyer<PointerType, Destroyer> >;

struct OpenSSLFree {
	void operator()(uint8_t *ptr) const
	{
		OPENSSL_free(ptr);
	}
};

/* Several typedefs are provided for crypto-specific primitives, for
 * short-hand and prevalence.
 */
using ScopedBIGNUM = ScopedOpenSSL<BIGNUM, BN_free>;
using ScopedEC_Key = ScopedOpenSSL<EC_KEY, EC_KEY_free>;
using ScopedEC_GROUP = ScopedOpenSSL<EC_GROUP, EC_GROUP_free>;
using ScopedEC_POINT = ScopedOpenSSL<EC_POINT, EC_POINT_free>;

/* The bytes must have been allocated with OPENSSL_malloc. */
using ScopedOpenSSLBytes = std::unique_ptr<uint8_t, OpenSSLFree>;

#endif /* __CROS_EC_SCOPED_OPENSSL_TYPES_HPP */
