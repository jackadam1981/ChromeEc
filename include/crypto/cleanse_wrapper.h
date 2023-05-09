/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* A Drop-in replacement wrapper for OPENSSL_cleanse. */

#ifndef __CROS_EC_CRYPTO_CLEANSE_WRAPPER_H
#define __CROS_EC_CRYPTO_CLEANSE_WRAPPER_H

#include "openssl/mem.h"

#include <type_traits>
#include <utility>

/* This only work for standard layout type. */
template <typename T, typename = std::enable_if_t<std::is_standard_layout_v<T> > >
class CleanseWrapper : public T {
    public:
	using T::T;
	using T::operator=;

	constexpr CleanseWrapper(T &&t)
		: T(std::move(t))
	{
	}
	constexpr CleanseWrapper(const T &t)
		: T(t)
	{
	}

	~CleanseWrapper()
	{
		OPENSSL_cleanse(this, sizeof(*this));
	}
};

#endif /* __CROS_EC_CRYPTO_CLEANSE_WRAPPER_H */
