/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_TIMELS_H
#define __CROS_TIMELS_H

/* Low speed timer driver for Chrome EC */

#include "hooks.h"

/* Set up timers for the start of sleep */
int timels_setup_sleep(int is_deep_sleep);

/* When resuming from sleep update the hw timer based on the low speed clock */
void timels_update_hw_timer(void);
#endif  /* __CROS_TIMELS_H */
