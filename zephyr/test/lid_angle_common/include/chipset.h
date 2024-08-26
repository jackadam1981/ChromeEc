/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file This file is used to mock the chipset functionality used by the
 * lid_angle_common.c source.
 */

#ifndef __ZEPHYR_TEST_LID_ANGLE_COMMON_INCLUDE_CHIPSET_H_
#define __ZEPHYR_TEST_LID_ANGLE_COMMON_INCLUDE_CHIPSET_H_

#include <zephyr/fff.h>

enum chip_state {
	CHIPSET_STATE_ON = 0,
	CHIPSET_STATE_OFF = 1,
};

DECLARE_FAKE_VALUE_FUNC(int, chipset_in_state, enum chip_state);

#endif /* __ZEPHYR_TEST_LID_ANGLE_COMMON_INCLUDE_CHIPSET_H_ */
