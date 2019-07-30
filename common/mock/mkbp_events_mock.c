/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Mock event handling for MKBP keyboard protocol
 */

#include "mock/mkbp_events_mock.h"

#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "util.h"

struct mock_ctrl_mkbp_events mock_ctrl_mkbp_events = \
	MOCK_CTRL_DEFAULT_MKBP_EVENTS;

size_t mock_ctrl_fill_mkbp_events(const uint8_t *data, size_t size) {
	size_t copy_size = MIN(sizeof(mock_ctrl_mkbp_events), size);
	memcpy(&mock_ctrl_mkbp_events, data, copy_size);
	return copy_size;
}

int mkbp_send_event(uint8_t event_type)
{
	return mock_ctrl_mkbp_events.mkbp_send_event_return;
}
