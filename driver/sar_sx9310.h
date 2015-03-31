/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SX9310 Specific Absorption Rate (SAR) module for Chrome EC */

#ifndef __CROS_EC_SAR_SX9310_H
#define __CROS_EC_SAR_SX9310_H

#define SX9310_ADDR             0x50

#define SX9310_WHO_AM_I         0x01
#define SX9310_WHO_AM_I_REG     0x42

#define SX9310_IRQSTAT_REG      0x00
#define SX9310_STAT0_REG        0x01
#define SX9310_STAT1_REG        0x02
#define SX9310_IRQ_ENABLE_REG   0x03
#define SX9310_IRQFUNC_REG      0x04

#define SX9310_CPS_CTRL_REG0    0x10
#define SX9310_CPS_CTRL_REG1    0x11
#define SX9310_CPS_CTRL_REG2    0x12
#define SX9310_CPS_CTRL_REG3    0x13
#define SX9310_CPS_CTRL_REG4    0x14
#define SX9310_CPS_CTRL_REG5    0x15
#define SX9310_CPS_CTRL_REG6    0x16
#define SX9310_CPS_CTRL_REG7    0x17
#define SX9310_CPS_CTRL_REG8    0x18
#define SX9310_CPS_CTRL_REG9    0x19
#define SX9310_CPS_CTRL_REG10   0x1A
#define SX9310_CPS_CTRL_REG11   0x1B
#define SX9310_CPS_CTRL_REG12   0x1C
#define SX9310_CPS_CTRL_REG13   0x1D
#define SX9310_CPS_CTRL_REG14   0x1E
#define SX9310_CPS_CTRL_REG15   0x1F
#define SX9310_CPS_CTRL_REG16   0x20
#define SX9310_CPS_CTRL_REG17   0x21
#define SX9310_CPS_CTRL_REG18   0x22
#define SX9310_CPS_CTRL_REG19   0x23

#define SX9310_SAR_CTRL_REG0    0x2A
#define SX9310_SAR_CTRL_REG1    0x2B
#define SX9310_SAR_CTRL_REG2    0x2C

#define SX9310_SOFTRESET_REG    0x7F
#define SX9310_SOFTRESET        0xDE

#define SX9310_CPSRD            0x30

#define SX9310_USEMSB           0x31
#define SX9310_USELSB           0x32

#define SX9310_AVGMSB           0x33
#define SX9310_AVGLSB           0x34

#define SX9310_DIFFMSB          0x35
#define SX9310_DIFFLSB          0x36

#define SX9310_OFFSETMSB        0x37
#define SX9310_OFFSETLSB        0x38
#define SX9310_SARMSB           0x39
#define SX9310_SARLSB           0x3A

enum sx9310_irq_status {
	SX9310_STS_RESET        = 1 << 7,
	SX9310_STS_TOUCH        = 1 << 6,
	SX9310_STS_RELEASE      = 1 << 5,
	SX9310_STS_COMPDONE     = 1 << 4,
	SX9310_STS_CONV         = 1 << 3,
	SX9310_STS_TXEN         = 1 << 0,
};

enum sx9310_irq_function {
	SX9310_IRQ_POLARITY_INV =  1 << 5,
	SX9310_IRQ_PROXSTATANY  =  1 << 1,
	SX9310_IRQ_PROXSTAT0    =  2 << 1,
	SX9310_IRQ_PROXSTAT1    =  3 << 1,
	SX9310_IRQ_PROXSTAT2    =  4 << 1,
	SX9310_IRQ_PROXSTATCOMB =  5 << 1,
	SX9310_IRQ_PROXSTAT12   =  6 << 1,
	SX9310_IRQ_PROXSTATALL  =  7 << 1,
	SX9310_IRQ_BODYSTAT0    =  8 << 1,
	SX9310_IRQ_BODYSTAT12   =  9 << 1,
	SX9310_IRQ_TABLESTAT12  = 10 << 1,
	SX9310_IRQ_SMARTSARSTAT = 11 << 1,
	SX9310_IRQ_CONVSTAT     = 12 << 1,
	SX9310_IRQ_COMPSTAT     = 13 << 1,
	SX9310_IRQ_PINMOD_PP    =  1 << 0,
};

enum sx9310_reg_stat1 {
	SX9310_PROXSTAT12       =  1 << 7,
	SX9310_PROXSTATANY      =  1 << 6,
	SX9310_PROXSTATALL      =  1 << 5,
	SX9310_CONVSTAT         =  1 << 4,
	SX9310_COMPSTAT         = 15 << 0,
};

enum sx9310_thr_type {
	PROXTHRESH0 = 0,
	PROXTHRESH12 = 1,
};

extern const struct sar_drv sx9310_drv;

#endif /* __CROS_EC_SAR_SX9310_H */

