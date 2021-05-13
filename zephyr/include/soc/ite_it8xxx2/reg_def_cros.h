/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * @file
 * @brief ITE IT8XXX2 register structure definitions used by the Chrome OS EC.
 */

#ifndef _ITE_IT8XXX2_REG_DEF_CROS_H
#define _ITE_IT8XXX2_REG_DEF_CROS_H

/* Shared Memory Flash Interface Bridge (SMFI) registers */
struct flash_it8xxx2_regs {
	volatile uint8_t reserved1[59];
	/* 0x3B: EC-Indirect memory address 0 */
	volatile uint8_t SMFI_ECINDAR0;
	/* 0x3C: EC-Indirect memory address 1 */
	volatile uint8_t SMFI_ECINDAR1;
	/* 0x3D: EC-Indirect memory address 2 */
	volatile uint8_t SMFI_ECINDAR2;
	/* 0x3E: EC-Indirect memory address 3 */
	volatile uint8_t SMFI_ECINDAR3;
	/* 0x3F: EC-Indirect memory data */
	volatile uint8_t SMFI_ECINDDR;
	/* 0x40: Scratch SRAM 0 address low byte */
	volatile uint8_t SMFI_SCAR0L;
	/* 0x41: Scratch SRAM 0 address middle byte */
	volatile uint8_t SMFI_SCAR0M;
	/* 0x42: Scratch SRAM 0 address high byte */
	volatile uint8_t SMFI_SCAR0H;
	volatile uint8_t reserved2[95];
	/* 0xA2: Flash control 6 */
	volatile uint8_t SMFI_FLHCTRL6R;
};

/* SMFI register fields */
/* EC-Indirect read internal flash */
#define EC_INDIRECT_READ_INTERNAL_FLASH BIT(6)
/* Enable EC-indirect page program command */
#define IT8XXX2_SMFI_MASK_ECINDPP BIT(3)

#endif /* _ITE_IT8XXX2_REG_DEF_CROS_H */
