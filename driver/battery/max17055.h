/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery driver for MAX17055.
 */

#ifndef __CROS_EC_MAX17055_H
#define __CROS_EC_MAX17055_H

#define MAX17055_ADDR               0x6c
#define MAX17055_DEVICE_ID          0x4010

#define REG_STATUS                  0x00
#define REG_AT_RATE                 0x04
#define REG_REMAINING_CAPACITY      0x05
#define REG_STATE_OF_CHARGE         0x06
#define REG_TEMPERATURE             0x08
#define REG_VOLTAGE                 0x09
#define REG_CURRENT                 0x0a
#define REG_AVERAGE_CURRENT         0x0b
#define REG_FULL_CHARGE_CAPACITY    0x10
#define REG_TIME_TO_EMPTY           0x11
#define REG_QR_TABLE00              0x12
#define REG_CONFIG                  0x1D
#define REG_AVERAGE_TEMPERATURE     0x16
#define REG_CYCLE_COUNT             0x17
#define REG_DESIGN_CAPACITY         0x18
#define REG_AVERAGE_VOLTAGE         0x19
#define REG_CHARGE_TERM_CURRENT     0x1e
#define REG_TIME_TO_FULL            0x20
#define REG_DEVICE_NAME             0x21
#define REG_QR_TABLE10              0x22
#define REG_LEARNCFG                0x28
#define REG_QR_TABLE20              0x32
#define REG_RCOMP0                  0x38
#define REG_TEMPCO                  0x39
#define REG_EMPTY_VOLTAGE           0x3a
#define REG_FSTAT                   0x3d
#define REG_QR_TABLE30              0x42
#define REG_DQACC                   0x45
#define REG_DPACC                   0x46
#define REG_STATUS2                 0xb0
#define REG_HIBCFG                  0xba
#define REG_CONFIG2                 0xbb
#define REG_MODELCFG                0xdb

/* Status reg (0x00) flags */
#define STATUS_POR                  0x0002
#define STATUS_BST                  0x0008

/* Config reg (0x1d) flags */
#define CONF_TSEL                   0x8000

/* FStat reg (0x3d) flags */
#define FSTAT_DNR                   0x0001

/* ModelCfg reg (0xdb) flags */
#define MODELCFG_REFRESH            0x8000
#define MODELCFG_VCHG               0x0400

/*
 * max17055 needs some special battery parameters for fuel gauge
 * learning algorithm. Maxim can help characterize the battery pack
 * to get a full parameter list.
 */
struct max17055_batt_params {
/*
 * If is_ez_config is nonzero, we only use design_cap, ichg_term, and v_empty
 * to config max17055 (a.k.a. EZ-config).
 */
	uint8_t is_ez_config;
	uint16_t design_cap;
	uint16_t ichg_term;
	uint16_t v_empty;

/* The parameters below are used for advanced (non-EZ) config. */
	uint16_t learn_cfg;
	uint16_t dpacc;
	uint16_t rcomp0;
	uint16_t tempco;
	uint16_t qr_table00;
	uint16_t qr_table10;
	uint16_t qr_table20;
	uint16_t qr_table30;
};

/* Return the special battery parameters max17055 needs. */
const struct max17055_batt_params *max17055_get_batt_params(void);

#endif /* __CROS_EC_MAX17055_H */
