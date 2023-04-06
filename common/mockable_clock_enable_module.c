/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "mockable_clock_enable_module.h"

test_mockable void mockable_clock_enable_module(enum module_id module,
						int enable)
{
	clock_enable_module(module, enable);
}
