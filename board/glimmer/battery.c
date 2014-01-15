/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

/*
 * Copyright 2013 Lenovo ALL RIGHTS RESERVED
 * Copyright 2013 Quanta Computer Inc. ALL RIGHTS RESERVED
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Lenovo, Quanta Computer Inc. nor the names of those
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "battery.h"
#include "battery_smart.h"
#include "console.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "util.h"

/* FET ON/OFF cammand write to fet off register */
#define SB_FETOFF_DATA1	0x0000
#define SB_FETOFF_DATA2	0x1000
#define SB_FETON_DATA1	0x2000
#define SB_FETON_DATA2	0x4000
#define BATTERY_FETOFF	0x0100

static const struct battery_info info = {
	.voltage_max    = 8400,		/* mV */
	.voltage_normal = 7400,
	.voltage_min    = 6000,
	.precharge_current  = 200,	/* mA */
	.start_charging_min_c = 0,
	.start_charging_max_c = 45,
	.charging_min_c       = 0,
	.charging_max_c       = 45,
	.discharging_min_c    = -20,
	.discharging_max_c    = 60,
};

const struct battery_info *battery_get_info(void)
{
	return &info;
}

static void wakeup(void)
{
	int d;

	sb_read(SB_FET_OFF, &d);

	if (extpower_is_present() && (BATTERY_FETOFF == d)) {
		sb_write(SB_FET_OFF, SB_FETON_DATA1);
		sb_write(SB_FET_OFF, SB_FETON_DATA2);
	}
}
DECLARE_HOOK(HOOK_AC_CHANGE, wakeup, HOOK_PRIO_DEFAULT);

static int cutoff(void)
{
	int rv;

	/* Ship mode command must be sent sequentially to take effect */
	rv = sb_write(SB_FET_OFF, SB_FETOFF_DATA1);

	if (rv != EC_SUCCESS)
		return rv;

	return sb_write(SB_FET_OFF, SB_FETOFF_DATA2);
}

static int battery_command_cut_off(struct host_cmd_handler_args *args)
{
	return cutoff() ? EC_RES_SUCCESS : EC_RES_ERROR;
}
DECLARE_HOST_COMMAND(EC_CMD_BATTERY_CUT_OFF, battery_command_cut_off,
		     EC_VER_MASK(0));

static int command_battcutoff(int argc, char **argv)
{
	return cutoff();
}
DECLARE_CONSOLE_COMMAND(battcutoff, command_battcutoff,
			NULL,
			"Enable battery cutoff (ship mode)",
			NULL);
