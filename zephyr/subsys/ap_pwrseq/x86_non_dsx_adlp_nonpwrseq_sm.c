/*
 * Copyright (c) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ap_pwrseq/x86_non_dsx_adlp_pwrseq_sm.h>

LOG_MODULE_DECLARE(ap_pwrseq, 4);

/* This will add all the customizable inputs */
int all_sys_pwrgd_handler()
{
	// override
	return 0;
}

int intel_x86_get_pg_ec_dsw_pwrok(void)
{
	// override
	return 0;
}

