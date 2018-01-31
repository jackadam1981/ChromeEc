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
#define PW_LEAF_MAJOR_VERSION 0
#define PW_LEAF_MINOR_VERSION 0

#define PW_MAX_MESSAGE_SIZE 2048

/* The block size of encryption used for wrapped_leaf_data_t. */
#define PW_WRAP_BLOCK_SIZE 16

#define PW_ALIGN_TO_WRD __aligned(4)

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
	PW_ERR_LEAF_VERSION_MISMATCH,
	PW_ERR_HMAC_AUTH_FAILED,
	PW_ERR_LOWENT_AUTH_FAILED,
	PW_ERR_RESET_AUTH_FAILED,
	PW_ERR_CRYPTO_FAILURE,
	PW_ERR_RATE_LIMIT_REACHED,
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

struct PW_PACKED leaf_version_t {
	union {
		uint32_t v;
		struct PW_PACKED {
			uint16_t minor;
			uint16_t major;
		};
	};
};

/* Do not change this within the same PW_LEAF_MAJOR_VERSION. */
struct PW_PACKED leaf_header_t {
	/* Always have leaf_version at the beginning of
	 * struct wrapped_leaf_data_t to maintain preditable behavior across
	 * versions.
	 */
	struct leaf_version_t leaf_version;
	uint16_t pub_len;
	uint16_t sec_len;
};

/* Do not remove fields within the same PW_LEAF_MAJOR_VERSION. */
/* Unencrypted part of the leaf data. */
struct PW_PACKED leaf_public_data_t {
	struct label_t label;
	struct delay_schedule_entry_t delay_schedule[PW_SCHED_COUNT];

	/* State used to rate limit. */
	struct pw_timestamp_t timestamp;
	struct attempt_count_t attempt_count;
};

/* Do not remove fields within the same PW_LEAF_MAJOR_VERSION. */
/* Encrypted part of the leaf data. */
struct PW_PACKED PW_ALIGN_TO_BLK leaf_sensitive_data_t {
	uint8_t low_entropy_secret[PW_SECRET_SIZE];
	uint8_t high_entropy_secret[PW_SECRET_SIZE];
	uint8_t reset_secret[PW_SECRET_SIZE];
};

/* Represents leaf data in a form that can be exported for storage. */
struct PW_PACKED wrapped_leaf_data_t {
	/* This is first so that head.leaf_version will be the first field
	 * in the struct to keep the meaning of the struct from becoming
	 * ambiguous across versions.
	 */
	struct leaf_header_t head;
	/* Covers .head, .pub, and .cipher_text. */
	uint8_t hmac[PW_HASH_SIZE];
	uint8_t iv[PW_WRAP_BLOCK_SIZE];
	struct leaf_public_data_t pub;
	uint8_t cipher_text[sizeof(struct leaf_sensitive_data_t)];
};

/* Represents a struct of unknown length to be imported to process a request. */
struct PW_PACKED unimported_leaf_data_t {
	/* This is first so that head.leaf_version will be the first field
	 * in the struct to make handling different struct versions easier.
	 */
	struct leaf_header_t head;
	/* Covers .head, .iv, .pub, and .cipher_text. */
	uint8_t hmac[PW_HASH_SIZE];
	uint8_t iv[PW_WRAP_BLOCK_SIZE];
	/* Has following layout:
	 * uint8_t pub_data[head.pub_len];
	 * uint8_t ciphter_text[head.sec_len];
	 */
	uint8_t payload[];
};

/* Same as above except this struct is used to separate the case where the
 * sibling hashes are included.
 */
struct PW_PACKED unimported_leaf_data_and_hashes_t {
	/* This is first so that head.leaf_version will be the first field
	 * in the struct to make handling different struct versions easier.
	 */
	struct leaf_header_t head;
	/* Covers .head, .pub, and .cipher_text. */
	uint8_t hmac[PW_HASH_SIZE];
	uint8_t iv[PW_WRAP_BLOCK_SIZE];
	/* Has following layout:
	 * uint8_t pub_data[head.pub_len];
	 * uint8_t ciphter_text[head.sec_len];
	 * uint8_t path_hashes[get_path_auxiliary_hash_count(.)][PW_HASH_SIZE];
	 */
	uint8_t payload[];
};

/******************************************************************************/
/* Message structs
 *
 * The message format is a pw_request_header_t followed by the data
 */

enum pw_message_type_enum {
	PW_MT_INVALID = 0,

	/* Request / "Question" types. */
	PW_RESET_TREE = 1,
	PW_INSERT_LEAF,
	PW_REMOVE_LEAF,
	PW_TRY_AUTH,
	PW_RESET_AUTH,
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
	struct unimported_leaf_data_t unimported_leaf_data;
};

struct PW_PACKED pw_request_remove_leaf_t {
	struct label_t leaf_location;
	uint8_t leaf_hmac[PW_HASH_SIZE];
	uint8_t path_hashes[][PW_HASH_SIZE];
};

struct PW_PACKED pw_request_try_auth_t {
	uint8_t low_entropy_secret[PW_SECRET_SIZE];
	struct unimported_leaf_data_and_hashes_t unimported_leaf_data;
};

struct PW_PACKED pw_response_try_auth_t {
	union {
		/* Valid for the PW_ERR_RATE_LIMIT_REACHED return code only. */
		struct time_diff_t seconds_to_wait;
		struct {
			/* Valid for the EC_SUCCESS return code only. */
			uint8_t high_entropy_secret[PW_SECRET_SIZE];
			/* Valid for the PW_ERR_LOWENT_AUTH_FAILED and
			 * EC_SUCCESS return codes.
			 */
			struct unimported_leaf_data_t unimported_leaf_data;
		};
	};
};

struct PW_PACKED pw_request_reset_auth_t {
	uint8_t reset_secret[PW_SECRET_SIZE];
	struct unimported_leaf_data_and_hashes_t unimported_leaf_data;
};

struct PW_PACKED pw_response_reset_auth_t {
	struct unimported_leaf_data_t unimported_leaf_data;
};

struct PW_PACKED pw_request_t {
	struct pw_request_header_t header;
	union {
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

		struct pw_response_insert_leaf_t insert_leaf;
		struct pw_response_try_auth_t try_auth;
		struct pw_response_reset_auth_t reset_auth;
	} data;
};

#define PW_MAX_PATH_SIZE 1536

#endif  /* __CROS_EC_PINWEAVER_TYPES_H */
