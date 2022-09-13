/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Controls for the mock rollback block library
 */

#ifndef __MOCK_ROLLBACK_MOCK_H
#define __MOCK_ROLLBACK_MOCK_H

#include <stdbool.h>

struct mock_ctrl_rollback {
	bool get_secret_fail;
};

enum mock_ctrl_latest_rollback_type {
	GET_LATEST_ROLLBACK_FAIL,
	GET_LATEST_ROLLBACK_ZEROS,
	GET_LATEST_ROLLBACK_REAL,
};

struct mock_ctrl_latest_rollback {
	enum mock_ctrl_latest_rollback_type output_type;
};

#define MOCK_CTRL_DEFAULT_ROLLBACK        \
	(struct mock_ctrl_rollback)       \
	{                                 \
		.get_secret_fail = false, \
	}

#define MOCK_CTRL_DEFAULT_LATEST_ROLLBACK    \
	((struct mock_ctrl_latest_rollback){ \
		.output_type = GET_LATEST_ROLLBACK_REAL })

extern struct mock_ctrl_rollback mock_ctrl_rollback;

extern struct mock_ctrl_latest_rollback mock_ctrl_latest_rollback;

#endif /* __MOCK_ROLLBACK_MOCK_H */
