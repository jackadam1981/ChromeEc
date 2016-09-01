/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef EC_PANICINFO_H
#define EC_PANICINFO_H

#include "panic.h"

int parse_panic_info(struct panic_data *pdata);

#endif /* EC_PANICINFO_H */
