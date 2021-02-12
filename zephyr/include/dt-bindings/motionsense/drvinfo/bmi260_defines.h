/*
 * Copyright 2021 Google LLC.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef DT_BINDINGS_BMI260_DEFINES_H
#define DT_BINDINGS_BMI260_DEFINES_H

/* The following definitions are from accelgyro_bmi260_public.h */

/*
 * The addr field of motion_sensor support both SPI and I2C:
 * This is defined in include/i2c.h and is no longer an 8bit
 * address. The 7/10 bit address starts at bit 0 and leaves
 * room for a 10 bit address, although we don't currently
 * have any 10 bit slaves.  I2C or SPI is indicated by a
 * more significant bit
 */

/* I2C addresses */
#define BMI260_ADDR0_FLAGS			0x68

#endif /* DT_BINDINGS_BMI260_DEFINES_H */
