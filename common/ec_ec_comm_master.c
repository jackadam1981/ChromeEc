/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC-EC communication, functions and definitions for master.
 */

#include <stdint.h>
#include "console.h"
#include "crc8.h"
#include "ec_commands.h"
#include "ec_ec_comm_master.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHARGER, outstr)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)

/*
 * TODO(b:65697620): Move these to some the second position of some battery
 * array, depending on a config option.
 */
struct ec_response_battery_static_info base_battery_static;
struct ec_response_battery_dynamic_info base_battery_dynamic;

/*
 * The packed structures below will not play well if we force EC host commands
 * structures to be aligned on 32-bit boundary. There are ways to fix that, but
 * it would not be very clean, possibly requiring copying data around, or
 * modifying uart_alt_pad_write_read API.
 */
#ifdef CONFIG_HOSTCMD_ALIGNED
#error "Cannot define CONFIG_HOSTCMD_ALIGNED with EC-EC communication master."
#endif

#define EC_EC_HOSTCMD_VERSION 4

/* Print extra debugging information */
#undef EXTRA_DEBUG

/*
 * During early debugging, we would like to check that the error rate does
 * grow out of control.
 */
#define DEBUG_EC_COMM_STATS
#ifdef DEBUG_EC_COMM_STATS
int total;
int errtimeout;
int errbusy;
int errunknown;
int errdatacrc;
int errcrc;
int errinval;
#endif

/**
 * Write a command on the EC-EC communication UART channel.
 *
 * @param command	One of EC_CMD_*.
 * @param tx_rx		Packed structure with this layout:
 * struct {
 *	struct {
 *		struct ec_host_request4 head;
 *		struct ec_params_* param;
 *		uint8_t crc8;
 *	} tx;
 *	struct {
 *		struct ec_host_response4 head;
 *		struct ec_response_* info;
 *		uint8_t crc8;
 *	} rx;
 * } __packed data;
 *
 * Where tx is the data to be transmitted (head and crc8 are computed by this
 * function), and rx is the data to be received (head integrity and crc8 are
 * verified by this function).
 *
 * When a command does not take parameters, param/crc8 must be omitted in
 * tx structure. The same applies to rx structure if the response does not
 * include a payload: info/crc8 must be omitted.
 *
 * @param txlen is the size of tx.param (0 if no parameter is passed).
 * @param rxlen is the size of rx.param (0 if no information is returned).
 *
 * @return
 *  - EC_SUCCESS on success.
 *  - EC_ERROR_TIMEOUT when remote end times out replying.
 *  - EC_ERROR_BUSY when UART is busy and cannot transmit currently.
 *  - EC_ERROR_CRC when the header or data CRC is invalid.
 *  - EC_ERROR_INVAL when the received header is invalid.
 *  - EC_ERROR_UNKNOWN on other error.
 */
