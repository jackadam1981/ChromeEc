/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Policy Engine module */

#ifndef __CROS_EC_USB_PE_H
#define __CROS_EC_USB_PE_H

enum pe_error {
	ERR_RCH_CHUNKED,
	ERR_RCH_MSG_REC,
	ERR_TCH_CHUNKED,
	ERR_TCH_XMIT,
	ERR_PRL_TX,
};


void pe_init(int port);
int policy_engine(int port, int evt);
void pe_message_sent(int port);
void pe_report_error(int port, enum pe_error e);
void pe_pass_up_message(int port);
void pe_got_hard_reset(int port);
void pe_got_soft_reset(int port);
void pe_hard_reset_sent(int port);
int pe_pd_capable(int port);
#endif

