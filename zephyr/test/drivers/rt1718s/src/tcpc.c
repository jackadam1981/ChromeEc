/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/rt1718s_public.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
LOG_MODULE_REGISTER(rt1718s_tcpc_test, CONFIG_TCPCI_EMUL_LOG_LEVEL);

#define RT1718S_NODE DT_NODELABEL(rt1718s_emul)

static const int tcpm_rt1718s_port = USBC_PORT_C0;

ZTEST_SUITE(rt1718s_tcpc, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST(rt1718s_tcpc, test_init)
{
	zassert_ok(rt1718s_tcpm_drv.init(tcpm_rt1718s_port),
		   "Cannot initialize rt1718s");
}
