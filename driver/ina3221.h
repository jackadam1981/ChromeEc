/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI INA3221 Current/Power monitor driver.
 */

#ifndef __CROS_EC_INA3221_H
#define __CROS_EC_INA3221_H

#define INA3221_REG_CONFIG	0x00
#define INA3221_REG_MASK	0x0F

/* Bus voltage: mV per LSB */
#define INA3221_BUS_MV(reg) ((reg) / 2)
/* Shunt voltage: uV per LSB */
#define INA3221_SHUNT_UV(reg) ((reg) * 2)

enum ina3221_channel {
	INA3221_CHAN_1 = 0,
	INA3221_CHAN_2 = 1,
	INA3221_CHAN_3 = 2,
	INA3221_CHAN_COUNT = 3
};

/* Registers for each channel */
enum ina3221_register {
	INA3221_SHUNT_VOLT = 0,
	INA3221_BUS_VOLT = 1,
	INA3221_CRITICAL = 2,
	INA3221_WARNING = 3,
	INA3221_MAX_REG = 4
};

/* Configuration table - defined in board file. */
struct ina3221_t {
	int port;             /* I2C port index */
	uint8_t address;      /* I2C address */
	const char *name[INA3221_CHAN_COUNT];  /* Channel names */
};

#if defined(CONFIG_INA3221) && \
	(defined(CONFIG_INA231) || defined(CONFIG_INA219))
#error "CONFIG_INA3221 must not be defined with either" \
	" CONFIG_INA231 or CONFIG_INA219"
#endif


/* Read INA3221 register. */
uint16_t ina3221_read(int unit, uint8_t reg);

/* Read INA3221 Channel register. */
uint16_t ina3221_chan_read(int unit, enum ina3221_channel chan,
			   enum ina3221_register reg);

/* Write INA3221 register. */
int ina3221_write(int unit, uint8_t reg, uint16_t val);

/* Set measurement parameters */
int ina3221_init(int unit, uint16_t config);

/* External config in board file */
extern const struct ina3221_t ina3221[];
extern const unsigned int ina3221_count;

#endif /* __CROS_EC_INA3221_H */
