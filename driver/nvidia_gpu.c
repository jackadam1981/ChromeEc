/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nvidia_gpu.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "charge_manager.h"
#include "usb_common.h"
#include "timer.h"

#include <stddef.h>

#define CPRINTS(format, args...) cprints(CC_GPIO, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_GPIO, format, ## args)

#define MEMMAP_D_NOTIFY_MASK	0x7
#define MEMMAP_SHUTDOWN		0x8

static enum d_notify_level d_notify_level = D_NOTIFY_1;
static bool policy_initialized = false;
static const struct d_notify_policy *d_notify_policy = NULL;

void nvidia_gpu_init_policy(const struct d_notify_policy *policy)
{
	if (policy) {
		d_notify_policy = policy;
		policy_initialized = true;
	}
}

static void set_d_notify_level(enum d_notify_level level)
{
	uint8_t *memmap_gpu = (uint8_t *)host_get_memmap(EC_MEMMAP_GPU);
	
	if (level == d_notify_level)
		return;

	d_notify_level = level;
	*memmap_gpu = (*memmap_gpu & ~MEMMAP_D_NOTIFY_MASK) | d_notify_level;
	host_set_single_event(EC_HOST_EVENT_GPU);
	CPRINTS("NVIDIA GPU: Set new D-notify level to D%c", ('1' + (int)d_notify_level));
}

static void reevaluate_d_notify_level(void)
{
	enum d_notify_level level;
	const bool on_ac = extpower_is_present();
	const int charger_watts = on_ac ?
		charge_manager_get_power_limit_uw() : 0;
	const int battery_soc = usb_get_battery_soc();
	const struct d_notify_policy *policy = d_notify_policy;
	
	if (!policy_initialized)
		return;

	if (on_ac) {
		for (level = D_NOTIFY_1; level <= D_NOTIFY_5; level++) {
			if (policy[level].power_source != D_NOTIFY_AC &&
			    policy[level].power_source != D_NOTIFY_AC_DC)
				continue;

			if (policy[level].power_source == D_NOTIFY_AC) {
				if (charger_watts >= policy[level].ac.min_charger_watts) {
					set_d_notify_level(level);
					break;
				}
			} else {
				set_d_notify_level(level);
				break;
			}
		}
	} else {
		for (level = D_NOTIFY_5; level >= D_NOTIFY_1; level--) {
			if (policy[level].power_source == D_NOTIFY_DC) {
				if (battery_soc <= policy[level].dc.min_battery_soc) {
					set_d_notify_level(level);
					break;
				}
			} else if (policy[level].power_source == D_NOTIFY_AC_DC) {
				set_d_notify_level(level);
				break;
			}
		
		}
	}
}

static void disable_gpu_acoff(void)
{
	gpio_set_level(GPIO_NVIDIA_GPU_ACOFF_ODL, 1);
	reevaluate_d_notify_level();
}
DECLARE_DEFERRED(disable_gpu_acoff);

static void handle_battery_soc_change(void)
{
	reevaluate_d_notify_level();
}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, handle_battery_soc_change,
	     HOOK_PRIO_DEFAULT);

static void handle_ac_change(void)
{
	/* Loss of A/C */
	if (!extpower_is_present()) {
		gpio_set_level(GPIO_NVIDIA_GPU_ACOFF_ODL, 0);
		set_d_notify_level(D_NOTIFY_5);
		hook_call_deferred(&disable_gpu_acoff_data, 100 * MSEC);
	}
}
DECLARE_HOOK(HOOK_AC_CHANGE, handle_ac_change, HOOK_PRIO_DEFAULT);
