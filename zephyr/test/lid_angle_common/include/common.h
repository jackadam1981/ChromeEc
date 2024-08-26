/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file This file is used to mock the common functionalities. Specifically:
 * - __overridable attribute
 * - IS_ENABLED macro
 */

#ifndef __ZEPHYR_TEST_LID_ANGLE_COMMON_INCLUDE_COMMON_H_
#define __ZEPHYR_TEST_LID_ANGLE_COMMON_INCLUDE_COMMON_H_

#include <zephyr/sys/util_macro.h>

#define __overridable __attribute__((weak))

#endif /* __ZEPHYR_TEST_LID_ANGLE_COMMON_INCLUDE_COMMON_H_ */
