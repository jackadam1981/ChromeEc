/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Feature set module for Chrome EC */

#ifndef __CROS_EC_FEATURE_SET_H
#define __CROS_EC_FEATURE_SET_H

#include "common.h"

enum feature_code {
	EC_FEATURE_I8042 = 1,
};

#define EC_FEATURE_MASK(feature_code) (1UL << ((feature_code) - 1))

uint32_t features_are_supported(uint32_t mask);
uint32_t features_are_enabled(uint32_t mask);

void unlock_features(void);

#endif  /* __CROS_EC_FEATURE_SET_H */
