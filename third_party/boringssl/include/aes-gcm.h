/* ====================================================================
 * Copyright (c) 2008 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ==================================================================== */

#ifndef __CROS_EC_AES_GCM_H
#define __CROS_EC_AES_GCM_H

#include <stdalign.h>

#include "aes.h"
#include "common.h"
#include "util.h"

// GCM definitions
typedef struct { uint64_t hi,lo; } u128;

// gmult_func multiplies |Xi| by the GCM key and writes the result back to
// |Xi|.
typedef void (*gmult_func)(uint64_t Xi[2], const u128 Htable[16]);

// ghash_func repeatedly multiplies |Xi| by the GCM key and adds in blocks from
// |inp|. The result is written back to |Xi| and the |len| argument must be a
// multiple of 16.
typedef void (*ghash_func)(uint64_t Xi[2], const u128 Htable[16],
                           const uint8_t *inp, size_t len);

typedef struct gcm128_key_st {
	// Note the MOVBE-based, x86-64, GHASH assembly requires |H| and
	// |Htable| to be the first two elements of this struct. Additionally,
	// some assembly routines require a 16-byte-aligned |Htable| when
	// hashing data, but not initialization. |GCM128_KEY| is not itself
	// aligned to simplify embedding in |EVP_AEAD_CTX|, but |Htable|'s
	// offset must be a multiple of 16.
	u128 H;
	u128 Htable[16];
	gmult_func gmult;
	ghash_func ghash;

	block128_f block;

	// use_aesni_gcm_crypt is true if this context should use the assembly
	// functions |aesni_gcm_encrypt| and |aesni_gcm_decrypt| to process
	// data.
	unsigned use_aesni_gcm_crypt : 1;
} GCM128_KEY;

// GCM128_CONTEXT contains state for a single GCM operation. The structure
// should be zero-initialized before use.
typedef struct {
	// The following 5 names follow names in GCM specification
	union {
		uint64_t u[2];
		uint32_t d[4];
		uint8_t c[16];
		size_t t[16 / sizeof(size_t)];
	} Yi, EKi, EK0, len, Xi;

	// Note that the order of |Xi| and |gcm_key| is fixed by the
	// MOVBE-based, x86-64, GHASH assembly. Additionally, some assembly
	// routines require |gcm_key| to be 16-byte aligned. |GCM128_KEY| is not
	// itself aligned to simplify embedding in |EVP_AEAD_CTX|.
	alignas(16) GCM128_KEY gcm_key;

	unsigned mres, ares;
} GCM128_CONTEXT;

// CRYPTO_gcm128_init initialises |ctx| to use |block| (typically AES) with
// the given key. |block_is_hwaes| is one if |block| is |aes_hw_encrypt|.
void CRYPTO_gcm128_init_key(GCM128_KEY *gcm_key, const void *key,
			    block128_f block, int block_is_hwaes);

// CRYPTO_gcm128_setiv sets the IV (nonce) for |ctx|. The |key| must be the
// same key that was passed to |CRYPTO_gcm128_init|.
void CRYPTO_gcm128_setiv(GCM128_CONTEXT *ctx, const void *key,
                                        const uint8_t *iv, size_t iv_len);

// CRYPTO_gcm128_aad sets the authenticated data for an instance of GCM.
// This must be called before and data is encrypted. It returns one on success
// and zero otherwise.
int CRYPTO_gcm128_aad(GCM128_CONTEXT *ctx, const uint8_t *aad,
                                     size_t len);

// CRYPTO_gcm128_encrypt encrypts |len| bytes from |in| to |out|. The |key|
// must be the same key that was passed to |CRYPTO_gcm128_init|. It returns one
// on success and zero otherwise.
int CRYPTO_gcm128_encrypt(GCM128_CONTEXT *ctx, const void *key,
                                         const uint8_t *in, uint8_t *out,
                                         size_t len);

// CRYPTO_gcm128_decrypt decrypts |len| bytes from |in| to |out|. The |key|
// must be the same key that was passed to |CRYPTO_gcm128_init|. It returns one
// on success and zero otherwise.
int CRYPTO_gcm128_decrypt(GCM128_CONTEXT *ctx, const void *key,
                                         const uint8_t *in, uint8_t *out,
                                         size_t len);

// CRYPTO_gcm128_finish calculates the authenticator and compares it against
// |len| bytes of |tag|. It returns one on success and zero otherwise.
int CRYPTO_gcm128_finish(GCM128_CONTEXT *ctx, const uint8_t *tag,
                                        size_t len);

// CRYPTO_gcm128_tag calculates the authenticator and copies it into |tag|.
// The minimum of |len| and 16 bytes are copied into |tag|.
void CRYPTO_gcm128_tag(GCM128_CONTEXT *ctx, uint8_t *tag,
                                      size_t len);


#endif  // __CROS_EC_AES_GCM_H
