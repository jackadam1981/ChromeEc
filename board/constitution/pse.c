/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * The LTC4291 is a power over ethernet (PoE) power sourcing equipment (PSE)
 * controller.
 */
#include "pse.h"

/*
 * Port 1: 100W
 * Port 2-4: 15W
 */
int pse_port_hpmd[4] = {
	LTC4291_HPMD_MAX,
	LTC4291_HPMD_MIN,
	LTC4291_HPMD_MIN,
	LTC4291_HPMD_MIN,
};
