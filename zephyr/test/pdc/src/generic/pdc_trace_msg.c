/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/pdc.h"
#include "drivers/pdc_trace_msg.h"
#include "host_command.h"

#include <stdint.h>

#include <zephyr/ztest.h>

#define TEST_PORT 0

/*
 * This is the default size in the real implementation
 */
static const int msg_fifo_size_log2 = 10;
static const int msg_fifo_size = 1 << msg_fifo_size_log2;

static void pdc_trace_msg_before_test(void *data)
{
	/*
	 * Tracing is typically off by default, let's make sure.
	 */
	pdc_trace_msg_enable(EC_PDC_TRACE_MSG_PORT_NONE);
	pdc_trace_msg_fifo_reset();
}

ZTEST_SUITE(pdc_trace_msg, NULL, NULL, pdc_trace_msg_before_test, NULL, NULL);

ZTEST_USER(pdc_trace_msg, test_enable_for_port)
{
	int status;

	status = pdc_trace_msg_enable(EC_PDC_TRACE_MSG_PORT_ALL);
	zassert_equal(status, EC_PDC_TRACE_MSG_PORT_NONE,
		      "expected %d but got %d", EC_PDC_TRACE_MSG_PORT_NONE,
		      status);

	status = pdc_trace_msg_enable(TEST_PORT);
	zassert_equal(status, EC_PDC_TRACE_MSG_PORT_ALL, "expected %d, got %d",
		      EC_PDC_TRACE_MSG_PORT_ALL, status);

	status = pdc_trace_msg_enable(EC_PDC_TRACE_MSG_PORT_NONE);
	zassert_equal(status, TEST_PORT, "expected %d, got %d", TEST_PORT,
		      status);

	status = pdc_trace_msg_enable(EC_PDC_TRACE_MSG_PORT_NONE);
	zassert_equal(status, (EC_PDC_TRACE_MSG_PORT_NONE),
		      "expected %d, got %d", (EC_PDC_TRACE_MSG_PORT_NONE),
		      status);
}

static int hc_msg_enable(struct ec_response_pdc_trace_msg_enable *r)
{
	struct ec_params_pdc_trace_msg_enable msg_enable_p = {
		.port = TEST_PORT,
	};
	struct host_cmd_handler_args msg_enable_args = BUILD_HOST_COMMAND(
		EC_CMD_PDC_TRACE_MSG_ENABLE, 0, *r, msg_enable_p);

	return host_command_process(&msg_enable_args);
}

static int hc_msg_get(struct ec_response_pdc_trace_msg_get_entries *r)
{
	struct host_cmd_handler_args msg_get_args = BUILD_HOST_COMMAND_RESPONSE(
		EC_CMD_PDC_TRACE_MSG_GET_ENTRIES, 0, *r);

	return host_command_process(&msg_get_args);
}

static int walk_pl(const uint8_t *const pl, const int pl_size)
{
	const struct pdc_trace_msg_entry *e;

	int bytes_processed = 0;
	int n_messages = 0;

	do {
		int e_size;

		zassert_true(
			bytes_processed + sizeof(struct pdc_trace_msg_entry) <=
				pl_size,
			"partial pdc_trace_msg_entry in payload at offset %d of %d",
			bytes_processed, pl_size);

		e = (const struct pdc_trace_msg_entry *)&pl[bytes_processed];
		zassert_equal(e->port_num, TEST_PORT,
			      "got port_num %d instead of %d", e->port_num,
			      TEST_PORT);
		zassert_equal(e->msg_type, PDC_TRACE_CHIP_TYPE_RTS54XX,
			      "got msg_type %d instead of %d", e->port_num,
			      PDC_TRACE_CHIP_TYPE_RTS54XX);
		e_size = e->pdc_data_size;
		zassert_not_equal(e_size, 0, "got empty entry");
		e_size += sizeof(struct pdc_trace_msg_entry);
		zassert_true(bytes_processed + e_size <= pl_size,
			     "entry sizes exceed buffer");
		bytes_processed += e_size;
		++n_messages;
	} while (bytes_processed < pl_size);

	return n_messages;
}

ZTEST_USER(pdc_trace_msg, test_fifo_ops)
{
	uint8_t payload[sizeof(struct pdc_trace_msg_entry) + msg_fifo_size];
	int pl_bytes;

	pdc_trace_msg_enable(TEST_PORT);

	/*
	 * Fill up FIFO until full.
	 */

	for (pl_bytes = 1; pl_bytes < msg_fifo_size; ++pl_bytes) {
		bool status;

		for (int i = 0; i < pl_bytes; ++i)
			payload[i] = (pl_bytes + i) & 0xff;

		/*
		 * _req vs. _resp are interchangeable for FIFO tests,
		 * so alternate between them.
		 */
		if (pl_bytes & 0x01) {
			status = pdc_trace_msg_req(TEST_PORT,
						   PDC_TRACE_CHIP_TYPE_RTS54XX,
						   payload, pl_bytes);
		} else {
			status = pdc_trace_msg_resp(TEST_PORT,
						    PDC_TRACE_CHIP_TYPE_RTS54XX,
						    payload, pl_bytes);
		}
		if (!status)
			break;
	}
	zassert_not_equal(pl_bytes, msg_fifo_size,
			  "message FIFO did not report overflow condition");

	/*
	 * Verify the FIFO drop count incremented by one.
	 */
	struct ec_response_pdc_trace_msg_enable msg_enable_r;
	zassert_ok(hc_msg_enable(&msg_enable_r));
	zassert_equal(msg_enable_r.dropped_count, 1,
		      "expected drop count 1 but got %d",
		      msg_enable_r.dropped_count);

	uint8_t res_buf[msg_fifo_size];
	struct ec_response_pdc_trace_msg_get_entries *r =
		(struct ec_response_pdc_trace_msg_get_entries *)res_buf;
	const int msg_count = pl_bytes - 1;
	int returned_messages = 0;
	int msg;

	/*
	 * Returned messages may be batched.
	 */
	for (msg = 0; msg < msg_count; ++msg) {
		int n_messages;

		zassert_ok(hc_msg_get(r));
		if (r->pl_size == 0)
			break;
		n_messages = walk_pl(r->payload, r->pl_size);
		returned_messages += n_messages;
	}

	/*
	 * The FIFO should now be empty.
	 */
	zassert_ok(hc_msg_get(r));
	zassert_equal(r->pl_size, 0, "got pl_size %d but expected 0",
		      r->pl_size);

	/*
	 * Did we receive all the messages we sent?
	 */
	zassert_equal(returned_messages, msg_count,
		      "got %d messages but expected %d", returned_messages,
		      msg_count);
}
