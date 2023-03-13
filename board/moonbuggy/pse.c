/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * The LTC4291 is a power over ethernet (PoE) power sourcing equipment (PSE)
 * controller.
 */
#include "pse_ltc4291.h"

/*
 * Port 1-4: 15W
 */
int pse_port_hpmd[LTC4291_PORT_MAX] = {
	LTC4291_HPMD_MIN,
	LTC4291_HPMD_MIN,
	LTC4291_HPMD_MIN,
	LTC4291_HPMD_MIN,
};
