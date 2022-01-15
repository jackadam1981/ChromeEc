/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __X86_NON_DSX_ADLP_H__
#define __X86_NON_DSX_ADLP_H__

#include <logging/log.h>
#include <x86_common_pwrseq.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>

extern struct power_seq_context pwrseq_ctx;

extern void gpio_set_lvl(const char *net_name, int val);
extern int gpio_get_lvl(const char *net_name);
extern struct common_pwrseq_config com_cfg;
extern void dsw_pwrok_pass_thru_handler(void);

struct chipset_pwrseq_config {
	int dsw_pwrok_delay_ms;
	int pch_pwrok_delay_ms;
	int sys_pwrok_delay_ms;
	int sys_reset_delay_ms;
	int vccst_pwrgd_delay_ms;
	int vrrdy_timeout_ms;
	int all_sys_pwrgd_timeout;
};

#endif /* __X86_NON_DSX_ADLP_H__ */
