/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Novatek NT50358A DC to DC converter register map.
 */

#ifndef __CROS_EC_PMU_NT50358A_H

/* I2C interface */
#define NT50358A_I2C_ADDR		(0x3E << 1)

/* NT50358A registers */
#define NT50358A_REG_AVDD		0x00
#define NT50358A_REG_AVEE		0x01
#define NT50358A_REG_CTRL		0x03
#define NT50358A_REG_MTP_WRITE		0xFF

/* Masks */
#define NT50358A_AVDD_MASK		0x1F

#define NT50358A_AVEE_MASK		0x1F

#define NT50358A_CTRL_DISN		BIT(0)
#define NT50358A_CTRL_DISP		BIT(1)
#define NT50358A_CTRL_FTS_SHIFT		4
#define NT50358A_CTRL_FTS_MASK		(0x11 << NT50358A_CTRL_FTS_SHIFT)

#define NT50358A_MTP_WRITE_WED		BIT(7)

#endif	/* __CROS_EC_PMU_NT50358A_H */

