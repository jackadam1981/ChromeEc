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

/* Simplistic helper that wraps a call to a deleter function. */
template <auto Destroyer> struct OpenSSLDestroyer {
	template <typename T> void operator()(T *ptr) const
	{
		Destroyer(ptr);
	}
};

/* Warp openssl type into the unique_ptr. */
template <typename PointerType, auto Destroyer>
using ScopedOpenSSL =
	std::unique_ptr<PointerType, OpenSSLDestroyer<Destroyer> >;

/* Several typedefs are provided for crypto-specific primitives, for
 * short-hand and prevalence.
 */
using ScopedBIGNUM = ScopedOpenSSL<BIGNUM, BN_free>;
using ScopedEC_GROUP = ScopedOpenSSL<EC_GROUP, EC_GROUP_free>;
using ScopedEC_POINT = ScopedOpenSSL<EC_POINT, EC_POINT_free>;

#endif /* __CROS_EC_SCOPED_OPENSSL_TYPES_HPP */
