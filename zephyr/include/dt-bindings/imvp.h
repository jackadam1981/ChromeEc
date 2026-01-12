/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef DT_BINDINGS_IMVP_H_
#define DT_BINDINGS_IMVP_H_

#define RT3645_UPDATE_ENTRY(page, reg, val) ((page) << 16 | (reg) << 8 | (val))

/* RT3645 Pages */
/* Global page */
#define RT3645_PAGE_GLOBAL 0x0
/* Pages from 0x01 - 0x08 exists, but they are not documented in the vendor
 * data sheet. They can be added upon request in specific board.
 */
#define RT3645_PAGE_4 0x04
#define RT3645_PAGE_5 0x05
#define RT3645_PAGE_6 0x06
/* Pages 0x09 - 0x0C contains the functional registers of rails A, B, C
 * & D.
 */
#define RT3645_PAGE_9 0x09
#define RT3645_PAGE_A 0x0A
#define RT3645_PAGE_B 0x0B
#define RT3645_PAGE_C 0x0C
/* Page 0x0D contains the functional registers for general settings */
#define RT3645_PAGE_D 0x0D

/* Functional Registers for Pages - 0x09, 0x0A, 0x0B & 0x0C */
#define ICCMAX_REG 0x00
#define ICCMAX_HC_SR_KTON_REG 0x01
#define VBOOT_EN_RIMON_MSB_REG 0x02
#define RIMON_REG 0x03
#define RLL_REG 0x04
#define COMP_GAIN_REG 0x05
#define COMP_MODE_PZ_REG 0x06
#define VSEN_COMP_LPF_REG 0x07
#define DVID_ENHANCE_SPM_EN_REG 0x08
#define RESERVED_REG 0x09
#define SOCP_TH_ANTIOVS_TH_REG 0x0A
#define AR_TH_REG 0x0B
#define DEM_SHRINK_TON_REG 0x0C
#define ZCD_VID_RTH_REG 0x0D
#define ZCD_I_TH_HYS_REG 0x0E
#define DVID_TAU_AQR_TH_REG 0x0F
#define RIPPLE_COMP_SVID_ADDR_REG 0x10
#define SVID_VBOOT_REG 0x11
#define SVID_DCLL_REG 0x12

/* Functional Registers for Page - 0x0D */
#define ICCMAX_AUX_REG 0x00
#define AUX_HC_IMVP_RAIL_EN_REG 0x01
#define PH_SET_SPM_TH5_A_REG 0x02
#define SPM_TH4_TH3_A_REG 0x03
#define SPM_TH2_A_B_REG 0x04
#define SPM_HYS_DVIDUP_PH_A_REG 0x05
#define SPS_OFS_A_SPM_HYS_B_REG 0x06
#define DVIDUP_PH_B_DVIDDN_PH_SET_REG 0x07
#define SOCP_TIME_DRVEN_MODE_REG 0x08
#define EN_SPS_FASTDN_PSYS_LEVEL_REG 0x09
#define TSEN_SPS_SEL_VRHOT_HYS_REG 0x0A
#define TSEN_ALERT_HYS_REG 0x0B
#define TSEN_VRHOT_REG 0x0C
#define TSEN_ALERT_REG 0x0D
#define ANS_EN_SEL_IGNORE_PS4_REG 0x0E
#define SPS_OFS_B_REG 0x0F
#define RESERVED_FW_VER_L_REG 0x10
#define RESERVED_FW_VER_H_REG 0x11
#define PWM_HIZ_SEL_REG 0x12
#define CRC_CHECK_REG 0x13

#endif
