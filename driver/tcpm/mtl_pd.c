/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Meteorlake compatible USB PD driver
 * https://cdrdv2.intel.com/v1/dl/getContent/634442
 */

#include "console.h"
#include "mtl_pd.h"
#include "ec_commands.h"
#include "usb_pd_tbt.h"

static union mtl_data_status data_status[CONFIG_USB_PD_PORT_MAX_COUNT];
//static union mtl_data_control data_control[CONFIG_USB_PD_PORT_MAX_COUNT];

void pd_request_data_swap(int port)
{
}

enum tcpc_cc_polarity pd_get_polarity(int port)
{
	return data_status[port].conn_ori;
}

/* TODO start: Get from PD spec */
enum pd_power_role pd_get_power_role(int port)
{
	return PD_ROLE_SINK;
}

uint8_t pd_get_task_state(int port)
{
	return 0;
}

int pd_comm_is_enabled(int port)
{
	return 1;
}

bool pd_get_vconn_state(int port)
{
	return true;
}

bool pd_get_partner_dual_role_power(int port)
{
	return false;
}

bool pd_get_partner_data_swap_capable(int port)
{
	return false;
}

bool pd_get_partner_usb_comm_capable(int port)
{
	return true;
}

bool pd_get_partner_unconstr_power(int port)
{
	return true;
}

const char *pd_get_task_state_name(int port)
{
	return "";
}

enum pd_cc_states pd_get_task_cc_state(int port)
{
	return PD_CC_NONE;
}

enum tbt_compat_rounded_support get_tbt_rounded_support(int port)
{
	return TBT_GEN3_NON_ROUNDED;
}
/* TODO end */

bool pd_capable(int port)
{
	return true;
}

int pd_is_connected(int port)
{
	return data_status[port].data_conn;
}

enum pd_data_role pd_get_data_role(int port)
{
	return data_status[port].data_role;
}

__override uint8_t get_dp_pin_mode(int port)
{
	return data_status[port].dp_pin;
}

enum tbt_compat_cable_speed get_tbt_cable_speed(int port)
{
	return data_status[port].cable_speed;
}
