/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file This file is used to mock the keyboard scanning functionality needed by
 * lid_angle_common.c.
 */

#ifndef __ZEPHYR_TEST_LID_ANGLE_COMMON_INCLUDE_KEYBOARD_SCAN_H_
#define __ZEPHYR_TEST_LID_ANGLE_COMMON_INCLUDE_KEYBOARD_SCAN_H_

#include <zephyr/fff.h>

enum kb_scan_disable_masks {
	KB_SCAN_DISABLE_LID_ANGLE = 0,
};

DECLARE_FAKE_VOID_FUNC(keyboard_scan_enable, int, enum kb_scan_disable_masks);

#endif /* __ZEPHYR_TEST_LID_ANGLE_COMMON_INCLUDE_KEYBOARD_SCAN_H_ */
