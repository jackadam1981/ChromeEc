/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Mock of Device Policy Manager implementation
 * Refer to USB PD 3.0 spec, version 2.0, sections 8.2 and 8.3
 */

#include "usb_pd.h"
#include "usb_pd_dpm.h"

static struct {
	bool mode_entry_done;
	bool mode_exit_request;
} dpm[CONFIG_USB_PD_PORT_MAX_COUNT];

void dpm_init(int port)
{
	dpm[port].mode_entry_done = false;
	dpm[port].mode_exit_request = false;
}

void dpm_vdm_acked(int port, enum tcpm_transmit_type type, int vdo_count,
		uint32_t *vdm)
{
}

void dpm_vdm_naked(int port, enum tcpm_transmit_type type, uint16_t svid,
		uint8_t vdm_cmd)
{
}

void dpm_run(int port)
{
}
