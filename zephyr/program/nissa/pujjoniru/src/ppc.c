/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Pujjoniru PPC alt configuration */

#include "cros_cbi.h"
#include "driver/ppc/rt1739.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "system.h"
#include "usbc/ppc.h"

LOG_MODULE_REGISTER(alt_dev_replacement, LOG_LEVEL_INF);

static bool alt_ppc;

static void alt_ppc_init(void)
{
	alt_ppc = cros_cbi_ssfc_check_match(
		CBI_SSFC_VALUE_ID(DT_NODELABEL(alt_ppc_rt1739)));

	/*setup_alt_ppc*/
	if (alt_ppc) {
		PPC_ENABLE_ALTERNATE_BY_NODELABEL(0, ppc_port0_alt);
		PPC_ENABLE_ALTERNATE_BY_NODELABEL(1, ppc_port1_alt);
		LOG_INF("Register PPC RT1937");
	} else {
		LOG_INF("Register PPC SYV682x");
	}
}
DECLARE_HOOK(HOOK_INIT, alt_ppc_init, HOOK_PRIO_POST_I2C);

static int set_rt1739(void)
{
	/*
	 * (b:286803490#comment12)
	 * this is a workaround, we initialize rt1739 in an early stage to turn
	 * on an internal MOS, so the system can boot up with lower voltage. We
	 * only want to perform this workaround once, so we do it in RO, and not
	 * do it again in RW, otherwise,re-initialize rt1739 in RW again would
	 * cause a temporary voltage drop due to switching an internal MOS, and
	 * EC would have abnormal behaviors due to sensing the wrong voltage.
	 */
	if (!system_is_in_rw()) {
		rt1739_init(0);
		rt1739_init(1);
	}
	return 0;
}

SYS_INIT(set_rt1739, POST_KERNEL, 61);
