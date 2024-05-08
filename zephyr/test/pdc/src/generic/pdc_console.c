/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "drivers/ucsi_v3.h"
#include "emul/emul_pdc.h"
#include "mock_pdc_power_mgmt.h"

#include <zephyr/shell/shell.h>
#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>

#define TEST_PORT 0
#define SLEEP_MS 120
#define RTS5453P_NODE DT_NODELABEL(rts5453p_emul)

static const struct emul *emul = EMUL_DT_GET(RTS5453P_NODE);

static void console_cmd_pdc_setup(void)
{
	struct pdc_info_t info = {
		.fw_version = 0x001a2b3c,
		.pd_version = 0xabcd,
		.pd_revision = 0x1234,
		.vid_pid = 0x12345678,
	};

	/* Set a FW version in the emulator for `test_info` */
	emul_pdc_set_info(emul, &info);

	zassume(TEST_PORT < CONFIG_USB_PD_PORT_MAX_COUNT,
		"TEST_PORT is invalid");
}

static void console_cmd_pdc_reset(void *fixture)
{
	shell_backend_dummy_clear_output(get_ec_shell());

	helper_reset_pdc_power_mgmt_fakes();
}

ZTEST_SUITE(console_cmd_pdc, NULL, console_cmd_pdc_setup, console_cmd_pdc_reset,
	    console_cmd_pdc_reset, NULL);

ZTEST_USER(console_cmd_pdc, test_no_args)
{
	int rv;

	rv = shell_execute_cmd(get_ec_shell(), "pdc");
	zassert_equal(rv, SHELL_CMD_HELP_PRINTED, "Expected %d, but got %d",
		      SHELL_CMD_HELP_PRINTED, rv);
}

/**
 * @brief Custom fake for pdc_power_mgmt_get_cable_prop that outputs some test
 *        cable property info.
 */
static int
custom_fake_pdc_power_mgmt_get_cable_prop(int port, union cable_property_t *out)
{
	zassert_not_null(out);

	*out = (union cable_property_t){
		.bm_speed_supported = 0xabcd,
		/* 50mA units. This should represent 500mA */
		.b_current_capability = 10,
		.vbus_in_cable = 1,
		.cable_type = 1,
		.directionality = 1,
		.plug_end_type = USB_TYPE_C,
		.mode_support = 1,
		.cable_pd_revision = 3,
		.latency = 0xf,
	};

	return 0;
}

ZTEST_USER(console_cmd_pdc, test_cable_prop)
{
	int rv;
	const char *outbuffer;
	size_t buffer_size;

	/* Invalid port number */
	rv = shell_execute_cmd(get_ec_shell(), "pdc cable_prop 99");
	zassert_equal(rv, -EINVAL, "Expected %d, but got %d", -EINVAL, rv);

	/* Internal pdc_power_mgmt_get_cable_prop() failure */
	pdc_power_mgmt_get_cable_prop_fake.return_val = 1;

	rv = shell_execute_cmd(get_ec_shell(), "pdc cable_prop 0");
	zassert_equal(rv, pdc_power_mgmt_get_cable_prop_fake.return_val,
		      "Expected %d, but got %d",
		      pdc_power_mgmt_get_cable_prop_fake.return_val, rv);

	RESET_FAKE(pdc_power_mgmt_get_cable_prop);

	/* Happy case */
	pdc_power_mgmt_get_cable_prop_fake.custom_fake =
		custom_fake_pdc_power_mgmt_get_cable_prop;

	rv = shell_execute_cmd(get_ec_shell(), "pdc cable_prop 0");
	zassert_equal(rv, EC_SUCCESS, "Expected %d, but got %d", EC_SUCCESS,
		      rv);

	/* Sample command output:
	 *
	 * ec:> pdc cable_prop 0
	 * Port 0 GET_CABLE_PROP:
	 *    bm_speed_supported               : 0x0000
	 *    b_current_capability             : 0 mA
	 *    vbus_in_cable                    : 0
	 *    cable_type                       : 0
	 *    directionality                   : 0
	 *    plug_end_type                    : 0
	 *    mode_support                     : 0
	 *    cable_pd_revision                : 0
	 *    latency                          : 0
	 */

	outbuffer =
		shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_true(buffer_size > 0, NULL);

	zassert_not_null(strstr(outbuffer, "Port 0 GET_CABLE_PROP:"));
	zassert_not_null(
		strstr(outbuffer, "bm_speed_supported               : 0xabcd"));
	zassert_not_null(
		strstr(outbuffer, "b_current_capability             : 500 mA"));
	zassert_not_null(
		strstr(outbuffer, "vbus_in_cable                    : 1"));
	zassert_not_null(
		strstr(outbuffer, "cable_type                       : 1"));
	zassert_not_null(
		strstr(outbuffer, "directionality                   : 1"));
	zassert_not_null(
		strstr(outbuffer, "plug_end_type                    : 2"));
	zassert_not_null(
		strstr(outbuffer, "mode_support                     : 1"));
	zassert_not_null(
		strstr(outbuffer, "cable_pd_revision                : 3"));
	zassert_not_null(
		strstr(outbuffer, "latency                          : 15"));
}

