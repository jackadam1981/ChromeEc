/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_USB_UPGRADE_H
#define __CROS_EC_USB_UPGRADE_H

#define UNOFFICIAL_USB_SUBCLASS_GOOGLE_CR50   0x53

/* Commands from host */
#define UPGRADE_DONE          0xB007AB1E

extern void fw_upgrade_command_handler(void *body,
				       size_t cmd_size,
				       size_t *response_size);

#endif	/* __CROS_EC_USB_UPGRADE_H */
