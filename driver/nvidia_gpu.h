/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Nvidia GPU D-Notify driver header file
 */

#ifndef DRIVER_NVIDIA_GPU_H
#define DRIVER_NVIDIA_GPU_H

#define NVIDIA_GPU_ACOFF_DURATION (100 * MSEC)

enum d_notify_level {
	D_NOTIFY_1 = 0,
	D_NOTIFY_2,
	D_NOTIFY_3,
	D_NOTIFY_4,
	D_NOTIFY_5,
	D_NOTIFY_COUNT,
};

/**
 * A D-Notify policy consists of 5 struct d_notify_policy elements, which
 * define minimum wattage and battery soc required for each Dx level.
 *
 * min_soc_up is used when a battery is charging and min_soc_down is used when
 * a battery is discharging. These are needed to avoid frequent level
 * transitions between two levels (e.g. D3->D2->D3...).
 */
struct d_notify_policy {
	unsigned int min_watts;
	unsigned int min_soc_up;
	unsigned int min_soc_down;
};

/**
 * Helper macro to define d_notify_policy.
 *
 * w: minimum wattage required for this Dx level.
 * c1: minimum battery soc required for this Dx level when charging.
 * c2: minimum battery soc required for this Dx level when discharging.
 *
 * You need c1 > c2 to make hysteresis work.
 */
#define D_NOTIFY_ATLEAST(w, c1, c2) \
	{ .min_watts = (w), .min_soc_up = (c1), .min_soc_down = (c2) }

void nvidia_gpu_init_policy(const struct d_notify_policy *policies);

/**
 * Notify the host of assertion or deassertion of GPU over temperature.
 *
 * @param assert  True for assert. False for deassert.
 */
void nvidia_gpu_over_temp(int assert);

#endif /* DRIVER_NVIDIA_GPU_H */
