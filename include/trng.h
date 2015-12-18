/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __EC_INCLUDE_TRNG_H
#define __EC_INCLUDE_TRNG_H

/**
 * Initialize the true random number generator.
 *
 * Not supported by all platforms.
 **/
void init_trng(void);

/**
 * Retrieve a 32 bit random value.
 *
 * Not supported on all platforms.
 **/
uint32_t rand(void);


/**
 * Output len random bytes to buffer buf.
 *
 * Not supported on all platforms.
 **/
void rand_bytes(uint8_t *buf, uint32_t len);

#endif /* __EC_INCLUDE_TRNG_H */
