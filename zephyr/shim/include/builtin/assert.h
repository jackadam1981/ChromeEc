/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ASSERT_H
#define __CROS_EC_ASSERT_H


#undef ASSERT
#undef assert

#ifndef CONFIG_MEMFAULT
#include <zephyr/sys/__assert.h>
#define ASSERT __ASSERT_NO_MSG
#define assert __ASSERT_NO_MSG

#else
#define MEMFAULT_NORETURN __attribute__((__noreturn__))
#include "memfault/panics/assert.h"

#define ASSERT MEMFAULT_ASSERT
#define assert MEMFAULT_ASSERT
#endif

#endif /* __CROS_EC_ASSERT_H */
