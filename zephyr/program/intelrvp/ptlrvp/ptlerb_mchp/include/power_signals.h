/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PTLERB_MCHP_POWER_SIGNALS_H_
#define PTLERB_MCHP_POWER_SIGNALS_H_

#define PWR_EN_PP3300_S5     MCHP_GPIO_DECODE_226
#define PWR_RSMRST_PWRGD     MCHP_GPIO_DECODE_012
#define PWR_EC_PCH_RSMRST    MCHP_GPIO_DECODE_054
#define PWR_SLP_S0           MCHP_GPIO_DECODE_052
#define PWR_PCH_PWROK        MCHP_GPIO_DECODE_100
#define PWR_EC_PCH_SYS_PWROK MCHP_GPIO_DECODE_015
#define PWR_ALL_SYS_PWRGD    MCHP_GPIO_DECODE_057

#endif /* PTLERB_MCHP_POWER_SIGNALS_H_ */
