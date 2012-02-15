/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TMP006 temperature sensor module for Chrome EC */

#ifndef __CROS_EC_TMP006_H
#define __CROS_EC_TMP006_H

#define TMP006_ADDR(PORT,REG) ((PORT << 16) + REG)
#define TMP006_PORT(ADDR) (ADDR >> 16)
#define TMP006_REG(ADDR) (ADDR & 0xffff)

#define TMP006_DIE ((void *)0)
#define TMP006_OBJECT ((void *)1)

struct tmp006_t {
	const char* name;
	/* I2C address formed by TMP006_ADDR macro. */
	int addr;
};

/* Poll all TMP006 sensors. Return 0 on success. */
int tmp006_poll(void);

/* Get the last polled value of a sensor. Return temperature in K. */
int tmp006_get_val(int idx, void *param);

#endif  /* __CROS_EC_TMP006_H */
