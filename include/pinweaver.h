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

#define PW_STORAGE_VERSION 0

/* Persistent information used by this feature. */
struct PW_PACKED merkle_tree_t {
	/* log2(Fan out). */
	struct bits_per_level_t bits_per_level;
	/* Height of the tree or param_l / bits_per_level. */
	struct height_t height;

	/* Root hash of the Merkle tree. */
	uint8_t root[PW_HASH_SIZE];

	/* Key used to compute the HMACs of the metadata of the leaves. */
	uint8_t hmac_key[32];

	/* Key used to encrypt and decrypt the metadata of the leaves. */
	uint8_t wrap_key[32];
};

struct PW_PACKED leaf_data_t {
	struct leaf_public_data_t pub;
	struct leaf_sensitive_data_t sec;
};

/* Handler for incoming messages after they have been reconstructed.
 *
 * merkle_tree->root needs to be updated with new_root outside of this function.
 */
int pw_handle_request(struct merkle_tree_t *merkle_tree,
		      const struct pw_request_t *request,
		      struct pw_response_t *response);

/******************************************************************************/
/* Struct helper functions.
 */

/* This is allowed because the path hashes are extra data at the end. */
inline const struct unimported_leaf_data_t
*cast_uldah_uld(const struct unimported_leaf_data_and_hashes_t *ptr);

/* This is allowed because struct wrapped_leaf_data_t meets all the
 * requirements for struct unimported_leaf_data_t.
 */
inline const struct unimported_leaf_data_t
*cast_wld_uld(const struct wrapped_leaf_data_t *ptr);

inline const struct leaf_public_data_t
*get_pub(const struct unimported_leaf_data_t *ptr);

inline const uint8_t *get_cipher_text(const struct unimported_leaf_data_t *ptr);

inline void
*get_path_hashes(const struct unimported_leaf_data_and_hashes_t *ptr);

/* Calculate how much is needed to add to the size of structs containing
 * an struct unimported_leaf_data_t because the variable length fields at the
 * end of the struct are not included by sizeof().
 */
#define PW_RESP_SIZE_DISCREPENCY (sizeof(struct wrapped_leaf_data_t) - \
		sizeof(struct unimported_leaf_data_t))


/******************************************************************************/
/* Utility functions exported for better test coverage.
 */

/* Computes the total number of the sibling hashes along a path. */
int get_path_auxiliary_hash_count(const struct merkle_tree_t *merkle_tree);

/* Computes the parent hash for an array of child hashes. */
void compute_hash(const uint8_t hashes[][PW_HASH_SIZE], uint16_t num_hashes,
		  struct index_t location,
		  const uint8_t child_hash[PW_HASH_SIZE],
		  uint8_t result[PW_HASH_SIZE]);

#endif  /* __CROS_EC_PINWEAVER_H */
