/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dfu.h"

#include <zephyr/init.h>
#include <zephyr/kernel.h>

/*
 * Handle the final application initialization before starting threads.
 */
int app_init(const struct device *device)
{
	UNUSED(device);
	dfu_boot_check();
	return 0;
}

SYS_INIT(app_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
