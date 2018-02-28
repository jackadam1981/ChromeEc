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
	PW_ERR_ROOT_NOT_FOUND,
	PW_ERR_NV_EMPTY,
	PW_ERR_NV_LENGTH_MISMATCH,
	PW_ERR_NV_VERSION_MISMATCH,
};

/* Represents the log2(fan out) of a tree. */
struct PW_PACKED bits_per_level_t {
	uint8_t v;
};
#define BITS_PER_LEVEL_MIN 1
#define BITS_PER_LEVEL_MAX 5

 /* Represent the height of a tree. */
struct PW_PACKED height_t {
	uint8_t v;
};
#define HEIGHT_MIN 1
/* This will crash for LOGK == 0 so that condition must not be allowed when
 * using this.
 */
#define HEIGHT_MAX(LOGK) ((sizeof(struct label_t) * 8) / LOGK)

/* Represents a child index of a node in a tree. */
struct PW_PACKED index_t {
	uint8_t v;
};

/* Represents the child index for each level of a tree along a path to a leaf.
 * It is a Little-endian unsigned integer with the following value (MSB->LSB)
 * | Zero padding | 1st level index | ... | leaf index |,
 * where each index is represented by bits_per_level bits.
 */
struct PW_PACKED label_t {
	uint64_t v;
};

/* Represents a count of failed login attempts. This is capped at UINT32_MAX. */
struct PW_PACKED attempt_count_t {
	uint32_t v;
};

/* Represents a notion of time. */
struct PW_PACKED pw_timestamp_t {
	/* Number of boots. This is used to track if Cr50 has rebooted since
	 * timer_value was recorded.
	 */
	uint32_t boot_count;
	/* Seconds since boot. */
	uint64_t timer_value;
};

/* Represents a time interval in seconds.
 *
 * This only needs to be sufficiently large to represent the longest time
 * between allowed attempts.
 */
struct PW_PACKED time_diff_t {
	uint32_t v;
};
#define PW_BLOCK_ATTEMPTS UINT32_MAX

/* Number of bytes required for a hash or hmac value in the merkle tree. */
#define PW_HASH_SIZE 32

/* Represents a single entry in a delay schedule table. */
struct PW_PACKED delay_schedule_entry_t {
	struct attempt_count_t attempt_count;
	struct time_diff_t time_diff;
};

/* Represents the number of entries in the delay schedule table which can be
 * used to determine the next time an authentication attempt can be made.
 */
#define PW_SCHED_COUNT 16

/* Number of bytes required to store a secret.
 */
#define PW_SECRET_SIZE 32

/* Unencrypted part of the leaf data. */
struct PW_PACKED leaf_public_data_t {
	uint32_t protocol_version;
	struct label_t label;
	struct delay_schedule_entry_t delay_schedule[PW_SCHED_COUNT];

	/* State used to rate limit. */
	struct pw_timestamp_t timestamp;
	struct attempt_count_t attempt_count;
};

/* Encrypted part of the leaf data. */
struct PW_PACKED PW_ALIGN_TO_BLK leaf_sensitive_data_t {
	uint8_t low_entropy_secret[PW_SECRET_SIZE];
	uint8_t high_entropy_secret[PW_SECRET_SIZE];
	uint8_t reset_secret[PW_SECRET_SIZE];
};

struct PW_PACKED leaf_data_t {
	struct leaf_public_data_t pub;
	struct leaf_sensitive_data_t sec;
};


/* Represents leaf data in a form that can be exported for storage. */
struct PW_PACKED wrapped_leaf_data_t {
	/* This is first so that pub.protocol_version will be the first field
	 * in the struct to make handling different struct versions easier.
	 */
	struct leaf_public_data_t pub;
	/* Covers .pub and .cipher_text. */
	uint8_t hmac[PW_HASH_SIZE];
	uint8_t iv[PW_WRAP_BLOCK_SIZE];
	uint8_t cipher_text[sizeof(struct leaf_sensitive_data_t)];
};

/******************************************************************************/
/* Message structs
 *
 * The message format is a pw_request_header_t followed by the data
 */

enum pw_message_type_enum {
	PW_MT_ERROR_RATE_LIMTED = -2,
	PW_MT_ERROR_MSG = -1,
	PW_MT_INVALID = 0,

	/* Request / "Question" types. */
	PW_RESET_TREE = 1,
	PW_INSERT_LEAF,
	PW_REMOVE_LEAF,
	PW_TRY_AUTH,
	PW_RESET_AUTH,
	PW_GET_LOG,
	PW_LOG_REPLAY,
};

struct PW_PACKED pw_message_type_t {
	int8_t v;
};

struct PW_PACKED pw_request_header_t {
	uint8_t version;
	struct pw_message_type_t type;
	uint16_t data_length;
};

struct PW_PACKED pw_response_header_t {
	uint8_t version;
	uint16_t data_length;
	int32_t result_code;
	uint8_t root[PW_HASH_SIZE];
};

struct PW_PACKED pw_request_reset_tree_t {
	struct bits_per_level_t bits_per_level;
	struct height_t height;
};

struct PW_PACKED pw_request_insert_leaf_t {
	struct label_t label;
	struct delay_schedule_entry_t delay_schedule[PW_SCHED_COUNT];
	uint8_t low_entropy_secret[PW_SECRET_SIZE];
	uint8_t high_entropy_secret[PW_SECRET_SIZE];
	uint8_t reset_secret[PW_SECRET_SIZE];
	uint8_t path_hashes[][PW_HASH_SIZE];
};

