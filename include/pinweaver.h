/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PINWEAVER_H
#define __CROS_EC_PINWEAVER_H

#include <pinweaver_types.h>

/* Enable the ability to stub functionality specific to Cr50. */
#ifdef CHIP_HOST
#include <dcrypto_mocks.h>
#else
#include <dcrypto.h>
#endif  /* CHIP_HOST */

#define PW_STORAGE_VERSION 0

/* Persistent information used by this feature. */
typedef struct PW_PACKED {
	/* log2(Fan out). */
	bits_per_level_t bits_per_level;
	/* Height of the tree or param_l / bits_per_level. */
	height_t height;

	/* Root hash of the Merkle tree. */
	hash_t root;

	/* Key used to compute the HMACs of the metadata of the leaves. */
	uint8_t hmac_key[32];

	/* Key used to encrypt and decrypt the metadata of the leaves. */
	uint8_t wrap_key[AES256_BLOCK_CIPHER_KEY_SIZE];
} merkle_tree_t;

/* Creates an empty Merkle_tree with the given parameters. */
int create_merkle_tree(bits_per_level_t bits_per_level, height_t height,
			merkle_tree_t *merkle_tree);

/* Writes the current state of the Merkle tree to flash. */
int store_merkle_tree(uint8_t slot, const merkle_tree_t *merkle_tree);

/* Loads a Merkle tree from flash. */
int load_merkle_tree(uint8_t slot, merkle_tree_t *merkle_tree);

/* Computes the total number of the sibling hashes along a path. */
int get_path_length(const merkle_tree_t *merkle_tree);

/* Extract the child index from the label at the specified level of the tree. */
index_t get_index(const merkle_tree_t *merkle_tree, label_t label,
		  height_t level);

/* Computes the HMAC for an encrypted leaf using the key in the merkle_tree. */
void compute_hmac(const merkle_tree_t *merkle_tree,
		  const wrapped_leaf_data_t *wrapped_leaf_data, hash_t *result);

/* Computes the parent hash for an array of child hashes. */
void compute_hash(const hash_t (*hashes)[], uint16_t num_hashes,
		  index_t location, const hash_t *child_hash, hash_t *result);

/* Computes the root hash for the specified path and child hash. */
void compute_root_hash(const merkle_tree_t *merkle_tree, label_t path,
		       const hash_t (*hashes)[], const hash_t *child_hash,
		       hash_t *new_root);

/* Checks to see the specified path is valid. The length of the path should be
 * validated prior to calling this function.
 *
 * Returns 0 on success or an error code otherwise.
 */
int authenticate_path(const merkle_tree_t *merkle_tree, label_t path,
		      const hash_t (*hashes)[], const hash_t *child_hash);

/* Encrypts the leaf meta data. */
int encrypt_leaf_data(const merkle_tree_t *merkle_tree,
		      const leaf_data_t *leaf_data,
		      wrapped_leaf_data_t *wrapped_leaf_data);

/* Decrypts the leaf meta data. */
int decrypt_leaf_data(const merkle_tree_t *merkle_tree,
		      const wrapped_leaf_data_t *wrapped_leaf_data,
		      leaf_data_t *leaf_data);

/* Handler for incoming messages after they have been reconstructed.
 *
 * merkle_tree->root needs to be updated with new_root outside of this function.
 */
int pw_handle_request(merkle_tree_t *merkle_tree, const pw_request_t *request,
		       pw_response_t *response);
int pw_handle_reset_tree(merkle_tree_t *merkle_tree,
			  const pw_request_reset_tree_t *request,
			  hash_t *new_root);
int pw_handle_insert_leaf(merkle_tree_t *merkle_tree,
			   const pw_request_insert_leaf_t *request,
			   pw_response_insert_leaf_t *response,
			   hash_t *new_root);
int pw_handle_remove_leaf(merkle_tree_t *merkle_tree,
			   const pw_request_remove_leaf_t *request,
			   hash_t *new_root);
int pw_handle_try_auth(merkle_tree_t *merkle_tree,
			const pw_request_try_auth_t *request,
			pw_response_try_auth_t *response,
			hash_t *new_root);
int pw_handle_reset_auth(merkle_tree_t *merkle_tree,
			  const pw_request_reset_auth_t *request,
			  pw_response_reset_auth_t *response,
			  hash_t *new_root);

/* TODO(allenwebb) remove this. */
void print_array(const uint8_t *data, size_t n) __attribute__ ((unused));
void print_hex(const uint8_t *data, size_t n) __attribute__ ((unused));

#endif  /* __CROS_EC_PINWEAVER_H */
