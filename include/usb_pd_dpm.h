/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Device Policy Manager implementation
 * Refer to USB PD 3.0 spec, version 2.0, sections 8.2 and 8.3
 */

#ifndef __CROS_EC_USB_DPM_H
#define __CROS_EC_USB_DPM_H

/*
 * Initializes DPM state for a port.
 *
 * @param port USB-C port number
 */
void dpm_init(int port);

/*
 * Informs the DPM that the mode entry sequence (including appropriate
 * configuration) is done for a port.
 *
 * @param port USB-C port number
 */
void dpm_set_mode_entry_done(int port);

/*
 * Drives the Policy Engine through the mode entry/configuration process. Each
 * call to this function requests that the PE send one VDM, whichever is next in
 * the mode entry sequence. This only happens if preconditions for mode entry
 * are met.
 *
 * @param port USB-C port number
 */
void dpm_attempt_mode_entry(int port);

#endif  /* __CROS_EC_USB_DPM_H */
