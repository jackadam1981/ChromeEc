// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ec_commands.h"
#include "led.h"
#include "led_common.h"

__override enum pwr_led_sup pwr_led_support_check(void)
{
	return PWR_LED_PRESENT;
}
