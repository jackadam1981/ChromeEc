/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "common.h"
#include "registers.h"
#include "hooks.h"
#include "ish_fwst.h"

/* Hooks to update firmware status registers */

void fwst_set_fw_up(void)
{
	/*
	 * Set status to FWSTS_FW_IS_RUNNING as soon as possible
	 * (pre-init): this serves as an indication that we are now in
	 * ECOS as opposed to a previously running image, like the ISH
	 * shim loader.
	 */
	ish_fwst_set_fw_status(FWSTS_FW_IS_RUNNING);
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, fwst_set_fw_up, HOOK_PRIO_FIRST);

void fwst_set_fw_loaded(void)
{
	/*
	 * This happens when we have finished the hard coded init
	 * functions and ECOS is loaded, but hook-defined init
	 * functions (like i2c_init) have not run yet.
	 */
	ish_fwst_set_fw_status(FWSTS_SENSOR_APP_LOADED);
}
DECLARE_HOOK(HOOK_INIT, fwst_set_fw_loaded, HOOK_PRIO_FIRST);

void fwst_set_fw_running(void)
{
	/*
	 * All hook-defined init functions have finished execution,
	 * and the system is fully ready!
	 */
	ish_fwst_set_fw_status(FWSTS_SENSOR_APP_RUNNING);
}
DECLARE_HOOK(HOOK_INIT, fwst_set_fw_running, HOOK_PRIO_LAST);
