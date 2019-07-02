/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Clocks, PLL and power settings */

#ifndef __CROS_EC_PMIC_H
#define __CROS_EC_PMIC_H

#include "common.h"

#define PMIC_CHIP_ID (0xa)

/* Error handle */
#define E_PWR_WAIT_IDLE_TIMEOUT         (1)
#define E_PWR_WAIT_IDLE_TIMEOUT_READ    (2)

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
#define GET_WACS_SYNC_IDLE(x)	(((x >> RDATA_SYNC_IDLE_SHIFT) & RDATA_SYNC_IDLE_MASK))

int16_t pwrap_scp(uint16_t write, uint16_t adr, uint16_t wdata, uint16_t *rdata);
static inline int16_t pmic_scp_read(uint16_t addr, uint16_t *rdata)
{
	return pwrap_scp(0, addr, 0, rdata);
}

static inline int16_t pmic_scp_write(uint16_t addr, uint16_t wdata)
{
	return pwrap_scp(1, addr, wdata, 0);
}

static inline uint16_t pmic_read_field(uint16_t reg, uint16_t mask, uint16_t shift)
{
	uint16_t rdata;
	pmic_scp_read(reg, &rdata);
	rdata &= (mask << shift);
	rdata = (rdata >> shift);
	return rdata;
}

static inline void pmic_write_field(uint16_t reg, uint16_t val, uint16_t mask, uint16_t shift)
{
	uint16_t old, new;
	pmic_scp_read(reg, &old);
	new = old & ~(mask << shift);
	new |= (val << shift);
	pmic_scp_write(reg, new);
}

void scp_pmic_init(void);

#endif /* __CROS_EC_PMIC_H */
