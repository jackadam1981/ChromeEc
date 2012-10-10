/*
 * Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Keyboard scanner test module for Chrome EC */

#ifndef __CROS_EC_KEYBOARD_TEST_H
#define __CROS_EC_KEYBOARD_TEST_H

#include <timer.h>

/*
 * Keyboard scan test item - contains a single scan to 'present' to key scan
 * logic.
 */
struct keyscan_item {
	timestamp_t abs_time;	/* absolute timestamp to present this item */
	uint32_t time_us;	/* time for this item relative to test start */
	uint8_t done;		/* 1 if we managed to present this */
	uint8_t scan[KB_OUTPUTS];
};

#ifdef CONFIG_KEYBOARD_TEST
/**
 * Get the next key scan from the test sequence, if any
 *
 * @param column	Column to read (-1 to OR all columns together
 * @param scan		Raw scan data read from GPIOs
 * @return test scan, or just 'scan' if no test is active
 */
uint8_t keyscan_seq_get_scan(int column, uint8_t scan);

/**
 * Check if we are in the middle of a test sequence
 *
 * @return 0 if no test sequence is active, non-zero if it is active.
 */
int keyscan_seq_is_active(void);
#else

/* When we don't support keyboard test, just return the original scan */
#define keyscan_seq_get_scan(column, scan)	(scan)

/* We don't ever have an active scan */
#define keyscan_seq_is_active()		0
#endif

#endif
