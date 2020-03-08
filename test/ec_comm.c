/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test ec_comm
 */

#include "common.h"
#include "crc8.h"
#include "ec_comm.h"
#include "test_util.h"
#include "timer.h"
#include "tpm_nvmem.h"
#include "tpm_nvmem_ops.h"
#include "uart.h"
#include "util.h"
#include "vboot.h"

#define EC_HASH_EXPECTED	"\x40\x41\x42\x43\x44\x45\x46\x47" \
				"\x48\x49\x4A\x4B\x4C\x4D\x4E\x4F" \
				"\x50\x51\x52\x53\x54\x55\x56\x57" \
				"\x58\x59\x5A\x5B\x5C\x5D\x5E\x5F"

struct vb2_secdata_kernel test_secdata = {
	.struct_version = VB2_SECDATA_KERNEL_STRUCT_VERSION_MIN,
	.struct_size = sizeof(struct vb2_secdata_kernel),
	.reserved0 = 0,
	.kernel_versions = VB2_SECDATA_KERNEL_UID,
	.ec_hash = EC_HASH_EXPECTED,
};

union cr50_test_packet {
	struct cr50_comm_packet ph;
	uint8_t packet[CR50_COMM_MAX_PACKET_SIZE * 2];
};

union cr50_test_packet sample_packet_set_mode = {
	.ph.magic = CR50_COMM_MAGIC_WORD,
	.ph.version = CR50_COMM_VERSION,
	.ph.crc = 0,
	.ph.cmd = CR50_COMM_CMD_SET_BOOT_MODE,
	.ph.size = 1,
};

union cr50_test_packet sample_packet_verify_hash = {
	.ph.magic = CR50_COMM_MAGIC_WORD,
	.ph.version = CR50_COMM_VERSION,
	.ph.crc = 0,
	.ph.cmd = CR50_COMM_CMD_VERIFY_HASH,
	.ph.size = SHA256_DIGEST_SIZE,
	.ph.data = EC_HASH_EXPECTED,
};

/* EC Reset Count. It is used to see if ec has been reset. */
static int ec_reset_count_;

/*
 * Return 1 if EC has been reset since the last call of this function, or
 *        0 otherwise.
 */
static int ec_has_reset(void)
{
	static int prev_ec_reset_count;

	if (prev_ec_reset_count == ec_reset_count_)
		return 0;

	prev_ec_reset_count = ec_reset_count_;
	return 1;
}

void board_reboot_ec_deferred(int usec_delay_used)
{
	/* ec_reset */
	ec_reset_count_++;
	ec_efs_reset();
}

enum tpm_read_rv read_tpm_nvmem(uint16_t object_index, uint16_t object_size,
				void *obj_value)
{
	/* Check the input parameter */
	if (object_index != KERNEL_NV_INDEX)
		return TPM_READ_NOT_FOUND;
	if (object_size != test_secdata.struct_size)
		return TPM_READ_TOO_SMALL;

	/*
	 * Copy the test_secdata to obj_value as if it was loaded
	 * from NVMEM.
	 */
	memcpy(obj_value, (void *)&test_secdata, object_size);

	return TPM_READ_SUCCESS;
}

/*
 * Return 1 if the byte is found in buf string.
 *        0 otherwise
 */
static int find_byte(const char *buf, uint8_t byte)
{
	int i = strlen(buf);

	while (i--) {
		if (*buf == byte)
			return 1;
		buf++;
	}
	return 0;
}

/*
 * Calculate CRC8 of the the given CR50 packet and fill it in
 * the packet.
 */
static void calculate_crc8(union cr50_test_packet *pk)
{
	const int offset_cmd = offsetof(struct cr50_comm_packet, cmd);

	/* Calculate the sample EC-CR50 packet.	*/
	pk->ph.crc = crc8((uint8_t *)&pk->ph.cmd,
			  (sizeof(struct cr50_comm_packet)
			  + pk->ph.size - offset_cmd));
}

/*
 * Test EC CR50 communication with the given EC-CR50 packet.
 *
 * @param pk         an ec_cr50 comm packet to test
 * @param preambles  the number of preambles
 * @param resp_exp   the expected response in two bytes as in
 *                   CR50_COMM_RESPONSE.
 *                   if it is zero, then no response is expected.
 */
