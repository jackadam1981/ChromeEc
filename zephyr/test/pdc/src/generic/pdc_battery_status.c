/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "drivers/pdc.h"
#include "emul/emul_pdc.h"
#include "fakes.h"
#include "hooks.h"
#include "test/util.h"
#include "usbc/pdc_power_mgmt.h"

#include <zephyr/ztest.h>

#define PDC_NODE_PORT0 DT_NODELABEL(pdc_emul1)
#define TEST_USBC_PORT0 USBC_PORT_FROM_PDC_DRIVER_NODE(PDC_NODE_PORT0)
#define SLEEP_MS 120

static const struct emul *emul = EMUL_DT_GET(PDC_NODE_PORT0);

static void *pdc_battery_status_setup(void)
{
    zassume(TEST_USBC_PORT0 < CONFIG_USB_PD_PORT_MAX_COUNT,
        "TEST_USBC_PORT0 is invalid");

    return NULL;
}

static void pdc_battery_status_before(void *data)
{
    emul_pdc_reset(emul);
    emul_pdc_set_response_delay(emul, 0);
    emul_pdc_disconnect(emul);
    zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_USBC_PORT0, -1));

    RESET_FAKE(battery_design_voltage);
    RESET_FAKE(battery_remaining_capacity);
    RESET_FAKE(battery_status);
    RESET_FAKE(battery_design_capacity);
    RESET_FAKE(battery_full_charge_capacity);
    set_battery_present(BP_YES);
}

ZTEST_SUITE(pdc_battery_status, NULL, pdc_battery_policy_setup,
        pdc_battery_status_before, NULL, NULL);

ZTEST_USER(pdc_battery_status, test_battery_status_update)
{
    union connector_status_t connector_status = {};
    union battery_status_t bstat;

    /* Setup fake battery functions */
    battery_remaining_capacity_fake.return_val = 0;
    battery_remaining_capacity_fake.arg0_val = 1000; /* 1000 mAh */
    battery_design_voltage_fake.return_val = 0;
    battery_design_voltage_fake.arg0_val = 7700; /* 7.7V */
    battery_status_fake.return_val = 0;
    battery_status_fake.arg0_val = STATUS_DISCHARGING;

    /* Connect a sink partner */
    emul_pdc_configure_snk(emul, &connector_status);
    emul_pdc_connect_partner(emul, &connector_status);
    zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_USBC_PORT0, -1));

    /* Trigger hook */
    hook_notify(HOOK_BATTERY_SOC_CHANGE);
    k_sleep(K_MSEC(SLEEP_MS));

    /* Verify emulator received correct battery status */
    zassert_ok(emul_pdc_get_battery_status(emul, &bstat));
    zassert_equal(bstat.battery_present, 1);
    zassert_equal(bstat.battery_state, BSDO_BATTERY_STATE_DISCHARGING);
    /* 1000mAh * 7.7V = 7700mWh = 7.7Wh. In 0.1Wh units, this is 77. */
    zassert_equal(bstat.present_capacity, 77);
}

ZTEST_USER(pdc_battery_status, test_battery_capability_update)
{
    union connector_status_t connector_status = {};
    union battery_capability_t bcap;

    /* Setup fake battery functions */
    battery_design_voltage_fake.return_val = 0;
    battery_design_voltage_fake.arg0_val = 7700; /* 7.7V */
    battery_design_capacity_fake.return_val = 0;
    battery_design_capacity_fake.arg0_val = 5000; /* 5000 mAh */
    battery_full_charge_capacity_fake.return_val = 0;
    battery_full_charge_capacity_fake.arg0_val = 4800; /* 4800 mAh */

    /* Connect a sink partner */
    emul_pdc_configure_snk(emul, &connector_status);
    emul_pdc_connect_partner(emul, &connector_status);
    zassert_ok(pdc_power_mgmt_wait_for_sync(TEST_USBC_PORT0, -1));

    /* Trigger hook */
    hook_notify(HOOK_BATTERY_SOC_CHANGE);
    k_sleep(K_MSEC(SLEEP_MS));

    /* Verify emulator received correct battery capability */
    zassert_ok(emul_pdc_get_battery_capability(emul, &bcap));
    zassert_equal(bcap.vid, CONFIG_PLATFORM_EC_USB_VID);
    zassert_equal(bcap.pid, CONFIG_PLATFORM_EC_USB_PID);
    /* Design: 5000mAh * 7.7V = 38500mWh = 38.5Wh. In 0.1Wh units, this is 385. */
    zassert_equal(bcap.design_capacity, 385);
    /* Full charge: 4800mAh * 7.7V = 36960mWh = 36.96Wh. In 0.1Wh units, this is 370. */
    zassert_equal(bcap.last_full_charge_capacity, 370);
}