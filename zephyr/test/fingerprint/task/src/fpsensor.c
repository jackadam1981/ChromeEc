/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "mock_fingerprint_algorithm.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>

#include <drivers/fingerprint.h>
#include <ec_commands.h>
#include <host_command.h>

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, mkbp_send_event, uint8_t);

ZTEST_USER(fpsensor, test_tpm_seed_init)
{
	struct ec_response_fp_encryption_status status;
	struct ec_params_fp_seed params = {
		.struct_version = 4,
		.reserved = 0,
		.seed = "very_secret_32_bytes_of_tpm_seed",
	};

	/* Set TPM seed. */
	zassert_ok(ec_cmd_fp_seed(NULL, &params));

	/* Get FP encryption flags. */
	zassert_ok(ec_cmd_fp_encryption_status(NULL, &status));

	/* Confirm that FP_ENC_STATUS_SEED_SET is set. */
	zassert_true(status.valid_flags & FP_ENC_STATUS_SEED_SET);
	zassert_true(status.status & FP_ENC_STATUS_SEED_SET);
}

ZTEST_SUITE(fpsensor, NULL, NULL, NULL, NULL, NULL);
