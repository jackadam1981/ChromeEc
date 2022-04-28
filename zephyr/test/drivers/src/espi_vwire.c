/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>

#include "ec_commands.h"
#include "host_command.h"
#include "test/drivers/test_state.h"
#include "lpc.h"

#include"espi.h"
#include"zephyr_espi_shim.h"
#include"drivers/espi_emul.h"
#include"power_signals.h"

#define PORT 0
#define ESPI_DEVICE DEVICE_DT_GET(DT_CHOSEN(cros_ec_espi))

static int vwire_received_cnt = 0;
static struct espi_event vwire_received_evt;

static int sci_received_cnt = 0;
static int sci_received_vals[8] = {0};
static int smi_received_cnt = 0;
static int smi_received_vals[8] = {0};

static void espi_vwire_cb(const struct device *dev,
			  struct espi_callback *cb,
			  struct espi_event espi_evt)
{
	vwire_received_cnt++;
	vwire_received_evt = espi_evt;

	printk("espi_vwire_cb from host: %d\n", vwire_received_cnt);
}

static void espi_smi_sci_cb(const struct device *dev,
			  struct espi_callback *cb,
			  struct espi_event espi_evt)
{
	if(espi_evt.evt_type == ESPI_BUS_EVENT_VWIRE_RECEIVED) {
		if(espi_evt.evt_details == ESPI_VWIRE_SIGNAL_SCI) {
			sci_received_vals[sci_received_cnt++] = espi_evt.evt_data;

			if(sci_received_cnt > ARRAY_SIZE(sci_received_vals)) {
				sci_received_cnt = 0;
			}
		} else if(espi_evt.evt_details == ESPI_VWIRE_SIGNAL_SMI) {
			smi_received_vals[smi_received_cnt++] = espi_evt.evt_data;

			if(smi_received_cnt > ARRAY_SIZE(smi_received_vals)) {
				smi_received_cnt = 0;
			}
		}
	}
}

/**
 * Utils
 */

ZTEST_USER(espi_vwire, test_convert_all_valid_signals_to_zephyr_vwires)
{
	enum espi_vw_signal id = VW_SIGNAL_START;

	for (; id < VW_SIGNAL_END; id++) {
		espi_vw_set_wire(id, 1);
	}
}

ZTEST_USER(espi_vwire, test_convert_invalid_vwire_signal)
{
	struct espi_event evt;
	evt.evt_type = ESPI_BUS_EVENT_VWIRE_RECEIVED;
	evt.evt_details = 0xBAADF00D;
	evt.evt_data = 5;

	espi_emul_raise_event(ESPI_DEVICE, evt);
	espi_vw_set_wire(0xBAADF00D, 1);
}

ZTEST_USER(espi_vwire, test_convert_all_valid_zephyr_vwires_to_signals)
{
	enum espi_vwire_signal id = ESPI_VWIRE_SIGNAL_SLP_S3;

	for (; id <= ESPI_VWIRE_SIGNAL_SUS_ACK; id++) {
		struct espi_event evt;
		evt.evt_type = ESPI_BUS_EVENT_VWIRE_RECEIVED;
		evt.evt_details = id;
		evt.evt_data = 0;

		espi_emul_raise_event(ESPI_DEVICE, evt);
	}
}

/**
 * Virtual wires
 */

ZTEST_USER(espi_vwire, set_vwire)
{
	uint8_t level;
	printk("set_vwire dev: %p\n", ESPI_DEVICE);

	vwire_received_cnt = 0;

	static struct espi_callback espi_cb_obj;
	espi_init_callback(&espi_cb_obj, espi_vwire_cb, ESPI_BUS_EVENT_VWIRE_RECEIVED);
	espi_emul_manage_host_callback(ESPI_DEVICE, &espi_cb_obj, 1);

	espi_vw_set_wire(VW_WAKE_L, 0);
	zassert_equal(vwire_received_cnt, 1, "count should be 1");
	zassert_equal(vwire_received_evt.evt_details, ESPI_VWIRE_SIGNAL_WAKE, "invalid signal");
	zassert_equal(vwire_received_evt.evt_data, 0, "invalid level");

	espi_vw_set_wire(VW_WAKE_L, 1);
	zassert_equal(vwire_received_cnt, 2, "count should be 2");
	zassert_equal(vwire_received_evt.evt_details, ESPI_VWIRE_SIGNAL_WAKE, "invalid signal");
	zassert_equal(vwire_received_evt.evt_data, 1, "invalid level");

	espi_vw_set_wire(VW_WAKE_L, 0);
	zassert_equal(vwire_received_cnt, 3, "count should be 3");
	zassert_equal(vwire_received_evt.evt_details, ESPI_VWIRE_SIGNAL_WAKE, "invalid signal");
	zassert_equal(vwire_received_evt.evt_data, 0, "invalid level");

	printk("Vwire: %d\n", vwire_received_cnt);
	zassert_equal(vwire_received_cnt, 3, "Invalid number of vwire received");

	espi_emul_manage_host_callback(ESPI_DEVICE, &espi_cb_obj, 0);
}

