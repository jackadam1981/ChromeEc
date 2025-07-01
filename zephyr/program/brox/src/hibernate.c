/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "charger/isl923x_public.h"
#include "system.h"

__override void board_hibernate(void)
{
#if defined(CONFIG_CHARGER_ISL9238C) || defined(CONFIG_ZTEST)
	isl9238c_hibernate(CHARGER_SOLO);
#endif
}
