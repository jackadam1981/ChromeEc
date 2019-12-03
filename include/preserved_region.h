/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* A mechanism to preserve a data region across resets */

#ifndef __CROS_EC_PRESERVED_REGION_H
#define __CROS_EC_PRESERVED_REGION_H

#include "common.h"

extern char preserved_region[CONFIG_PRESERVED_REGION_SIZE];

#endif	/* __CROS_EC_PRESERVED_REGION_H */
