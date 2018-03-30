/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery port management */
#ifndef __CROS_EC_DRIVER_TCPM_IT83XX_H
#define __CROS_EC_DRIVER_TCPM_IT83XX_H

#define TASK_EVENT_PHY_TX_DONE TASK_EVENT_CUSTOM((1 << 17))

enum usbpd_cc_pin {
	USBPD_CC_PIN_1,
	USBPD_CC_PIN_2,
};

enum usbpd_ufp_volt_status {
	USBPD_UFP_STATE_SNK_OPEN = 0,
	USBPD_UFP_STATE_SNK_DEF  = 1,
	USBPD_UFP_STATE_SNK_1_5  = 3,
	USBPD_UFP_STATE_SNK_3_0  = 7,
};

enum usbpd_dfp_volt_status {
	USBPD_DFP_STATE_SRC_RA   = 0,
	USBPD_DFP_STATE_SRC_RD   = 1,
	USBPD_DFP_STATE_SRC_OPEN = 3,
};

struct usbpd_ctrl_t {
	volatile uint8_t *cc1;
	volatile uint8_t *cc2;
	uint8_t irq;
};

extern const struct usbpd_ctrl_t usbpd_ctrl_regs[];
extern const struct tcpm_drv it83xx_tcpm_drv;

#endif /* __CROS_EC_DRIVER_TCPM_IT83XX_H */
