/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Shared types between Cr50 and the AP side code. */

#ifndef __CROS_EC_PINWEAVER_TYPES_H
#define __CROS_EC_PINWEAVER_TYPES_H

#include <stdint.h>

#define PW_PACKED __attribute__((__packed__))

#define PW_ALIGN_TO(x) __attribute__((aligned(16)))

#define PW_PROTOCOL_VERSION 0

#define PW_MAX_MESSAGE_SIZE 2048

/* The block size of encryption used for wrapped_leaf_data_t. */
#define PW_WRAP_BLOCK_SIZE 16

typedef enum {
	PW_ERR_VERSION_MISMATCH = 0x10000, /* EC_ERROR_INTERNAL_FIRST */
	PW_ERR_TREE_INVALID,
	PW_ERR_LENGTH_INVALID,
	PW_ERR_TYPE_INVALID,
	PW_ERR_BITS_PER_LEVEL_INVALID,
	PW_ERR_HEIGHT_INVALID,
	PW_ERR_LABEL_INVALID,
	PW_ERR_DELAY_SCHEDULE_INVALID,
	PW_ERR_PATH_AUTH_FAILED,
	PW_ERR_HMAC_AUTH_FAILED,
	PW_ERR_LOWENT_AUTH_FAILED,
	PW_ERR_RESET_AUTH_FAILED,
	PW_ERR_CRYPTO_FAILURE,
	PW_ERR_RATE_LIMIT_REACHED,
} pw_error_codes_enum;

/* Represents the log2(fan out) of a tree. */
typedef uint8_t bits_per_level_t;
#define BITS_PER_LEVEL_MIN 1
#define BITS_PER_LEVEL_MAX 8

 /* Represent the height of a tree. */
typedef uint8_t height_t;
#define HEIGHT_MIN 1
/* This will crash for LOGK == 0 so that condition must not be allowed when
 * using this.
 */
#define HEIGHT_MAX(LOGK) ((sizeof(label_t) * 8) / LOGK)

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
typedef struct PW_PACKED {
	/* Number of boots. This is used to track if Cr50 has rebooted since
	 * timer_value was recorded.
	 */
	uint32_t boot_count;
	/* Currently a counter value since boot. */
	uint64_t timer_value;
} pw_timestamp_t;

/* Represents a time interval over the timer_value in timestamp_t.
 *
 * This only needs to be sufficiently large to represent the longest time
 * between allowed attempts.
 */
typedef uint32_t time_diff_t;
#define PW_BLOCK_ATTEMPTS UINT32_MAX

/* Represents either a hash or hmac value in the merkle tree. */
typedef uint8_t hash_t[32];

