/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "amd_test.h"
#include "atomic_t.h"
#include "emul/emul_stub_device.h"
#include "power.h"
#include "power/amd_x86.h"

/*
 * Provide standard array of power signals for the module based on our DTS enum
 * names we filled in
 */
const struct power_signal_info power_signal_list[] = {
	[X86_SLP_S3_N] = {
		.gpio = GPIO_PCH_SLP_S3_L,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_S3_DEASSERTED",
	},
	[X86_SLP_S5_N] = {
		.gpio = GPIO_PCH_SLP_S5_L,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_S5_DEASSERTED",
	},
	[X86_S0_PGOOD] = {
		.gpio = GPIO_S0_PGOOD,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "S0_PGOOD",
	},
	[X86_S5_PGOOD] = {
		.gpio = GPIO_S5_PGOOD,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "S5_PGOOD",
	},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

/* These 2 lines are needed because we don't define an espi host driver */
#define DT_DRV_COMPAT zephyr_espi_emul_espi_host
DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE);
