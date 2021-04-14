/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * @file
 * @brief ITE it8xxx2 register structure definitions used by the Chrome OS EC.
 */

#ifndef _ITE_IT8XXX2_REG_DEF_CROS_H
#define _ITE_IT8XXX2_REG_DEF_CROS_H

/*
 * KBS (Keyboard Scan) device registers
 */
struct kbs_reg {
	/* 0x000: Keyboard Scan Out */
	volatile uint8_t KBS_KSOL;
	/* 0x001: Keyboard Scan Out */
	volatile uint8_t KBS_KSOH1;
	/* 0x002: Keyboard Scan Out Control */
	volatile uint8_t KBS_KSOCTRL;
	/* 0x003: Keyboard Scan Out */
	volatile uint8_t KBS_KSOH2;
	/* 0x004: Keyboard Scan In */
	volatile uint8_t KBS_KSI;
	/* 0x005: Keyboard Scan In Control */
	volatile uint8_t KBS_KSICTRL;
	/* 0x006: Keyboard Scan In [7:0] GPIO Control */
	volatile uint8_t KBS_KSIGCTRL;
	/* 0x007: Keyboard Scan In [7:0] GPIO Output Enable */
	volatile uint8_t KBS_KSIGOEN;
	/* 0x008: Keyboard Scan In [7:0] GPIO Data */
	volatile uint8_t KBS_KSIGDAT;
	/* 0x009: Keyboard Scan In [7:0] GPIO Data Mirror */
	volatile uint8_t KBS_KSIGDMRR;
	/* 0x00A: Keyboard Scan Out [15:8] GPIO Control */
	volatile uint8_t KBS_KSOHGCTRL;
	/* 0x00B: Keyboard Scan Out [15:8] GPIO Output Enable */
	volatile uint8_t KBS_KSOHGOEN;
	/* 0x00C: Keyboard Scan Out [15:8] GPIO Data Mirror */
	volatile uint8_t KBS_KSOHGDMRR;
	/* 0x00D: Keyboard Scan Out [7:0] GPIO Control */
	volatile uint8_t KBS_KSOLGCTRL;
	/* 0x00E: Keyboard Scan Out [7:0] GPIO Output Enable */
	volatile uint8_t KBS_KSOLGOEN;
};

/* KBS register fields */
#define IT8XXX2_KBS_KSOPU	BIT(2)
#define IT8XXX2_KBS_KSOOD	BIT(0)
#define IT8XXX2_KBS_KSIPU	BIT(2)
#define IT8XXX2_KBS_KSO2GCTRL	BIT(2)
#define IT8XXX2_KBS_KSO2GOEN	BIT(2)

/*
 * ECPM (EC Clock and Power Management) device registers
 */
struct ecpm_reg {
	/* 0x000: Reserved1 */
	volatile uint8_t reserved1;
	/* 0x001: Clock Gating Control 1 */
	volatile uint8_t ECPM_CGCTRL1;
	/* 0x002: Clock Gating Control 2 */
	volatile uint8_t ECPM_CGCTRL2;
	/* 0x003: PLL Control */
	volatile uint8_t ECPM_PLLCTRL;
	/* 0x004: Auto Clock Gating */
	volatile uint8_t ECPM_AUTOCG;
	/* 0x005: Clock Gating Control 3 */
	volatile uint8_t ECPM_CGCTRL3;
	/* 0x006: PLL Frequency */
	volatile uint8_t ECPM_PLLFREQ;
	/* 0x007: Reserved2 */
	volatile uint8_t reserved2;
	/* 0x008: PLL Clock Source Status */
	volatile uint8_t ECPM_PLLCSS;
	/* 0x009: Clock Gating Control 4 */
	volatile uint8_t ECPM_CGCTRL4;
	/* 0x00A: Reserved3 */
	volatile uint8_t reserved3;
	/* 0x00B: Reserved4 */
	volatile uint8_t reserved4;
	/* 0x00C: System Clock Divide Control 0 */
	volatile uint8_t ECPM_SCDCR0;
	/* 0x00D: System Clock Divide Control 1 */
	volatile uint8_t ECPM_SCDCR1;
	/* 0x00E: System Clock Divide Control 2 */
	volatile uint8_t ECPM_SCDCR2;
	/* 0x00F: System Clock Divide Control 3 */
	volatile uint8_t ECPM_SCDCR3;
	/* 0x010: System Clock Divide Control 4 */
	volatile uint8_t ECPM_SCDCR4;
};

