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

#define PORT 0
#define ESPI_DEVICE DEVICE_DT_GET(DT_CHOSEN(cros_ec_espi))

static int kbc8042_ibf_cnt = 0;
static uint8_t kbc8042_data_ack = 0;
static uint8_t kbc8042_data_recv = 0;
static uint8_t kbc8042_data_recv_is_cmd = 0;

static void espi_kbc_ibf_cb(const struct device *dev,
			    struct espi_callback *cb,
			    struct espi_event espi_evt)
{
	uint32_t kbd_data = espi_evt.evt_data;

	printk("IBF CB going 1...\n");
	if(espi_evt.evt_details != ESPI_PERIPHERAL_8042_KBC)
		return;

	printk("IBF CB going 2...\n");
	if(is_8042_ibf(kbd_data)) {
		printk("IBF CB going 3...\n");
		kbc8042_data_recv_is_cmd = get_8042_type(kbd_data);
		kbc8042_data_recv = get_8042_data(kbd_data);

		kbc8042_ibf_cnt++;
	}
}

// static void espi_kbc_ack_cb(const struct device *dev,
// 			    struct espi_callback *cb,
// 			    struct espi_event espi_evt)
// {
// 	struct espi_evt_data_kbc *kbc_evt = (struct espi_evt_data_kbc *)&espi_evt.evt_data;
// 	kbc8042_data_ack = kbc_evt->data;
// 	kbc8042_data_recv_is_cmd = kbc_evt->type;
// }

// static void espi_kbc_recv_cb(const struct device *dev,
// 			     struct espi_callback *cb,
// 			     struct espi_event espi_evt)
// {
// 	kbc8042_data_ack++;
// }

ZTEST_USER(espi_peripheral, test_host_read_and_write_kb_char)
{
	int has_char;
	uint8_t kbc8042_reg;

	has_char = lpc_keyboard_has_char();
	zassert_equal(has_char, 0, "Should not have a char");

	lpc_keyboard_put_char(0x7E, 0);

	has_char = lpc_keyboard_has_char();
	zassert_equal(has_char, 1, "Should have a char");

	uint32_t reg;
	espi_emul_io_read(ESPI_DEVICE, 1, 0x60, &reg);

	zassert_equal(reg, 0x7E, "Data should be 0x7E");

	has_char = lpc_keyboard_has_char();
	zassert_equal(has_char, 0, "Should have a char");
}

ZTEST_USER(espi_peripheral, test_lpc_input_pending)
{
	int has_char;
	uint8_t kbc8042_reg;
	const uint32_t reg = 0xFE;

	// Prepare callbacks
	kbc8042_ibf_cnt = 0;
	struct espi_callback espi_cb_obj;
	espi_init_callback(&espi_cb_obj, espi_kbc_ibf_cb, ESPI_BUS_PERIPHERAL_NOTIFICATION);
	espi_add_callback(ESPI_DEVICE, &espi_cb_obj);

	espi_write_lpc_request(ESPI_DEVICE, E8042_PAUSE_IRQ, NULL);

	zassert_equal(lpc_keyboard_input_pending(), 0, "should be no input pending");
	espi_emul_io_write(ESPI_DEVICE, 1, EMUL_ESPI_KBC8042_PORT_OUT_DATA, reg);
	zassert_equal(lpc_keyboard_input_pending(), 1, "should be input pending");

	zassert_equal(kbc8042_ibf_cnt, 0, "IRQ should be paused");
	espi_write_lpc_request(ESPI_DEVICE, E8042_RESUME_IRQ, NULL);
	zassert_equal(kbc8042_ibf_cnt, 1, "IRQ should be raised");

	zassert_equal(kbc8042_data_recv, reg, "Invalid data recv");
	zassert_equal(kbc8042_data_recv_is_cmd, 0, "Invalid data type");

	zassert_equal(lpc_keyboard_input_pending(), 0, "buffer should be empty");

	// Clean callbacks
	espi_remove_callback(ESPI_DEVICE, &espi_cb_obj);
}

ZTEST_USER(espi_peripheral, test_lpc_kb_put_char)
{
	int has_char;
	uint8_t kbc8042_reg;
	const uint32_t reg = 0xFE;
	uint32_t reg_recv;

	lpc_keyboard_put_char(reg, 0);
	has_char = lpc_keyboard_has_char();
	zassert_equal(has_char, 1, "Must have char!");

	espi_emul_io_read(ESPI_DEVICE, 1, EMUL_ESPI_KBC8042_PORT_IN_DATA, &reg_recv);

	has_char = lpc_keyboard_has_char();
	zassert_equal(has_char, 0, "Should not have a char");
	zassert_equal(reg_recv, reg, "Received data must be valid!");
}

ZTEST_USER(espi_peripheral, test_lpc_aux_put_char)
{
	int has_char;

	has_char = lpc_keyboard_has_char();
	zassert_equal(has_char, 0, "Should not have a char");

	lpc_aux_put_char(0x7E, 0);

	has_char = lpc_keyboard_has_char();
	zassert_equal(has_char, 1, "Should have a char");

	lpc_keyboard_clear_buffer();
	has_char = lpc_keyboard_has_char();
	zassert_equal(has_char, 0, "Should not have a char");
}

ZTEST_USER(espi_peripheral, test_if_obe_ibf)
{
	uint32_t data = 0;
	struct espi_evt_data_kbc *kbc = (struct espi_evt_data_kbc*)&data;
	kbc->evt = HOST_KBC_EVT_OBE;

	zassert_equal(is_8042_obe(data), true, "obe");
	zassert_equal(is_8042_ibf(data), false, "ibf");

	kbc->evt = HOST_KBC_EVT_IBF;
	zassert_equal(is_8042_obe(data), false, "obe");
	zassert_equal(is_8042_ibf(data), true, "ibf");
}

ZTEST_SUITE(espi_peripheral, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
