/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Mock USB TC state machine*/

#ifndef __MOCK_USB_TC_SM_MOCK_H
#define __MOCK_USB_TC_SM_MOCK_H

#include "common.h"
#include "usb_tc_sm.h"

struct typec_t {
	int pd_enable;
};

extern struct typec_t mock_pd_port[CONFIG_USB_PD_PORT_MAX_COUNT];

#endif /* __MOCK_USB_TC_SM_MOCK_H */
