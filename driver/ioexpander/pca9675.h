/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * NXP PCA9675PW I/O Port expander driver header
 */

#ifndef __CROS_EC_IOEXPANDER_PCA9675_H
#define __CROS_EC_IOEXPANDER_PCA9675_H

#define PCA9675_IO_P00	BIT(0)
#define PCA9675_IO_P01	BIT(1)
#define PCA9675_IO_P02	BIT(2)
#define PCA9675_IO_P03	BIT(3)
#define PCA9675_IO_P04	BIT(4)
#define PCA9675_IO_P05	BIT(5)
#define PCA9675_IO_P06	BIT(6)
#define PCA9675_IO_P07	BIT(7)

#define PCA9675_IO_P10	BIT(8)
#define PCA9675_IO_P11	BIT(9)
#define PCA9675_IO_P12	BIT(10)
#define PCA9675_IO_P13	BIT(11)
#define PCA9675_IO_P14	BIT(12)
#define PCA9675_IO_P15	BIT(13)
#define PCA9675_IO_P16	BIT(14)
#define PCA9675_IO_P17	BIT(15)

/* Sent 06 to address 00 to reset the PCA9675 to back to power up state */
#define PCA9675_RESET_SEQ_DATA 0x06

/* Default I/O directons of PCA9675 is input */
#define PCA9675_DEFAULT_IO_DIRECTION 0xffff

extern const struct ioexpander_drv pca9675_ioexpander_drv;

#endif /* __CROS_EC_IOEXPANDER_PCA9675_H */
