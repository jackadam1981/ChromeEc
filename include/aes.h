/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* AES (Advanced Encryption Standard) cryptography primitives. */

#ifndef __CROS_EC_AES_H
#define __CROS_EC_AES_H

#include <stdint.h>

/* Flag definitions for the aes_init() 'flags' word */
#define AES_FLAG_DIR_OFFSET 0
#define AES_FLAG_KEYSIZE_OFFSET 1
#define AES_FLAG_MODE_OFFSET 4

#define AES_FLAG_MODE_ECB     (0 << AES_FLAG_MODE_OFFSET)
#define AES_FLAG_MODE_CBC     (1 << AES_FLAG_MODE_OFFSET)
#define AES_FLAG_MODE_CTR     (2 << AES_FLAG_MODE_OFFSET)
#define AES_FLAG_MODE_GCM     (3 << AES_FLAG_MODE_OFFSET)
#define AES_FLAG_MODE_MASK    (7 << AES_FLAG_MODE_OFFSET)

#define AES_FLAG_KEYSIZE_128  (0 << AES_FLAG_KEYSIZE_OFFSET)
#define AES_FLAG_KEYSIZE_192  (1 << AES_FLAG_KEYSIZE_OFFSET)
#define AES_FLAG_KEYSIZE_256  (2 << AES_FLAG_KEYSIZE_OFFSET)
#define AES_FLAG_KEYSIZE_MASK (3 << AES_FLAG_KEYSIZE_OFFSET)

#define AES_FLAG_ENCRYPT      (0 << AES_FLAG_DIR_OFFSET)
#define AES_FLAG_DECRYPT      (1 << AES_FLAG_DIR_OFFSET)
#define AES_FLAG_DIR_MASK     (1 << AES_FLAG_DIR_OFFSET)

/* Number of 32-bit words in the key according to AES_KEYSIZE flag */
#define AES_KEY_WORDS(flags) (4 + (((flags) & AES_FLAG_KEYSIZE_MASK) \
					    >> AES_FLAG_KEYSIZE_OFFSET) * 2)

/* AES data block are always 128-bit regardless of the key size */
#define AES_BLOCK_SIZE (128 / 8)
#define AES_BLOCK_WORDS (AES_BLOCK_SIZE / sizeof(uint32_t))

/* Configure ... */
int aes_init(const uint32_t *key, const uint32_t *iv, uint32_t flags);

/* process one 128-bit block */
int aes_block(const uint32_t *in, uint32_t *out);

/*  (lower power mode, free mutex) */
void aes_cleanup(void);

#endif  /* __CROS_EC_AES_H */
