/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/rt1718s.h"
#include "driver/tcpm/rt1718s_public.h"
#include "driver/tcpm/tcpci.h"
#include "emul/tcpc/emul_rt1718s.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/slist.h>
#include <zephyr/ztest.h>
LOG_MODULE_REGISTER(rt1718s_tcpc_test, CONFIG_TCPCI_EMUL_LOG_LEVEL);

#define RT1718S_NODE DT_NODELABEL(rt1718s_emul)

static const int tcpm_rt1718s_port = USBC_PORT_C0;
static const struct emul *rt1718s_emul = EMUL_DT_GET(RT1718S_NODE);

static void rt1718s_clear_set_reg_history(void *f)
{
	rt1718s_emul_reset_set_history(rt1718s_emul);
}

ZTEST_SUITE(rt1718s_tcpc, drivers_predicate_post_main, NULL,
	    rt1718s_clear_set_reg_history, rt1718s_clear_set_reg_history, NULL);

static uint16_t get_emul_reg(const struct emul *emul, int reg)
{
	uint16_t val;

	zassert_ok(rt1718s_emul_get_reg(emul, reg, &val),
		   "Cannot get reg %x on rt1718s emul", reg);
	return val;
}

static void compare_reg_val_with_mask(const struct emul *emul, int reg,
				      uint8_t expected, uint8_t mask)
{
	uint16_t masked_val = get_emul_reg(emul, reg) & mask;
	uint16_t masked_expected = expected & mask;

	zassert_equal(masked_val, masked_expected,
		      "expected register %x with mask %x should be %x, get %x",
		      reg, mask, masked_expected, masked_val);
}

ZTEST(rt1718s_tcpc, test_init_with_sw_reset)
{
	zassert_ok(rt1718s_tcpm_drv.init(tcpm_rt1718s_port),
		   "Cannot initialize rt1718s");
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_SYS_CTRL3, 0xFF,
				  RT1718S_SWRESET_MASK);
}

ZTEST(rt1718s_tcpc, test_set_vconn_enable)
{
	struct _snode *iter_node = NULL;
	struct set_reg_entry_t *iter_entry = NULL;
	struct set_reg_entry_t *set_vconn_limit_on_entry = NULL;
	struct set_reg_entry_t *set_vconn_limit_off_entry = NULL;
	struct rt1718s_emul_data *rt1718s_data = rt1718s_emul->data;
	struct _slist *set_private_reg_history =
		&rt1718s_data->set_private_reg_history;

	zassert_ok(rt1718s_tcpm_drv.set_vconn(tcpm_rt1718s_port, true));

	iter_node = sys_slist_peek_head(set_private_reg_history);
	while (iter_node != NULL) {
		iter_entry = SYS_SLIST_CONTAINER(iter_node, iter_entry, node);
		if (iter_entry->reg == RT1718S_VCON_CTRL3 &&
		    (iter_entry->val & RT1718S_VCON_LIMIT_MODE) == 1) {
			set_vconn_limit_on_entry = iter_entry;
			break;
		}
		iter_node = iter_node->next;
	}
	/* b/233698718#comment9 workaround should applied */
	zassert_not_null(set_vconn_limit_on_entry,
			 "No entry for setting RT1718S_VCON_CTRL3");
	while (iter_node != NULL) {
		iter_entry = SYS_SLIST_CONTAINER(iter_node, iter_entry, node);
		if (iter_entry->reg == RT1718S_VCON_CTRL3 &&
		    (iter_entry->val & RT1718S_VCON_LIMIT_MODE) == 0) {
			set_vconn_limit_off_entry = iter_entry;
			break;
		}
		iter_node = iter_node->next;
	}
	zassert_not_null(set_vconn_limit_off_entry,
			 "No entry for setting RT1718S_VCON_CTRL3");
	zassert_true(
		(set_vconn_limit_off_entry->access_time -
		 set_vconn_limit_on_entry->access_time) >= 10,
		"Workaround for two setting Vconn limit is smaller than 10ms");

	/* rt1718s should be in shutdown mode. */
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_VCON_CTRL3, 0x0,
				  RT1718S_VCON_LIMIT_MODE);
	/* Vconn RVP should be enabled. */
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_VCONN_CONTROL_2, 0xFF,
				  RT1718S_VCONN_CONTROL_2_RVP_EN);
}

ZTEST(rt1718s_tcpc, test_set_vconn_disable)
{
	zassert_ok(rt1718s_tcpm_drv.set_vconn(tcpm_rt1718s_port, false));
	/* Vconn RVP should be disabled. */
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_VCONN_CONTROL_2, 0,
				  RT1718S_VCONN_CONTROL_2_RVP_EN);
}

ZTEST(rt1718s_tcpc, test_enter_low_power_mode)
{
	zassert_ok(rt1718s_tcpm_drv.enter_low_power_mode(tcpm_rt1718s_port));
	compare_reg_val_with_mask(
		rt1718s_emul, RT1718S_SYS_CTRL2, RT1718S_SYS_CTRL2_LPWR_EN,
		RT1718S_SYS_CTRL2_LPWR_EN | RT1718S_SYS_CTRL2_BMCIO_OSC_EN);
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_RT2_SBU_CTRL_01, 0,
				  0xFF);
}

ZTEST(rt1718s_tcpc, test_set_sbu)
{
	uint8_t mask = RT1718S_RT2_SBU_CTRL_01_SBU_VIEN |
		       RT1718S_RT2_SBU_CTRL_01_SBU1_SWEN |
		       RT1718S_RT2_SBU_CTRL_01_SBU2_SWEN;

	zassert_ok(rt1718s_tcpm_drv.set_sbu(tcpm_rt1718s_port, true));
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_RT2_SBU_CTRL_01, 0xFF,
				  mask);

	zassert_ok(rt1718s_tcpm_drv.set_sbu(tcpm_rt1718s_port, false));
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_RT2_SBU_CTRL_01, 0,
				  mask);
}
