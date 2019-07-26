/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __MOCK_ROLLBACK_MOCK_H
#define __MOCK_ROLLBACK_MOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mock_ctrl_rollback {
	bool get_secret_fail;
};

#define MOCK_CTRL_DEFAULT_ROLLBACK             \
{                                              \
	.get_secret_fail = false,              \
}                                              \

extern struct mock_ctrl_rollback mock_ctrl_rollback;

size_t mock_ctrl_fill_rollback(const uint8_t *data, size_t size);

#endif  /* __MOCK_ROLLBACK_MOCK_H */
