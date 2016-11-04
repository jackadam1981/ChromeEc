/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fuses.h"
#include "registers.h"

#define FUSE(offset) REG32(GBASE(FUSE) + (offset))

int fuses_are_locked(void)
{
	return ((GR_FUSE(BNK0_INTG_LOCK) == FUSE_ENABLED) ||
		(GR_FUSE(BNK1_INTG_LOCK) == FUSE_ENABLED) ||
		(GR_FUSE(BNK2_INTG_LOCK) == FUSE_ENABLED) ||
		(GR_FUSE(BNK3_INTG_LOCK) == FUSE_ENABLED) ||
		(GR_FUSE(BNK4_INTG_LOCK) == FUSE_ENABLED));
}

static void copy_fuses(int start, int end, int prog_offset)
{
	int i;
	int prog_diff = prog_offset - start;

	for (i = start; i <= end; i += 4)
		FUSE(i + prog_diff) = FUSE(i);
}

void init_fuses(void)
{
	/*
	 * There are two groups of PROG registers that can not be populated by
	 * reading the fuse values. Copy the fuses between those groups.
	 */
	copy_fuses(GOFFSET(FUSE, BNK0_INTG_CHKSUM),
		   GOFFSET(FUSE, MBIST_VIA_TAP_DIS),
		   GOFFSET(FUSE, PROG_BNK0_INTG_CHKSUM));
	/* Space for MBIST_BOOTROM_MSR */
	copy_fuses(GOFFSET(FUSE, TAP_DISABLE),
		   GOFFSET(FUSE, OBFUSCATION_EN),
		   GOFFSET(FUSE, PROG_TAP_DISABLE));
	/* Space for OBS */
	copy_fuses(GOFFSET(FUSE, HIK_CREATE_LOCK),
		   GOFFSET(FUSE, FW_DEFINED_DATA_EXTRA_BLK6),
		   GOFFSET(FUSE, PROG_HIK_CREATE_LOCK));

	/*
	 * Make sure MBIST_BOOTROM_MISR is disabled by initializing registers to
	 * 0.
	 */
	GWRITE(FUSE, PROG_MBIST_BOOTROM_MISR_EN, 0);
	GWRITE(FUSE, PROG_MBIST_BOOTROM_MISR, 0);

	/* Initialize the obs fuses to some nonzero value */
	GWRITE(FUSE, PROG_OBS0, 3);
	GWRITE(FUSE, PROG_OBS1, 3);
	GWRITE(FUSE, PROG_OBS2, 3);
	GWRITE(FUSE, PROG_OBS3, 3);
	GWRITE(FUSE, PROG_OBS4, 3);
	GWRITE(FUSE, PROG_OBS5, 3);
	GWRITE(FUSE, PROG_OBS6, 3);
	GWRITE(FUSE, PROG_OBS7, 3);
}

void override_fuses(void)
{
	/* Start fuse override */
	GWRITE(FUSE, OVERRIDE_START, 0x894e4cf3);

	/* Wait until override is finished */
	while (!(GREAD(FUSE, STATUS) & (GC_FUSE_STATUS_OVERRIDE_DONE_MASK)))
		;
}
