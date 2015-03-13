/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "timels.h"

/*
 * Enable low speed XTL clock source
 * @param enable Set to 1 to enable, 0 to disable
 * @todo Add trimming support
 */
void timels_xtl_en(uint32_t enable)
{
	if (enable) {
		/* Power on */
		REG32(GC_PMU_BASE_ADDR + GC_PMUSETRTC_OFFSET) =
			1 << GC_PMUSETRTC_X_RTC_XTL_PDB_3P3_LSB;

		/* Set XTL installed bit */
		REG32(GC_RTC0_BASE_ADDR + GC_RTC_CTRL_OFFSET) |=
			1 << GC_RTC_CTRL_X_RTC_XTL_INSTALLED_3P3_LSB;
	} else {

		/* Unset XTL installed bit */
		REG32(GC_RTC0_BASE_ADDR + GC_RTC_CTRL_OFFSET) &=
			~(1 << GC_RTC_CTRL_X_RTC_XTL_INSTALLED_3P3_LSB);

		/* Power down */
		REG32(GC_PMU_BASE_ADDR + GC_PMUCLRRTC_OFFSET) =
			1 << GC_PMUCLRRTC_X_RTC_XTL_PDB_3P3_LSB;
	}

	/* Switch to XTL */
	GWRITE_FIELD(RTC, CTRL, X_RTC_MUX_CTRL_3P3, 0x2);
}

/*
 * Enable low speed RC clock source
 * @param enable Set to 1 to enable, 0 to disable
 * @todo Add trimming support
 */
void timels_rc_en(uint32_t enable)
{
	if (enable)
		/* Power on */
		REG32(GC_PMU_BASE_ADDR + GC_PMUSETRTC_OFFSET) =
			1 << GC_PMUSETRTC_X_RTC_RC_PDB_3P3_LSB;
	else
		/* Power off */
		REG32(GC_PMU_BASE_ADDR + GC_PMUCLRRTC_OFFSET) =
			1 << GC_PMUCLRRTC_X_RTC_RC_PDB_3P3_LSB;

	/* Switch to RC */
	GWRITE_FIELD(RTC, CTRL, X_RTC_MUX_CTRL_3P3, 0x3);
}
