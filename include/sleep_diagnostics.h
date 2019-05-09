/*
 * Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_BOARD_CR50_SLEEP_DIAGNOSTICS_H
#define __EC_BOARD_CR50_SLEEP_DIAGNOSTICS_H


/*
 * Subcommand code, used to pass different CCD commands using the same TPM
 * vendor command.
 */
enum sleep_info_subcommand {
	SLEEPV_RESET = 0,
	SLEEPV_RESETNEXT = 1,
	SLEEPV_MS = 2,
	SLEEPV_US = 3,
};

struct sleep_info_response {
	uint32_t sleep_scale;
	uint32_t total_sleep_time;
	uint32_t reset_next_sleep;
	uint32_t ds_time;
	uint32_t total_time;
} __packed;

void board_entered_sleep(void);

void board_left_sleep(void);

#endif  /* ! __EC_BOARD_SLEEP_DIAGNOSTICS_H */
