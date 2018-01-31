/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <common.h>
#include <dcrypto.h>
#include <trng.h>
#include <weaver_ng.h>

/* Compile time sanity checks. */
_Static_assert(sizeof(hash_t) >= SHA256_DIGEST_SIZE,
	       "hash_t is too small for a SHA256 digest/");

int create_merkel_tree(param_logk_t param_logk, param_h_t param_h,
			merkel_tree_t *merkel_tree)
{
	uint16_t param_k = 1 << param_logk;
	hash_t child_hashes[param_k];
	param_h_t hx;
	uint16_t kx;

	if (param_logk > 8 || param_k < 1)
		return EC_ERROR_PARAM1;
	if (param_h > 16 || param_h < 1)
		return EC_ERROR_PARAM2;

	merkel_tree->param_logk = param_logk;
	merkel_tree->param_h = param_h;
	rand_bytes(merkel_tree->hmac_key, sizeof(merkel_tree->hmac_key));
	/* TODO(allenwebb) generate public private key pair */

	/* Initialize the root hash. */
	memset(child_hashes, 0, sizeof(child_hashes));
	compute_hash(child_hashes, param_k, &merkel_tree->root);
	for (hx = 1; hx < param_h; ++hx) {
		for (kx = 0; kx < param_k; ++kx) {
			memcpy(&child_hashes[kx], &merkel_tree->root,
			       sizeof(merkel_tree->root));
		}
		compute_hash(child_hashes, param_k, &merkel_tree->root);
	}

	return EC_SUCCESS;
}

int store_merkel_tree(uint8_t slot, merkel_tree_t *merkel_tree)
{
	/* TODO(allenwebb)
	 * 1) Find some flash that can be dedicated to this feature
	 * 2) Implement this function
	 */
	return EC_ERROR_UNIMPLEMENTED;
}

int load_merkel_tree(uint8_t slot, merkel_tree_t *merkel_tree)
{
	/* TODO(allenwebb) Implement this function. */
	return EC_ERROR_UNIMPLEMENTED;
}

void compute_hmac(merkel_tree_t *merkel_tree, leaf_data_t *leaf_data,
		  hash_t *result)
{
	LITE_HMAC_CTX hmac;

	DCRYPTO_HMAC_SHA256_init(&hmac, merkel_tree->hmac_key,
				 sizeof(merkel_tree->hmac_key));
	HASH_update(&hmac.hash, leaf_data, sizeof(leaf_data));
	memcpy(result, DCRYPTO_HMAC_final(&hmac), sizeof(*result));
}

inline void compute_hash(hash_t *hashes, uint16_t num_hashes, hash_t *result)
{
	DCRYPTO_SHA256_hash((uint8_t *)hashes, sizeof(*hashes) * num_hashes,
			    (uint8_t *)result);
}
