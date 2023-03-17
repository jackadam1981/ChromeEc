/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "usb_mux.h"

#include <zephyr/shell/shell.h>
#include <zephyr/ztest.h>

#define TEST_PORT 0
#define AMD_FP6_NODE DT_NODELABEL(amd_fp6_emul0)

ZTEST_SUITE(amd_fp6_usb_mux, drivers_predicate_post_main, NULL, NULL, NULL,
	    NULL);

const struct emul *amd_fp6_emul = EMUL_DT_GET(AMD_FP6_NODE);

ZTEST(amd_fp6_usb_mux, test_usb_mode_set)
{
	/* Test a basic set to USB mode */
	usb_mux_set(TEST_PORT, USB_PD_MUX_USB_ENABLED, USB_SWITCH_CONNECT, 0);

	zassert_equal(usb_mux_get(TEST_PORT), USB_PD_MUX_USB_ENABLED);
}
