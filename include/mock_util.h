/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Mock utilities to be shared between test/ and fuzz/.
 */

#ifndef _CROS_EC_MOCK_UTIL_H
#define _CROS_EC_MOCK_UTIL_H

#include "common.h"
#include <stdbool.h>

struct mock_ctrl_rollback {
	bool get_secret_fail;
};

#define MOCK_CTRL_DEFAULT_ROLLBACK		\
{						\
	.get_secret_fail = false,		\
}						\

extern struct mock_ctrl_rollback mock_ctrl_rollback;

int rollback_get_secret(uint8_t *secret);

#endif /* _CROS_EC_MOCK_UTIL_H */
