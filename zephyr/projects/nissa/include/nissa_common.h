/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nissa common declarations */

#ifndef __CROS_EC_NISSA_NISSA_COMMON_H__
#define __CROS_EC_NISSA_NISSA_COMMON_H__

#include "usb_mux.h"

/** True if the board supports an HDMI sub-board. */
#define NISSA_BOARD_HAS_HDMI_SUPPORT DT_NODE_EXISTS(DT_NODELABEL(gpio_hdmi_sel))

enum nissa_sub_board_type {
	NISSA_SB_UNKNOWN = -1, /* Uninitialised */
	NISSA_SB_NONE = 0, /* No board defined */
	NISSA_SB_C_A = 1, /* USB type C, USB type A */
	NISSA_SB_C_LTE = 2, /* USB type C, WWAN LTE */
	NISSA_SB_HDMI_A = 3, /* HDMI, USB type A */
};

extern struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT];

enum nissa_sub_board_type nissa_get_sb_type(void);

#if NISSA_BOARD_HAS_HDMI_SUPPORT
/**
 * Configure the GPIO that controls core rails on the HDMI sub-board.
 *
 * This is the gpio_en_rails_odl pin, which is configured as active-low
 * open-drain output to enable power to the HDMI sub-board (typically when the
 * AP is in S5 or above).
 *
 * This function must be called if the pin is connected to the HDMI board and
 * power is not enabled by default.
 */
void nissa_configure_hdmi_rails(void);

/**
 * Configure the GPIO that controls the HDMI VCC pin on the HDMI sub-board.
 *
 * This is the gpio_hdmi_en_odl pin, which is configured as active-low
 * open-drain output to enable the VCC pin on the HDMI connector (typically when
 * the AP is on, in S0 or S0ix).
 *
 * This function must be called if the pin is connected to the HDMI board and
 * VCC is not enabled by default.
 */
void nissa_configure_hdmi_vcc(void);

/**
 * Configure the GPIOS controlling HDMI sub-board power (core rails and VCC).
 *
 * This function is called from shared code while configuring sub-boards, and
 * used if an HDMI sub-board is present. The default implementation enables the
 * core rails control pin (nissa_configure_hdmi_rails) but not VCC
 * (nissa_configure_hdmi_vcc), assuming that the pin for VCC is not connected
 * connected on most boards (and that VCC will be turned on whenever the core
 * rails are turned on).
 *
 * A board should override this function if it needs to enable more IOs for
 * HDMI, or if some pins need to be conditionally enabled.
 */
__overridable void nissa_configure_hdmi_power_gpios(void);
#endif /* NISSA_BOARD_HAS_HDMI_SUPPORT */

#endif /* __CROS_EC_NISSA_NISSA_COMMON_H__ */
