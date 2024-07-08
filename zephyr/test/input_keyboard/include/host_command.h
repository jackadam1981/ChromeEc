/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

#define DECLARE_HOST_COMMAND(...)

enum ec_status {
	EC_RES_SUCCESS,
	EC_RES_ACCESS_DENIED,
	EC_RES_INVALID_PARAM,
};

struct host_cmd_handler_args {
	const void *params;
};

struct ec_params_mkbp_simulate_key {
	uint8_t col;
	uint8_t row;
	uint8_t pressed;
};
