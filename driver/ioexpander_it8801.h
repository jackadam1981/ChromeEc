/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * IT8801 is an I/O expander with the keyboard matrix controller.
 *
 */

#ifndef __CROS_EC_IO_EXPANDER_IT8801_H
#define __CROS_EC_IO_EXPANDER_IT8801_H

/* I2C slave address(8-bit) */
#define IT8801_REG_ADDR 0x70

/* Keyboard Matrix Scan control (KBS) */
#define IT8801_REG_KSOMCR               0x40
#define IT8801_REG_MASK_KSOSDIC         BIT(7)
#define IT8801_REG_MASK_KSE             BIT(6)
#define IT8801_REG_MASK_AKSOSC          BIT(5)
#define IT8801_REG_KSIDR                0x41
#define IT8801_REG_KSIEER               0x42
#define IT8801_REG_KSIIER               0x43
#define IT8801_REG_SMBCR                0xfa
#define IT8801_REG_MASK_ARE             BIT(4)
#define IT8801_REG_GIECR                0xfb
#define IT8801_REG_MASK_GKSIIE          BIT(3)
#define IT8801_REG_GPIO00_KSO19         0x0a
#define IT8801_REG_GPIO01_KSO18         0x0b
#define IT8801_REG_GPIO22_KSO21         0x1c
#define IT8801_REG_GPIO23_KSO20         0x1d
#define IT8801_REG_MASK_GPIOAFS_FUNC2   BIT(6)
#define IT8801_REG_MASK_SELKSO2         0x02

/* ISR for IT8801's SMB_INT# */
void io_expander_it8801_interrupt(enum gpio_signal signal);

#endif /* __CROS_EC_KBEXPANDER_IT8801_H */
