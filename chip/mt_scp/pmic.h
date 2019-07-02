/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks, PLL and power settings */

#ifndef __CROS_EC_PMIC_H
#define __CROS_EC_PMIC_H

#include "common.h"

enum {
	RDATA_WACS_FSM_SHIFT   = 16,
	RDATA_SYNC_IDLE_SHIFT  = 20,
};

enum {
	RDATA_WACS_RDATA_MASK = 0xffff,
	RDATA_WACS_FSM_MASK   = 0x7,
	RDATA_SYNC_IDLE_MASK  = 0x1,
};

/* WACS_FSM */
enum {
	WACS_FSM_IDLE     = 0x00,
	WACS_FSM_REQ      = 0x02,
	WACS_FSM_WFDLE    = 0x04,
	WACS_FSM_WFVLDCLR = 0x06,
	WACS_SYNC_IDLE    = 0x01,
};

#define GET_WACS_FSM(x)	(((x >> RDATA_WACS_FSM_SHIFT) & RDATA_WACS_FSM_MASK))
#define GET_WACS_SYNC_IDLE(x)	\
		(((x >> RDATA_SYNC_IDLE_SHIFT) & RDATA_SYNC_IDLE_MASK))

void scp_pmic_init(void);

#endif /* __CROS_EC_PMIC_H */
