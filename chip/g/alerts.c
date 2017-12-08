/* Copyright (c) 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "signed_header.h"
#include "task.h"

#define BROM_FWBIT_APPLYSEC_UNKNOWN    -1
#define BROM_FWBIT_APPLYSEC_SC300       0
#define BROM_FWBIT_APPLYSEC_CAMO        1
#define BROM_FWBIT_APPLYSEC_BUSERR      2
#define BROM_FWBIT_APPLYSEC_BUSOBF      3
#define BROM_FWBIT_APPLYSEC_HEARTBEAT   4
#define BROM_FWBIT_APPLYSEC_BATMON      5
#define BROM_FWBIT_APPLYSEC_RTCCHECK    6
#define BROM_FWBIT_APPLYSEC_JITTERY     7
#define BROM_FWBIT_APPLYSEC_TRNG        8
#define BROM_FWBIT_APPLYSEC_VOLT        9
#define BROM_FWBIT_APPLYSEC_NOB5        10

struct alert_desc {
	const char *name;
	const int fuse_bit;
};

// https://android-io.teams.x20web.corp.google.com/specs/haven/revB2/submod/globalsec/alert_table.html
struct alert_desc alerts[] = {
	{ "camo0/breach", BROM_FWBIT_APPLYSEC_CAMO },
	{ "crypto0/dmem_parity", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "crypto0/drf_parity", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "crypto0/imem_parity", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "crypto0/pgm_fault", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "dbctrl_cpu0_D_if/bus_err", BROM_FWBIT_APPLYSEC_BUSERR },
	{ "dbctrl_cpu0_D_if/bus_err", BROM_FWBIT_APPLYSEC_BUSOBF },
	{ "dbctrl_cpu0_I_if/bus_err", BROM_FWBIT_APPLYSEC_BUSERR },
	{ "dbctrl_cpu0_I_if/bus_err", BROM_FWBIT_APPLYSEC_BUSOBF },
	{ "dbctrl_cpu0_S_if/bus_err", BROM_FWBIT_APPLYSEC_BUSERR },
	{ "dbctrl_cpu0_S_if/bus_err", BROM_FWBIT_APPLYSEC_BUSOBF },
	{ "dbctrl_ddma0_if/bus_err", BROM_FWBIT_APPLYSEC_BUSERR },
	{ "dbctrl_ddma0_if/bus_err", BROM_FWBIT_APPLYSEC_BUSOBF },
	{ "dbctrl_dsps0_if/bus_err", BROM_FWBIT_APPLYSEC_BUSERR },
	{ "dbctrl_dsps0_if/bus_err", BROM_FWBIT_APPLYSEC_BUSOBF },
	{ "dbctrl_dusb0_if/bus_err", BROM_FWBIT_APPLYSEC_BUSERR },
	{ "dbctrl_dusb0_if/bus_err", BROM_FWBIT_APPLYSEC_BUSOBF },
	{ "fuse0/fuse_defaults", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "globalsec/diff_fail", BROM_FWBIT_APPLYSEC_HEARTBEAT },
	{ "globalsec/fw0", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "globalsec/fw1", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "globalsec/fw2", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "globalsec/fw3", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "globalsec/heartbeat_fail", BROM_FWBIT_APPLYSEC_HEARTBEAT },
	{ "globalsec/proc_opcode_hash", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "globalsec/sram_parity_scrub", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "keymgr0/aes_exec_ctr_max", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "keymgr0/aes_hkey", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "keymgr0/cert_lookup", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "keymgr0/flash_entry", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "keymgr0/pw", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "keymgr0/sha_exec_ctr_max", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "keymgr0/sha_fault", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "keymgr0/sha_hkey", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "pmu/battery_mon", BROM_FWBIT_APPLYSEC_BATMON },
	{ "pmu/pmu_wdog", BROM_FWBIT_APPLYSEC_HEARTBEAT },
	{ "rtc0/rtc_dead", BROM_FWBIT_APPLYSEC_RTCCHECK },
	{ "temp0/max_temp", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "temp0/max_temp_diff", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "temp0/min_temp", BROM_FWBIT_APPLYSEC_UNKNOWN },
	{ "trng0/out_of_spec", BROM_FWBIT_APPLYSEC_TRNG },
	{ "trng0/timeout", BROM_FWBIT_APPLYSEC_TRNG },
	{ "volt0/volt_err", BROM_FWBIT_APPLYSEC_VOLT },
	{ "xo0/jittery_trim_dis", BROM_FWBIT_APPLYSEC_JITTERY },
};

/**
 * Return the image header for the current image copy
 */
extern const struct SignedHeader *get_current_image_header(void);

static int command_alerts(int argc, char **argv)
{
	int i;
	unsigned int intrstatus[2];
	const struct SignedHeader *hdr;
	unsigned int fuse_fwbits;

	intrstatus[0] = GREAD(GLOBALSEC, ALERT_INTR_STS0);
	intrstatus[1] = GREAD(GLOBALSEC, ALERT_INTR_STS1);

	hdr = get_current_image_header();
	fuse_fwbits =  GR_FUSE(FW_DEFINED_BROM_APPLYSEC) & hdr->applysec_;

	for (i = 0; i < ARRAY_SIZE(alerts); i++) {
		const char *name = alerts[i].name;
		char fuse_status;
		int reg = i / 32;
		int offset = i % 32;

		int status = !!(intrstatus[reg] & (1 << offset));
		int fuse_bit = alerts[i].fuse_bit;

		if (fuse_bit == BROM_FWBIT_APPLYSEC_UNKNOWN) {
			// not guarded by fuse, check with Scott what to output, for now print '?'
			fuse_status = '?';
		} else {
			fuse_status = (fuse_fwbits & (1 << fuse_bit)) ? '+' : '#';
		}

		ccprintf("%32s %c %d\n", name, fuse_status, status);
		cflush();
	}
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(alerts, command_alerts,
	NULL,
	"Print alerts status");