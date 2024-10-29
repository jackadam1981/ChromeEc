/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PTLRVP_ITE_BOARD_H_
#define PTLRVP_ITE_BOARD_H_

/* Power Signals */
#define PWR_EN_PP3300_S5 &gpioi 7
#define PWR_EN_PP5000_S5 &gpiog 1
#define PWR_RSMRST_PWRGD &gpiof 0
#define PWR_EC_PCH_RSMRST &gpiob 2
#define PWR_SLP_S0 &gpioj 5
#define PWR_PCH_PWROK &gpioj 1
#define PWR_ALL_SYS_PWRGD &gpiod 5

/* I2C Ports */
#define CHARGER_I2C i2c4

#define PD_POW_I2C i2c1

/* PD Interrupts */
#define PD_POW_IRQ_GPIO &gpiof 5

#endif /* PTLRVP_ITE_BOARD_H_ */