ZTEST_USER(espi_vwire, get_vwire)
{
	int value;

	espi_emul_internal_set_vwire(ESPI_DEVICE, ESPI_VWIRE_SIGNAL_SLP_S5, 1);
	value = espi_vw_get_wire(VW_SLP_S5_L);
	zassert_equal(value, 1, "vwire value should be correct");

	espi_emul_internal_set_vwire(ESPI_DEVICE, ESPI_VWIRE_SIGNAL_SLP_S5, 0);
	value = espi_vw_get_wire(VW_SLP_S5_L);
	zassert_equal(value, 0, "vwire value should be correct");
}

// ZTEST_USER(espi_vwire, get_vwire_inv_signal)
// {
// 	int value;

// 	value = espi_vw_get_wire(VW_WAKE_L);
// 	zassert_equal(value, 0, "Should return 0 as error");

// 	espi_send_vwire(ESPI_DEVICE, ESPI_VWIRE_SIGNAL_WAKE, 1);

// 	value = espi_vw_get_wire(VW_WAKE_L);
// 	zassert_equal(value, 0, "Should return 0 as error");
// }

// ZTEST_USER(espi_vwire, get_vwire_interrupt)
// {
// 	espi_vw_enable_wire_int(VW_PLTRST_L);

// 	espi_emul_internal_set_vwire(ESPI_DEVICE, ESPI_VWIRE_SIGNAL_PLTRST, 0);
// 	int power_signals = power_get_signals();

// 	// Signals should change
// 	espi_emul_internal_set_vwire(ESPI_DEVICE, ESPI_VWIRE_SIGNAL_PLTRST, 1);
// 	zassert_not_equal(power_get_signals(), power_signals, "Signals should change");
// 	espi_emul_internal_set_vwire(ESPI_DEVICE, ESPI_VWIRE_SIGNAL_PLTRST, 0);
// 	zassert_equal(power_get_signals(), power_signals, "Signals should change");

// 	espi_vw_disable_wire_int(VW_PLTRST_L);

// 	// Signals should NOT change
// 	espi_emul_internal_set_vwire(ESPI_DEVICE, ESPI_VWIRE_SIGNAL_PLTRST, 1);
// 	zassert_equal(power_get_signals(), power_signals, "Signals should NOT change");
// 	espi_emul_internal_set_vwire(ESPI_DEVICE, ESPI_VWIRE_SIGNAL_PLTRST, 0);
// 	zassert_equal(power_get_signals(), power_signals, "Signals should NOT change");
// }

/**
 * ACPI
 */

ZTEST_USER(espi_vwire, acpi_set_mask)
{
	uint32_t status = 0x00;

	espi_write_lpc_request(ESPI_DEVICE, EACPI_WRITE_STS, &status);

	// Set some bits
	lpc_set_acpi_status_mask(0xAA);
	espi_read_lpc_request(ESPI_DEVICE, EACPI_READ_STS, &status);
	zassert_equal(status, 0xAA, "should set bits");

	// Set rest of the bits
	lpc_set_acpi_status_mask(0x55);
	espi_read_lpc_request(ESPI_DEVICE, EACPI_READ_STS, &status);
	zassert_equal(status, 0xFF, "should set bits");

	// Clear some bits
	lpc_clear_acpi_status_mask(0xAA);
	espi_read_lpc_request(ESPI_DEVICE, EACPI_READ_STS, &status);
	zassert_equal(status, 0x55, "should clear bits");

	// Clear rest of the bits
	lpc_clear_acpi_status_mask(0x55);
	espi_read_lpc_request(ESPI_DEVICE, EACPI_READ_STS, &status);
	zassert_equal(status, 0x00, "should clear bits");
}

