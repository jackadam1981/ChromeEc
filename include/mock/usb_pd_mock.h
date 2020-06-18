/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Mock USB PD */

#ifndef __MOCK_USB_PD_MOCK_H
#define __MOCK_USB_PD_MOCK_H

#include "common.h"
#include "usb_pd.h"

struct mock_pd_port_t {
	enum pd_data_role data_role;
	enum pd_power_role power_role;
	enum tcpc_rp_value lcl_rp;
};

extern struct mock_pd_port_t mock_pd_port;

#endif /* __MOCK_USB_PD_MOCK_H */
