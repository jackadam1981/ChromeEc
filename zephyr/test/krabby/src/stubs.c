/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "usb_charge.h"
#include "usbc_ppc.h"
#include "charge_ramp.h"
#include "charge_state.h"

void board_set_charge_limit(int port, int supplier, int charge_ma, int max_ma,
			    int charge_mv)
{
}

int pd_check_vconn_swap(int port)
{
	return 0;
}

__attribute__((weak)) struct bc12_config bc12_ports[2];
