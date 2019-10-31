/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Shared USB-C policy for Volteer boards */

#include "compile_time_macros.h"
#include "usb_pd.h"

/* TODO(b/140578872): Whole file copied from zork. Figure out correct values for
 * volteer. */
#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP |\
			 PDO_FIXED_COMM_CAP)

const uint32_t pd_src_pdo[] = {
	PDO_FIXED(5000, 1500, PDO_FIXED_FLAGS),
};
const int pd_src_pdo_cnt = ARRAY_SIZE(pd_src_pdo);

const uint32_t pd_snk_pdo[] = {
	PDO_FIXED(5000, 500, PDO_FIXED_FLAGS),
	PDO_BATT(4750, 21000, 15000),
	PDO_VAR(4750, 21000, 3000),
};
const int pd_snk_pdo_cnt = ARRAY_SIZE(pd_snk_pdo);

int pd_check_data_swap(int port, int data_role)
{
	/* Allow data swap if we are a UFP, otherwise don't allow. */
	return (data_role == PD_ROLE_UFP);
}
