/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Throttle Type-C port to 1.5A if both ports are sourcing. */

#include "hooks.h"
#include "chipset.h"
#include "console.h"
#include "system.h"
#include "usb_mux.h"
#include "usbc_ppc.h"
#include "util.h"
#include "typec_control.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)

#define BOARD_EVOKRT_TEST

#ifdef BOARD_EVOKRT_TEST
#define POWER_DELAY_MS 1000 /* 1000 ms for test */
#else
#define POWER_DELAY_MS 2 /* run power_monitor every 2 ms. */
#endif

static void power_monitor(void);
DECLARE_DEFERRED(power_monitor);

int is_ac_present(void)
{
	/* check ACOK pin state */
	return gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_chg_acok_od));
}

static void power_monitor(void)
{
	/*
	 * If all type-C port is sourcing power,
	 * throttled all port to 1.5A.
	 */
#ifdef BOARD_EVOKRT_TEST
	CPRINTS("%s: ppc 0 :%d, ppc 1 :%d, AC:%d", __func__,
		ppc_is_sourcing_vbus(0), ppc_is_sourcing_vbus(1),
		is_ac_present());
#endif
	if (!is_ac_present() && ppc_is_sourcing_vbus(0) &&
	    ppc_is_sourcing_vbus(1)) {
		/* set port 0 current limit to 1.5A */
		ppc_set_vbus_source_current_limit(0, TYPEC_RP_1A5);
		tcpm_select_rp_value(0, TYPEC_RP_1A5);
		pd_update_contract(0);

		/* set port 1 current limit to 1.5A */
		ppc_set_vbus_source_current_limit(1, TYPEC_RP_1A5);
		tcpm_select_rp_value(1, TYPEC_RP_1A5);
		pd_update_contract(1);
#ifdef BOARD_EVOKRT_TEST
		CPRINTS("%s: throttled all type-C port to 1.5A", __func__);
#endif
	}
	hook_call_deferred(&power_monitor_data, POWER_DELAY_MS * MSEC);
}
DECLARE_HOOK(HOOK_INIT, power_monitor, HOOK_PRIO_DEFAULT);
