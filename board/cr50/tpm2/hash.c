/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "CryptoEngine.h"
#include "dcrypto.h"

#include "util.h"

/* Pickup g_hashData. */
#include "CpriHashData.c"

static const HASH_INFO *lookup_hash_info(TPM_ALG_ID alg)
{
	int i;
	const int num_algs = ARRAY_SIZE(g_hashData);

	for (i = 0; i < num_algs - 1; i++) {
		if (g_hashData[i].alg == alg)
			return &g_hashData[i];
	}
	return &g_hashData[num_algs - 1];
}

TPM_ALG_ID _cpri__GetContextAlg(CPRI_HASH_STATE *hash_state)
{
	return hash_state->hashAlg;
}

TPM_ALG_ID _cpri__GetHashAlgByIndex(uint32_t index)
{
	if (index >= HASH_COUNT)
		return TPM_ALG_NULL;
	return g_hashData[index].alg;
}

uint16_t _cpri__GetDigestSize(TPM_ALG_ID alg)
{
	return lookup_hash_info(alg)->digestSize;
}

uint16_t _cpri__GetHashBlockSize(TPM_ALG_ID alg)
{
	return lookup_hash_info(alg)->blockSize;
}

void _cpri__ImportExportHashState(CPRI_HASH_STATE *osslFmt,
				EXPORT_HASH_STATE *externalFmt,
				IMPORT_EXPORT direction)
{
	ecprintf("%s called\n", __func__);
}

uint16_t _cpri__HashBlock(TPM_ALG_ID alg, uint32_t in_len, uint8_t *in,
			uint32_t out_len, uint8_t *out)
{
	uint8_t digest[SHA_DIGEST_MAX_BYTES];
	const uint16_t digest_len = _cpri__GetDigestSize(alg);

	if (digest_len == 0)
		return 0;

	switch (alg) {
	case TPM_ALG_SHA1:
		DCRYPTO_SHA1_hash(in, in_len, digest);
		break;

	case TPM_ALG_SHA256:
		DCRYPTO_SHA256_hash(in, in_len, digest);
		break;
/* TODO: add support for SHA384 and SHA512
 *
 *	case TPM_ALG_SHA384:
 *	DCRYPTO_SHA384(in, in_len, p);
 *		break;
 *	case TPM_ALG_SHA512:
 *		DCRYPTO_SHA512(in, in_len, p);
 *		break; */
	default:
		FAIL(FATAL_ERROR_INTERNAL);
		break;
	}

	out_len = MIN(out_len, digest_len);
	memcpy(out, digest, out_len);
	return out_len;
}

uint16_t _cpri__StartHash(TPM_ALG_ID alg, BOOL sequence,
			CPRI_HASH_STATE *state)
{
	SHA1_CTX ctx1;
	SHA256_CTX ctx256;

	switch (alg) {
	case TPM_ALG_SHA1:
		DCRYPTO_SHA1_init(&ctx1, sequence);
		return CRYPT_SUCCESS;
	case TPM_ALG_SHA256:
		DCRYPTO_SHA256_init(&ctx256, sequence);
		return CRYPT_SUCCESS;
	default:
		return CRYPT_PARAMETER;
	}
}

void _cpri__UpdateHash(CPRI_HASH_STATE *state, uint32_t in_len,
		BYTE *in)
{
	/* TODO: deserialize / serialize ctx from / to state */
}

uint16_t _cpri__CompleteHash(CPRI_HASH_STATE *state,
			uint32_t out_len, uint8_t *out)
{
	/* TODO: deserialize / serialize ctx from / to state */
	return out_len;
}
