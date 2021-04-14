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
	/* 0x02: Chip Version */
	volatile uint8_t GCTRL_ECHIPVER;
	/* 0x03-0x05: Reserved2 */
	volatile uint8_t reserved2[3];
	/* 0x06: Reset Status */
	volatile uint8_t GCTRL_RSTS;
	/* 0x07-0x1B: Reserved3 */
	volatile uint8_t reserved3[21];
	/* 0x1C: Special Control 4 */
	volatile uint8_t GCTRL_SPCTRL4;
	/* 0x1D-0x1F: Reserved4 */
	volatile uint8_t reserved4[3];
	/* 0x20: Memory Controller Configuration 3 */
	volatile uint8_t GCTRL_MCCR3;
	/* 0x21: Reset Control 5 */
	volatile uint8_t GCTRL_RSTC5;
	/* 0x22-0x2F: Reserved5 */
	volatile uint8_t reserved5[14];
	/* 0x30: Memory Controller Configuration */
	volatile uint8_t GCTRL_MCCR;
	/* 0x31: Externel ILM/DLM Size */
	volatile uint8_t GCTRL_EIDSR;
	/* 0x32-0x36: Reserved6 */
	volatile uint8_t reserved6[5];
	/* 0x37: Eflash Protect Lock */
	volatile uint8_t GCTRL_EPLR;
	/* 0x38-0x84: Reserved7 */
	volatile uint8_t reserved7[77];
	/* 0x85: Chip ID Byte 1 */
	volatile uint8_t GCTRL_ECHIPID1;
	/* 0x86: Chip ID Byte 2 */
	volatile uint8_t GCTRL_ECHIPID2;
	/* 0x87: Chip ID Byte 3 */
	volatile uint8_t GCTRL_ECHIPID3;
};

/* GCTRL register fields */
#define IT8XXX2_GCTRL_VCCDO		(BIT(7) | BIT(6))
#define IT8XXX2_GCTRL_LRS		(BIT(1) | BIT(0))
#define IT8XXX2_GCTRL_LRSIWR		BIT(2)
#define IT8XXX2_GCTRL_LRSIPWRSWTR	BIT(1)
#define IT8XXX2_GCTRL_LRSIPGWR		BIT(0)


#endif /* _ITE_IT8XXX2_REG_DEF_CROS_H */
