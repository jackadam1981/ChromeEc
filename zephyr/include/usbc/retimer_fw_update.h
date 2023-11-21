/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Source file for PD task to configure USB-C Alternate modes on Intel SoC.
 */

int usb_retimer_fw_update_get_result(void);
void usb_retimer_fw_update_process_op(int port, int op);
