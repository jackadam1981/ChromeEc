/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 * @brief Mock event handling for MKBP keyboard protocol
 */

#include <stdint.h>

int mkbp_send_event(uint8_t event_type)
{
	return 1;
}