ZTEST_USER(espi_vwire, acpi_is_acpi_cmd)
{
	uint32_t value;
	struct espi_evt_data_acpi *acpi = (struct espi_evt_data_acpi *)&value;

	acpi->type = 0;
	zassert_false(is_acpi_command(value), "is not a command");

	acpi->type = 1;
	zassert_true(is_acpi_command(value), "is command");

	acpi->type = 0x91;
	zassert_true(is_acpi_command(value), "is command");
}

ZTEST_USER(espi_vwire, acpi_get_data)
{
	uint32_t value = 0xBAADF00D;
	struct espi_evt_data_acpi *acpi = (struct espi_evt_data_acpi *)&value;

	acpi->data = 0;
	zassert_equal(get_acpi_value(value), 0, "value should be correct");

	acpi->data = 31;
	zassert_equal(get_acpi_value(value), 31, "value should be correct");

	acpi->data = 89;
	zassert_equal(get_acpi_value(value), 89, "value should be correct");

	acpi->type = 123;
	zassert_equal(get_acpi_value(value), 89, "value should be correct");

	acpi->reserved = 0xBAD0;
	zassert_equal(get_acpi_value(value), 89, "value should be correct");
}

// lpc_update_host_event_status
ZTEST_USER(espi_vwire, acpi_need_sci_smi)
{
	uint32_t status = 0x00;
	sci_received_cnt = 0;
	smi_received_cnt = 0;

	static struct espi_callback espi_cb_obj;
	espi_init_callback(&espi_cb_obj, espi_smi_sci_cb, ESPI_BUS_EVENT_VWIRE_RECEIVED);
	espi_emul_manage_host_callback(ESPI_DEVICE, &espi_cb_obj, 1);

	lpc_set_host_event_mask(LPC_HOST_EVENT_SCI, 0);
	lpc_set_host_event_mask(LPC_HOST_EVENT_SMI, 0);
	host_set_events(LPC_HOST_EVENT_SCI | LPC_HOST_EVENT_SMI);

	// No sci & smi
	sci_received_cnt = 0;
	smi_received_cnt = 0;
	status = 0;
	espi_write_lpc_request(ESPI_DEVICE, EACPI_WRITE_STS, &status);
	lpc_update_host_event_status();

	zassert_equal(sci_received_cnt, 0, NULL);
	zassert_equal(smi_received_cnt, 0, NULL);

	// Need SMI
	lpc_set_host_event_mask(LPC_HOST_EVENT_SCI, 0);
	lpc_set_host_event_mask(LPC_HOST_EVENT_SMI, 1);
	host_set_events(LPC_HOST_EVENT_SCI | LPC_HOST_EVENT_SMI);
	sci_received_cnt = 0;
	smi_received_cnt = 0;
	status = 0;
	espi_write_lpc_request(ESPI_DEVICE, EACPI_WRITE_STS, &status);
	lpc_update_host_event_status();

	zassert_equal(sci_received_cnt, 0, NULL);
	zassert_equal(smi_received_cnt, 3, NULL);

	// // Need SCI
	lpc_set_host_event_mask(LPC_HOST_EVENT_SCI, 1);
	lpc_set_host_event_mask(LPC_HOST_EVENT_SMI, 0);
	host_set_events(LPC_HOST_EVENT_SCI | LPC_HOST_EVENT_SMI);
	sci_received_cnt = 0;
	smi_received_cnt = 0;
	status = 0;
	espi_write_lpc_request(ESPI_DEVICE, EACPI_WRITE_STS, &status);
	lpc_update_host_event_status();

	zassert_equal(sci_received_cnt, 3, NULL);
	zassert_equal(smi_received_cnt, 0, NULL);

	espi_emul_manage_host_callback(ESPI_DEVICE, &espi_cb_obj, 0);
}

ZTEST_SUITE(espi_vwire, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