static int test_ec_comm(union cr50_test_packet *pk, int preambles,
			uint16_t resp_exp)
{
	uint8_t buf[256];
	int leng;
	int i;
	const char *resp;

	leng = sizeof(struct cr50_comm_packet) + pk->ph.size;

	/* Prepare the input packet. */
	for (i = 0; i < preambles; i++)
		buf[i] = CR50_COMM_PREAMBLE;
	memcpy(&buf[i], pk, leng);

	/* Start the test */
	ec_comm_packet_mode_en(1);
	TEST_ASSERT(ec_comm_is_uart_in_packet_mode(UART_EC));

	test_capture_console(1);

	i = 0;
	while (i < (leng + preambles))
		ec_comm_process_packet(buf[i++]);

	resp = test_get_captured_console();

	if (resp_exp)
		TEST_ASSERT(*(uint16_t *)resp == resp_exp);
	else
		/* Check if there was any EC-CR50-comm response. */
		TEST_ASSERT(find_byte(resp, '\xec') == 0);

	test_capture_console(0);

	ec_comm_packet_mode_dis(1);
	TEST_ASSERT(!ec_comm_is_uart_in_packet_mode(UART_EC));

	return EC_SUCCESS;
}

/*
 * Test the failure case for packet errors.
 */
static int test_ec_comm_packet_failure(void)
{
	/* Copy the sample packet to buffer. */
	union cr50_test_packet pk = sample_packet_verify_hash;
	int preambles = MIN_LENGTH_PREAMBLE;

	ec_has_reset();

	/* Test 1: Test with less preambles than required. */
	calculate_crc8(&pk);
	TEST_ASSERT(!test_ec_comm(&pk, 1, 0));

	/* Test 2: Test a wrong magic */
	pk.ph.magic = 0x1234;
	calculate_crc8(&pk);
	TEST_ASSERT(!test_ec_comm(&pk, preambles, CR50_COMM_ERROR_MAGIC));

	/* Test 3: Test with a wrong CRC */
	pk = sample_packet_verify_hash;
	calculate_crc8(&pk);
	pk.ph.crc += 0x01;	/* corrupt the CRC */
	TEST_ASSERT(!test_ec_comm(&pk, preambles, CR50_COMM_ERROR_CRC));

	/* Test 4: Test with too large payload */
	pk = sample_packet_verify_hash;
	pk.ph.size = SHA256_DIGEST_SIZE + 1;
	pk.ph.data[SHA256_DIGEST_SIZE] = 0xff;
	calculate_crc8(&pk);

	TEST_ASSERT(!test_ec_comm(&pk, preambles, CR50_COMM_ERROR_SIZE));

	/* Test 5: Test with a undefined command */
	pk = sample_packet_verify_hash;
	pk.ph.cmd = 0x1000;
	calculate_crc8(&pk);
	TEST_ASSERT(!test_ec_comm(&pk, preambles,
				  CR50_COMM_ERROR_UNDEFINED_CMD));

	/* Test 6: Test with a wrong struct version */
	pk = sample_packet_verify_hash;
	pk.ph.version = CR50_COMM_VERSION + 0x01;
	calculate_crc8(&pk);
	TEST_ASSERT(!test_ec_comm(&pk, preambles,
				  CR50_COMM_ERROR_STRUCT_VERSION));

	/* Check if ec has ever been reset during these tests */
	TEST_CHECK(!ec_has_reset());
}

/*
 * Test the successful case for verify_hash command.
 */
