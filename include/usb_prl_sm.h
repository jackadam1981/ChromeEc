/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Protocol Layer module */

#ifndef __CROS_EC_USB_PRL_H
#define __CROS_EC_USB_PRL_H
#include "common.h"
#include "usb_pd.h"

void protocol_layer(int port, int evt);
void prl_init(int port);
int prl_send_ctrl_msg(int port, enum tcpm_transmit_type type,
	enum pd_ctrl_msg_type msg);
int prl_send_data_msg(int port, enum tcpm_transmit_type type,
	enum pd_data_msg_type msg);
int prl_send_ext_data_msg(int port, enum tcpm_transmit_type type,
	enum pd_ext_msg_type msg);
void prl_hard_reset_complete(int port);
void prl_start_ams(int port);
void prl_end_ams(int port);
#endif