static int write_command(uint16_t command,
			 uint8_t *tx_rx, int txlen, int rxlen,
			 int timeout)
{
	/* Sequence number. */
	static uint8_t cur_seq;
	int ret;
	int hascrc, response_seq;

	struct ec_host_request4 *request_header = (void *)tx_rx;
	/* Request (TX) length is header + (data + crc8), response follows. */
	int tx_total_length =
		sizeof(*request_header) + ((txlen > 0) ? (txlen + 1) : 0);

	struct ec_host_response4 *response_header =
		(void *)&tx_rx[tx_total_length];
	/* RX length is TX length + response from slave. */
	int rx_total_length = tx_total_length +
		sizeof(*request_header) + ((rxlen > 0) ? (rxlen + 1) : 0);

	/* Make sure there is a gap between each commands. */
	/* TODO(b:65697962): We can be much smarter than this. */
	usleep(10*MSEC);

#ifdef DEBUG_EC_COMM_STATS
	if ((total % 128) == 0) {
		CPRINTF("UART %d (T%dB%d,U%dC%dD%dI%d)\n", total,
			errtimeout, errbusy, errunknown,
			errcrc, errdatacrc, errinval);
	}
#endif

	cur_seq = (cur_seq + 1) &
		(EC_PACKET4_0_SEQ_NUM_MASK >> EC_PACKET4_0_SEQ_NUM_SHIFT);

	memset(request_header, 0, sizeof(*request_header));
	/* fields0: leave seq_dup and is_response as 0. */
	request_header->fields0 =
		EC_EC_HOSTCMD_VERSION | /* version */
		(cur_seq << EC_PACKET4_0_SEQ_NUM_SHIFT); /* seq_num */
	/* fields1: leave command_version as 0. */
	if (txlen > 0)
		request_header->fields1 |= EC_PACKET4_1_DATA_CRC_PRESENT_MASK;
	request_header->command = command;
	request_header->data_len = txlen;
	request_header->header_crc =
		crc8((uint8_t *)request_header, sizeof(*request_header)-1);
	if (txlen > 0)
		tx_rx[sizeof(*request_header) + txlen] =
			crc8(&tx_rx[sizeof(*request_header)], txlen);

	ret = uart_alt_pad_write_read((void *)tx_rx, tx_total_length,
				      (void *)tx_rx, rx_total_length, timeout);

#ifdef DEBUG_EC_COMM_STATS
	total++;
#endif

#ifdef EXTRA_DEBUG
	CPRINTF("EC-EC ret=%d/%d\n", ret, rx_total_length);
#endif

	if (ret != rx_total_length) {
		if (ret == -EC_ERROR_TIMEOUT) {
#ifdef DEBUG_EC_COMM_STATS
			errtimeout++;
#endif
			return EC_ERROR_TIMEOUT;
		}

		if (ret == -EC_ERROR_BUSY) {
#ifdef DEBUG_EC_COMM_STATS
			errbusy++;
#endif
			return EC_ERROR_BUSY;
		}
#ifdef DEBUG_EC_COMM_STATS
		errunknown++;
#endif
		return EC_ERROR_UNKNOWN;
	}

	if (response_header->header_crc !=
		crc8((uint8_t *)response_header, sizeof(*response_header)-1)) {
#ifdef DEBUG_EC_COMM_STATS
		errcrc++;
#endif
		return EC_ERROR_CRC;
	}

	hascrc = response_header->fields1 & EC_PACKET4_1_DATA_CRC_PRESENT_MASK;
	response_seq = (response_header->fields0 & EC_PACKET4_0_SEQ_NUM_MASK) >>
		EC_PACKET4_0_SEQ_NUM_SHIFT;

	/*
	 * Validate received header.
	 * Note that we _require_ data crc to be present if there is data to be
	 * read back, else we would not know how many bytes to read exactly.
	 */
	if ((response_header->fields0 & EC_PACKET4_0_STRUCT_VERSION_MASK)
			!= EC_EC_HOSTCMD_VERSION ||
		!(response_header->fields0 & EC_PACKET4_0_IS_RESPONSE_MASK) ||
		response_seq != cur_seq ||
		(response_header->data_len > 0 && !hascrc) ||
		response_header->data_len != rxlen) {
#ifdef DEBUG_EC_COMM_STATS
		errinval++;
#endif
		return EC_ERROR_INVAL;
	}

	/* Check data CRC. */
	if (hascrc && tx_rx[rx_total_length - 1] !=
		    crc8(&tx_rx[tx_total_length + sizeof(*request_header)],
				 rxlen)) {
#ifdef DEBUG_EC_COMM_STATS
		errdatacrc++;
#endif
		return EC_ERROR_CRC;
	}

	return EC_SUCCESS;
}

static int handle_error(int ret, int request_result)
{
	if (ret != EC_SUCCESS) {
		/* Do not print busy errors as they just spam the console. */
		if (ret != EC_ERROR_BUSY)
			CPRINTF("%s: tx error %d\n", __func__, ret);
		return EC_RES_ERROR;
	}

	if (request_result != EC_RES_SUCCESS)
		CPRINTF("%s: cmd error %d\n", __func__, ret);

	return request_result;
}

