/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_HPET_H
#define __CROS_EC_HPET_H

#define GENERAL_CAPS_ID_REG		0x0
#define GENERAL_CONFIG_REG		0x10
#define GENERAL_INT_STAT_REG		0x20
#define MAIN_COUNTER_REG		0xF0

#define TIMER0_CONF_CAP_REG		0x100
#define TIMER0_COMP_VAL_REG		0x108
#define TIMER0_FSB_IR_REG		0x110

#define TIMER1_CONF_CAP_REG		0x120
#define TIMER1_COMP_VAL_REG		0x128
#define TIMER1_FSB_IR_REG		0x130

#define TIMER2_CONF_CAP_REG		0x140
#define TIMER2_COMP_VAL_REG		0x148
#define TIMER2_FSB_IR_REG		0x150

#define CONTROL_AND_STATUS_REG		0x160

#define HPET_ENABLE_CNF			(1<<0)
#define HPET_LEGACY_RT_CNF		(1<<1)

#define HPET_Tn_INT_TYPE_CNF            (1<<1)
#define HPET_Tn_INT_ENB_CNF		(1<<2)
#define HPET_Tn_TYPE_CNF		(1<<3)
#define HPET_Tn_32MODE_CNF		(1<<8)
#define HPET_Tn_INT_ROUTE_CNF_SHIFT     0x9
#define HPET_Tn_INT_ROUTE_CNF_MASK      (0x1f << 9)

#define HPET_GENERAL_CONFIG		REG32(HPET_BASE + GENERAL_CONFIG_REG)
#define HPET_MAIN_COUNTER		REG32(HPET_BASE + MAIN_COUNTER_REG)

#define HPET_TIMER_CONF_CAP(x) \
	REG32(HPET_BASE + TIMER0_CONF_CAP_REG + (x * 0x20))
#define HPET_TIMER_COMP(x) \
	REG32(HPET_BASE + TIMER0_COMP_VAL_REG + (x * 0x20))

#define HPET_INTR_CLEAR			REG32(HPET_BASE + GENERAL_INT_STAT_REG)

#endif /* __CROS_EC_HPET_H */
