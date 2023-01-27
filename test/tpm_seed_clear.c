/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "board.h"
#include "common.h"
#include "ec_commands.h"
#include "fpc_private.h"
#include "test_util.h"

#ifdef SECTION_IS_RW
#include "fpc/fpc_sensor.h"
static const uint32_t fp_sensor_hwid = FP_SENSOR_HWID;
#include "fpsensor_state.h"
#else
static const uint32_t fp_sensor_hwid = UINT32_MAX;
#endif

#include <stdint.h>

const uint8_t default_fake_tpm_seed[] = {
	0xd9, 0x71, 0xaf, 0xc4, 0xcd, 0x36, 0xe3, 0x60, 0xf8, 0x5a, 0xa0,
	0xa6, 0x2c, 0xb3, 0xf5, 0xe2, 0xeb, 0xb9, 0xd8, 0x2f, 0xb5, 0x78,
	0x5c, 0x79, 0x82, 0xce, 0x06, 0x3f, 0xcc, 0x23, 0xb9, 0xe7,
};

test_static int test_try(void)
{
	memcpy(tpm_seed, default_fake_tpm_seed, FP_CONTEXT_TPM_BYTES);

	ccprints("The size of tpm_seed %d\n", (int32_t)sizeof(tpm_seed));
	for (int i = 0; i < sizeof(tpm_seed); ++i) {
		ccprints("tpm_seed[%d]: (0x%x)", i, tpm_seed[i]);
	}

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	RUN_TEST(test_try);
	test_print_result();
}
