/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ISH_AON_SHARE_H
#define __CROS_EC_ISH_AON_SHARE_H

#include "ia_structs.h"

typedef struct {
	tss_t *tss_ptr;
	uint32_t ldt_ptr;
	uint32_t ldt_size;
	int pm_state;
	idt_ptr_t main_fw_idt_ptr;
} __attribute__((packed)) ish_aon_share_t;

#endif /* __CROS_EC_ISH_AON_SHARE_H */
