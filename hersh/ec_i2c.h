/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_I2C_H
#define __CROS_EC_I2C_H

// TODO: set these properly
#define TASKLIST_PRIORITY 0 // set in board/*/ec.tasklist

#define STATUS_REG		0x51
#define CONTROL_REG		0x52
#define VENDOR_ID_REG 		0x53
#define FIRMWARE_VERSION_REG 	0x54


/*
 * Spawn a thread hook the _____ TODO ____ function from ec_i2c.c into the 
 * location specified by the `TASKLIST_PRIORITY` macro.
 */
int init_ap2ec_i2c_thread(void);


#endif /* __CROS_EC_I3C_H */