/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Nyan board-specific pmu/charger routines */

#include "console.h"

#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)

int pmu_shutdown(void)
{
	/* no EC controlled pmu */
	CPRINTF("[pmu] No pmu shutdown supported !!!\n");
	return EC_ERROR_UNKNOWN;
}
