/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "hooks.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(trulo, LOG_LEVEL_INF);

/* Trigger hibernate by enabling the Z-sleep circuit */
__override void board_hibernate_late(void)
{
}