/*
 * General Control (GCTRL) registers
 */
struct gctrl_reg {
	/* 0x00-0x01: Reserved1 */
	volatile uint8_t reserved1[2];
	/* 0x02:  */
	volatile uint8_t GCTRL_ECHIPVER;
	/* 0x03:  */
	volatile uint8_t GCTRL_DBGROS;
	/* 0x04:  */
	volatile uint8_t GCTRL_IDR;
	/* 0x05: Reserved2 */
	volatile uint8_t reserved2;
	/* 0x06:  */
	volatile uint8_t GCTRL_RSTS;
	/* 0x07-0x09:  reserved3 */
	volatile uint8_t reserved3[3];
	/* 0x0A:  */
	volatile uint8_t GCTRL_BADRSEL;
	/* 0x0B-0x0C:  reserved4 */
	volatile uint8_t reserved4[2];
	/* 0x0D:  */
	volatile uint8_t GCTRL_SPCTRL1;
	/* 0x0E-0x0F:  reserved5 */
	volatile uint8_t reserved5[2];
	/* 0x10:  */
	volatile uint8_t GCTRL_RSTDMMC;
	/* 0x11:  */
	volatile uint8_t GCTRL_RSTC4;
	/* 0x12-0x1B:  reserved6 */
	volatile uint8_t reserved6[10];
	/* 0x1C:  */
	volatile uint8_t GCTRL_SPCTRL4;
	/* 0x1D-0x1F:  reserved7 */
	volatile uint8_t reserved7[3];
	/* 0x20:  */
	volatile uint8_t GCTRL_MCCR3;
	/* 0x21:  */
	volatile uint8_t GCTRL_RSTC5;
	/* 0x22-0x2F:  */
	volatile uint8_t reserved8[14];
	/* 0x30:  */
	volatile uint8_t GCTRL_MCCR;
	/* 0x31:  */
	volatile uint8_t GCTRL_EIDSR;
	/* 0x32:  */
	volatile uint8_t GCTRL_PMER1_;
	/* 0x33:  */
	volatile uint8_t GCTRL_PMER2_;
	/* 0x34-0x36:  */
	volatile uint8_t reserved9[3];
	/* 0x37:  */
	volatile uint8_t GCTRL_EPLR;
	/* 0x38-0x40:  */
	volatile uint8_t reserved10[8];
	/* 0x41:  */
	volatile uint8_t GCTRL_IVTBAR;
	/* 0x42-0x43:  */
	volatile uint8_t reserved11[2];
	/* 0x44:  */
	volatile uint8_t GCTRL_MCCR2;
	/* 0x45:  */
	volatile uint8_t reserved12;
	/* 0x46:  */
	volatile uint8_t GCTRL_PIN_MUX0;
	/* 0x47-0x49:  */
	volatile uint8_t reserved13[3];
	/* 0x4A:  */
	volatile uint8_t GCTRL_SSCR;
	/* 0x4B:  */
	volatile uint8_t GCTRL_ETWDUARTCR;
	/* 0x4C:  */
	volatile uint8_t GCTRL_WMCR;
	/* 0x4D-0x52:  */
	volatile uint8_t reserved14[6];
	/* 0x53:  */
	volatile uint8_t GCTRL_H2ROFSR;
	/* 0x54-0x5C:  */
	volatile uint8_t reserved15[9];
	/* 0x5D:  */
	volatile uint8_t GCTRL_RVILMCR0;
	/* 0x5E-0x84:  */
	volatile uint8_t reserved16[39];
	/* 0x85:  */
	volatile uint8_t GCTRL_ECHIPID1;
	/* 0x86:  */
	volatile uint8_t GCTRL_ECHIPID2;
	/* 0x87:  */
	volatile uint8_t GCTRL_ECHIPID3;
};

#endif /* _ITE_IT8XXX2_REG_DEF_CROS_H */
