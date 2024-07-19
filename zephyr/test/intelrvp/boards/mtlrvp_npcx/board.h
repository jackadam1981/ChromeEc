/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef MTLRVP_NPCX_BOARD_H_
#define MTLRVP_NPCX_BOARD_H_

/* Power Signals */
#define PWR_EN_PP3300_S5 &gpioc 4
#define PWR_RSMRST_PWRGD &gpio6 6
#define PWR_EC_PCH_RSMRST &gpioa 4
#define PWR_SLP_S0 &gpioa 1
#define PWR_PCH_PWROK &gpiod 3
#define PWR_EC_PCH_SYS_PWROK &gpiof 5
#define PWR_SYS_RST &gpioc 5
#define PWR_ALL_SYS_PWRGD &gpio7 0
#define STD_ADP_PRSNT &gpioc 6

/* USB-C signals */
#define GPIO_CCD_MODE_ODL &gpio9 2
#define GPIO_USBC_TCPC_ALRT_P0 &gpio4 0
#define GPIO_USB_C0_C1_TCPC_RST_ODL &gpiod 0
#define GPIO_USBC_TCPC_PPC_ALRT_P0 &gpiod 1
#define GPIO_USBC_TCPC_PPC_ALRT_P1 &gpioe 4
#define GPIO_USBC_TCPC_ALRT_P2 &gpio9 1
#define GPIO_USBC_TCPC_ALRT_P3 &gpiof 3

#define I2C_TYPEC_AIC1 i2c0_0
#define I2C_TYPEC_AIC2 i2c1_0

#endif /* MTLRVP_NPCX_BOARD_H_ */