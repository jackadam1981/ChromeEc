/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Skylake IMVP8 / ROP PMIC chipset power control module for Chrome EC */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "power.h"
#include "system.h"
#include "util.h"
#include "wireless.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

/* Input state flags */
#define IN_PCH_SLP_S0_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S0_DEASSERTED)
#define IN_PCH_SLP_S3_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S3_DEASSERTED)
#define IN_PCH_SLP_S4_DEASSERTED  POWER_SIGNAL_MASK(X86_SLP_S4_DEASSERTED)
#define IN_PCH_SLP_SUS_DEASSERTED POWER_SIGNAL_MASK(X86_SLP_SUS_DEASSERTED)

#define IN_ALL_PM_SLP_DEASSERTED (IN_PCH_SLP_S3_DEASSERTED | \
				  IN_PCH_SLP_S4_DEASSERTED | \
				  IN_PCH_SLP_SUS_DEASSERTED)

#define IN_PGOOD_ALL_CORE POWER_SIGNAL_MASK(X86_PMIC_DPWROK)

#define IN_ALL_S0 (IN_PGOOD_ALL_CORE | IN_ALL_PM_SLP_DEASSERTED)

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

void chipset_force_shutdown(void)
{
	CPRINTS("%s()", __func__);
}

void chipset_force_g3(void)
{
	CPRINTS("Forcing G3");

	gpio_set_level(GPIO_PCH_RSMRST_L, 0);
	gpio_set_level(GPIO_PP1800_DX_SENSOR_EN, 0);
	gpio_set_level(GPIO_PP1800_DX_AUDIO_EN, 0);
	gpio_set_level(GPIO_PP3300_WLAN_EN, 0);
}

void chipset_reset(int cold_reset)
{
	CPRINTS("%s(%d)", __func__, cold_reset);

	if (cold_reset) {
		if (gpio_get_level(GPIO_SYS_RESET_L) == 0)
			return;
		gpio_set_level(GPIO_SYS_RESET_L, 0);
		udelay(100);
		gpio_set_level(GPIO_SYS_RESET_L, 1);
	} else {
		/*
		 * Send a RCIN_PCH_RCIN_L
		 * assert INIT# to the CPU without dropping power or asserting
		 * PLTRST# to reset the rest of the system.
		 */

		/* Pulse must be at least 16 PCI clocks long = 500 ns */
		gpio_set_level(GPIO_PCH_RCIN_L, 0);
		udelay(10);
		gpio_set_level(GPIO_PCH_RCIN_L, 1);
	}
}

void chipset_thottle_cpu(int throttle)
{
	if (chipset_in_state(CHIPSET_STATE_ON))
		gpio_set_level(GPIO_CPU_PROCHOT, throttle);
}

enum power_state power_chipset_init(void)
{
	disable_sleep(SLEEP_MASK_AP_RUN);
	return POWER_G3;
}

struct {
	char *name;
	int mask;
} monitor_sigs[] = { { "SLP_S0", 1 << X86_SLP_S0_DEASSERTED },
		     { "SLP_S3", 1 << X86_SLP_S3_DEASSERTED },
		     { "SLP_S4", 1 << X86_SLP_S4_DEASSERTED },
		     { "SLP_SUS", 1 << X86_SLP_SUS_DEASSERTED }, };

enum power_state power_handle_state(enum power_state state)
{
	static uint32_t old_signals;
	static int initialized;
	uint32_t new_signals = power_get_signals();
	int i;

	/* Print any monitor signal changes */
	for (i = 0; i < ARRAY_SIZE(monitor_sigs); ++i)
		if (!initialized ||
		   ((new_signals & monitor_sigs[i].mask) !=
		    (old_signals & monitor_sigs[i].mask)))
			CPRINTS("%s: %x",
				monitor_sigs[i].name,
				!!(new_signals & monitor_sigs[i].mask));
	old_signals = new_signals;
	initialized = 1;

	/* Pretend to stay in S5, mirror RSMRST input to PCH */
	gpio_set_level(GPIO_PCH_RSMRST_L, gpio_get_level(GPIO_RSMRST_L_PGOOD));
	return POWER_S5;
}
