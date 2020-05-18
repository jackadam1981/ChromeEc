/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "usb_pd.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/*
 * TODO(b/167711550): These 4 functions need to be implemented for honeybuns
 * and are required to build with TCPMv2 enabled. Currently, they only allow the
 * build to work. They will be implemented in a subsequent CL.
 */

int pd_check_vconn_swap(int port)
{
	return 0;
}

void pd_power_supply_reset(int port)
{

}

int pd_set_power_supply_ready(int port)
{

	return 0;
}

int pd_snk_is_vbus_provided(int port)
{
	return 0;
}

void pd_set_input_current_limit(int port, uint32_t max_ma,
				uint32_t supply_voltage)
{

}
