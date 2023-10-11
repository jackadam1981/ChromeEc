/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/ppc/ktu1125.h"
#include "emul/emul_ktu1125.h"
#include "test/drivers/test_state.h"
#include "usbc_ppc.h"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VOID_FUNC(pd_got_frs_signal, int);

#define FFF_FAKES_LIST(FAKE) FAKE(pd_got_frs_signal)

#define KTU1125_PORT 1
#define KTU1125_NODE DT_NODELABEL(ktu1125_emul)

const struct emul *ktu1125_emul = EMUL_DT_GET(KTU1125_NODE);

ZTEST(ppc_ktu1125, test_cover_set_frs_enable)
{
	ktu1125_drv.set_frs_enable(KTU1125_PORT, true);
	ktu1125_drv.set_frs_enable(KTU1125_PORT, false);
}

ZTEST(ppc_ktu1125, test_cover_set_vconn)
{
	ktu1125_drv.set_vconn(KTU1125_PORT, true);
	ktu1125_drv.set_vconn(KTU1125_PORT, false);
}

ZTEST(ppc_ktu1125, test_cover_vbus_sink_enable)
{
	ktu1125_drv.vbus_sink_enable(KTU1125_PORT, 0);
	ktu1125_drv.vbus_sink_enable(KTU1125_PORT, 1);
	ktu1125_drv.vbus_sink_enable(KTU1125_PORT, 0);
}

ZTEST(ppc_ktu1125, test_cover_vbus_source_enable)
{
	ktu1125_drv.vbus_source_enable(KTU1125_PORT, 0);
	ktu1125_drv.vbus_source_enable(KTU1125_PORT, 1);
	ktu1125_drv.vbus_source_enable(KTU1125_PORT, 0);
}

ZTEST(ppc_ktu1125, test_cover_set_polarity)
{
	ktu1125_drv.set_polarity(KTU1125_PORT, POLARITY_CC1);
	ktu1125_drv.set_polarity(KTU1125_PORT, POLARITY_CC2);
}

ZTEST(ppc_ktu1125, test_cover_set_sbu)
{
	ktu1125_drv.set_sbu(KTU1125_PORT, 0);
	ktu1125_drv.set_sbu(KTU1125_PORT, 1);
}

ZTEST(ppc_ktu1125, test_cover_set_vbus_source_current_limit)
{
	ktu1125_drv.set_vbus_source_current_limit(KTU1125_PORT, TYPEC_RP_USB);
	ktu1125_drv.set_vbus_source_current_limit(KTU1125_PORT, TYPEC_RP_1A5);
	ktu1125_drv.set_vbus_source_current_limit(KTU1125_PORT, TYPEC_RP_3A0);
}

ZTEST(ppc_ktu1125, test_cover_discharge_vbus)
{
	ktu1125_drv.discharge_vbus(KTU1125_PORT, 0);
	ktu1125_drv.discharge_vbus(KTU1125_PORT, 1);
}

ZTEST(ppc_ktu1125, test_cover_reg_dump)
{
	ktu1125_drv.reg_dump(KTU1125_PORT);
}

ZTEST(ppc_ktu1125, test_cover_interrupt)
{
	ktu1125_drv.interrupt(KTU1125_PORT);
}

ZTEST(ppc_ktu1125, test_cover_init)
{
	ktu1125_drv.init(KTU1125_PORT);
}

static void ktu1125_test_before(void *data)
{
	FFF_FAKES_LIST(RESET_FAKE);
	FFF_RESET_HISTORY();
}

ZTEST_SUITE(ppc_ktu1125, drivers_predicate_pre_main, NULL, ktu1125_test_before,
	    NULL, NULL);
