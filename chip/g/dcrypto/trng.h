/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __EC_CHIP_G_DCRYPTO_TRNG_H
#define __EC_CHIP_G_DCRYPTO_TRNG_H

/**
 * Initialize the true random number generator.
 **/
void init_trng(void);

/**
 * Retrieve a 32 bit random value.
 **/
uint32_t rand(void);


/**
 * Output len random bytes to buffer buf.
 **/
void rand_bytes(uint8_t *buf, uint32_t len);

#endif /* ! __EC_CHIP_G_DCRYPTO_TRNG_H */
