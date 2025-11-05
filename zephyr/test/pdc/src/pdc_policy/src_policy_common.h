/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef TEST_SRC_POLICY_COMMON_H
#define TEST_SRC_POLICY_COMMON_H

#include "chipset.h"
#include "emul/emul_pdc.h"

#define PDC_TEST_TIMEOUT 2000

/* TODO: b/343760437 - Once the emulator can detect the PDC threads are idle,
 * remove the sleep delay to let the policy code run.
 */
#define PDC_POLICY_DELAY_MS 500
#define PDC_NODE_PORT0 DT_NODELABEL(pdc_emul1)
#define PDC_NODE_PORT1 DT_NODELABEL(pdc_emul2)

#define TEST_USBC_PORT0 USBC_PORT_FROM_PDC_DRIVER_NODE(PDC_NODE_PORT0)
#define TEST_USBC_PORT1 USBC_PORT_FROM_PDC_DRIVER_NODE(PDC_NODE_PORT1)

bool pdc_power_mgmt_is_pd_attached(int port);

struct src_policy_fixture {
	const struct emul *emul_pdc[CONFIG_USB_PD_PORT_MAX_COUNT];
};

struct ec_response_usb_pd_power_info host_cmd_power_info(int port);

/* Read the LPM's source PDO and verify the voltage and current. */
int verify_lpm_source_pdo(struct src_policy_fixture *fixture, uint32_t port,
			  int mv, int ma, int PDO_PEAK_OCP);

#endif /* TEST_SRC_POLICY_COMMON_H */
