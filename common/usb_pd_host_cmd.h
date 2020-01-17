/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Host commands header for USB-PD module.
 */

#ifndef __CROS_EC_USB_PD_HOST_CMD_H
#define __CROS_EC_USB_PD_HOST_CMD_H

/* 4 entry rw_hash table of type-C devices that AP has firmware updates for. */
#ifdef CONFIG_COMMON_RUNTIME
#define RW_HASH_ENTRIES 4
extern struct ec_params_usb_pd_rw_hash_entry rw_hash_table[RW_HASH_ENTRIES];
#endif /* CONFIG_COMMON_RUNTIME */

#endif /* __CROS_EC_USB_PD_HOST_CMD_H */
