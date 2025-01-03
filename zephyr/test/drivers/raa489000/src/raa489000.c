/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/raa489000.h"
#include "driver/tcpm/tcpci.h"
#include "emul/tcpc/emul_raa489000.h"
#include "emul/tcpc/emul_tcpci.h"
#include "test/drivers/tcpci_test_common.h"
#include "test/drivers/test_state.h"
#include "usb_pd_tcpm.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define RAA489000_PORT 0
#define RAA489000_EMUL_NODE DT_NODELABEL(raa489000_emul)

FAKE_VALUE_FUNC(bool, pd_vbus_valid_for_bist, int);

ZTEST(tcpc_raa489000, test_check_vendor)
{
	int v;

	zassert_ok(tcpc_read16(RAA489000_PORT, TCPC_REG_VENDOR_ID, &v));
	zassert_equal(v, 0x45b);

	tcpm_dump_registers(RAA489000_PORT);
}

void test_tcpci_get_rx_message_raw(const struct emul *emul,
				   struct i2c_common_emul_data *common_data,
				   enum usbc_port port)
{
	const struct tcpm_drv *drv = tcpc_config[port].drv;
	struct tcpci_emul_msg msg;
	uint32_t payload[1];
	/* bist mode message header and payload */
	uint8_t buf[32] = {0x3, 0x30, 0x0, 0x0, 0x0, 0x80};
	int head;
	int size;
	bool bist_enabled;

	RESET_FAKE(pd_vbus_valid_for_bist);
	pd_vbus_valid_for_bist_fake.return_val = 1;

	tcpci_emul_set_reg(emul, TCPC_REG_RX_DETECT,
			   TCPC_REG_RX_DETECT_SOP | TCPC_REG_RX_DETECT_SOPP);

	msg.buf = buf;
	msg.cnt = 31;
	msg.sop_type = TCPCI_MSG_SOP;
	zassert_equal(TCPCI_EMUL_TX_SUCCESS,
		      tcpci_emul_add_rx_msg(emul, &msg, true),
		      "Failed to setup emulator message");

	/* Test fail on reading message */
	i2c_common_emul_set_read_fail_reg(common_data, TCPC_REG_RX_BUFFER);
	zassert_equal(EC_ERROR_UNKNOWN,
		      drv->get_message_raw(port, payload, &head), NULL);
	i2c_common_emul_set_read_fail_reg(common_data,
					  I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test bist mode true */
	size = 28;
	msg.cnt = size + 2;
	msg.sop_type = TCPCI_MSG_SOP_PRIME;
	zassert_equal(TCPCI_EMUL_TX_SUCCESS,
		      tcpci_emul_add_rx_msg(emul, &msg, true),
		      "Failed to setup emulator message");

	zassert_equal(EC_SUCCESS, drv->set_bist_test_mode(port, false));
	zassert_equal(EC_SUCCESS, drv->get_message_raw(port, payload, &head),
		      NULL);
	zassert_equal(EC_SUCCESS, drv->get_bist_test_mode(port, &bist_enabled),
		      NULL);
	zassert_true(bist_enabled, "BIST mode should be enabled");
	

	/* Test bist mode already true */
	zassert_equal(TCPCI_EMUL_TX_SUCCESS,
		      tcpci_emul_add_rx_msg(emul, &msg, true),
		      "Failed to setup emulator message");

	zassert_equal(EC_SUCCESS, drv->set_bist_test_mode(port, true));
	zassert_equal(EC_SUCCESS, drv->get_message_raw(port, payload, &head),
		      NULL);
	zassert_equal(EC_SUCCESS, drv->get_bist_test_mode(port, &bist_enabled),
		      NULL);
	zassert_true(bist_enabled, "BIST mode should be disabled");
}

static void test_raa489000_tcpci_get_rx_message_raw(void)
{
	const struct emul *raa489000_emul = EMUL_DT_GET(RAA489000_EMUL_NODE);
	struct i2c_common_emul_data *common_data =
		emul_tcpci_generic_get_i2c_common_data(raa489000_emul);

	test_tcpci_get_rx_message_raw(raa489000_emul, common_data, RAA489000_PORT);
}

ZTEST(tcpc_raa489000, test_tcpci_get_rx_message_raw)
{
	test_raa489000_tcpci_get_rx_message_raw();
}

ZTEST_SUITE(tcpc_raa489000, drivers_predicate_post_main, NULL, NULL, NULL,
	    NULL);
