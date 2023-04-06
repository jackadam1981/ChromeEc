/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* The mockable version of clock_enable_module */

#ifndef __CROS_EC_MOCKABLE_CLOCK_ENABLE_MODULE_H
#define __CROS_EC_MOCKABLE_CLOCK_ENABLE_MODULE_H

#include "clock.h"

#ifdef __cplusplus
extern "C" {
#endif

void mockable_clock_enable_module(enum module_id module, int enable);

#ifdef __cplusplus
}
#endif

#endif /* __CROS_EC_MOCKABLE_CLOCK_ENABLE_MODULE_H */