static int test_ec_comm_set_boot_mode(void)
{
	/* Copy the sample packet to buffer. */
	union cr50_test_packet pk = sample_packet_set_mode;
	int preambles;

	ec_has_reset();

	/* Test 1: Attempt to set boot mode to NORMAL. */
	pk.ph.data[0] = EC_EFS_BOOT_MODE_NORMAL;
	calculate_crc8(&pk);
	preambles = MIN_LENGTH_PREAMBLE * 2;
	TEST_ASSERT(!test_ec_comm(&pk, preambles, CR50_COMM_SUCCESS));
	TEST_ASSERT(!ec_has_reset());	/* EC must not be reset. */

	/* Test 2: Attempt to set boot mode to NORMAL again. */
	preambles = MIN_LENGTH_PREAMBLE;
	TEST_ASSERT(!test_ec_comm(&pk, preambles, CR50_COMM_SUCCESS));
	TEST_ASSERT(!ec_has_reset());	/* EC must not be reset. */

	/*
	 * Test 3: Attempt to set boot mode to NO BOOT.
	 *         EC should not be reset with this boot mode change from NORMAL
	 *         to NO_BOOT.
	 */
	pk.ph.data[0] = EC_EFS_BOOT_MODE_NO_BOOT;
	calculate_crc8(&pk);
	TEST_ASSERT(!test_ec_comm(&pk, preambles, CR50_COMM_SUCCESS));
	TEST_ASSERT(!ec_has_reset());	/* EC must not be reset. */

	/*
	 * Test 4: Attempt to set boot mode to NORMAL.
	 *         EC should be reset with this boot mode change from NO_BOOT
	 *         to NORMAL.
	 */
	pk.ph.data[0] = EC_EFS_BOOT_MODE_NORMAL;
	calculate_crc8(&pk);
	TEST_ASSERT(!test_ec_comm(&pk, preambles, 0));
	TEST_ASSERT(ec_has_reset());	/* EC must be reset. */

	return EC_SUCCESS;
}

/*
 * Test the successful case for verify_hash command.
 */
static int test_ec_comm_verify_hash(void)
{
	/* Copy the sample packet to buffer. */
	union cr50_test_packet pk = sample_packet_verify_hash;
	int preambles = MIN_LENGTH_PREAMBLE;

	ec_has_reset();

	/* Test 1: Attempt to verify EC Hash. */
	calculate_crc8(&pk);
	preambles = MIN_LENGTH_PREAMBLE * 2;
	TEST_ASSERT(!test_ec_comm(&pk, preambles, CR50_COMM_SUCCESS));
	TEST_ASSERT(!ec_has_reset());

	/* Test 2: Attempt to verify EC Hash again. */
	preambles = MIN_LENGTH_PREAMBLE;
	TEST_ASSERT(!test_ec_comm(&pk, preambles, CR50_COMM_SUCCESS));
	TEST_ASSERT(!ec_has_reset());

	/* Test 3: Attempt to verify a wrong EC Hash. */
	pk.ph.data[0] ^= 0xff;	/* corrupt the payload */
	calculate_crc8(&pk);
	TEST_ASSERT(!test_ec_comm(&pk, preambles, CR50_COMM_ERROR_BAD_PAYLOAD));
	TEST_ASSERT(!ec_has_reset());	/* EC should not be reset though. */

	/* Test 4: Attempt to verify a wrong EC Hash again. */
	TEST_ASSERT(!test_ec_comm(&pk, preambles, CR50_COMM_ERROR_BAD_PAYLOAD));
	TEST_ASSERT(!ec_has_reset());	/* EC should not be reset though. */

	/*
	 * Test 5: Attempt to verify the correct EC Hash.
	 *         EC should be reset because EC Boot mode is NO BOOT.
	 */
	pk = sample_packet_verify_hash;
	calculate_crc8(&pk);
	preambles = MIN_LENGTH_PREAMBLE * 2;
	TEST_ASSERT(!test_ec_comm(&pk, preambles, 0));
	TEST_ASSERT(ec_has_reset());	/* EC must be reset. */

	/* Check if ec has ever been reset during these tests */
	return EC_SUCCESS;
}

void run_test(void)
{
	uint8_t size_to_crc;

	/* Calculate the CRC8 for the sample kernel secdata. */
	size_to_crc = test_secdata.struct_size -
		      offsetof(struct vb2_secdata_kernel, crc8) -
		      sizeof(test_secdata.crc8);
	test_secdata.crc8 = crc8((uint8_t *)&test_secdata.reserved0,
				  size_to_crc);

	/* Module init */
	board_reboot_ec_deferred(0);
	ec_efs_refresh();

	/* Start test */
	test_reset();

	RUN_TEST(test_ec_comm_packet_failure);
	RUN_TEST(test_ec_comm_set_boot_mode);
	RUN_TEST(test_ec_comm_verify_hash);

	test_print_result();
}