#ifdef CONFIG_EC_EC_COMM_BATTERY
int ec_ec_master_base_get_dynamic_info(void)
{
	int ret;
	struct {
		struct {
			struct ec_host_request4 head;
			struct ec_params_battery_dynamic_info param;
			uint8_t crc8;
		} tx;
		struct {
			struct ec_host_response4 head;
			struct ec_response_battery_dynamic_info info;
			uint8_t crc8;
		} rx;
	} __packed data;

	data.tx.param.index = 0;

	ret = write_command(EC_CMD_BATTERY_GET_DYNAMIC,
			(void *)&data, sizeof(data.tx.param),
			sizeof(data.rx.info), 15000);
	ret = handle_error(ret, data.rx.head.result);
	if (ret != EC_RES_SUCCESS)
		return ret;

#ifdef EXTRA_DEBUG
	CPRINTF("V:          %d mV\n", data.rx.info.voltage);
	CPRINTF("I:          %d mA\n", data.rx.info.current);
	CPRINTF("Remaining:  %d mAh\n", data.rx.info.remaining_capacity);
	CPRINTF("Cap-full:   %d mAh\n", data.rx.info.full_capacity);
	CPRINTF("Flags:      %04x\n", data.rx.info.flags);
	CPRINTF("V-desired:  %d mV\n", data.rx.info.desired_voltage);
	CPRINTF("I-desired:  %d mA\n", data.rx.info.desired_current);
#endif

	memcpy(&base_battery_dynamic, &data.rx.info,
				sizeof(base_battery_dynamic));
	return EC_RES_SUCCESS;
}

int ec_ec_master_base_get_static_info(void)
{
	int ret;
	struct {
		struct {
			struct ec_host_request4 head;
			struct ec_params_battery_static_info param;
			uint8_t crc8;
		} tx;
		struct {
			struct ec_host_response4 head;
			struct ec_response_battery_static_info info;
			uint8_t crc8;
		} rx;
	} __packed data;

	data.tx.param.index = 0;

	ret = write_command(EC_CMD_BATTERY_GET_STATIC,
			(void *)&data, sizeof(data.tx.param),
			sizeof(data.rx.info), 15000);
	ret = handle_error(ret, data.rx.head.result);
	if (ret != EC_RES_SUCCESS)
		return ret;

#ifdef EXTRA_DEBUG
	CPRINTF("Cap-design: %d mAh\n", data.rx.info.design_capacity);
	CPRINTF("V-design:   %d mV\n", data.rx.info.design_voltage);
	CPRINTF("Manuf:      %s\n", data.rx.info.manufacturer);
	CPRINTF("Model:      %s\n", data.rx.info.model);
	CPRINTF("Serial:     %s\n", data.rx.info.serial);
	CPRINTF("Type:       %s\n", data.rx.info.type);
	CPRINTF("C-count:    %d\n", data.rx.info.cycle_count);
#endif

	memcpy(&base_battery_static, &data.rx.info,
				sizeof(base_battery_static));
	return EC_RES_SUCCESS;
}

int ec_ec_master_base_charge_control(int max_current,
				     int otg_voltage,
				     int allow_charging)
{
	int ret;
	struct {
		struct {
			struct ec_host_request4 head;
			struct ec_params_charger_control ctrl;
			uint8_t crc8;
		} tx;
		struct {
			struct ec_host_response4 head;
		} rx;
	} __packed data;

	data.tx.ctrl.allow_charging = allow_charging;
	data.tx.ctrl.max_current = max_current;
	data.tx.ctrl.otg_voltage = otg_voltage;

	ret = write_command(EC_CMD_CHARGER_CONTROL,
		(void *)&data, sizeof(data.tx.ctrl), 0,	30000);

	return handle_error(ret, data.rx.head.result);
}
#endif /* CONFIG_EC_EC_COMM_BATTERY */
