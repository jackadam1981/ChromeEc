/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __UTIL_ITECOMMON_H
#define __UTIL_ITECOMMON_H

/* I2C communication primitives */
int i2c_write_byte(void *ctxt, uint8_t cmd, uint8_t data);
int i2c_read_byte(void *ctxt, uint8_t cmd, uint8_t *data);

/* read and validate CHIP ID */
int check_chipid(void *ctxt);

/* in-system debugging */
int debug_mode(void *ctxt, int interactive, char *cmdline);

#endif /* __UTIL_ITECOMMON_H */
