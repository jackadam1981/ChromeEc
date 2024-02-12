/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef UM_PPM_TPS6699X_H_
#define UM_PPM_TPS6699X_H_

#include "include/pd_driver.h"
#include "include/smbus.h"

#define TI_DEFAULT_PORT 0

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

int tps6699x_get_boot_flags(struct tps6699x_device *dev,
			    struct tps6699x_boot_flags *flags);

int tps6699x_get_version(struct tps6699x_device *dev, uint32_t *version_out);

int tps6699x_get_device_info(struct tps6699x_device *dev,
			     struct tps6699x_device_info *device_info);

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
