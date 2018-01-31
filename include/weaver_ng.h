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

	/* Root hash of the Merkel tree. */
	hash_t root;

	/* Key used to compute the HMACs of the metadata of the leaves. */
	uint8_t hmac_key[32];

	/* Public private key pair used to exchange a session key used to
	 * encrypt secrets in transit.
	 */
	uint8_t public_key[256];
	uint8_t private_key[256];
} merkel_tree_t;

/* Leaf metadata wrapped by a key */
typedef struct PACKED {
	label_t label;
	attempt_count_t attempt_count;
	timestamp_t timestamp;
	delay_schedule_t delay_schedule;
	low_entropy_secret_t low_entropy_secret;
	high_entropy_secret_t high_entropy_secret;
	high_entropy_secret_t reset_secret;
} leaf_data_t;

/* Creates an empty Merkel_tree with the given parameters. */
int create_merkel_tree(param_logk_t param_logk, param_h_t param_h,
			merkel_tree_t *merkel_tree);

/* Writes the current state of the Merkel tree to flash*/
int store_merkel_tree(uint8_t slot, merkel_tree_t *merkel_tree);

/* Loads a Merkel tree from flash*/
int load_merkel_tree(uint8_t slot, merkel_tree_t *merkel_tree);

/* Computes the HMAC for a specific leaf using the key in the merkel_tree. */
void compute_hmac(merkel_tree_t *merkel_tree, leaf_data_t *leaf_data,
		  hash_t *result);

/* Computes the parent hash for an array of child hashes. */
inline void compute_hash(hash_t *hashes, uint16_t num_hashes, hash_t *result);

#endif  /* __CROS_EC_WEAVER_NG_H */
