/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef MTLRVP_MCHP_BOARD_H_
#define MTLRVP_MCHP_BOARD_H_

#include <dt-bindings/gpio/microchip-xec-gpio.h>

/* Power Signals */
#define PWR_EN_PP3300_S5 MCHP_GPIO_DECODE_025
#define PWR_RSMRST_PWRGD MCHP_GPIO_DECODE_011
#define PWR_EC_PCH_RSMRST MCHP_GPIO_DECODE_054
#define PWR_SLP_S0 MCHP_GPIO_DECODE_002
#define PWR_PCH_PWROK MCHP_GPIO_DECODE_106
#define PWR_EC_PCH_SYS_PWROK MCHP_GPIO_DECODE_202
#define PWR_SYS_RST MCHP_GPIO_DECODE_165
#define PWR_ALL_SYS_PWRGD MCHP_GPIO_DECODE_057

#endif /* MTLRVP_MCHP_BOARD_H_ */