/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ASSERT_H
#define __CROS_EC_ASSERT_H

#include <zephyr/fff.h>
#include <zephyr/sys/__assert.h>

#undef ASSERT
#undef assert
#ifdef CONFIG_MOCK_ASSERT
DECLARE_FAKE_VOID_FUNC(ASSERT, bool);
#define assert ASSERT
#else
#define ASSERT __ASSERT_NO_MSG
#define assert __ASSERT_NO_MSG
#endif

#endif /* __CROS_EC_ASSERT_H */
