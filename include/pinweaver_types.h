/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Shared types between Cr50 and the AP side code. */

#ifndef __CROS_EC_PINWEAVER_TYPES_H
#define __CROS_EC_PINWEAVER_TYPES_H

#include <stdint.h>

#define PW_PACKED __packed

#define PW_PROTOCOL_VERSION 0

#define PW_MAX_MESSAGE_SIZE 2048

/* The block size of encryption used for wrapped_leaf_data_t. */
#define PW_WRAP_BLOCK_SIZE 16

#define PW_ALIGN_TO_BLK __aligned(PW_WRAP_BLOCK_SIZE)

enum pw_error_codes_enum {
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
};

/* Represents the log2(fan out) of a tree. */
#define bits_per_level_t uint8_t
#define BITS_PER_LEVEL_MIN 1
#define BITS_PER_LEVEL_MAX 5

 /* Represent the height of a tree. */
#define height_t uint8_t
#define HEIGHT_MIN 1
/* This will crash for LOGK == 0 so that condition must not be allowed when
 * using this.
 */
#define HEIGHT_MAX(LOGK) ((sizeof(label_t) * 8) / LOGK)

/* Represents a child index of a node in a tree. */
#define index_t uint8_t

/* Represents the child index for each level of a tree along a path to a leaf.
 */
#define label_t uint64_t

/* Represents a count of failed login attempts. TODO(allenwebb@) This is capped
 * at twice the max attempt_count in the delay schedule to prevent overflows.
 */
#define attempt_count_t uint32_t

/* Represents a notion of time. */
struct PW_PACKED pw_timestamp_t {
	/* Number of boots. This is used to track if Cr50 has rebooted since
	 * timer_value was recorded.
	 */
	uint32_t boot_count;
	/* Currently a counter value since boot. */
	uint64_t timer_value;
};

/* Represents a time interval over the timer_value in timestamp_t.
 *
 * This only needs to be sufficiently large to represent the longest time
 * between allowed attempts.
 */
#define time_diff_t uint32_t
#define PW_BLOCK_ATTEMPTS UINT32_MAX

/* Represents either a hash or hmac value in the merkle tree. */
#define PW_HASH_SIZE 32

/* Represents a single entry in a delay schedule table. */
struct PW_PACKED delay_schedule_entry_t {
	attempt_count_t attempt_count;
	time_diff_t time_diff;
};

/* Represents the number of entries in the delay schedule table which can be
 * used to determine the next time an authentication attempt can be made.
 */
#define PW_SCHED_COUNT 16

/* Data time used to store enough information to authenticate a low entropy
 * secret.
 */
#define PW_SECRET_SIZE 32

struct PW_PACKED leaf_immutable_data_t {
	label_t label;
	struct delay_schedule_entry_t delay_schedule[PW_SCHED_COUNT];
	uint8_t low_entropy_secret[PW_SECRET_SIZE];
	uint8_t high_entropy_secret[PW_SECRET_SIZE];
	uint8_t reset_secret[PW_SECRET_SIZE];
};

/* Leaf metadata wrapped by a key */
struct PW_PACKED PW_ALIGN_TO_BLK leaf_data_t {
	uint32_t version;
	/* Immutable state. */
	struct leaf_immutable_data_t idat;

	/* State used to rate limit. */
	struct pw_timestamp_t timestamp;
	attempt_count_t attempt_count;
};

struct PW_PACKED leaf_plain_text_t {
	struct leaf_data_t leaf_data;
};

/* Represents leaf data in a form that can be exported for storage. */
struct PW_PACKED wrapped_leaf_data_t {
	/* This is included so the storage manager can compute updates to hash
	 * tree.
	 */
	uint8_t hmac[PW_HASH_SIZE];
	uint8_t iv[PW_WRAP_BLOCK_SIZE];
	uint8_t cipher_text[sizeof(struct leaf_plain_text_t)];
};

/******************************************************************************/
/* Message structs
 *
 * The message format is a pw_request_header_t followed by the data
 */

enum pw_message_type_enum {
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
};

#define pw_message_type_t int8_t

struct PW_PACKED pw_request_header_t {
	uint8_t version;
	pw_message_type_t type;
	uint16_t data_length;
};

struct PW_PACKED pw_response_header_t {
	uint8_t version;
	pw_message_type_t type;
	uint16_t data_length;
	int32_t result_code;
	uint8_t root[PW_HASH_SIZE];
};

struct PW_PACKED pw_request_reset_tree_t {
	bits_per_level_t bits_per_level;
	height_t height;
};

struct PW_PACKED pw_request_insert_leaf_t {
	struct leaf_immutable_data_t idat;
	uint8_t path_hashes[][PW_HASH_SIZE];
};

struct PW_PACKED pw_response_insert_leaf_t {
	/* This field only be sent on success. */
	struct wrapped_leaf_data_t wrapped_leaf_data;
};

struct PW_PACKED pw_request_remove_leaf_t {
	label_t leaf_location;
	uint8_t leaf_hmac[PW_HASH_SIZE];
	uint8_t path_hashes[][PW_HASH_SIZE];
};

struct PW_PACKED pw_request_try_auth_t {
	label_t leaf_location;
	struct wrapped_leaf_data_t wrapped_leaf_data;
	uint8_t low_entropy_secret[PW_SECRET_SIZE];
	uint8_t path_hashes[][PW_HASH_SIZE];
};

struct PW_PACKED pw_response_try_auth_t {
	uint8_t high_entropy_secret[PW_SECRET_SIZE];
	struct wrapped_leaf_data_t wrapped_leaf_data;
};

struct PW_PACKED pw_request_reset_auth_t {
	label_t leaf_location;
	struct wrapped_leaf_data_t wrapped_leaf_data;
	uint8_t reset_secret[PW_SECRET_SIZE];
	uint8_t path_hashes[][PW_HASH_SIZE];
};

struct PW_PACKED pw_response_reset_auth_t {
	/* This field only be sent on success. */
	struct wrapped_leaf_data_t wrapped_leaf_data;
};

struct PW_PACKED pw_request_t {
	struct pw_request_header_t header;
	union {
		/* Reserve space in the union for the path_hashes fields. */
		uint8_t raw[PW_MAX_MESSAGE_SIZE -
				sizeof(struct pw_request_header_t)];

		struct pw_request_reset_tree_t reset_tree;
		struct pw_request_insert_leaf_t insert_leaf;
		struct pw_request_remove_leaf_t remove_leaf;
		struct pw_request_try_auth_t try_auth;
		struct pw_request_reset_auth_t reset_auth;
	} data;
};

struct PW_PACKED pw_response_t {
	struct pw_response_header_t header;
	union {
		/* Reserve space in the union for the path_hashes fields. */
		uint8_t raw[PW_MAX_MESSAGE_SIZE -
				sizeof(struct pw_response_header_t)];

		struct pw_response_insert_leaf_t insert_leaf;
		struct pw_response_try_auth_t try_auth;
		struct pw_response_reset_auth_t reset_auth;
	} data;
};

#define PW_MAX_PATH_SIZE (PW_MAX_MESSAGE_SIZE - \
		sizeof(struct pw_request_header_t) - \
		sizeof(union { \
				struct pw_request_insert_leaf_t insert_leaf; \
				struct pw_request_remove_leaf_t remove_leaf; \
				struct pw_request_try_auth_t try_auth; \
				struct pw_request_reset_auth_t reset_auth; }))

#endif  /* __CROS_EC_PINWEAVER_TYPES_H */
