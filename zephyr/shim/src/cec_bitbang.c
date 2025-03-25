/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cec.h"
#include "driver/cec/bitbang.h"
#include "drivers/cros_cec_bitbang.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(shim_cros_cec_bitbang, LOG_LEVEL_ERR);

static const struct device *cec_bitbang_dev;

/* Do-nothing implementations which can be overridden for testing. */

test_mockable void cec_tmr_cap_start(int port, enum cec_cap_edge edge,
				     int timeout)
{
	cros_cec_bitbang_tmr_cap_start(cec_bitbang_dev, port, edge, timeout);
}

void cec_tmr_cap_stop(int port)
{
	cros_cec_bitbang_tmr_cap_stop(cec_bitbang_dev, port);
}

test_mockable int cec_tmr_cap_get(int port)
{
	return cros_cec_bitbang_tmr_cap_get(cec_bitbang_dev, port);
}

test_mockable void cec_debounce_enable(int port)
{
	cros_cec_bitbang_debounce_enable(cec_bitbang_dev, port);
}

test_mockable void cec_debounce_disable(int port)
{
	cros_cec_bitbang_debounce_disable(cec_bitbang_dev, port);
}

test_mockable void cec_trigger_send(int port)
{
	cros_cec_bitbang_trigger_send(cec_bitbang_dev, port);
}

void cec_enable_timer(int port)
{
	cros_cec_bitbang_enable_timer(cec_bitbang_dev, port);
}

void cec_disable_timer(int port)
{
	cros_cec_bitbang_disable_timer(cec_bitbang_dev, port);
}

void cec_init_timer(int port)
{
	cros_cec_bitbang_init_timer(cec_bitbang_dev, port);
}
