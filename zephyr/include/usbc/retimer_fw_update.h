/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Header file to expose retimer firmware update APIs.
 */

int usb_retimer_fw_update_get_result(void);
void usb_retimer_fw_update_process_op(int port, int op);
