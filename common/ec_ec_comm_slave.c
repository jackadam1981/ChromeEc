/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC-EC communication, task and functions for slave.
 */

#include "common.h"
#include "battery.h"
#include "charge_state_v2.h"
#include "console.h"
#include "crc8.h"
#include "ec_commands.h"
#include "ec_ec_comm_slave.h"
#include "hwtimer.h"
#include "queue.h"
#include "queue_policies.h"
#include "task.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

/*
 * TODO(b:65697620): Move these to some other C file, depending on a config
 * option.
 */
struct ec_response_battery_static_info base_battery_static;
struct ec_response_battery_dynamic_info base_battery_dynamic;

/*
 * Our command buffer must be big enough to fit any command with its parameters
 * and crc byte.
 */
#define COMMAND_BUFFER_SIZE 17

#define COMMAND_BUFFER_PARAM_SIZE \
	(COMMAND_BUFFER_SIZE - sizeof(struct ec_host_request4))

BUILD_ASSERT(COMMAND_BUFFER_PARAM_SIZE >=
	(sizeof(struct ec_params_battery_static_info) + 1));
BUILD_ASSERT(COMMAND_BUFFER_PARAM_SIZE >=
	(sizeof(struct ec_params_battery_dynamic_info) + 1));
BUILD_ASSERT(COMMAND_BUFFER_PARAM_SIZE >=
	(sizeof(struct ec_params_charger_control) + 1));

/*
 * Maximum time to wait for a command, commands are at most 17 bytes, so should
 * not take more than 2ms to be sent at 115200 bps.
 */
#define COMMAND_TIMEOUT (5 * MSEC)


void ec_ec_comm_slave_written(struct consumer const *consumer, size_t count)
{
	task_wake(TASK_ID_ECCOMM);
}

/*
 * Discard all data from the input queue.
 *
 * Note that we always sleep for at least 1ms, to make sure that we give enough
 * time for the next byte to arrive, even if the queue is empty to start with.
 */
static void discard_queue(void)
{
	do {
		queue_advance_head(&ec_ec_comm_slave_input,
				queue_count(&ec_ec_comm_slave_input));
		usleep(1*MSEC);
	} while (queue_count(&ec_ec_comm_slave_input) > 0);
}

/* Write response to master. */
static void write_response(uint16_t res, int seq, uint8_t *data, int len)
{
	struct ec_host_response4 header;
	uint8_t crc;

	header.fields0 =
		4 | /* version */
		EC_PACKET4_0_IS_RESPONSE_MASK | /* is_response */
		(seq << EC_PACKET4_0_SEQ_NUM_SHIFT); /* seq_num */
	/* Set data_crc_present if there is data */
	header.fields1 = (len > 0) ? EC_PACKET4_1_DATA_CRC_PRESENT_MASK : 0;
	header.result = res;
	header.data_len = len;
	header.reserved = 0;
	header.header_crc =
		crc8((uint8_t *)&header, sizeof(header)-1);
	QUEUE_ADD_UNITS(&ec_ec_comm_slave_output,
			(uint8_t *)&header, sizeof(header));

	if (len > 0) {
		QUEUE_ADD_UNITS(&ec_ec_comm_slave_output, data, len);
		crc = crc8(data, len);
		QUEUE_ADD_UNITS(&ec_ec_comm_slave_output, &crc, sizeof(crc));
	}
}

/* Read len bytes into buffer. Waiting up to COMMAND_TIMEOUT_US after start.
 *
 * Returns EC_SUCCESS or EC_ERROR_TIMEOUT.
 */
static int read_data(void *buffer, size_t len, uint32_t start)
{
	while (queue_count(&ec_ec_comm_slave_input) < len) {
		if ((__hw_clock_source_read() - start)
			> COMMAND_TIMEOUT)
			return EC_ERROR_TIMEOUT;

		/* Every incoming byte should wake the task. */
		task_wait_event(1 * MSEC);
	}

	/* Fetch header */
	QUEUE_REMOVE_UNITS(&ec_ec_comm_slave_input, buffer, len);

	return EC_SUCCESS;
}