/* Represents a single entry in a delay schedule table. */
typedef struct PW_PACKED {
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

typedef struct PW_PACKED {
	label_t label;
	delay_schedule_t delay_schedule;
	low_entropy_secret_t low_entropy_secret;
	high_entropy_secret_t high_entropy_secret;
	high_entropy_secret_t reset_secret;
} leaf_immutable_data_t;

/* Leaf metadata wrapped by a key */
typedef struct PW_PACKED PW_ALIGN_TO(PW_WRAP_BLOCK_SIZE) {
	uint32_t version;
	/* Immutable state. */
	leaf_immutable_data_t idat;

	/* State used to rate limit. */
	pw_timestamp_t timestamp;
	attempt_count_t attempt_count;
} leaf_data_t;

typedef struct PW_PACKED {
	leaf_data_t leaf_data;
} leaf_plain_text_t;

/* Represents leaf data in a form that can be exported for storage. */
typedef struct PW_PACKED {
	/* This is included so the storage manager can compute updates to hash
	 * tree.
	 */
	hash_t hmac;
	uint8_t iv[PW_WRAP_BLOCK_SIZE];
	uint8_t cipher_text[sizeof(leaf_plain_text_t)];
} wrapped_leaf_data_t;

/******************************************************************************/
/* Message structs
 *
 * The message format is a pw_request_header_t followed by the data
 */

typedef enum {
	PW_MT_ERROR_MSG = -1,
	PW_MT_INVALID = 0,

	/* Request / "Question" types. */
	PW_MTQ_RESET_TREE = 1,
	PW_MTQ_INSERT_LEAF,
	PW_MTQ_REMOVE_LEAF,
	PW_MTQ_TRY_AUTH,
	PW_MTQ_RESET_AUTH,

	/* Response / "Answer" types. */
	PW_MTA_RESET_TREE = 65,
	PW_MTA_INSERT_LEAF,
	PW_MTA_REMOVE_LEAF,
	PW_MTA_TRY_AUTH,
	PW_MTA_RESET_AUTH,
} pw_message_type_enum;

typedef int8_t pw_message_type_t;

typedef struct PW_PACKED {
	uint8_t version;
	pw_message_type_t type;
	uint16_t data_length;
} pw_request_header_t;

typedef struct PW_PACKED {
	uint8_t version;
	pw_message_type_t type;
	uint16_t data_length;
	int32_t result_code;
	hash_t root;
} pw_response_header_t;

typedef struct PW_PACKED {
	bits_per_level_t bits_per_level;
	height_t height;
} pw_request_reset_tree_t;

typedef struct PW_PACKED {
	leaf_immutable_data_t idat;
	hash_t path_hashes[];
} pw_request_insert_leaf_t;

typedef struct PW_PACKED {
	/* This field only be sent on success. */
	wrapped_leaf_data_t wrapped_leaf_data;
} pw_response_insert_leaf_t;

typedef struct PW_PACKED {
	label_t leaf_location;
	hash_t leaf_hmac;
	hash_t path_hashes[];
} pw_request_remove_leaf_t;

typedef struct PW_PACKED {
	label_t leaf_location;
	wrapped_leaf_data_t wrapped_leaf_data;
	low_entropy_secret_t low_entropy_secret;
	hash_t path_hashes[];
} pw_request_try_auth_t;

typedef struct PW_PACKED {
	high_entropy_secret_t high_entropy_secret;
	wrapped_leaf_data_t wrapped_leaf_data;
} pw_response_try_auth_t;

typedef struct PW_PACKED {
	label_t leaf_location;
	wrapped_leaf_data_t wrapped_leaf_data;
	high_entropy_secret_t reset_secret;
	hash_t path_hashes[];
} pw_request_reset_auth_t;

typedef struct PW_PACKED {
	/* This field only be sent on success. */
	wrapped_leaf_data_t wrapped_leaf_data;
} pw_response_reset_auth_t;

typedef struct PW_PACKED {
	pw_request_header_t header;
	union {
		/* Reserve space in the union for the path_hashes fields. */
		uint8_t raw[PW_MAX_MESSAGE_SIZE -
				sizeof(pw_request_header_t)];

		pw_request_reset_tree_t reset_tree;
		pw_request_insert_leaf_t insert_leaf;
		pw_request_remove_leaf_t remove_leaf;
		pw_request_try_auth_t try_auth;
		pw_request_reset_auth_t reset_auth;
	} data;
} pw_request_t;

typedef struct PW_PACKED {
	pw_response_header_t header;
	union {
		/* Reserve space in the union for the path_hashes fields. */
		uint8_t raw[PW_MAX_MESSAGE_SIZE -
				sizeof(pw_response_header_t)];

		pw_response_insert_leaf_t insert_leaf;
		pw_response_try_auth_t try_auth;
		pw_response_reset_auth_t reset_auth;
	} data;
} pw_response_t;

#define PW_MAX_PATH_SIZE (PW_MAX_MESSAGE_SIZE - sizeof(pw_request_header_t) \
		- sizeof(union { \
				pw_request_insert_leaf_t insert_leaf; \
				pw_request_remove_leaf_t remove_leaf; \
				pw_request_try_auth_t try_auth; \
				pw_request_reset_auth_t reset_auth; }))

#endif  /* __CROS_EC_PINWEAVER_TYPES_H */
