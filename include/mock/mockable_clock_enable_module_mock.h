/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __MOCK_MOCKABLE_CLOCK_ENABLE_MODULE_H
#define __MOCK_MOCKABLE_CLOCK_ENABLE_MODULE_H

#include "clock.h"

#ifdef __cplusplus
extern "C" {
#endif

void mockable_clock_enable_module(enum module_id module, int enable);

int get_mock_fast_cpu_status(void);

#ifdef __cplusplus
}
#endif

#endif /* __MOCK_MOCKABLE_CLOCK_ENABLE_MODULE_H */
