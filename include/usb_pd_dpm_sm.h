/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Protocol Layer module */

#ifndef __CROS_EC_USB_PD_DPM_SM_H
#define __CROS_EC_USB_PD_DPM_SM_H
#include "common.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "usb_sm.h"
#include "timer.h"
#include "usb_pd_tcpm.h"

/**
 * Runs the Protocol Layer State Machine
 *
 * @param port USB-C port number
 * @param evt  system event, ie: PD_EVENT_RX
 * @param en   0 to disable the machine, 1 to enable the machine
 */
void dpm_run(int port, int evt, int en);

#endif /* __CROS_EC_USB_PD_DPM_SM_H */
