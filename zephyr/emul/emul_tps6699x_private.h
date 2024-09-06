/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EMUL_TPS6699X_PRIVATE_H_
#define __EMUL_TPS6699X_PRIVATE_H_

#include "drivers/ucsi_v3.h"
#include "emul/emul_tps6699x.h"

#include <stdint.h>

#include <zephyr/drivers/gpio.h>

enum tps6699x_reg_offset {
	/* TODO(b/345292002): Fill out */
	TPS6699X_REG_MODE = 0x03,
	TPS6699X_REG_CUSTOMER_USE = 0x06,
	TPS6699X_REG_COMMAND_I2C1 = 0x8,
	TPS6699X_REG_DATA_I2C1 = 0x9,
	TPS6699X_REG_VERSION = 0x0f,
	TPS6699X_REG_INTERRUPT_EVENT_FOR_I2C1 = 0x14,
	TPS6699X_REG_POWER_PATH_STATUS = 0x26,
	TPS6699X_REG_PORT_CONFIGURATION = 0x28,
	TPS6699X_REG_PORT_CONTROL = 0x29,
	TPS6699X_REG_TX_IDENTITY = 0x47,
	TPS6699X_REG_RECEIVED_SOP_IDENTITY_DATA_OBJECT = 0x48,
	TPS6699X_REG_RECEIVED_SOP_PRIME_IDENTITY_DATA_OBJECT = 0x49,
	TPS6699X_REG_ADC_RESULTS = 0x6a,
	TPS6699X_NUM_REG = 0xa4,
};

/* Results of a task, indicated by the PDC in byte 1 of the relevant DATAX
 * register after a command completes. See TPS6699x TRM May 2023, table 10-1
 * Standard Task Response.
 */
enum tps6699x_command_result {
	COMMAND_RESULT_SUCCESS = 0,
	COMMAND_RESULT_TIMEOUT = 1,
	COMMAND_RESULT_REJECTED = 2,
	COMMAND_RESULT_RX_LOCKED = 4,
};

#endif /* __EMUL_TPS6699X_PRIVATE_H_ */
