/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helpers for the boringssl elliptic curve key interface. */

#ifndef __CROS_EC_ELLIPTIC_CURVE_KEY_H
#define __CROS_EC_ELLIPTIC_CURVE_KEY_H

#ifdef CONFIG_BORINGSSL_CRYPTO
#include "openssl/ec_key.h"
#include "openssl/mem.h"
#else
#include <stdint.h>
#endif

/**
 * Generate a p256 ECC key.
 * @return key on success, nullptr on failure
 */
#ifdef CONFIG_BORINGSSL_CRYPTO
bssl::UniquePtr<EC_KEY> generate_elliptic_curve_key();
#else
uint32_t *generate_elliptic_curve_key();
#endif

#endif /* __CROS_EC_ELLIPTIC_CURVE_KEY_H */
