/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Global Access Registers */
#define RT3645_NVM_PROGRAM_STATUS 0xEC
#define RT3645_NVM_PROGRAM_CTRL 0xED
#define RT3645_PAGE 0xEF
#define RT3645_ENTER_CONFIG_MODE 0xF1
#define RT3645_NVM_FAULT_STATUS 0xF9
#define RT3645_NVM_CONFIG_SEL 0xFA
#define RT3645_PRODUCT_ID 0xFE

/* RT3645_NVM_PROGRAM_CTRL */
#define RT3645_NVM_PROGRAM_CTRL_RELOAD 0x66
#define RT3645_NVM_PROGRAM_CTRL_PROGRAM 0xA8
