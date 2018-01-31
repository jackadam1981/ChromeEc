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

/* Represents the log2(fan out) of a tree. */
typedef uint8_t param_logk_t;
/* Represent the height of a tree. */
typedef uint8_t param_h_t;

/* Represents a child index of a node in a tree. */
typedef uint8_t index_t;

/* Represents the child index for each level of a tree along a path to a leaf.
 *
 * This would cap the maximum height of a tree to 8, but that should be much
 * more than needed for a fan-out of 32 (2^40 leaves).
 */
typedef index_t label_t[8];

/* Represents a count of failed login attempts. TODO(allenwebb@) This is capped
 * at twice the max attempt_count in the delay schedule to prevent overflows.
 */
typedef uint32_t attempt_count_t;

/* Represents a notion of time. */
typedef struct PACKED {
	/* Number of boots. This is used to track if Cr50 has rebooted since
	 * timer_value was recorded.
	 */
	uint64_t boot_count;
	/* Currently a counter value since boot. */
	uint64_t timer_value;
} timestamp_t;

/* Represents a time interval over the timer_value in timestamp_t.
 *
 * This only needs to be sufficiently large to represent the longest time
 * between allowed attempts.
 */
typedef uint32_t time_diff_t;

/* Represents either a hash or hmac value in the merkel tree. */
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

#endif  /* __CROS_EC_WEAVER_NG_TYPES_H */
