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

/* Column mapping to KSO of IT8801 */
#ifdef CONFIG_KEYBOARD_KSO_IT8801
extern const uint8_t kso_mapping[];
#endif

/* General Purpose I/O Port (GPIO) */
#define IT8801_SUPPORT_GPIO_FLAGS (GPIO_OPEN_DRAIN | GPIO_INPUT | \
		GPIO_OUTPUT | GPIO_LOW | GPIO_HIGH)
#define IT8801_REG_MASK_GPIOAFS_FUNC1   (0x00 << 7)
#define IT8801_REG_GPIO_DATA_IN(n)      (0x00 + n)
#define IT8801_REG_GPIO_DATA_OUT(n)     (0x05 + n)
#define IT8801_REG_GPIOXXCR(n)          (0x0a + (n * 8))
#define IT8801_REG_GPIODIR              BIT(5)
#define IT8801_REG_GPIOOT               BIT(4)

#define IT8801_CHIP_INFO                0

/* IT8801 only supports GPIO 0/1/2 */
#define IT8801_VALID_GPIO_G0_MASK       0xDB
#define IT8801_VALID_GPIO_G1_MASK       0x3F
#define IT8801_VALID_GPIO_G2_MASK       0x0F

extern const struct ioexpander_drv it8801_ioexpander_drv;

#endif /* __CROS_EC_KBEXPANDER_IT8801_H */
