/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "CryptoEngine.h"
#include "dcrypto.h"

static int check_params(const RSA_KEY *key,  TPM_ALG_ID padding_alg,
			TPM_ALG_ID hash_alg, enum padding_mode *padding,
			enum hashing_mode *hashing)
{
	if (key->publicKey->size & 0x3)
		/* Only word-multiple sizes supported. */
		return CRYPT_FAIL;

	if (padding_alg == TPM_ALG_RSAES) {
		*padding = PADDING_MODE_PKCS1;
	} else if (padding_alg == TPM_ALG_OAEP) {
		/* Only SHA1 and SHA256 supported with OAEP. */
		if (hash_alg == TPM_ALG_SHA1)
			*hashing = HASH_SHA1;
		else if (hash_alg == TPM_ALG_SHA256)
			*hashing = HASH_SHA256;
		else
			/* Unsupported hash algorithm. */
			return CRYPT_FAIL;
		*padding = PADDING_MODE_OAEP;
	} else {
		return CRYPT_FAIL;  /* NULL padding unsupported. */
	}
	return CRYPT_SUCCESS;
}

CRYPT_RESULT _cpri__EncryptRSA(uint32_t *out_len, uint8_t *out,
			RSA_KEY *key, TPM_ALG_ID padding_alg,
			uint32_t in_len, uint8_t *in,
			TPM_ALG_ID hash_alg, const char *label)
{
	RSA rsa;
	enum padding_mode padding;
	enum hashing_mode hashing;

	if (check_params(key, padding_alg, hash_alg, &padding, &hashing)
		!= CRYPT_SUCCESS)
		return CRYPT_FAIL;

	rsa.e = key->exponent;
	rsa.N.dmax = key->publicKey->size >> 2;
	rsa.N.d = (uint32_t *) &key->publicKey->buffer;
	rsa.d.dmax = 0;
	rsa.d.d = NULL;

	if (DCRYPTO_rsa_encrypt(&rsa, out, out_len, in, in_len, padding,
					hashing, label))
		return CRYPT_SUCCESS;
	else
		return CRYPT_FAIL;
}

CRYPT_RESULT _cpri__DecryptRSA(uint32_t *out_len, uint8_t *out,
			RSA_KEY *key, TPM_ALG_ID padding_alg,
			uint32_t in_len, uint8_t *in,
			TPM_ALG_ID hash_alg, const char *label)
{
	RSA rsa;
	enum padding_mode padding;
	enum hashing_mode hashing;

	if (check_params(key, padding_alg, hash_alg, &padding, &hashing)
		!= CRYPT_SUCCESS)
		return CRYPT_FAIL;

	if (key->privateKey->size & 0x3)
		return CRYPT_FAIL;    /* Only word-multiple sizes supported. */

	rsa.e = key->exponent;
	rsa.N.dmax = key->publicKey->size >> 2;
	rsa.N.d = (uint32_t *) &key->publicKey->buffer;
	rsa.d.dmax = key->privateKey->size >> 2;
	rsa.d.d = (uint32_t *) &key->privateKey->buffer;

	if (DCRYPTO_rsa_decrypt(&rsa, out, out_len, in, in_len, padding,
					hashing, label))
		return CRYPT_SUCCESS;
	else
		return CRYPT_FAIL;
}
