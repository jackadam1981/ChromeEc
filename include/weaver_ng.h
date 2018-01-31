/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_WEAVER_NG_H
#define __CROS_EC_WEAVER_NG_H

#include <dcrypto.h>
#include <weaver_ng_types.h>

/* Persistent information used by this feature. */
typedef struct PACKED {
	/* log2(Fan out). */
	param_logk_t param_logk;
	/* Height of the tree. */
	param_h_t param_h;

	/* Root hash of the Merkle tree. */
	hash_t root;

	/* Key used to compute the HMACs of the metadata of the leaves. */
	uint8_t hmac_key[32];

	/* Key used to encrypt and decrypt the metadata of the leaves. */
	uint8_t wrap_key[AES256_BLOCK_CIPHER_KEY_SIZE];

	/* Public private key pair used to exchange a session key used to
	 * encrypt secrets in transit.
	 */
	uint8_t public_key[256];
	uint8_t private_key[256];
} merkle_tree_t;

/* Creates an empty Merkle_tree with the given parameters. */
int create_merkle_tree(param_logk_t param_logk, param_h_t param_h,
			merkle_tree_t *merkle_tree);

/* Writes the current state of the Merkle tree to flash*/
int store_merkle_tree(uint8_t slot, merkle_tree_t *merkle_tree);

/* Loads a Merkle tree from flash*/
int load_merkle_tree(uint8_t slot, merkle_tree_t *merkle_tree);

/* Computes the expected path length of the Merkle tree. */
int get_path_length(merkle_tree_t *merkle_tree);

/* Computes the HMAC for an encrypted leaf using the key in the merkle_tree. */
void compute_hmac(merkle_tree_t *merkle_tree,
		  wrapped_leaf_data_t *wrapped_leaf_data, hash_t *result);

/* Computes the parent hash for an array of child hashes. */
inline void compute_hash(hash_t (*hashes)[], uint16_t num_hashes,
			 hash_t *result);

/* Encrypts the leaf meta data */
int encrypt_leaf_data(merkle_tree_t *merkle_tree, leaf_data_t *leaf_data,
		      wrapped_leaf_data_t *wrapped_leaf_data);

/* Checks to see the specified path is valid. The length of the path should be
 * validated prior to calling this function.
 *
 * Returns 0 on success or an error code otherwise.
 */
int authenticate_path(merkle_tree_t *merkle_tree, hash_t (*hashes)[],
		      label_t path);

#endif  /* __CROS_EC_WEAVER_NG_H */
