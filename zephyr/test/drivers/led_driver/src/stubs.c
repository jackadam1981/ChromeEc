// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ec_commands.h"
#include "led.h"
#include "led_common.h"

__override int board_led_alt_policy(void)
{
	return 1;
}
