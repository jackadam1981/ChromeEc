/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "fakes.h"
#include "queue.h"
#include "rollback.h"
#include "system.h"
#include "update_fw.h"
#include "usb-stream.h"
#include "usb_descriptor.h"

#include <zephyr/device.h>
#include <zephyr/fff.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/ztest.h>

void send_error_reset(uint8_t resp_value);

static void send_vendor_command(enum update_extra_command command,
				const void *data, size_t data_size)
{
	const struct queue *rx_queue = usb_update.producer.queue;
	struct update_frame_header pdu;
	uint8_t buffer[USB_MAX_PACKET_SIZE];
	size_t total_size = sizeof(pdu) + sizeof(uint16_t) + data_size;

	pdu.block_size = sys_cpu_to_be32(total_size);
	pdu.cmd.block_digest = sys_cpu_to_be32(0);
	pdu.cmd.block_base = sys_cpu_to_be32(UPDATE_EXTRA_CMD);

	memcpy(buffer, &pdu, sizeof(pdu));
	sys_put_be16(command, buffer + sizeof(pdu));
	memcpy(buffer + sizeof(pdu) + sizeof(uint16_t), data, data_size);

	queue_add_units(rx_queue, buffer, total_size);
}

ZTEST(vendor_command, test_immediate_reset)
{
	send_vendor_command(UPDATE_EXTRA_CMD_IMMEDIATE_RESET, NULL, 0);
	zassert_equal(system_reset_fake.call_count, 1);
	zassert_equal(system_reset_fake.arg0_history[0],
		      SYSTEM_RESET_MANUALLY_TRIGGERED);
}

ZTEST(vendor_command, test_rollback_update)
{
	const struct queue *tx_queue = usb_update.consumer.queue;
	uint8_t resp;

	/* fail with no payload */
	send_vendor_command(UPDATE_EXTRA_CMD_INJECT_ENTROPY, NULL, 0);
	zassert_equal(queue_count(tx_queue), 1);
	queue_remove_units(tx_queue, &resp, 1);
	zassert_equal(resp, EC_RES_INVALID_PARAM);

	/* test valid update */
	const uint8_t entropy[CONFIG_ROLLBACK_SECRET_SIZE] = {
		'H', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '!',
	};
	send_vendor_command(UPDATE_EXTRA_CMD_INJECT_ENTROPY, entropy,
			    sizeof(entropy));
	zassert_equal(queue_count(tx_queue), 1);
	queue_remove_units(tx_queue, &resp, 1);
	zassert_equal(resp, EC_SUCCESS);

	/* SHA256(b'\x00' * 32 + b'Hello world!' + b'\x00' * 20) */
	const uint8_t expected[] = { 88,  245, 173, 96,	 220, 172, 12,	124,
				     201, 206, 235, 114, 215, 128, 166, 113,
				     50,  2,   143, 104, 67,  153, 82,	49,
				     172, 208, 131, 173, 40,  243, 227, 122 };
	uint8_t secret[CONFIG_ROLLBACK_SECRET_SIZE];

	rollback_get_secret(secret);
	zassert_mem_equal(secret, expected, CONFIG_ROLLBACK_SECRET_SIZE);
}

ZTEST(vendor_command, test_usb_pairing)
{
	const struct queue *tx_queue = usb_update.consumer.queue;
	uint8_t resp;
	struct pair_challenge challenge;

	/* fail with no payload */
	send_vendor_command(UPDATE_EXTRA_CMD_PAIR_CHALLENGE, NULL, 0);
	zassert_equal(queue_count(tx_queue), 1);
	queue_remove_units(tx_queue, &resp, 1);
	zassert_equal(resp, EC_RES_INVALID_PARAM);

	/* test valid request */
	send_vendor_command(UPDATE_EXTRA_CMD_PAIR_CHALLENGE, &challenge,
			    sizeof(challenge));
	zassert_equal(queue_count(tx_queue), 1 + 32 + 16);
	queue_remove_units(tx_queue, &resp, 1);
	zassert_equal(resp, EC_SUCCESS);
}

int custom_touchpad_debug(const uint8_t *param, unsigned int param_size,
			  uint8_t **data, unsigned int *data_size)
{
	static uint8_t buffer[] = { 'H', 'e', 'l', 'l', 'o' };

	*data = buffer;
	*data_size = sizeof(buffer);

	return 0;
}

ZTEST(vendor_command, test_touchpad)
{
	const struct queue *tx_queue = usb_update.consumer.queue;
	struct touchpad_info tp_info;

	/* TOUCHPAD_INFO */
	touchpad_get_info_fake.return_val = sizeof(struct touchpad_info);
	send_vendor_command(UPDATE_EXTRA_CMD_TOUCHPAD_INFO, NULL, 0);
	zassert_equal(queue_count(tx_queue), sizeof(tp_info));
	queue_remove_units(tx_queue, &tp_info, sizeof(tp_info));
	zassert_equal(tp_info.fw_address, CONFIG_TOUCHPAD_VIRTUAL_OFF);
	zassert_equal(tp_info.fw_size, CONFIG_TOUCHPAD_VIRTUAL_SIZE);

	/* TOUCHPAD_DEBUG, expect return the string "Hello" */
	char output[5];

	touchpad_debug_fake.custom_fake = custom_touchpad_debug;
	send_vendor_command(UPDATE_EXTRA_CMD_TOUCHPAD_DEBUG, NULL, 0);
	zassert_equal(queue_count(tx_queue), 5);
	queue_remove_units(tx_queue, output, 5);
	zassert_mem_equal(output, "Hello", 5);
}

ZTEST(vendor_command, test_get_version)
{
	const struct queue *tx_queue = usb_update.consumer.queue;
	uint8_t resp;

	send_vendor_command(UPDATE_EXTRA_CMD_GET_VERSION_STRING, NULL, 0);
	zassert_true(queue_count(tx_queue) >= 1);
	queue_remove_units(tx_queue, &resp, 1);
	zassert_equal(resp, EC_SUCCESS);
}

ZTEST(vendor_command, test_invalid_command)
{
	const struct queue *tx_queue = usb_update.consumer.queue;
	uint8_t resp;

	send_vendor_command(99, NULL, 0);
	zassert_equal(queue_count(tx_queue), 1);
	queue_remove_units(tx_queue, &resp, 1);
	zassert_equal(resp, EC_RES_INVALID_COMMAND);
}

static void vendor_command_before(void *f)
{
	/* reset the usb_updater's internal state */
	send_error_reset(0);

	/* clear RX/TX queue */
	queue_init(usb_update.consumer.queue);
	queue_init(usb_update.producer.queue);

	FFF_FAKES_LIST(RESET_FAKE);
	FFF_RESET_HISTORY();
}

ZTEST_SUITE(vendor_command, NULL, NULL, vendor_command_before, NULL, NULL);
