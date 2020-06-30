/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Asurada SCP configuration */

#include "registers.h"

#include "gpio_list.h"

#include "ipi_chip.h"
static void x(int32_t id, void *data, uint32_t len)
{
	ccprints("%s", __func__);
}
DECLARE_IPI(1, x, 0);
