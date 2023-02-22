/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "driver/charger/sm5803.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_sm5803.h"
#include "emul/tcpc/emul_tcpci_partner_src.h"
#include "test/drivers/charger_utils.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/ztest.h>

#define CHARGER_NUM get_charger_num(&sm5803_drv)
#define SM5803_EMUL EMUL_DT_GET(DT_NODELABEL(sm5803_emul))

ZTEST_SUITE(sm5803, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST(sm5803, test_fast_charge_current)
{
	int ma;

	/*
	 * Can set and read back charge current limit,
	 * which is adjusted when 0.
	 */
	zassert_ok(charger_set_current(CHARGER_NUM, 0));
	zassert_equal(
		1, sm5803_emul_read_chg_reg(SM5803_EMUL, SM5803_REG_FAST_CONF4),
		"Zero current limit should be converted to nonzero");
	zassert_ok(charger_get_current(CHARGER_NUM, &ma));
	zassert_equal(ma, 100,
		      "Actual current should be 100 mA times register value");

	/* Errors are propagated. */
	i2c_common_emul_set_read_fail_reg(sm5803_emul_get_i2c_chg(SM5803_EMUL),
					  SM5803_REG_FAST_CONF4);
	zassert_not_equal(
		0, charger_set_current(CHARGER_NUM, 1000),
		"set_current should fail if FAST_CONF4 is unreadable");
	zassert_not_equal(
		0, charger_get_current(CHARGER_NUM, &ma),
		"get_current should fail if FAST_CONF4 is unreadable");
}

ZTEST(sm5803, test_input_current_limit)
{
	int icl;

	/* Can set and read back the input current limit. */
	zassert_ok(charger_set_input_current_limit(CHARGER_NUM, 2150));
	zassert_equal(21, sm5803_emul_read_chg_reg(SM5803_EMUL,
						   SM5803_REG_CHG_ILIM));
	zassert_ok(charger_get_input_current_limit(CHARGER_NUM, &icl));
	zassert_equal(2100, icl,
		      "expected 2100 mA input current limit, but was %d", icl);

	/* Communication errors bubble up. */
	i2c_common_emul_set_read_fail_reg(sm5803_emul_get_i2c_chg(SM5803_EMUL),
					  SM5803_REG_CHG_ILIM);
	zassert_not_equal(0,
			  charger_get_input_current_limit(CHARGER_NUM, &icl));
	i2c_common_emul_set_write_fail_reg(sm5803_emul_get_i2c_chg(SM5803_EMUL),
					   SM5803_REG_CHG_ILIM);
	zassert_not_equal(0,
			  charger_set_input_current_limit(CHARGER_NUM, 1400));
}

/* Analog measurement of VBUS. */
ZTEST(sm5803, test_get_vbus_voltage)
{
	int mv;

	/* Regular measurement with VBUS ADC enabled works. */
	sm5803_emul_set_vbus_voltage(SM5803_EMUL, 5032);
	zassert_ok(charger_get_vbus_voltage(CHARGER_NUM, &mv));
	/* 5.031 is the nearest value representable by the VBUS ADC. */
	zassert_equal(mv, 5031, "driver reported %d mV VBUS", mv);

	/* Communication errors for ADC value bubble up. */
	i2c_common_emul_set_read_fail_reg(sm5803_emul_get_i2c_meas(SM5803_EMUL),
					  SM5803_REG_VBUS_MEAS_LSB);
	zassert_not_equal(0, charger_get_vbus_voltage(CHARGER_NUM, &mv));
	i2c_common_emul_set_read_fail_reg(sm5803_emul_get_i2c_meas(SM5803_EMUL),
					  SM5803_REG_VBUS_MEAS_MSB);
	zassert_not_equal(0, charger_get_vbus_voltage(CHARGER_NUM, &mv));

	/* Returns a NOT_POWERED error if the VBUS ADC is disabled. */
	sm5803_emul_set_gpadc_conf(SM5803_EMUL,
				   (uint8_t)~SM5803_GPADCC1_VBUS_EN, 0);
	zassert_equal(EC_ERROR_NOT_POWERED,
		      charger_get_vbus_voltage(CHARGER_NUM, &mv));
}

/* Digital VBUS presence detection derived from DHG_DET. */
ZTEST(sm5803, test_digital_vbus_presence_detect)
{
	/*
	 * CHG_DET going high (from VBUS presence) triggers an interrupt and
	 * presence update.
	 */
	sm5803_emul_set_vbus_voltage(SM5803_EMUL, 5000);
	k_sleep(K_SECONDS(1));	/* Allow interrupt to be serviced. */
	zassert_true(sm5803_is_vbus_present(CHARGER_NUM));

	/* VBUS going away triggers another interrupt and update. */
	sm5803_emul_set_vbus_voltage(SM5803_EMUL, 0);
	k_sleep(K_SECONDS(1));	/* Allow interrupt to be serviced. */
	zassert_false(sm5803_is_vbus_present(CHARGER_NUM));
}

/* VBUS detection for PD, analog or digital depending on chip state. */
ZTEST(sm5803, test_check_vbus_level)
{
	/* Default state with VBUS ADC enabled: uses analog value */
	zassert_true(sm5803_check_vbus_level(CHARGER_NUM, VBUS_REMOVED));
	sm5803_emul_set_vbus_voltage(SM5803_EMUL, 5000);
	zassert_true(sm5803_check_vbus_level(CHARGER_NUM, VBUS_PRESENT));

	/* 4.6V is less than vSafe5V */
	sm5803_emul_set_vbus_voltage(SM5803_EMUL, 4600);
	k_sleep(K_SECONDS(1));
	zassert_false(sm5803_check_vbus_level(CHARGER_NUM, VBUS_PRESENT));

	/*
	 * With ADC disabled, uses digital presence only. 4.6V is high enough
	 * to trip CHG_DET but wasn't enough to count as present with the analog
	 * reading.
	 */
	sm5803_emul_set_gpadc_conf(SM5803_EMUL, 0, 0);
	zassert_true(sm5803_check_vbus_level(CHARGER_NUM, VBUS_PRESENT));

	/* 0.4V is !CHG_DET */
	sm5803_emul_set_vbus_voltage(SM5803_EMUL, 400);
	k_sleep(K_SECONDS(1));
	zassert_true(sm5803_check_vbus_level(CHARGER_NUM, VBUS_REMOVED));
}

ZTEST(sm5803, test_lpm)
{
	const struct emul *tcpci_emul = EMUL_GET_USBC_BINDING(0, tcpc);
	struct tcpci_partner_data partner;
	struct tcpci_src_emul_data partner_src;

	tcpci_partner_init(&partner, PD_REV30);
	partner.extensions = tcpci_src_emul_init(&partner_src, &partner, NULL);

	zassert_ok(tcpci_partner_connect_to_tcpci(&partner, tcpci_emul));
	sm5803_emul_set_vbus_voltage(SM5803_EMUL, 5000);
	k_sleep(K_SECONDS(10));

	// LPM is disabled on PD connect, and enabled on PD disconnect.
	// GPADCs are not disabled if the port is still connected when
	// the hook executes.
}