struct PW_PACKED pw_response_insert_leaf_t {
	/* This field only be sent on success. */
	struct wrapped_leaf_data_t wrapped_leaf_data;
};

struct PW_PACKED pw_request_remove_leaf_t {
	struct label_t leaf_location;
	uint8_t leaf_hmac[PW_HASH_SIZE];
	uint8_t path_hashes[][PW_HASH_SIZE];
};

struct PW_PACKED pw_request_try_auth_t {
	struct wrapped_leaf_data_t wrapped_leaf_data;
	uint8_t low_entropy_secret[PW_SECRET_SIZE];
	uint8_t path_hashes[][PW_HASH_SIZE];
};

struct PW_PACKED pw_response_try_auth_t {
	union {
		/* Valid for the PW_ERR_RATE_LIMIT_REACHED return code only. */
		struct time_diff_t seconds_to_wait;
		struct {
			/* Valid for the PW_ERR_LOWENT_AUTH_FAILED and
			 * EC_SUCCESS return codes.
			 */
			struct wrapped_leaf_data_t wrapped_leaf_data;
			/* Valid for the EC_SUCCESS return code only. */
			uint8_t high_entropy_secret[PW_SECRET_SIZE];
		};
	};
};

struct PW_PACKED pw_request_reset_auth_t {
	struct wrapped_leaf_data_t wrapped_leaf_data;
	uint8_t reset_secret[PW_SECRET_SIZE];
	uint8_t path_hashes[][PW_HASH_SIZE];
};

struct PW_PACKED pw_response_reset_auth_t {
	/* This field only be sent on success. */
	struct wrapped_leaf_data_t wrapped_leaf_data;
};

struct PW_PACKED pw_request_get_log_t {
	/* This field only be sent on success. */
	uint8_t root[PW_HASH_SIZE];
};

struct PW_PACKED pw_request_log_replay_t {
	/* The root hash after the desired log event.
	 * The log entry that matches this hash contains all the necessary
	 * data to update wrapped_leaf_data
	 */
	uint8_t log_root[PW_HASH_SIZE];
	struct wrapped_leaf_data_t wrapped_leaf_data;
	uint8_t path_hashes[][PW_HASH_SIZE];
};

struct PW_PACKED pw_response_log_replay_t {
	struct wrapped_leaf_data_t wrapped_leaf_data;
};

struct PW_PACKED pw_get_log_entry_t {
	/* The root hash after this operation. */
	uint8_t root[PW_HASH_SIZE];
	/* The label of the leaf that was operated on. */
	struct label_t label;
	/* The type of operation. This should be one of
	 * PW_INSERT_LEAF,
	 * PW_REMOVE_LEAF,
	 * PW_TRY_AUTH.
	 *
	 * Successful PW_RESET_AUTH events are included
	 */
	struct pw_message_type_t type;
	/* Type specific fields. */
	union {
		/* PW_INSERT_LEAF */
		uint8_t leaf_hmac[PW_HASH_SIZE];
		/* PW_REMOVE_LEAF */
		/* PW_TRY_AUTH */
		struct PW_PACKED {
			struct pw_timestamp_t timestamp;
			int return_code;
		};
	};
};

struct PW_PACKED pw_request_t {
	struct pw_request_header_t header;
	union {
		/* Reserve space in the union for the path_hashes fields.
		 * This makes the size of the struct match the size of the TPM
		 * command buffer.
		 */
		uint8_t raw[PW_MAX_MESSAGE_SIZE -
				sizeof(struct pw_request_header_t)];

		struct pw_request_reset_tree_t reset_tree;
		struct pw_request_insert_leaf_t insert_leaf;
		struct pw_request_remove_leaf_t remove_leaf;
		struct pw_request_try_auth_t try_auth;
		struct pw_request_reset_auth_t reset_auth;
		struct pw_request_get_log_t get_log;
		struct pw_request_log_replay_t log_replay;
	} data;
};

struct PW_PACKED pw_response_t {
	struct pw_response_header_t header;
	union {
		/* Reserve space in the union for the get_log fields.
		 * This makes the size of the struct match the size of the TPM
		 * command buffer.
		 */
		uint8_t raw[PW_MAX_MESSAGE_SIZE -
				sizeof(struct pw_response_header_t)];

		struct pw_response_insert_leaf_t insert_leaf;
		struct pw_response_try_auth_t try_auth;
		struct pw_response_reset_auth_t reset_auth;
		/* pw_response_get_log_t is an array of type
		 * pw_get_log_entry_t with as many entries as are present
		 * in the log up to the present time or will fit in the message.
		 */
		struct pw_response_log_replay_t log_replay;
	} data;
};

#define PW_MAX_PATH_SIZE (PW_MAX_MESSAGE_SIZE - \
		sizeof(struct pw_request_header_t) - \
		sizeof(union { \
				struct pw_request_insert_leaf_t insert_leaf; \
				struct pw_request_remove_leaf_t remove_leaf; \
				struct pw_request_try_auth_t try_auth; \
				struct pw_request_reset_auth_t reset_auth; \
				struct pw_request_get_log_t get_log; \
				struct pw_request_log_replay_t log_replay; }))

#endif  /* __CROS_EC_PINWEAVER_TYPES_H */
