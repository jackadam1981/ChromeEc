/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Ask the EC to set the lightbar state to reflect the CPU activity */

#ifndef __CROS_EC_LIGHTBAR_H
#define __CROS_EC_LIGHTBAR_H

/****************************************************************************/
/* Internal stuff */

/* Define the types of sequences */
#define LBMSG(state) LIGHTBAR_##state
#include "lightbar_msg_list.h"
enum lightbar_sequence {
	LIGHTBAR_MSG_LIST
	LIGHTBAR_NUM_SEQUENCES
};
#undef LBMSG

/* Request a preset sequence from the lightbar task. */
void lightbar_sequence(enum lightbar_sequence s);

struct rgb_s {
	uint8_t r, g, b;
};

/* List of tweakable parameters. NOTE: It's __packed so it can be sent in a
 * host command, but the alignment is the same regardless. Keep it that way.
 */
struct lightbar_params {
	/* Timing */
	int google_ramp_up;
	int google_ramp_down;
	int s3s0_ramp_up;
	int s0_tick_delay[2];			/* AC=0/1 */
	int s0s3_ramp_down;
	int s3_sleep_for;
	int s3_tick_delay;

	/* Phase shift */
	uint8_t w_ofs;

	/* Brightness limits based on the backlight and AC. */
	uint8_t bright_bl_off_fixed[2];		/* AC=0/1 */
	uint8_t bright_bl_on_min[2];		/* AC=0/1 */
	uint8_t bright_bl_on_max[2];		/* AC=0/1 */

	/* Map [AC][battery_level] to color index */
	uint8_t s0_idx[2][4];			/* AP is running */
	uint8_t s3_idx[2][4];			/* AP is sleeping */

	/* Color pallette */
	struct rgb_s color[8];			/* 0-3 are Google colors */
} __packed;


/****************************************************************************/
/* External stuff */

/* These are used for demo purposes */
extern void demo_battery_level(int inc);
extern void demo_is_charging(int ischarge);
extern void demo_brightness(int inc);

#endif  /* __CROS_EC_LIGHTBAR_H */
