/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Version number for Chrome EC */

#ifndef __CROS_EC_WEAVER_NG_TYPES_H
#define __CROS_EC_WEAVER_NG_TYPES_H

#include <stdint.h>

#ifndef PACKED
#define PACKED __attribute__((__packed__))
#endif  /* PACKED */

typedef uint8_t index_t;

/* This would cap the maximum depth of a tree to 16, but that should be much
 * more than needed for a fan-out of 256 (2^128 leaves).
 */
typedef index_t label_t[16];

typedef uint32_t attempt_count_t;

typedef struct PACKED {
	uint64_t timer_value;
} timestamp_t;

typedef union {
	uint8_t bytes[32];
} hmac_t;

typedef enum {
	/* Force a signed type to get well defined behavior. */
	DS_RESERVED_1 = -1,
	/* Prevent 0 from having a meaningful value. */
	DS_RESERVED_0 = 0,

	PIN_SCHEDULE = 1,
	PASSWORD_SCHEDULE = 2,

	/* Force a 32-bit size to control the struct alignment */
	DS_MAX = INT32_MAX,
} delay_schedule_t;

typedef union {
	uint8_t bytes[32];
} low_entropy_secret_t;

typedef union {
	uint8_t bytes[32];
} high_entropy_secret_t;

typedef struct PACKED {
	label_t label;
	attempt_count_t attempt_count;
	timestamp_t timestamp;
	delay_schedule_t delay_schedule;
	low_entropy_secret_t low_entropy_secret;
	high_entropy_secret_t high_entropy_secret;
	high_entropy_secret_t reset_secret;
} leaf_data_t;

#endif  /* __CROS_EC_WEAVER_NG_TYPES_H */
