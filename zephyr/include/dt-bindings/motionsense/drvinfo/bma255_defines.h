/*
 * Copyright 2021 Google LLC.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef DT_BINDINGS_BMA255_DEFINES_H
#define DT_BINDINGS_BMA255_DEFINES_H

/* The following definitions are from accel_bma2x2_public.h */

/* I2C ADDRESS DEFINITIONS    */
/* The following definition of I2C address is used for the following sensors
* BMA253
* BMA255
* BMA355
* BMA280
* BMA282
* BMA223
* BMA254
* BMA284
* BMA250E
* BMA222E
*/

#define BMA2x2_I2C_ADDR1_FLAGS			0x18
#define BMA2x2_I2C_ADDR2_FLAGS			0x19

/* The following definition of I2C address is used for the following sensors
* BMC150
* BMC056
* BMC156
*/
#define BMA2x2_I2C_ADDR3_FLAGS			0x10
#define BMA2x2_I2C_ADDR4_FLAGS			0x11

#endif /* DT_BINDINGS_BMA255_DEFINES_H */
