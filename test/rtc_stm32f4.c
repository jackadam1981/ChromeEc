/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock_chip.h"
#include "test_util.h"

extern int handle_command(char *input);


test_static int test_rtc_alarm(void)
{
	struct ec_params_flash_protect params;
	struct ec_response_flash_protect resp;
	int res;

	char input1[]="md 0x2000cfd8 2";
	char input2[]="md 0x2000cf67 2";
	char input3[]="rw 0x2000cf67 0x38";
	char input4[]="md 0x2000cf67";


	res = handle_command(input1);
	cflush();
	ccprints ("after md command is: %d\n", res);

	params.flags = 0; //EC_FLASH_PROTECT_ALL_NOW;
	params.mask = 0; // EC_FLASH_PROTECT_ALL_NOW;

	res = test_send_host_command(EC_CMD_FLASH_PROTECT, 1, &params, sizeof(params), &resp, sizeof(resp));
	cflush();
	ccprints ("after flash protect command is: %d\n", res);


	res = handle_command(input2);
	cflush();
	ccprints ("after md command is: %d\n", res);


	res = handle_command(input3);
	cflush();
	ccprints ("after rw command is: %d\n", res);

	res = handle_command(input4);
	cflush();
	ccprints ("after md command is: %d\n", res);

	return EC_SUCCESS;
}


void run_test(int argc, const char **argv)
{
	test_reset();

	RUN_TEST(test_rtc_alarm);
	//RUN_TEST(test_rtc_match_delay);

	test_print_result();
}
