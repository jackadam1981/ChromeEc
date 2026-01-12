/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DT_BINDINGS_IMVP_H_
#define DT_BINDINGS_IMVP_H_

#define RT3645_UPDATE_ENTRY(page, reg, val) ((page) << 16 | (reg) << 8 | (val))

/* RT3645 Pages */
#define RT3645_PAGE_GLOBAL 0x0
#define RT3645_PAGE_5 0x05
#define RT3645_PAGE_9 0x09
#define RT3645_PAGE_A 0x0A
#define RT3645_PAGE_B 0x0B
#define RT3645_PAGE_C 0x0C
#define RT3645_PAGE_D 0x0D

/* RT3645 Registers */
#define ICC_MAX_REG 0x00
#define ICCMAX_HC_SR_KTON_REG 0x01
#define RIMON_REG 0x03
#define RLL_REG 0x04
#define VID_STEP_COMP_GAIN_REG 0x05
#define COMP_MODE_PZ_REG 0x06
#define VSEN_COMP_LPF_REG 0x07
#define DVID_ENHANCE_SPM_EN_REG 0x08
#define AR_TH_REG 0x0B
#define DEM_SHRINK_TON_REG 0x0C
#define ZCD_VID_R_TH_REG 0x0D
#define ZCD_I_TH_HYS_REG 0x0E
#define DVID_TAU_AQR_TH_REG 0x0F
#define RIPPLE_COMP_SVID_ADDR_REG 0x10
#define SVID_DCLL_REG 0x12

#endif

