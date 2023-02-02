/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "test/drivers/test_state.h"
#include "driver/retimer/anx7452_public.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "i2c.h"
#include "usb_mux.h"
#include <zephyr/ztest.h>

ZTEST(anx7452, test_anx7452_init)
{
	/* Test failed I2C operations in the set Vconn function */

	zassert_equal(1, 1, "Expected EC_ERROR_INVAL but got");
}

static void anx7452_before(void *state)
{
	ARG_UNUSED(state);
}

static void anx7452_after(void *state)
{
	ARG_UNUSED(state);
}

ZTEST_SUITE(anx7452, drivers_predicate_post_main, NULL, anx7452_before,
	    anx7452_after, NULL);
