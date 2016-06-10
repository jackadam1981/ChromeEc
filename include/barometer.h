/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_BAROMETER_H
#define __CROS_EC_BAROMETER_H

#include "common.h"
#include "motion_sense.h"

enum barosensor_func {
	BAROSENSOR_FUNC_PRESSURE = 1,
	BAROSENSOR_FUNC_TEMPERATURE = 2,
	BAROSENSOR_FUNC_MAX,
};

#endif
