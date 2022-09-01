

#pragma clang optimize off

/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest_assert.h>
#include <zephyr/drivers/i2c_emul.h>

#include "keyboard_config.h"
#include "mkbp_fifo.h"
#include "test/drivers/test_state.h"
#include "test/drivers/test_state.h"

/* Tests for Matrix Keyboard Protocol (MKBP) */

/* Largest event size that we support */

#define EC_MKBP_EVENT_KEY_MATRIX_EVENT_DATA_SIZE KEYBOARD_COLS_MAX

#define MAX_EVENT_DATA_SIZE EC_MKBP_EVENT_KEY_MATRIX_EVENT_DATA_SIZE

struct mkbp_fifo_fixture {
	uint8_t input_event_data[MAX_EVENT_DATA_SIZE];
};

static void *mkbp_fifo_setup(void)
{
	static struct mkbp_fifo_fixture fixture;

	return &fixture;
}

static void mkbp_fifo_before(void *data)
{
	struct mkbp_fifo_fixture *fixture = data;
	mkbp_clear_fifo();
	memset(fixture->input_event_data, 0, KEYBOARD_COLS_MAX);
}

static void mkbp_fifo_after(void *data)
{
	mkbp_clear_fifo();
}

ZTEST_F(mkbp_fifo, test_fifo_add_keyboard_key_matrix_event)
{
	uint8_t out[KEYBOARD_COLS_MAX + 1];

	memset(out, 0, sizeof(out));

	fixture->input_event_data[0] = 0x1;
	fixture->input_event_data[1] = 0x2;
	fixture->input_event_data[2] = 0x3;
	fixture->input_event_data[3] = 0x4;
	fixture->input_event_data[4] = 0x5;

	/* Keyboard Key Matrix Event */
	zassert_ok(mkbp_fifo_add(EC_MKBP_EVENT_KEY_MATRIX,
				 fixture->input_event_data),
		   NULL);

	int dequeued_data_size =
		mkbp_fifo_get_next_event(out, EC_MKBP_EVENT_KEY_MATRIX);

	zassert_equal(dequeued_data_size,
		      EC_MKBP_EVENT_KEY_MATRIX_EVENT_DATA_SIZE, NULL);
	zassert_mem_equal(fixture->input_event_data, out, MAX_EVENT_DATA_SIZE,
			  NULL);
}

ZTEST_SUITE(mkbp_fifo, drivers_predicate_post_main, mkbp_fifo_setup,
	    mkbp_fifo_before, mkbp_fifo_after, NULL);
