/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_HAMMER_TOUCHPAD_PASSTHRU
#define __CROS_HAMMER_TOUCHPAD_PASSTHRU

#include <stdint.h>

#define ROWS		24
#define COLS		14

#define FRAME_SIZE	(ROWS * COLS * 2)

struct __attribute__((packed)) touchpad_passthru_report {
	uint8_t frame[FRAME_SIZE];
};

void touchpad_passthru_generate_event(void);

#endif /* __CROS_HAMMER_TOUCHPAD_PASSTHRU */
