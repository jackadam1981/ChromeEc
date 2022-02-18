/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include <drivers/gpio/gpio_emul.h>

#include "ec_commands.h"
#include "ec_tasks.h"
#include "driver/tcpm/ps8xxx_public.h"
#include "emul/emul_isl923x.h"
#include "emul/tcpc/emul_ps8xxx.h"
#include "emul/tcpc/emul_tcpci.h"
#include "emul/tcpc/emul_tcpci_partner_drp.h"
#include "emul/tcpc/emul_tcpci_partner_snk.h"
#include "emul/tcpc/emul_tcpci_partner_src.h"
#include "host_command.h"
#include "stubs.h"
#include "tcpm/tcpci.h"
#include "test/usb_pe.h"
#include "test_state.h"
#include "usb_integration.h"
#include "utils.h"

#define DEFAULT_VBUS_MV 5000
#define DEFAULT_VBUS_MA 3000

struct integration_usb_test_fixture {
	struct integration_usb_fixture fixture;
};

static void integration_usb_test_before(void *state)
{
	struct integration_usb_fixture *fixture = state;

	integration_usb_reset(fixture);

	integration_usb_sink_attach(fixture);

	integration_usb_source_attach(fixture);
}

static void integration_usb_test_after(void *state)
{
	struct integration_usb_fixture *fixture = state;

	integration_usb_source_detach(fixture);
	integration_usb_sink_detach(fixture);

	/* Give time to actually disconnect */
	k_sleep(K_SECONDS(1));

	/* Reset vbus to 0mV */
	/* TODO(b/217737667): Remove driver specific code. */
	isl923x_emul_set_adc_vbus(fixture->charger, 0);
}

ZTEST_F(integration_usb_test, verify_detach_src_snk)
{
	struct integration_usb_fixture *fixture = &this->fixture;

	integration_usb_source_detach(fixture);
	integration_usb_sink_detach(fixture);

	k_sleep(K_SECONDS(1));
	isl923x_emul_set_adc_vbus(fixture->charger, 0);
}

ZTEST_F(integration_usb_test, verify_detach_snk_src)
{
	struct integration_usb_fixture *fixture = &this->fixture;

	integration_usb_sink_detach(fixture);
	integration_usb_source_detach(fixture);

	k_sleep(K_SECONDS(1));
	isl923x_emul_set_adc_vbus(fixture->charger, 0);
}

ZTEST_F(integration_usb_test, verify_detach_sink)
{
	struct integration_usb_fixture *fixture = &this->fixture;
	struct ec_response_usb_pd_power_info pd_power_info;

	integration_usb_sink_detach(fixture);
	k_sleep(K_SECONDS(1));
	isl923x_emul_set_adc_vbus(fixture->charger, 0);

	integration_usb_get_pd_power_info(USB_SINK_PORT, &pd_power_info);

	/* Assert */
	zassert_equal(pd_power_info.role, USB_PD_PORT_POWER_SINK,
		      "Power role %d, but PD reports role %d",
		      USB_PD_PORT_POWER_SINK, pd_power_info.role);
	zassert_equal(pd_power_info.type, USB_CHG_TYPE_PD,
		      "Charger type %d, but PD reports type %d",
		      USB_CHG_TYPE_PD, pd_power_info.type);

	zassert_equal(pd_power_info.meas.voltage_max, DEFAULT_VBUS_MV,
		      "Charging at VBUS %dmV, but PD reports %dmV",
		      DEFAULT_VBUS_MV, pd_power_info.meas.voltage_max);

	zassert_within(pd_power_info.meas.voltage_now, 0, 10,
		       "Actually charging at VBUS %dmV, but PD reports %dmV", 0,
		       pd_power_info.meas.voltage_now);

	zassert_equal(pd_power_info.meas.current_max, DEFAULT_VBUS_MA,
		      "Charging at VBUS max %dmA, but PD reports %dmA",
		      DEFAULT_VBUS_MA, pd_power_info.meas.current_max);

	zassert_true(pd_power_info.meas.current_lim >= DEFAULT_VBUS_MA,
		     "Charging at VBUS max %dmA, but PD current limit %dmA",
		     DEFAULT_VBUS_MA, pd_power_info.meas.current_lim);

	zassert_equal(pd_power_info.max_power,
		      DEFAULT_VBUS_MV * DEFAULT_VBUS_MA,
		      "Charging up to %duW, PD max power %duW",
		      DEFAULT_VBUS_MV * DEFAULT_VBUS_MA,
		      pd_power_info.max_power);
}

ZTEST_F(integration_usb_test, verify_detach_source)
{
	struct integration_usb_fixture *fixture = &this->fixture;

	integration_usb_source_detach(fixture);
	k_sleep(K_SECONDS(1));
	isl923x_emul_set_adc_vbus(fixture->charger, 0);
}

ZTEST_SUITE(integration_usb_test, drivers_predicate_post_main,
	    integration_usb_setup, integration_usb_test_before,
	    integration_usb_test_after, NULL);
