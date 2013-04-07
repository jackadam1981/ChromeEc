/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Macros for mocking functions */

#ifndef __CROS_EC_MOCK_H
#define __CROS_EC_MOCK_H

#ifdef TEST_BUILD
#define test_mockable __attribute__((weak))
#define test_mockable_static __attribute__((weak))
#else
#define test_mockable
#define test_mockable_static static
#endif

#endif  /* __CROS_EC_MOCK_H */
