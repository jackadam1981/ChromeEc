/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "usb_sm.h"
#include "util.h"

extern const struct test_sm_data test_tc_sm_data[];
extern const int test_tc_sm_data_size;
extern const struct test_sm_data test_pe_sm_data[];
extern const int test_pe_sm_data_size;
extern const struct test_sm_data test_prl_sm_data[];
extern const int test_prl_sm_data_size;

static int print_exec(int port, state_execution exec, int start_end,
		      const struct test_sm_data *data, int data_size)
{
	const char *exec_type = NULL;
	int di, ni;

	for (di = 0; di < data_size; di++) {
		for (ni = 0; ni < data[di].names_size; ni++) {
			if (exec == data[di].base[ni].entry) {
				exec_type = "entry";
				break;
			} else if (exec == data[di].base[ni].run) {
				exec_type = "run";
				break;
			} else if (exec == data[di].base[ni].exit) {
				exec_type = "exit";
				break;
			}
		}
		if (exec_type != NULL)
			break;
	}

	if (exec_type != NULL)
		ccprintf("p%d %s %s (%s)\n", port, data[di].names[ni],
			 exec_type, start_end == 0 ? "start" : "end");

	return (exec_type != NULL);
}

void trace_sm_execute(int port, state_execution exec, int start_end)
{
	if (print_exec(port, exec, start_end,
		       test_tc_sm_data, test_tc_sm_data_size))
		return;
	if (print_exec(port, exec, start_end,
		       test_pe_sm_data, test_pe_sm_data_size))
		return;
	if (print_exec(port, exec, start_end,
		       test_prl_sm_data, test_prl_sm_data_size))
		return;
	ccprintf("p%d UNKNOWN 0x%x (%s)\n", port, (uint32_t) exec,
		 start_end == 0 ? "start" : "end");
}
