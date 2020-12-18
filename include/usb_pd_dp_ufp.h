/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Device Policy Manager implementation
 * Refer to USB PD 3.0 spec, version 2.0, sections 8.2 and 8.3
 */

#ifndef __CROS_EC_USB_PD_DP_UFP_H
#define __CROS_EC_USB_PD_DP_UFP_H

struct hpd_to_pd_config_t {
	int port;
	enum gpio_signal signal;
};

extern const struct hpd_to_pd_config_t hpd_config;
/*
 * Initializes DPM state for a port.
 *
 * @param port USB-C port number
 */
void usb_pd_hpd_edge_event(int signal);

/*
 * Initializes DPM state for a port.
 *
 * @param port USB-C port number
 */
void usb_pd_hpd_converter_enable(int enable);



#endif  /* __CROS_EC_USB_PD_DP_UFP_H */
