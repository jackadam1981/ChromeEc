/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef UM_PPM_TPS6699X_H_
#define UM_PPM_TPS6699X_H_

#include "include/pd_driver.h"
#include "include/smbus.h"

#define TI_DEFAULT_PORT 1

/* Forward declaration. */
struct tps6699x_device;

/* SMBUS supports only 32 bytes in reads. Need to implement the custom I2C
 * handling for byte reads beyond that.
 */

/* #define I2C_READ_BEYOND_32BYTES */

struct tps6699x_boot_flags {
	/* Bit 0 */
	uint8_t boot_stage;
#define BOOT_STAGE_MASK(b) ((b) & 0xf)
	uint8_t reserved_1[7];

	/* Bit 64 */
	uint8_t port_info;
#define NUM_PORTS_MASK(p) ((p) & 0x3)
	uint8_t reserved_2[7];

	/* Bit 128 */
	uint8_t batt_sink_flags;
	uint8_t reserved_3[3];

	/* Bit 160 */
	uint8_t i2c1_port_a_address;
	uint8_t i2c1_port_b_address;
	uint8_t i2c2_port_a_address;
	uint8_t i2c2_port_b_address;
	uint8_t i2c4_port_a_address;
	uint8_t i2c4_port_b_address;
	uint8_t reserved_4[2];

	/* Bit 224 */
	uint8_t bank_info;
#define ACTIVE_BANK_MASK(b) ((b) & 0x3)
#define BANK0_VALID(b) (((b) & (1 << 2)) >> 2)
#define BANK1_VALID(b) (((b) & (1 << 3)) >> 3)

	uint8_t reserved_5[3];

	/* Bit 256 */
	uint32_t fw_version_bank0;
	uint32_t fw_version_bank1;

	/* Bit 320 */
	uint16_t adc_in_value;
	uint16_t adc_in_index;
	uint8_t reserved_6[8];
} __attribute__((__packed__));

struct tps6699x_device_info {
	char data[40];
};

#define GAID_SWITCH_BANK 0xAC
#define GAID_COPY_BANK 0xAC

struct tps6699x_gaid_input {
	uint8_t switch_banks;
	uint8_t copy_banks;
} __attribute__((__packed__));

/* List of 4CC tasks supported by TPS6699x.
 *
 * These need to be passed to |tps6699x_4cc_run_task| in order to run.
 */
enum tps6699x_4cc_tasks {
	/* Control tasks */
	TPSCMD_GAID, /* Cold reset */

	/* Firmware Update Tasks */
	TPSCMD_TFUs, /* Enter TFU Mode. */
	TPSCMD_TFUc, /* Complete Phase. */
	TPSCMD_TFUd, /* Data Phase. */
	TPSCMD_TFUe, /* Exit. */
	TPSCMD_TFUi, /* Initiate update. */
	TPSCMD_TFUq, /* Query status. */

	TPSCMD_UCSI, /* All UCSI commands. */

	/* For counting only (not a valid command). */
	TPSCMD_MAX_COUNT,
};

int tps6699x_get_boot_flags(struct tps6699x_device *dev,
			    struct tps6699x_boot_flags *flags);

int tps6699x_get_version(struct tps6699x_device *dev, uint32_t *version_out);

int tps6699x_get_device_info(struct tps6699x_device *dev,
			     struct tps6699x_device_info *device_info);

/*
 * Run 4CC Tasks.
 *
 * Given task, will set the CMD and DATA registers correctly to execute task and
 * read any output from it. If |no_validation| is set, this will simply write
 * the task to CMD and return if the write is successful.
 *
 */
int tps6699x_4cc_run_task(struct tps6699x_device *dev, uint8_t port,
			  uint8_t task, uint8_t *data_in, size_t data_in_length,
			  uint8_t *data_out, size_t data_out_length,
			  bool no_validation);

/* Use I2C burst to stream buffer to the broadcast address. */
int tps6699x_broadcast_stream(struct tps6699x_device *dev,
			      uint8_t broadcast_address,
			      void* buf,
			      size_t length);

/* Reset the PDC. */
int tps6699x_reset_pdc(struct ucsi_pd_driver* pd);

/* Establish connection and get basic info about the PD controller. */
int tps6699x_get_info(struct ucsi_pd_driver *pd);

/* Firmware update for the PD controller. */
int tps6699x_do_firmware_update(struct ucsi_pd_driver *pd, const char *filepath,
				int dry_run);

/**
 * Open TPS6699x device using SMBUS driver.
 *
 * @param smbus_driver: Already open smbus connection.
 * @param config: Configuration for this driver.
 */
struct ucsi_pd_driver *tps6699x_open(struct smbus_driver *smbus,
				     struct pd_driver_config *config);

/*
 * Get the driver configuration for the TPS6699x driver.
 */
struct pd_driver_config tps6699x_get_driver_config();

#endif /* UM_PPM_TPS6699X_H_ */
