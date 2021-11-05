/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Richtek RT1739 Type-C Power Path Controller */

#ifndef __CROS_EC_PPC_RT1739_H
#define __CROS_EC_PPC_RT1739_H

#include "usb_charge.h"
#include "usbc_ppc.h"

#define RT1739_ADDR1 0x70
#define RT1739_ADDR2 0x71
#define RT1739_ADDR3 0x72
#define RT1739_ADDR4 0x73

#define RT1739_REG_SW_RESET		0x04

#define RT1739_REG_INT_MASK5		0x0D
#define RT1739_REG_INT_MASK5_BC12_SNK_DONE	BIT(0)

#define RT1739_REG_INT_EVENT5		0x15
#define RT1739_REG_INT_EVENT5_BC12_SNK_DONE	BIT(0)

#define RT1739_REG_INT_STS4		0x1C
#define RT1739_REG_INT_STS4_VBUS_VALID		BIT(2)
#define RT1739_REG_INT_STS4_VBUS_PRESENT	BIT(0)

#define RT1739_REG_SYS_CTRL		0x20
#define RT1739_REG_SYS_CTRL_OT_EN		BIT(4)
#define RT1739_REG_SYS_CTRL_SHUTDOWN_OFF	BIT(0)

#define RT1739_REG_VBUS_SWITCH_CTRL	0x21
#define RT1739_REG_VBUS_SWITCH_CTRL_LV_SRC_EN	BIT(2)
#define RT1739_REG_VBUS_SWITCH_CTRL_HV_SRC_EN	BIT(1)
#define RT1739_REG_VBUS_SWITCH_CTRL_HV_SNK_EN	BIT(0)

#define RT1739_REG_VBUS_DET_EN		0x27
#define RT1739_REG_VBUS_DET_EN_VBUS_SAFE5V	BIT(2)
#define RT1739_REG_VBUS_DET_EN_VBUS_SAFE0V	BIT(1)
#define RT1739_REG_VBUS_DET_EN_VBUS_PRESENT	BIT(0)


#define RT1739_REG_BC12_SNK_FUNC	0x40
#define RT1739_REG_BC12_SNK_FUNC_BC12_SNK_EN	BIT(7)

#define RT1739_REG_BC12_STAT		0x41
#define RT1739_REG_BC12_STAT_PORT_STAT_MASK	0x0F
#define RT1739_REG_BC12_STAT_SDP		0b1101
#define RT1739_REG_BC12_STAT_CDP		0b1110
#define RT1739_REG_BC12_STAT_DCP		0b1111


extern const struct ppc_drv rt1739_ppc_drv;
extern const struct bc12_drv rt1739_bc12_drv;

void rt1739_interrupt(enum gpio_signal signal);

#endif /* defined(__CROS_EC_PPC_RT1739_H) */
