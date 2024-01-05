/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 *
 * @brief Backend API for Realtek RTS5453P Type-C Power Delivery Controller
 * emulator
 */

#ifndef __EMUL_REALTEK_RTS5453P_H
#define __EMUL_REALTEK_RTS5453P_H

#include "emul/emul_common_i2c.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>

struct rts54_command {
	uint8_t command_code;
	uint8_t byte_count;
	uint8_t data[32];
};

struct rts54_ic_status {
	uint8_t byte_count;
	uint8_t is_flash_code;
	uint8_t reserved0[2];
	uint8_t fw_main_version;
	uint8_t fw_sub_version[2];
	uint8_t reserved1[2];
	uint8_t pd_ready : 1;
	uint8_t reserved2 : 2;
	uint8_t typec_connected : 1;
	uint8_t reserved3 : 4;
	uint8_t vid[2];
	uint8_t pid[2];
	uint8_t reserved4;
	uint8_t running_flash_bank_offset;
	uint8_t reserved5[7];
	uint8_t pd_revision[2];
	uint8_t pd_version[2];
	uint8_t reserved6[6];
};

enum cmd_sts_t {
	/** Command has not been started */
	CMD_BUSY = 0,
	/** Command has completed */
	CMD_COMPLETE = 1,
	/** Command has been started but has not completed */
	CMD_DEFERRED = 2,
	/** Command completed with error. Send GET_ERROR_STATUS for details */
	CMD_ERROR = 3,
};

struct ping_status {
	/** Command status */
	uint8_t cmd_sts : 2;
	/** Length of data read to read */
	uint8_t data_len : 6;
};

/** @brief Emulated properties */
struct rts5453p_emul_pdc_data {
	uint8_t vnd_command;
	struct rts54_ic_status ic_status;
	struct rts54_command request;

	bool read_ping;
	union {
		struct ping_status ping_status;
		uint8_t ping_raw_value;
	};
	uint8_t response[32];
};

/**
 * @brief Returns pointer to i2c_common_emul_data for argument emul
 *
 * @param emul Pointer to rts5453p emulator
 * @return Pointer to i2c_common_emul_data from argument emul
 */
struct i2c_common_emul_data *
rts5453p_emul_get_i2c_common_data(const struct emul *emul);

#endif /* __EMUL_REALTEK_RTS5453P_H */