ZTEST_USER(console_cmd_pdc, test_trysrc)
{
	int rv;

	rv = shell_execute_cmd(get_ec_shell(), "pdc trysrc enable");
	zassert_equal(rv, -EINVAL, "Expected %d, but got %d", -EINVAL, rv);

	rv = shell_execute_cmd(get_ec_shell(), "pdc trysrc 0");
	zassert_equal(rv, EC_SUCCESS, "Expected %d, but got %d", EC_SUCCESS,
		      rv);
	k_sleep(K_MSEC(SLEEP_MS));

	rv = shell_execute_cmd(get_ec_shell(), "pdc trysrc 1");
	zassert_equal(rv, EC_SUCCESS, "Expected %d, but got %d", EC_SUCCESS,
		      rv);
	k_sleep(K_MSEC(SLEEP_MS));

	rv = shell_execute_cmd(get_ec_shell(), "pdc trysrc 2");
	zassert_equal(rv, -EINVAL, "Expected %d, but got %d", -EINVAL, rv);
}

ZTEST_USER(console_cmd_pdc, test_info)
{
	int rv;

	/* Bad chip number */
	rv = shell_execute_cmd(get_ec_shell(), "pdc info x");
	zassert_equal(rv, -EINVAL, "Expected %d, but got %d", -EINVAL, rv);

	/* Bad live param (should be int) */
	rv = shell_execute_cmd(get_ec_shell(), "pdc info 0 y");
	zassert_equal(rv, -EINVAL, "Expected %d, but got %d", -EINVAL, rv);

	/* Get chip #0 info live */
	rv = shell_execute_cmd(get_ec_shell(), "pdc info 0");
	zassert_equal(rv, EC_SUCCESS, "Expected %d, but got %d", EC_SUCCESS,
		      rv);

	/* Get chip #0 info cached */
	rv = shell_execute_cmd(get_ec_shell(), "pdc info 0 0");
	zassert_equal(rv, EC_SUCCESS, "Expected %d, but got %d", EC_SUCCESS,
		      rv);
}

/** Test PDOs for `test_srccaps` */
static const uint32_t source_caps[] = {
	PDO_FIXED(5000 /*mV*/, 3000 /*mA*/, PDO_FIXED_DUAL_ROLE),
	PDO_FIXED(5000 /*mV*/, 3000 /*mA*/, PDO_FIXED_UNCONSTRAINED),
	PDO_FIXED(5000 /*mV*/, 3000 /*mA*/, PDO_FIXED_COMM_CAP),
	PDO_FIXED(5000 /*mV*/, 3000 /*mA*/, PDO_FIXED_DATA_SWAP),
	PDO_FIXED(5000 /*mV*/, 3000 /*mA*/, PDO_FIXED_FRS_CURR_MASK),
	PDO_VAR(5000 /*mV*/, 20000 /*mV*/, 1500 /*mA*/),
	PDO_BATT(5000 /*mV*/, 20000 /*mV*/, 50000 /*mW*/),
	PDO_AUG(9000 /*mV*/, 15000 /*mV*/, 2000 /*mA*/),
};

/**
 * @brief Custom fake for pdc_power_mgmt_get_src_caps(). Because the return
 *        value of this function is a const ptr to a const, we cannot override
 *        the `.return_val` member in the fake's FFF struct. Instead, use a
 *        custom fake to return a source cap list.
 */
