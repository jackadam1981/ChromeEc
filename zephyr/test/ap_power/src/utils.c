/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include <shell/shell_uart.h>
#include <drivers/gpio/gpio_emul.h>

#include "hooks.h"
#include "chipset.h"
#include "utils.h"

void chipset_force_shutdown(enum chipset_shutdown_reason reason)
{
	new_chipset_force_shutdown();
}
