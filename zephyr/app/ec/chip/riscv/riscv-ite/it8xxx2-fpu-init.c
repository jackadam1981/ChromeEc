/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <zephyr/init.h>

static int configure_fcsr(const struct device *unused_device)
{
	/*
	 * Floating-point instructions with dynamic rounding mode will cause
	 * an illegal instruction trap if the frm bits in fcsr are set to an
	 * invalid value. Before any user may use floating-point, initialize
	 * frm to RNE mode and clear fflags while leaving the upper bits (which
	 * may be used by other extensions) untouched.
	 */
	__asm__ volatile(" jal t0, it8xxx2_fpu_enable\n"
			 " li t0, 0xff\n"
			 " not t0, t0\n"
			 " .word 0x00302373\n" /* frcsr t1 */
			 " and t0, t1, t0\n"
			 " .word 0x00329073\n" /* fscsr t0 */
			 ::
				 : "t0", "t1");
	return 0;
}
SYS_INIT(configure_fcsr, POST_KERNEL, 10);
