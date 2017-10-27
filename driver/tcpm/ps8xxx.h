/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Parade Tech Type-C port controller */

#ifndef __CROS_EC_USB_PD_TCPM_PS8XXX_H
#define __CROS_EC_USB_PD_TCPM_PS8XXX_H


/* BIST Carrier Mode 2 registers not defined in programmering guide.
 *
 * Register Name	| Offset | Type | Reset | Description
 *			|        |      | Value |
 * ---------------------------------------------------------------
 * BIST_CONT_MODE_BYTE0	| 0xBC   | 0xFF | R/W   | BIST timer byte0
 * BIST_CONT_MODE_BYTE1	| 0xBD   | 0x0F | R/W   | BIST timer byte1
 * BIST_CONT_MODE_BYTE2	| 0xBE   | 0x00 | R/W   | BIST timer byte2
 *			|        |      |       |
 * BIST_CONT_MODE_CTR	| 0xBF   | 0x00 | R/W   | BIST carrier mode 2.
 *			|        |      |       | Two modes to control
 *			|        |      |       | how BIST transfer terminates:
 *			|        |      |       |
 *			|        |      |       | 1:
 *			|        |      |       |  When the BIST_CONT_MODE_BYTE*
 *			|        |      |       |  timer expires, the BIST
 *			|        |      |       |  transfer terminates.
 *			|        |      |       |
 *			|        |      |       | 2:
 *			|        |      |       |  When this timer is disabled,
 *			|        |      |       |  Bit[1] is asserted and the
 *			|        |      |       |  BIST transfer terminates.
 *			|        |      |       |
 *			|        |      |       | Bit[0]:
 *			|        |      |       |  1: The BIST timer is disabled
 *			|        |      |       |  0: The BIST timer is enabled
 *			|        |      |       |
 *			|        |      |       | Bit[1]:
 *			|        |      |       |  1: BIST transfer has
 *			|        |      |       |     terminated and BIST
 *			|	 |	|	|     transfer mode exited
 *			|        |      |       |  0: BIST data is transmitting.
 *			|        |      |       |
 *			|        |      |       | Bit[7:2]: Reserved
 *			|        |      |       |
 * REG_DET_CTRL0(Page6)	| 0x08   |      | R/W   | Bit[3:0] Reserved
 *			|        |      |       |
 *			|        |      |       | Bit[4] SW_CC1_EN
 *			|        |      |       |  1: enable
 *			|        |      |       |  0: disable
 *			|        |      |       |
 *			|        |      |       | Bit[5] SW_CC2_EN
 *			|        |      |       |  1 enable
 *			|        |      |       |  0 disable
 *			|        |      |       |
 *			|        |      |       | Bit[7:6] Reserved
 */



#define PS8XXX_VENDOR_ID  0x1DA0
#define PS8XXX_REG_I2C_DEBUGGING_ENABLE		0xA0
#define PS8XXX_REG_BIST_CONT_MODE_BYTE0		0xBC
#define PS8XXX_REG_BIST_CONT_MODE_BYTE1		0xBD
#define PS8XXX_REG_BIST_CONT_MODE_BYTE2		0xBE
#define PS8XXX_REG_BIST_CONT_MODE_CTR		0XBF
#define PS8XXX_REG_DET_CTRL0			0x08

#if defined(CONFIG_USB_PD_TCPM_PS8751)
/* Vendor defined registers */
#define PS8XXX_PRODUCT_ID 0x8751

#define FW_VER_REG                              0x90
#define PS8XXX_REG_VENDOR_ID_L                  0x00
#define PS8XXX_REG_VENDOR_ID_H                  0x01
#define MUX_IN_HPD_ASSERTION_REG                0xD0
#define IN_HPD  (1 << 0)
#define HPD_IRQ (1 << 1)
#define PS8XXX_REG_MUX_DP_EQ_CONFIGURATION      0xD3
#define PS8XXX_REG_MUX_USB_C2SS_EQ              0xE7
#define PS8XXX_REG_MUX_USB_C2SS_HS_THRESHOLD    0xE8

#elif defined(CONFIG_USB_PD_TCPM_PS8805)
/* Vendor defined registers */
#define PS8XXX_PRODUCT_ID 0x8805

#define FW_VER_REG               0x82
#define MUX_IN_HPD_ASSERTION_REG 0xD0
#define IN_HPD  (1 << 0)
#define HPD_IRQ (1 << 1)

#endif

extern const struct tcpm_drv ps8xxx_tcpm_drv;
void ps8xxx_tcpc_update_hpd_status(int port, int hpd_lvl, int hpd_irq);
int ps8xxx_tcpc_get_fw_version(int port, int *version);

#ifdef CONFIG_CMD_I2C_STRESS_TEST_TCPC
extern struct i2c_stress_test_dev ps8xxx_i2c_stress_test_dev;
#endif /* defined(CONFIG_CMD_I2C_STRESS_TEST_TCPC) */

#endif /* defined(__CROS_EC_USB_PD_TCPM_PS8XXX_H) */
