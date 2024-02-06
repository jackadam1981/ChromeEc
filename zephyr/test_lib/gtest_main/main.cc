/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_app_main.h"
#include "hooks.h"
#include "pw_unit_test/framework.h"
#include "pw_unit_test/logging_event_handler.h"

#include <zephyr/kernel.h>

int main(void)
{
	ec_app_main();
	k_msleep(1000);

	testing::InitGoogleTest(nullptr, nullptr);
	pw::unit_test::LoggingEventHandler handler;
	pw::unit_test::RegisterEventHandler(&handler);
	return RUN_ALL_TESTS();
}
