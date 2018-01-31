/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <common.h>
#include <trng.h>
#include <weaver_ng.h>

/* Compile time sanity checks. */
_Static_assert(sizeof(hash_t) >= SHA256_DIGEST_SIZE,
	       "hash_t is too small for a SHA256 digest/");

_Static_assert(sizeof(leaf_data_t) == ((sizeof(leaf_data_t) + 15) & ~15),
	       "sizeof(leaf_data_t) % 16 should be zero");

int create_merkle_tree(param_logk_t param_logk, param_h_t param_h,
			merkle_tree_t *merkle_tree)
{
	uint16_t param_k = 1 << param_logk;
	hash_t child_hashes[param_k];
	param_h_t hx;
	uint16_t kx;

	if (param_logk > 8 || param_k < 1)
		return EC_ERROR_UNKNOWN;
	if (param_h > 16 || param_h < 1)
		return EC_ERROR_UNKNOWN;

	merkle_tree->param_logk = param_logk;
	merkle_tree->param_h = param_h;

	/* Initialize the root hash. */
	memset(child_hashes, 0, sizeof(child_hashes));
	compute_hash(&child_hashes, param_k, &merkle_tree->root);
	for (hx = 1; hx < param_h; ++hx) {
		for (kx = 0; kx < param_k; ++kx) {
			memcpy(child_hashes[kx], &merkle_tree->root,
			       sizeof(merkle_tree->root));
		}
		compute_hash(&child_hashes, param_k, &merkle_tree->root);
	}

	rand_bytes(merkle_tree->hmac_key, sizeof(merkle_tree->hmac_key));

	rand_bytes(merkle_tree->wrap_key, sizeof(merkle_tree->wrap_key));

	/* TODO(allenwebb) generate public private key pair */
	return EC_SUCCESS;
}

int store_merkle_tree(uint8_t slot, merkle_tree_t *merkle_tree)
{
	/* TODO(allenwebb)
	 * 1) Find some flash that can be dedicated to this feature
	 * 2) Implement this function
	 */
	return EC_ERROR_UNIMPLEMENTED;
}

int load_merkle_tree(uint8_t slot, merkle_tree_t *merkle_tree)
{
	/* TODO(allenwebb) Implement this function. */
	return EC_ERROR_UNIMPLEMENTED;
}

int get_path_length(merkle_tree_t *merkle_tree)
{
	return (1 << merkle_tree->param_logk) * merkle_tree->param_h;
}

void compute_hmac(merkle_tree_t *merkle_tree,
		  wrapped_leaf_data_t *wrapped_leaf_data, hash_t *result)
{
	LITE_HMAC_CTX hmac;

	DCRYPTO_HMAC_SHA256_init(&hmac, merkle_tree->hmac_key,
				 sizeof(merkle_tree->hmac_key));
	HASH_update(&hmac.hash, wrapped_leaf_data->cipher_text,
		    sizeof(wrapped_leaf_data->cipher_text));
	memcpy(*result, DCRYPTO_HMAC_final(&hmac), sizeof(*result));
}

inline void compute_hash(hash_t (*hashes)[], uint16_t num_hashes,
			 hash_t *result)
{
	hash_t (*view)[num_hashes] = hashes;

	DCRYPTO_SHA256_hash((uint8_t *)(view), sizeof(*view),
			    (uint8_t *)*result);
}

int encrypt_leaf_data(merkle_tree_t *merkle_tree, leaf_data_t *leaf_data,
		      wrapped_leaf_data_t *wrapped_leaf_data) {
	/* Generate a random IV. */
	rand_bytes(wrapped_leaf_data->iv, sizeof(wrapped_leaf_data->iv));
	if (!DCRYPTO_aes_ctr(wrapped_leaf_data->cipher_text,
			     merkle_tree->wrap_key,
			     sizeof(merkle_tree->wrap_key) << 3,
			     wrapped_leaf_data->iv, (uint8_t *)leaf_data,
			     sizeof(leaf_data))) {
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

int authenticate_path(merkle_tree_t *merkle_tree, hash_t (*hashes)[],
		      label_t path) {
	uint16_t param_k = 1 << merkle_tree->param_logk;
	/* Ugly way to convert a 1D array to a 2D one. */
	hash_t (*view)[merkle_tree->param_h][param_k] = (void *)hashes;
	hash_t parent;
	param_h_t hx;

	/* Compute the hash for the first row. */
	compute_hash(view[0], param_k, &parent);
	for (hx = 1; hx < merkle_tree->param_h; ++hx) {
		index_t index = path[merkle_tree->param_h - hx];

		/* Verify the hash from the previous row. */
		if (memcmp(parent, view[hx][index], sizeof(parent)) != 0)
			return -hx;

		/* Compute the hash from the current row. */
		compute_hash(view[hx], param_k, &parent);
	}

	/* Verify the root hash*/
	if (memcmp(parent, merkle_tree->root, sizeof(parent)) != 0)
		return -hx;
	return 0;
}
