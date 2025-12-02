/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/it8xxx2.h"
#include "driver/tcpm/it8xxx2_public.h"
#include "driver/tcpm/tcpci.h"
#include "emul/tcpc/emul_it8xxx2.h"
#include "test/drivers/test_state.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define IT8XXX2_PORT 1
#define IT8XXX2_NODE DT_NODELABEL(it8xxx2_emul)

const struct emul *it8xxx2_emul = EMUL_DT_GET(IT8XXX2_NODE);

ZTEST(tcpc_it8xxx2, test_enter_l_p_m)
{
	zassert_ok(tcpm_enter_low_power_mode(IT8XXX2_PORT));
}

ZTEST(tcpc_it8xxx2, test_set_vconn)
{
	zassert_ok(tcpm_set_vconn(IT8XXX2_PORT, 0));
	zassert_ok(tcpm_set_vconn(IT8XXX2_PORT, 1));
	zassert_ok(tcpm_set_vconn(IT8XXX2_PORT, 0));
}

ZTEST(tcpc_it8xxx2, test_set_polarity)
{
	zassert_ok(tcpm_set_polarity(IT8XXX2_PORT, POLARITY_CC1));

	zassert_ok(
		tcpci_emul_set_reg(it8xxx2_emul, TCPC_REG_CC_STATUS,
				   TCPC_REG_CC_STATUS_SET(0, TYPEC_CC_VOLT_RA,
							  TYPEC_CC_VOLT_OPEN)));

	zassert_ok(tcpm_set_polarity(IT8XXX2_PORT, POLARITY_CC1));
}

static void it8xxx2_test_before(void *data)
{
	zassert_ok(
		tcpci_emul_set_reg(it8xxx2_emul, TCPC_REG_CC_STATUS,
				   TCPC_REG_CC_STATUS_SET(0, TYPEC_CC_VOLT_OPEN,
							  TYPEC_CC_VOLT_OPEN)));
}

ZTEST_SUITE(tcpc_it8xxx2, drivers_predicate_post_main, NULL,
	    it8xxx2_test_before, NULL, NULL);
