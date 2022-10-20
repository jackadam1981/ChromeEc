/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "button.h"
#include "charge_ramp.h"
#include "charger.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "driver/als_tcs3400.h"
#include "fw_config.h"
#include "hooks.h"
#include "lid_switch.h"
#include "panic.h"
#include "power_button.h"
#include "power.h"
#include "power/alderlake_slg4bd44540.h"
#include "power/intel_x86.h"
#include "registers.h"
#include "switch.h"
#include "throttle_ap.h"
#include "usbc_config.h"

#include "gpio_list.h" /* Must come after other header files. */

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ##args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

struct power_signal_info power_signal_list[] = {
	[X86_SLP_S0_DEASSERTED] = {
		.gpio = GPIO_PCH_SLP_S0_L,
		.flags = POWER_SIGNAL_ACTIVE_HIGH |
			POWER_SIGNAL_DISABLE_AT_BOOT,
		.name = "SLP_S0_DEASSERTED",
	},
	[X86_SLP_S3_DEASSERTED] = {
		.gpio = SLP_S3_SIGNAL_L,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_S3_DEASSERTED",
	},
	[X86_SLP_S4_DEASSERTED] = {
		.gpio = (enum gpio_signal)SLP_S4_SIGNAL_L,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_S4_DEASSERTED",
	},
	[X86_SLP_S5_DEASSERTED] = {
		.gpio = (enum gpio_signal)SLP_S5_SIGNAL_L,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_S5_DEASSERTED",
	},
	[X86_SLP_SUS_DEASSERTED] = {
		.gpio = GPIO_SLP_SUS_L,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_SUS_DEASSERTED",
	},
	[X86_RSMRST_L_PGOOD] = {
		.gpio = GPIO_PG_EC_RSMRST_ODL,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "RSMRST_L_PGOOD",
	},
	[X86_DSW_DPWROK] = {
		.gpio = GPIO_PG_EC_DSW_PWROK,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "DSW_DPWROK",
	},
	[X86_ALL_SYS_PGOOD] = {
		.gpio = GPIO_SEQ_EC_ALL_SYS_PG_OLD,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "ALL_SYS_PWRGD",
	},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

static void board_chipset_startup(void)
{
	/* Allow keyboard backlight to be enabled */

	gpio_set_level(GPIO_EC_KB_BL_EN, 1);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, board_chipset_startup, HOOK_PRIO_DEFAULT);

static void board_chipset_shutdown(void)
{
	/* Turn off the keyboard backlight if it's on. */

	gpio_set_level(GPIO_EC_KB_BL_EN, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, board_chipset_shutdown, HOOK_PRIO_DEFAULT);

static void set_board_id_4_gpios(void)
{
	if (get_board_id() > 3) {
		power_signal_list[X86_ALL_SYS_PGOOD].gpio = GPIO_SEQ_EC_ALL_SYS_PG_NEW;
	}
}
DECLARE_HOOK(HOOK_INIT, set_board_id_4_gpios, HOOK_PRIO_FIRST);

__override int intel_x86_get_pg_ec_all_sys_pwrgd(void)
{
	if (get_board_id() > 3)
		return gpio_get_level(GPIO_SEQ_EC_ALL_SYS_PG_NEW);
	else
		return gpio_get_level(GPIO_SEQ_EC_ALL_SYS_PG_OLD);
}
