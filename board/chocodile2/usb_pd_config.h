/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Power delivery board configuration */

#ifndef __CROS_EC_USB_PD_CONFIG_H
#define __CROS_EC_USB_PD_CONFIG_H

int pd_adc_read(int port, int cc);
void pd_select_polarity(int port, int polarity);

static inline void pd_set_host_mode(int port, int enable)
{
	/* Do nothing */
}

#endif
