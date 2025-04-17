/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_TEST_DRIVERS_USBC_ALT_MODE_TEST_USBC_ALT_MODE_H_
#define ZEPHYR_TEST_DRIVERS_USBC_ALT_MODE_TEST_USBC_ALT_MODE_H_

#include "console.h"
#include "ec_commands.h"
#include "ec_tasks.h"
#include "emul/emul_isl923x.h"
#include "emul/tcpc/emul_ps8xxx.h"
#include "emul/tcpc/emul_tcpci.h"
#include "emul/tcpc/emul_tcpci_partner_snk.h"
#include "host_command.h"
#include "tcpm/tcpci.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "usb_dp_alt_mode.h"
#include "usb_pd_vdo.h"
#include "usb_prl_sm.h"

#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/ztest.h>

#define TEST_PORT USBC_PORT_C0

/* Remove polarity for any mux checks */
#define USB_MUX_CHECK_MASK ~USB_PD_MUX_POLARITY_INVERTED
#define DPAM_VER_VDO(x) (x << 30)

struct usbc_dp_mode_fixture {
	const struct emul *tcpci_emul;
	const struct emul *charger_emul;
	struct tcpci_partner_data partner;
	struct tcpci_snk_emul_data snk_ext;
};

struct usbc_dp_mode_20_fixture {
	const struct emul *tcpci_emul;
	const struct emul *charger_emul;
	struct tcpci_partner_data partner;
	struct tcpci_snk_emul_data snk_ext;
};

#endif /* ZEPHYR_TEST_DRIVERS_USBC_ALT_MODE_TEST_USBC_ALT_MODE_H_ */
