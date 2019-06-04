/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ec_commands.h"
#include "fpsensor_state.h"
#include "host_command.h"
#include "test_util.h"
#include "util.h"

test_static int test_fp_tpm_seed_not_set(void)
{
	int rv;
	struct ec_response_fp_encryption_status resp;

	memset(&resp, 0, sizeof(resp));
	/* Initially the seed should not have been set. */
	rv = test_send_host_command(EC_CMD_FP_ENC_STATUS, 0,
				    NULL, 0,
				    &resp, sizeof(resp));
	if (rv != EC_RES_SUCCESS || resp.status & FP_ENC_STATUS_SEED_SET) {
		ccprintf("%s:%s(): rv = %d, seed is set: %d\n", __FILE__,
			__func__, rv, resp.status & FP_ENC_STATUS_SEED_SET);
		return -1;
	}

	return EC_SUCCESS;
}

test_static int test_set_fp_tpm_seed(void)
{
	int rv;
	struct ec_params_fp_seed params;
	struct ec_response_fp_encryption_status resp;

	params.struct_version = FP_TEMPLATE_FORMAT_VERSION;
	params.seed[0] = 0;

	rv = test_send_host_command(EC_CMD_FP_SEED, 0,
					&params, sizeof(params),
					NULL, 0);
	if (rv != EC_RES_SUCCESS) {
		ccprintf("%s:%s(): rv = %d, set seed failed\n",
			__FILE__, __func__, rv);
		return -1;
	}

	memset(&resp, 0, sizeof(resp));
	/* Now seed should have been set. */
	rv = test_send_host_command(EC_CMD_FP_ENC_STATUS, 0,
					NULL, 0,
					&resp, sizeof(resp));
	if (rv != EC_RES_SUCCESS || !(resp.status & FP_ENC_STATUS_SEED_SET)) {
		ccprintf("%s:%s(): rv = %d, seed is set: %d\n", __FILE__,
			__func__, rv, resp.status & FP_ENC_STATUS_SEED_SET);
		return -1;
	}

	return EC_SUCCESS;
}

test_static int test_set_fp_tpm_seed_again(void)
{
	int rv;
	struct ec_params_fp_seed params;
	struct ec_response_fp_encryption_status resp;

	params.struct_version = FP_TEMPLATE_FORMAT_VERSION;
	params.seed[0] = 0;

	rv = test_send_host_command(EC_CMD_FP_SEED, 0,
					&params, sizeof(params),
					NULL, 0);
	if (rv != EC_RES_ACCESS_DENIED) {
		ccprintf("%s:%s(): rv = %d, setting seed the second time "
		"should result in EC_RES_ACCESS_DENIED but did not.\n",
			__FILE__, __func__, rv);
		return -1;
	}

	memset(&resp, 0, sizeof(resp));
	/* Now seed should still be set. */
	rv = test_send_host_command(EC_CMD_FP_ENC_STATUS, 0,
					NULL, 0,
					&resp, sizeof(resp));
	if (rv != EC_RES_SUCCESS || !(resp.status & FP_ENC_STATUS_SEED_SET)) {
		ccprintf("%s:%s(): rv = %d, seed is set: %d\n", __FILE__,
			__func__, rv, resp.status & FP_ENC_STATUS_SEED_SET);
		return -1;
	}

	return EC_SUCCESS;
}

void run_test(void)
{
	RUN_TEST(test_fp_tpm_seed_not_set);
	RUN_TEST(test_set_fp_tpm_seed);
	RUN_TEST(test_set_fp_tpm_seed_again);

	test_print_result();
}
