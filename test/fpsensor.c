/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>
#include <stdbool.h>

#include "cryptoc/p256.h"
#include "ec_commands.h"
#include "mock/fpsensor_detect_mock.h"
#include "mock/fpsensor_state_mock.h"
#include "string.h"
#include "test_util.h"
#include "trng.h"
#include "common/fpsensor/fpsensor_private.h"

static const struct ec_response_get_protocol_info expected_info[] = {
	[FP_TRANSPORT_TYPE_SPI] = {
		.flags = 1,
		.max_response_packet_size = 544,
		.max_request_packet_size = 544,
		.protocol_versions = 8,
	},
	[FP_TRANSPORT_TYPE_UART] = {
		.flags = 1,
		.max_response_packet_size = 256,
		.max_request_packet_size = 544,
		.protocol_versions = 8,
	}
};

test_static int test_validate_fp_buffer_offset_success(void)
{
	TEST_EQ(validate_fp_buffer_offset(1, 0, 1), EC_SUCCESS, "%d");
	return EC_SUCCESS;
}

test_static int test_validate_fp_buffer_offset_failure_no_overflow(void)
{
	TEST_EQ(validate_fp_buffer_offset(1, 1, 1), EC_ERROR_INVAL, "%d");
	return EC_SUCCESS;
}

test_static int test_validate_fp_buffer_offset_failure_overflow(void)
{
	TEST_EQ(validate_fp_buffer_offset(1, UINT32_MAX, 1), EC_ERROR_OVERFLOW,
		"%d");
	return EC_SUCCESS;
}

test_static int test_host_command_protocol_info(
	enum fp_transport_type transport_type,
	const struct ec_response_get_protocol_info *expected)
{
	struct ec_response_get_protocol_info info;
	int rv;

	mock_ctrl_fpsensor_detect.get_fp_sensor_type_return =
		FP_SENSOR_TYPE_FPC;
	mock_ctrl_fpsensor_detect.get_fp_transport_type_return = transport_type;

	rv = test_send_host_command(EC_CMD_GET_PROTOCOL_INFO, 0, NULL, 0, &info,
				    sizeof(info));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	TEST_EQ(info.flags, expected->flags, "%d");
	TEST_EQ(info.max_request_packet_size, expected->max_request_packet_size,
		"%d");
	TEST_EQ(info.max_response_packet_size,
		expected->max_response_packet_size, "%d");
	TEST_EQ(info.protocol_versions, expected->protocol_versions, "%d");

	return EC_SUCCESS;
}

test_static int test_host_command_protocol_info_uart(void)
{
	return test_host_command_protocol_info(
		FP_TRANSPORT_TYPE_UART, &expected_info[FP_TRANSPORT_TYPE_UART]);
}

test_static int test_host_command_protocol_info_spi(void)
{
	return test_host_command_protocol_info(
		FP_TRANSPORT_TYPE_SPI, &expected_info[FP_TRANSPORT_TYPE_SPI]);
}

test_static int test_host_command_establish_and_load_pk(void)
{
	int rv;
	struct ec_params_fp_seed seed_params;
	struct ec_response_fp_establish_pk_keygen keygen_response;
	struct ec_params_fp_establish_pk_wrap wrap_params;
	struct ec_response_fp_establish_pk_wrap wrap_response;
	struct ec_params_fp_load_pk load_params;

	uint8_t privkey[FP_PK_EC_PRIVATE_KEY_LEN];
	p256_int n, x, y;

	seed_params.struct_version = FP_TEMPLATE_FORMAT_VERSION;
	memcpy(seed_params.seed, default_fake_tpm_seed,
	       sizeof(default_fake_tpm_seed));

	rv = test_send_host_command(EC_CMD_FP_SEED, 0, &seed_params,
				    sizeof(seed_params), NULL, 0);

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PK_KEYGEN, 0, NULL, 0,
				    &keygen_response, sizeof(keygen_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	memcpy(&wrap_params.enc_privkey_info, &keygen_response.enc_privkey_info,
	       sizeof(keygen_response.enc_privkey_info));

	memcpy(wrap_params.enc_privkey, keygen_response.enc_privkey,
	       sizeof(keygen_response.enc_privkey));

	trng_init();
	trng_rand_bytes(privkey, FP_PK_EC_PRIVATE_KEY_LEN);
	trng_exit();

	p256_from_bin(privkey, &n);
	p256_base_point_mul(&n, &x, &y);
	p256_to_bin(&x, wrap_params.peers_pubkey_x);
	p256_to_bin(&y, wrap_params.peers_pubkey_y);

	rv = test_send_host_command(EC_CMD_FP_ESTABLISH_PK_WRAP, 0,
				    &wrap_params, sizeof(wrap_params),
				    &wrap_response, sizeof(wrap_response));

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	memcpy(&load_params.enc_pk_info, &wrap_response.enc_pk_info,
	       sizeof(wrap_response.enc_pk_info));

	memcpy(load_params.enc_pk, wrap_response.enc_pk,
	       sizeof(wrap_response.enc_pk));

	rv = test_send_host_command(EC_CMD_FP_LOAD_PK, 0, &load_params,
				    sizeof(load_params), NULL, 0);

	TEST_EQ(rv, EC_RES_SUCCESS, "%d");

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	RUN_TEST(test_host_command_establish_and_load_pk);

	if (IS_ENABLED(HAS_TASK_FPSENSOR)) {
		/* TODO(b/171924356): The "emulator" build only builds RO and
		 *  the functions used in the tests are only in RW, so these
		 *  tests are not run on the emulator.
		 */
		RUN_TEST(test_validate_fp_buffer_offset_success);
		RUN_TEST(test_validate_fp_buffer_offset_failure_no_overflow);
		RUN_TEST(test_validate_fp_buffer_offset_failure_overflow);
	}

	/* The tests after this only work on device right now. */
	if (IS_ENABLED(EMU_BUILD)) {
		test_print_result();
		return;
	}

	if (argc < 2) {
		ccprintf("usage: runtest [uart|spi]\n");
		test_fail();
		return;
	}

	/* The transport type is cached in a static variable, so the tests
	 * cannot be run back to back (without reboot).
	 */
	if (strncmp(argv[1], "uart", 4) == 0 && IS_ENABLED(BOARD_BLOONCHIPPER))
		RUN_TEST(test_host_command_protocol_info_uart);
	else if (strncmp(argv[1], "spi", 3) == 0)
		RUN_TEST(test_host_command_protocol_info_spi);

	test_print_result();
}
