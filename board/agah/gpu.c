/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

struct d_notify_policy policy = {
	/*  { .power_source = POWER_SOURCE_AC,
	    .ac = { .min_power_w = 100 }}, */
	.level[D_NOTIFY_1] = AC_ATLEAST_W(110),
	.level[D_NOTIFY_2] = AC_ATLEAST_W(65),
	.level[D_NOTIFY_3] = AC_DC
	/* { .power_source = POWER_SOURCE_DC,
	   .dc = { .batt_soc_thresh = 20 }}, */
	.level[D_NOTIFY_4] = DC_AT_LEAST_SOC(20)
	.level[D_NOTIFY_5] = DC_AT_LEAST_SOC(0),
};

static void gpu_init(void)
{
	
}
DECLARE_HOOK(HOOK_INIT, gpu_init, GPIO_PRIO_DEFAULT)