#ifdef CONFIG_EC_EC_COMM_BATTERY
static void handle_cmd_charger_control(
	const struct ec_params_charger_control *params,
	int data_len, int seq)
{
	struct ec_response_charger_control response;

	if (data_len != sizeof(*params)) {
		write_response(EC_RES_INVALID_COMMAND, seq,
			NULL, 0);
		return;
	}

	if (params->max_current >= 0) {
		charger_enable_otg_power(0);
		charge_set_input_current_limit(
			MIN(MAX_CURRENT_MA, params->max_current), 0);
	} else {
		if (-params->max_current > MAX_OTG_CURRENT_MA ||
				params->otg_voltage > MAX_OTG_VOLTAGE_MV) {
			write_response(EC_RES_INVALID_PARAM, seq, NULL, 0);
			return;
		}

		/* Reset input current to minimum. */
		charge_set_input_current_limit(CONFIG_CHARGER_INPUT_CURRENT, 0);
		/* Setup and enable "OTG". */
		charger_set_otg_current_voltage(-params->max_current,
						params->otg_voltage);
		charger_enable_otg_power(1);
	}

	write_response(EC_RES_SUCCESS, seq,
		(void *)&response, sizeof(response));
}
#endif

void ec_ec_comm_slave_task(void *u)
{
	uint8_t command[COMMAND_BUFFER_SIZE];
	struct ec_host_request4 *header = (void *)&command[0];
	unsigned int len, seq, hascrc, cmdver;
	uint32_t start;

	while (1) {
		task_wait_event(-1);

		if (queue_count(&ec_ec_comm_slave_input) == 0)
			continue;

		/* We got some data, start timeout counter. */
		start = __hw_clock_source_read();

		/* Wait for whole header to be available and read it. */
		if (read_data((void *)header, sizeof(*header), start)) {
			CPRINTS("%s timeout (header)", __func__);
			goto discard;
		}

#if 0
		CPRINTS("%s f0=%02x f1=%02x cmd=%02x, length=%d", __func__,
			header->fields0, header->fields1,
			header->command, header->data_len);
#endif

		/* Ignore response (we wrote that ourselves) */
		if (header->fields0 & EC_PACKET4_0_IS_RESPONSE_MASK)
			goto discard;

		/* Validate version and crc. */
		if ((header->fields0 & EC_PACKET4_0_STRUCT_VERSION_MASK) != 4 ||
			   header->header_crc !=
				crc8((uint8_t *)header, sizeof(*header)-1)) {
			CPRINTS("%s header/crc error", __func__);
			goto discard;
		}

		len = header->data_len;
		hascrc = header->fields1 & EC_PACKET4_1_DATA_CRC_PRESENT_MASK;
		if (hascrc)
			len += 1;

		/*
		 * Ignore commands that are too long to fit in our buffer.
		 * TODO(b:65697962): Should we reply with an error code?
		 */
		if (len > (sizeof(command) - sizeof(*header))) {
			CPRINTS("%s len error (%d)", __func__, len);
			goto discard;
		}

		seq = (header->fields0 & EC_PACKET4_0_SEQ_NUM_MASK) >>
			EC_PACKET4_0_SEQ_NUM_SHIFT;

		cmdver = header->fields1 & EC_PACKET4_1_COMMAND_VERSION_MASK;

		/* Wait for the rest of the data to be available and read it. */
		if (read_data((void *)&command[sizeof(*header)], len, start)) {
			CPRINTS("%s timeout (header)", __func__);
			goto discard;
		}

		/* Check data CRC */
		if (hascrc && command[sizeof(*header)+len-1] !=
			crc8((void *)&command[sizeof(*header)], len-1)) {
			CPRINTS("%s data crc error", __func__);
		}

		/* For now, all commands have version 0. */
		if (cmdver != 0) {
			CPRINTS("%s bad command version", __func__);
			goto discard;
		}

		switch (header->command) {
#ifdef CONFIG_EC_EC_COMM_BATTERY
		case EC_CMD_BATTERY_GET_STATIC:
			write_response(EC_RES_SUCCESS, seq,
				(void *)&base_battery_static,
				sizeof(base_battery_static));
			break;
		case EC_CMD_BATTERY_GET_DYNAMIC:
			write_response(EC_RES_SUCCESS, seq,
				(void *)&base_battery_dynamic,
				sizeof(base_battery_dynamic));
			break;
		case EC_CMD_CHARGER_CONTROL: {
			handle_cmd_charger_control(
				(void *)&command[sizeof(*header)],
				header->data_len, seq);
			break;
		}
#endif
		default:
			write_response(EC_RES_INVALID_COMMAND, seq,
				NULL, 0);
		}

		continue;
discard:
		/*
		 * Some error occurred: discard all data in the queue.
		 */
		discard_queue();
	}
}