static const uint32_t *const custom_fake_pdc_power_mgmt_get_src_caps(int port)
{
	return (const uint32_t *const)&source_caps;
}

ZTEST_USER(console_cmd_pdc, test_srccaps)
{
	int rv;
	const char *outbuffer;
	size_t buffer_size;

	/* Invalid port number */
	rv = shell_execute_cmd(get_ec_shell(), "pdc srccaps 99");
	zassert_equal(rv, -EINVAL, "Expected %d, but got %d", -EINVAL, rv);

	/* No source caps present */
	pdc_power_mgmt_get_src_cap_cnt_fake.return_val = 0;

	rv = shell_execute_cmd(get_ec_shell(), "pdc srccaps 0");
	zassert_equal(rv, EC_SUCCESS, "Expected %d, but got %d", EC_SUCCESS,
		      rv);

	zassert_equal(1, pdc_power_mgmt_get_src_caps_fake.call_count);
	zassert_equal(1, pdc_power_mgmt_get_src_cap_cnt_fake.call_count);

	outbuffer =
		shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_true(buffer_size > 0, NULL);
	zassert_not_null(strstr(outbuffer, "No source caps for port"));

	RESET_FAKE(pdc_power_mgmt_get_src_caps);
	RESET_FAKE(pdc_power_mgmt_get_src_cap_cnt);

	/* Successful path w/ source caps */
	pdc_power_mgmt_get_src_caps_fake.custom_fake =
		&custom_fake_pdc_power_mgmt_get_src_caps;
	pdc_power_mgmt_get_src_cap_cnt_fake.return_val =
		ARRAY_SIZE(source_caps);

	rv = shell_execute_cmd(get_ec_shell(), "pdc srccaps 0");
	zassert_equal(rv, EC_SUCCESS, "Expected %d, but got %d", EC_SUCCESS,
		      rv);

	zassert_equal(1, pdc_power_mgmt_get_src_caps_fake.call_count);
	zassert_equal(1, pdc_power_mgmt_get_src_cap_cnt_fake.call_count);

	outbuffer =
		shell_backend_dummy_get_output(get_ec_shell(), &buffer_size);

	zassert_true(buffer_size > 0, NULL);

	/*
	 * Sample output:
	 *
	 * Src 00: 2001912c FIX          5000mV,  3000mA [DRP               ]
	 * Src 01: 0801912c FIX          5000mV,  3000mA [    UP            ]
	 * Src 02: 0401912c FIX          5000mV,  3000mA [       USB        ]
	 * Src 03: 0201912c FIX          5000mV,  3000mA [           DRD    ]
	 * Src 04: 0181912c FIX          5000mV,  3000mA [               FRS]
	 * Src 05: 99019096 VAR  5000mV-20000mV,  1500mA
	 * Src 06: 590190c8 BAT  5000mV-20000mV,  3000mW
	 * Src 07: c12c5a28 AUG  9000mV-15000mV,  2000mA
	 */

	zassert_not_null(strstr(outbuffer,
				"Src 00: 2001912c FIX          5000mV,  3000mA "
				"[DRP               ]"));
	zassert_not_null(strstr(outbuffer,
				"Src 01: 0801912c FIX          5000mV,  3000mA "
				"[    UP            ]"));
	zassert_not_null(strstr(outbuffer,
				"Src 02: 0401912c FIX          5000mV,  3000mA "
				"[       USB        ]"));
	zassert_not_null(strstr(outbuffer,
				"Src 03: 0201912c FIX          5000mV,  3000mA "
				"[           DRD    ]"));
	zassert_not_null(strstr(outbuffer,
				"Src 04: 0181912c FIX          5000mV,  3000mA "
				"[               FRS]"));
	zassert_not_null(strstr(
		outbuffer, "Src 05: 99019096 VAR  5000mV-20000mV,  1500mA"));
	zassert_not_null(strstr(
		outbuffer, "Src 06: 590190c8 BAT  5000mV-20000mV,  3000mW"));
	zassert_not_null(strstr(
		outbuffer, "Src 07: c12c5a28 AUG  9000mV-15000mV,  2000mA"));
}
