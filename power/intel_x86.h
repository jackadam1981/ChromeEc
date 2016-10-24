/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel X86 chipset power control module for Chrome EC */


#ifndef __CROS_EC_INTEL_X86_H
#define __CROS_EC_INTEL_X86_H

#include "power.h"

#ifdef CONFIG_CHIPSET_APOLLOLAKE
/* Input state flags */
#define IN_RSMRST_N	POWER_SIGNAL_MASK(X86_RSMRST_N)
#define IN_ALL_SYS_PG	POWER_SIGNAL_MASK(X86_ALL_SYS_PG)
#define IN_SLP_S3_N	POWER_SIGNAL_MASK(X86_SLP_S3_N)
#define IN_SLP_S4_N	POWER_SIGNAL_MASK(X86_SLP_S4_N)
#define IN_SUSPWRDNACK	POWER_SIGNAL_MASK(X86_SUSPWRDNACK)
#define IN_SUS_STAT_N	POWER_SIGNAL_MASK(X86_SUS_STAT_N)

#define IN_ALL_PM_SLP_DEASSERTED (IN_SLP_S3_N | \
				  IN_SLP_S4_N)

#define IN_PGOOD_ALL_CORE (IN_RSMRST_N)
#define IN_ALL_S0 (IN_PGOOD_ALL_CORE | IN_ALL_PM_SLP_DEASSERTED)
#endif /* CONFIG_CHIPSET_APOLLOLAKE */

#ifdef CONFIG_CHIPSET_SKYLAKE
/* Input state flags */
#define IN_PCH_SLP_S3_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S3_DEASSERTED)
#define IN_PCH_SLP_S4_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S4_DEASSERTED)
#define IN_PCH_SLP_SUS_DEASSERTED POWER_SIGNAL_MASK(X86_SLP_SUS_DEASSERTED)

#define IN_ALL_PM_SLP_DEASSERTED (IN_PCH_SLP_S3_DEASSERTED | \
				  IN_PCH_SLP_S4_DEASSERTED | \
				  IN_PCH_SLP_SUS_DEASSERTED)

/*
 * DPWROK is NC / stuffing option on initial boards.
 * TODO(shawnn): Figure out proper control signals.
 */
#define IN_PGOOD_ALL_CORE 0
#define IN_ALL_S0 (IN_PGOOD_ALL_CORE | IN_ALL_PM_SLP_DEASSERTED)
#endif /* CONFIG_CHIPSET_SKYLAKE */

extern int forcing_shutdown;  /* Forced shutdown in progress? */
extern int forcing_coldreset; /* Forced coldreset in progress? */
extern int power_s5_up;       /* Chipset is sequencing up or down */

void handle_rsmrst(enum power_state state);
void chipset_force_g3(void);
enum power_state _power_handle_state(enum power_state state);

#endif /* __CROS_EC_INTEL_X86_H */
