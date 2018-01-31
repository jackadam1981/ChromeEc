/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Shared types between Cr50 and the AP side code. */

#ifndef __CROS_EC_WEAVER_NG_TYPES_H
#define __CROS_EC_WEAVER_NG_TYPES_H

#include <stdint.h>

#ifndef PACKED
#define PACKED __attribute__((__packed__))
#endif  /* PACKED */

#define WNG_PROTOCOL_VERSION 0

/* The block size of encryption used for wrapped_leaf_data_t. */
#define WRAP_KEY_BLOCK_SIZE 16

/* Represents the log2(fan out) of a tree. */
typedef uint8_t param_logk_t;
/* Represent the height of a tree. */
typedef uint8_t param_h_t;

/* Represents a child index of a node in a tree. */
typedef uint8_t index_t;

/* Represents the child index for each level of a tree along a path to a leaf.
 */
typedef uint64_t label_t;

/* Represents a count of failed login attempts. TODO(allenwebb@) This is capped
 * at twice the max attempt_count in the delay schedule to prevent overflows.
 */
typedef uint32_t attempt_count_t;

/* Represents a notion of time. */
typedef struct PACKED {
	/* Number of boots. This is used to track if Cr50 has rebooted since
	 * timer_value was recorded.
	 */
	uint32_t boot_count;
	/* Currently a counter value since boot. */
	uint64_t timer_value;
} timestamp_t;

/* Represents a time interval over the timer_value in timestamp_t.
 *
 * This only needs to be sufficiently large to represent the longest time
 * between allowed attempts.
 */
typedef uint32_t time_diff_t;

/* Represents either a hash or hmac value in the merkle tree. */
typedef uint8_t hash_t[32];

/* Represents a single entry in a delay schedule table. */
typedef struct PACKED {
	attempt_count_t attempt_count;
	time_diff_t time_diff;
} delay_schedule_entry_t;

/* Represents table which can be used to determine the next time an
 * authentication attempt can be made.
 */
typedef delay_schedule_entry_t delay_schedule_t[16];

/* Data time used to store enough information to authenticate a low entropy
 * secret.
 */
typedef uint8_t low_entropy_secret_t[32];

/* Representation of a high entropy secret. */
typedef uint8_t high_entropy_secret_t[32];

typedef struct PACKED {
	label_t label;
	delay_schedule_t delay_schedule;
	low_entropy_secret_t low_entropy_secret;
	high_entropy_secret_t high_entropy_secret;
	high_entropy_secret_t reset_secret;
} leaf_immutable_data_t;

/* Leaf metadata wrapped by a key */
typedef struct PACKED __aligned(16) {
	uint32_t version;
	/* Immutable state. */
	leaf_immutable_data_t idat;

	/* State used to rate limit. */
	timestamp_t timestamp;
	attempt_count_t attempt_count;
} leaf_data_t;

typedef struct PACKED {
	leaf_data_t leaf_data;
} leaf_plain_text_t;

/* Represents leaf data in a form that can be exported for storage. */
typedef struct PACKED {
	/* This is included so the storage manager can compute updates to hash
	 * tree.
	 */
	hash_t hmac;
	uint8_t iv[WRAP_KEY_BLOCK_SIZE];
	uint8_t cipher_text[sizeof(leaf_plain_text_t)];
} wrapped_leaf_data_t;

/******************************************************************************/
/* Message structs
 *
 * The message format is a wng_message_header_t followed by the data
 */

enum {
	WNG_MT_ERROR_MSG = -1,
	WNG_MT_INVALID = 0,

	/* Request / "Question" types. */
	WNG_MTQ_RESET_TREE = 1,
	WNG_MTQ_INSERT_LEAF,
	WNG_MTQ_REMOVE_LEAF,
	WNG_MTQ_TRY_AUTH,
	WNG_MTQ_RESET_AUTH,

	/* Response / "Answer" types. */
	WNG_MTA_RESET_TREE = 65,
	WNG_MTA_INSERT_LEAF,
	WNG_MTA_REMOVE_LEAF,
	WNG_MTA_TRY_AUTH,
	WNG_MTA_RESET_AUTH,
} wng_message_type_enum;

typedef int8_t wng_message_type_t;

typedef struct PACKED {
	uint8_t version;
	wng_message_type_t type;
	uint16_t data_length;
} wng_message_header_t;

typedef struct PACKED {
	int32_t error_code;
} wng_message_error_t;

typedef struct PACKED {
	param_logk_t param_logk;
	param_h_t param_h;
} wng_request_reset_tree_t;

typedef struct PACKED {
	int32_t result_code;
} wng_response_reset_tree_t;

typedef struct PACKED {
	leaf_immutable_data_t idat;
	hash_t path_hashes[];
} wng_request_insert_leaf_t;

typedef struct PACKED {
	int32_t result_code;
	/* This field will be length 1 for success or 0 on failure. */
	wrapped_leaf_data_t wrapped_leaf_data[];
} wng_response_insert_leaf_t;

typedef struct PACKED {
	label_t leaf_location;
	hash_t leaf_hmac;
	hash_t path_hashes[];
} wng_request_remove_leaf_t;

typedef struct PACKED {
	int32_t result_code;
} wng_response_remove_leaf_t;

typedef struct PACKED {
	label_t leaf_location;
	wrapped_leaf_data_t wrapped_leaf_data;
	low_entropy_secret_t low_entropy_secret;
	hash_t path_hashes[];
} wng_request_try_auth_t;

typedef struct PACKED {
	int32_t result_code;
	high_entropy_secret_t high_entropy_secret;
	wrapped_leaf_data_t wrapped_leaf_data;
} wng_response_try_auth_t;

typedef struct PACKED {
	label_t leaf_location;
	wrapped_leaf_data_t wrapped_leaf_data;
	high_entropy_secret_t reset_secret;
	hash_t path_hashes[];
} wng_request_reset_auth_t;

typedef struct PACKED {
	int32_t result_code;
	wrapped_leaf_data_t wrapped_leaf_data;
} wng_response_reset_auth_t;

typedef struct {
	wng_message_header_t header;
	union {
		/* Reserve space in the union for the path_hashes fields. */
		uint8_t raw[2048 - sizeof(wng_message_header_t)];
		wng_message_error_t error;

		wng_request_reset_tree_t req_reset_tree;
		wng_request_insert_leaf_t req_insert_leaf;
		wng_request_remove_leaf_t req_remove_leaf;
		wng_request_try_auth_t req_try_auth;
		wng_request_reset_auth_t req_reset_auth;

		wng_response_reset_tree_t rsp_reset_tree;
		wng_response_insert_leaf_t rsp_insert_leaf;
		wng_response_remove_leaf_t rsp_remove_leaf;
		wng_response_try_auth_t rsp_try_auth;
		wng_response_reset_auth_t rsp_reset_auth;
	} data;
} wng_message_t;

#endif  /* __CROS_EC_WEAVER_NG_TYPES_H */
