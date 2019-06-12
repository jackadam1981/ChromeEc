/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __FUZZ_ROLLBACK_MOCK_H
#define __FUZZ_ROLLBACK_MOCK_H

#include <stdbool.h>

struct mock_ctrl_rollback {
	bool get_secret_fail;
};

#define MOCK_CTRL_ROLLBACK_DEFAULT                 \
	{                                              \
		.get_secret_fail = false,                  \
	}                                              \

extern struct mock_ctrl_rollback mock_ctrl_rollback;

#endif  /* __FUZZ_ROLLBACK_MOCK_H */
