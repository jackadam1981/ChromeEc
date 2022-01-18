/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Mock USB Policy Engine Sink / Source module */

/* #include "common.h" */
/* #include "console.h" */
/* #include "ec_commands.h" */
/* #include "usb_tc_sm.h" */
/* #include "mock/usb_tc_sm_mock.h" */
/* #include "memory.h" */

#include <stdint.h>
#ifndef CONFIG_COMMON_RUNTIME
#define cprints(format, args...)
#endif

#ifndef TEST_BUILD
#error "Mocks should only be in the test build."
#endif
int pd_dev_store_rw_hash(int port, uint16_t dev_id, uint32_t *rw_hash,
					uint32_t current_image)
{
	return 0;
/* 	pe[port].dev_id = dev_id; */
/* 	memcpy(pe[port].dev_rw_hash, rw_hash, PD_RW_HASH_SIZE); */
/* #ifdef CONFIG_CMD_PD_DEV_DUMP_INFO */
/* 	pd_dev_dump_info(dev_id, rw_hash); */
/* #endif */
/* 	pe[port].current_image = current_image; */

/* 	if (IS_ENABLED(CONFIG_USB_PD_HOST_CMD)) { */
/* 		int i; */

/* 		/\* Search table for matching device / hash *\/ */
/* 		for (i = 0; i < RW_HASH_ENTRIES; i++) */
/* 			if (dev_id == rw_hash_table[i].dev_id) */
/* 				return !memcmp(rw_hash, */
/* 					       rw_hash_table[i].dev_rw_hash, */
/* 					       PD_RW_HASH_SIZE); */
/* 	} */

/* 	return 0; */
}
