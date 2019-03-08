/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ISH_AON_SHARE_H
#define __CROS_EC_ISH_AON_SHARE_H

#include "ia_structs.h"

/* shared data structure between main FW and aontask */
struct ish_aon_share {
	/* aontask's TSS segement entry */
	struct tss_entry *aon_tss;
	/* aontask's LDT start address */
	uint32_t ldt_ptr;
	/* aontask's LDT's limit size */
	uint32_t ldt_size;
	/* current power state */
	int pm_state;
	/* for store/restore main FW's IDT */
	struct idt_header main_fw_idt;
} __packed;

#endif /* __CROS_EC_ISH_AON_SHARE_H */
