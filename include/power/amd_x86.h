/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* AMD X86 chipset power control module for Chrome EC */

#ifndef __CROS_EC_AMD_X86_H_
#define __CROS_EC_AMD_X86_H_

#define G3S5_SHUTDOWN_REASON		CHIPSET_SHUTDOWN_G3
#define IN_ALL_S0			GPIO_S0_PGOOD
#define CHIPSET_G3S5_POWERUP_SIGNAL	POWER_SIGNAL_MASK(X86_S5_PGOOD)
#define IN_PGOOD_ALL_CORE		CHIPSET_G3S5_POWERUP_SIGNAL

#define CONFIG_CHIPSET_AMD

static inline void handle_power_failure(void){}

static inline void init_prochot(void){}

#endif /* __CROS_EC_AMD_X86_H_ */
