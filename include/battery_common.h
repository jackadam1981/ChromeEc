/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_BATTERY_COMMON_H
#define __CROS_EC_BATTERY_COMMON_H

/* Returns the battery percentage [0-100] of the system. */
int get_battery_soc(void);

#endif /* #ifndef __CROS_EC_BATTERY_COMMON_H */
