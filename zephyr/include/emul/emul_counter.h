/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_EMUL_EMUL_COUNTER_H_
#define ZEPHYR_INCLUDE_EMUL_EMUL_COUNTER_H_

struct emul_counter_ctrl {
	bool alarm_tmr_start;
	bool top_tmr_start;
};

struct emul_counter_ctrl emul_get_counter_ctrl_reg(void);

#endif /* ZEPHYR_INCLUDE_EMUL_EMUL_COUNTER_H_ */
