/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_WEAVER_NG_H
#define __CROS_EC_WEAVER_NG_H

/* Enable the ability to stub functionality specific to Cr50. */
#ifdef CHIP_HOST
#include <stdint.h>
#include <string.h>

#define AES256_BLOCK_CIPHER_KEY_SIZE 32
#define SHA256_DIGEST_SIZE 32

struct HASH_CTX {
	uint8_t digest[SHA256_DIGEST_SIZE];
};

typedef struct {
	struct HASH_CTX hash;
} LITE_HMAC_CTX;

void HASH_update(struct HASH_CTX *ctx, const void *data, size_t len);

const uint8_t *DCRYPTO_SHA256_hash(const void *data, uint32_t n,
				   uint8_t *digest);

void DCRYPTO_HMAC_SHA256_init(LITE_HMAC_CTX *ctx, const void *key,
			unsigned int len);
const uint8_t *DCRYPTO_HMAC_final(LITE_HMAC_CTX *ctx);

int DCRYPTO_aes_ctr(uint8_t *out, const uint8_t *key, uint32_t key_bits,
		const uint8_t *iv, const uint8_t *in, size_t in_len);

#else
#include <dcrypto.h>
#endif  /* CHIP_HOST */

#include <weaver_ng_types.h>

#define WNG_STORAGE_VERSION 0

/* Persistent information used by this feature. */
typedef struct PACKED {
	/* log2(Fan out). */
	param_logk_t param_logk;
	/* Height of the tree or param_l / param_logk. */
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
int store_merkle_tree(uint8_t slot, const merkle_tree_t *merkle_tree);

/* Loads a Merkle tree from flash*/
int load_merkle_tree(uint8_t slot, merkle_tree_t *merkle_tree);

/* Computes the total number of the sibling hashes along a path. */
int get_path_length(const merkle_tree_t *merkle_tree);

/* Extract the child index from the label at the specified level of the tree. */
index_t get_index(const merkle_tree_t *merkle_tree, label_t label,
		  param_h_t level);

/* Computes the HMAC for an encrypted leaf using the key in the merkle_tree. */
void compute_hmac(const merkle_tree_t *merkle_tree,
		  const wrapped_leaf_data_t *wrapped_leaf_data, hash_t *result);

/* Computes the parent hash for an array of child hashes. */
void compute_hash(const hash_t (*hashes)[], uint16_t num_hashes,
		  index_t location, const hash_t *child_hash, hash_t *result);

/* Computes the root hash for the specified path and child hash. */
void compute_root(const merkle_tree_t *merkle_tree, label_t path,
		  const hash_t (*hashes)[], const hash_t *child_hash,
		  hash_t *new_root);

/* Checks to see the specified path is valid. The length of the path should be
 * validated prior to calling this function.
 *
 * Returns 0 on success or an error code otherwise.
 */
int authenticate_path(const merkle_tree_t *merkle_tree, label_t path,
		      const hash_t (*hashes)[], const hash_t *child_hash);

/* Encrypts the leaf meta data */
int encrypt_leaf_data(const merkle_tree_t *merkle_tree,
		      const leaf_data_t *leaf_data,
		      wrapped_leaf_data_t *wrapped_leaf_data);

/* Decrypts the leaf meta data */
int decrypt_leaf_data(const merkle_tree_t *merkle_tree,
		      const wrapped_leaf_data_t *wrapped_leaf_data,
		      leaf_data_t *leaf_data);

/* Handler for incoming messages after they have been reconstructed.
 *
 * merkle_tree->root needs to be updated with new_root outside of this function.
 */
int wng_handle_request(merkle_tree_t *merkle_tree, const wng_request_t *request,
		       wng_response_t *response);
int wng_handle_reset_tree(merkle_tree_t *merkle_tree,
			  const wng_request_reset_tree_t *request,
			  hash_t *new_root);
int wng_handle_insert_leaf(merkle_tree_t *merkle_tree,
			   const wng_request_insert_leaf_t *request,
			   wng_response_insert_leaf_t *response,
			   hash_t *new_root);
int wng_handle_remove_leaf(merkle_tree_t *merkle_tree,
			   const wng_request_remove_leaf_t *request,
			   hash_t *new_root);
int wng_handle_try_auth(merkle_tree_t *merkle_tree,
			const wng_request_try_auth_t *request,
			wng_response_try_auth_t *response,
			hash_t *new_root);
int wng_handle_reset_auth(merkle_tree_t *merkle_tree,
			  const wng_request_reset_auth_t *request,
			  wng_response_reset_auth_t *response,
			  hash_t *new_root);

/* TODO(allenwebb) remove this. */
void print_array(const uint8_t *data, size_t n) __attribute__ ((unused));
void print_hex(const uint8_t *data, size_t n) __attribute__ ((unused));

#endif  /* __CROS_EC_WEAVER_NG_H */
