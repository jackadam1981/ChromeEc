/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "device_state.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "rdd.h"
#include "registers.h"
#include "system.h"
#include "timer.h"
#include "usb_i2c.h"

#define CPRINTS(format, args...) cprints(CC_USB, format, ## args)

void usb_i2c_board_disable(void)
{
}

int usb_i2c_board_enable(void)
{
	return EC_SUCCESS;
}
