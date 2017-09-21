/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Cannonlake chipset power control module for Chrome EC */

#ifndef __CROS_EC_CANNONLAKE_H
#define __CROS_EC_CANNONLAKE_H

/**
 * Enable/Disable the PP5000 rail.
 *
 * This function will turn on the 5V rail immediately if requested.  However,
 * the rail will not turn off until all tasks want it off.
 *
 * @param enable: 1 to turn on the rail, 0 to request the rail to be turned off.
 */
void cnl_5v_enable(int enable);

/* Input state flags. */
#define IN_PCH_SLP_S3_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S3_DEASSERTED)
#define IN_PCH_SLP_S4_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S4_DEASSERTED)
#define IN_PCH_SLP_SUS_DEASSERTED POWER_SIGNAL_MASK(X86_SLP_SUS_DEASSERTED)

#define IN_ALL_PM_SLP_DEASSERTED (IN_PCH_SLP_S3_DEASSERTED | \
				  IN_PCH_SLP_S4_DEASSERTED | \
				  IN_PCH_SLP_SUS_DEASSERTED)

#define IN_PGOOD_ALL_CORE POWER_SIGNAL_MASK(X86_PMIC_DPWROK)

#define IN_ALL_S0 (IN_PGOOD_ALL_CORE | IN_ALL_PM_SLP_DEASSERTED)

#define CHIPSET_G3S5_POWERUP_SIGNAL IN_PCH_SLP_SUS_DEASSERTED

#define CHARGER_INITIALIZED_DELAY_MS 100
#define CHARGER_INITIALIZED_TRIES 40

#endif /* __CROS_EC_CANNONLAKE_H */
