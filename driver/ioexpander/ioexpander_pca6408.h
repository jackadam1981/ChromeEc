/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * NXP PCA(L)6408 I/O expander
 */

#ifndef __CROS_EC_IOEXPANDER_PCA6408_H
#define __CROS_EC_IOEXPANDER_PCA6408_H

#define PCA6408_REG_INPUT		0x00
#define PCA6408_REG_OUTPUT		0x01
#define PCA6408_REG_POLARITY_INVERSION	0x02
#define PCA6408_REG_CONFIG		0x03
#define PCA6408_REG_OUT_STRENGTH0 	0x40
#define PCA6408_REG_OUT_STRENGTH1	0x41
#define PCA6408_REG_INPUT_LATCH		0x42
#define PCA6408_REG_PULL_ENABLE		0x43
#define PCA6408_REG_PULL_UP_DOWN	0x44
#define PCA6408_REG_INT_MASK		0x45
#define PCA6408_REG_INT_STATUS		0x46
#define PCA6408_REG_OUT_CONFIG		0x4f

#define PCA6408_VALID_GPIO_MASK		0xff

#define PCA6408_OUTPUT			0
#define PCA6408_INPUT			1

#define PCA6408_OUT_CONFIG_OPEN_DRAIN	0x01

#endif  /* __CROS_EC_IOEXPANDER_PCA6408_H */
