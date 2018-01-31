/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_PINWEAVER_H
#define __CROS_EC_PINWEAVER_H

/* This is required before pinweaver_types.h to provide __packed and __aligned
 * while preserving the ability of pinweaver_types.h to be used in code outside
 * of src/platform/ec.
 */
#include <common.h>
#include <pinweaver_types.h>

/* Enable the ability to stub functionality specific to Cr50. */
#ifdef CHIP_HOST
#include <dcrypto_mocks.h>
#else
#include <dcrypto.h>
#endif  /* CHIP_HOST */

#define PW_STORAGE_VERSION 0

/* Persistent information used by this feature. */
struct PW_PACKED merkle_tree_t {
	/* log2(Fan out). */
	bits_per_level_t bits_per_level;
	/* Height of the tree or param_l / bits_per_level. */
	height_t height;

	/* Root hash of the Merkle tree. */
	uint8_t root[PW_HASH_SIZE];

	/* Key used to compute the HMACs of the metadata of the leaves. */
	uint8_t hmac_key[32];

	/* Key used to encrypt and decrypt the metadata of the leaves. */
	uint8_t wrap_key[AES256_BLOCK_CIPHER_KEY_SIZE];
};

/* Handler for incoming messages after they have been reconstructed.
 *
 * merkle_tree->root needs to be updated with new_root outside of this function.
 */
int pw_handle_request(struct merkle_tree_t *merkle_tree,
		      const struct pw_request_t *request,
		      struct pw_response_t *response);

/******************************************************************************/
/* Utility functions exported for better test coverage.
 */

/* Computes the total number of the sibling hashes along a path. */
int get_path_length(const struct merkle_tree_t *merkle_tree);

/* Extract the child index from the label at the specified level of the tree. */
index_t get_index(const struct merkle_tree_t *merkle_tree, label_t label,
		  height_t level);

/* Computes the parent hash for an array of child hashes. */
void compute_hash(const uint8_t hashes[][PW_HASH_SIZE], uint16_t num_hashes,
		  index_t location, const uint8_t child_hash[PW_HASH_SIZE],
		  uint8_t result[PW_HASH_SIZE]);

#endif  /* __CROS_EC_PINWEAVER_H */
