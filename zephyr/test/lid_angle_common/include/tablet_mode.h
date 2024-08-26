/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file This file is used to mock the tablet mode functions required by the
 * lid_angle_common.c file.
 */

#ifndef __ZEPHYR_TEST_LID_ANGLE_COMMON_INCLUDE_TABLET_MODE_H_
#define __ZEPHYR_TEST_LID_ANGLE_COMMON_INCLUDE_TABLET_MODE_H_

#include <zephyr/fff.h>

DECLARE_FAKE_VALUE_FUNC(int, tablet_get_mode);

#endif /* __ZEPHYR_TEST_LID_ANGLE_COMMON_INCLUDE_TABLET_MODE_H_ */
