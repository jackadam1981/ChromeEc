/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test motion sense hid code.
 */
#include <math.h>
#include <stdio.h>

#include "accelgyro.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "motion_lid.h"
#include "motion_sense.h"
#include "motion_sense_hid.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"


#define TEST_LID_EC_RATE (10 * MSEC)

/*****************************************************************************/
/* Mock functions */
static int accel_init(const struct motion_sensor_t *s)
{
	return EC_SUCCESS;
}

static int accel_read(const struct motion_sensor_t *s, vector_3_t v)
{
	rotate(s->xyz, *s->rot_standard_ref, v);
	return EC_SUCCESS;
}

static int accel_set_range(const struct motion_sensor_t *s,
				const int range,
				const int rnd)
{
	return EC_SUCCESS;
}

static int accel_get_range(const struct motion_sensor_t *s)
{
	return 0;
}

static int accel_set_resolution(const struct motion_sensor_t *s,
				const int res,
				const int rnd)
{
	return EC_SUCCESS;
}

static int accel_get_resolution(const struct motion_sensor_t *s)
{
	return 0;
}

int test_data_rate[2] = { 0 };

static int accel_set_data_rate(const struct motion_sensor_t *s,
				const int rate,
				const int rnd)
{
	test_data_rate[s - motion_sensors] = rate | (rnd ? ROUND_UP_FLAG : 0);
	return EC_SUCCESS;
}

static int accel_get_data_rate(const struct motion_sensor_t *s)
{
	return test_data_rate[s - motion_sensors];
}

const struct accelgyro_drv test_motion_sense = {
	.init = accel_init,
	.read = accel_read,
	.set_range = accel_set_range,
	.get_range = accel_get_range,
	.set_resolution = accel_set_resolution,
	.get_resolution = accel_get_resolution,
	.set_data_rate = accel_set_data_rate,
	.get_data_rate = accel_get_data_rate,
};

struct motion_sensor_t motion_sensors[] = {
	{.name = "base",
	 .active_mask = SENSOR_ACTIVE_S0_S3_S5,
	 .chip = MOTIONSENSE_CHIP_LSM6DS0,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &test_motion_sense,
	 .rot_standard_ref = NULL,
	 .default_range = 2,  /* g, enough for laptop. */
	 .config = {
		 /* AP: by default shutdown all sensors */
		 [SENSOR_CONFIG_AP] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
		 /* EC use accel for angle detection */
		 [SENSOR_CONFIG_EC_S0] = {
			 .odr = 119000 | ROUND_UP_FLAG,
			 .ec_rate = TEST_LID_EC_RATE
		 },
		 /* Used for double tap */
		 [SENSOR_CONFIG_EC_S3] = {
			 .odr = 119000 | ROUND_UP_FLAG,
			 .ec_rate = TEST_LID_EC_RATE * 100,
		 },
		 [SENSOR_CONFIG_EC_S5] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
	 },
	},
	{.name = "lid",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_KXCJ9,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &test_motion_sense,
	 .rot_standard_ref = NULL,
	 .default_range = 2,  /* g, enough for laptop. */
	 .config = {
		 /* AP: by default shutdown all sensors */
		 [SENSOR_CONFIG_AP] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
		 /* EC use accel for angle detection */
		 [SENSOR_CONFIG_EC_S0] = {
			 .odr = 119000 | ROUND_UP_FLAG,
			 .ec_rate = TEST_LID_EC_RATE,
		 },
		 /* Used for double tap */
		 [SENSOR_CONFIG_EC_S3] = {
			 .odr = 200000 | ROUND_UP_FLAG,
			 .ec_rate = TEST_LID_EC_RATE * 100,
		 },
		 [SENSOR_CONFIG_EC_S5] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
	 },
	},
};

void (*send_response)(int len);

const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

static struct hid_descriptor hid_desc = {
	.wHIDDescLength = 0x001e, // 0x1e
	.bcdVersion = 0x0100,
	.wReportDescLength = sizeof(report_desc),
	.wReportDescRegister = REPORT_DESC_REGISTER,
	.wInputRegister = INPUT_REPORT_REGISTER,
	.wMaxInputLength = I2C_HID_HEADER_SIZE +
	sizeof(struct hid_accel_input_report),
	.wOutputRegister = 0,
	.wMaxOutputLength = 0,
	.wCommandRegister = COMMAND_REGISTER,
	.wDataRegister = DATA_REGISTER
};

/* Emulate host by sending set sampling frequency report */
static int test_set_accel_sampling_freq(void)
{
/* |buffer| is expected to contain the values written to the command register
 * followed by the values written to the data register, upon receiving a
 * SET_REPORT command, in the following byte sequence format:
 *
 *   00 30 - command register address (0x3000)
 *   xx    - report type and ID
 *   03    - SET_REPORT
 *   00 30 - data register address (0x3000)
 *   xx xx - length
 *   xx    - report ID
 *   xx... - report data from byte 9
 *
 * Note that command register and data register have the same address. Also,
 * any report ID >= 15 requires an extra byte after the SET_REPORT byte, which
 * is not supported here as we don't have any report ID >= 15.
 *
 * In summary, we expect |buffer| contains at least 10 bytes where the report
 * data starts at buffer[9]. If |buffer| contains the incorrect number bytes,
 * we ignore the report.
 */

// host buffer received with set_report cmd and odr to set
	uint8_t buffer[13];
	uint32_t odr;

	buffer[0] = 0x00; // cmd reg lsb
	buffer[1] = 0x30; // cmd reg msb
	buffer[2] = 0x31; // report type (3 for feature report) & report ID
	buffer[3] = 0x03; // set_report cmd opcode
	buffer[4] = 0x00; // data reg same as cmd reg. lsb
	buffer[5] = 0x30; // data reg msb
	buffer[6] = 0x00; // report len lsb
	buffer[7] = 0x04; // report len msb. len=4 as odr is stored as int32_t
	buffer[8] = REPORT_ID_BASE_ACCEL_SAMPLING_RATE; // report ID
	buffer[9] = 0x05; // odr data starts here
	buffer[10] = 0x0;
	buffer[11] = 0x0;
	buffer[12] = 0x0;

	odr = i2c_hid_process(sizeof(buffer), buffer, send_response);
	printf("ODR set is: %d\n", odr);
	TEST_ASSERT(odr == (buffer[9] & 0x0F));
	return EC_SUCCESS;

}

/* Emulate host by requesting for hid descriptor */
static int test_read_hid_descr(void)
{
	int HIDDescLength, bcdVersion, ReportDescLength;
	int ReportDescRegister, InputRegister, MaxInputLength, OutputRegister;
	int MaxOutputLength, CommandRegister, DataRegister;
	uint8_t buffer[30]; // max hid_descr len=30 bytes

	buffer[0] = 0x01; // hid_descr cmd reg lsb
	buffer[1] = 0x00; // hid_descr cmd reg msb
	printf("HID Descr len: %d\n", sizeof(hid_desc));
	printf("Report Descr len: %x\n", sizeof(report_desc));
	printf("wMaxInputLength: %x\n", hid_desc.wMaxInputLength);

	i2c_hid_process(sizeof(buffer), buffer, send_response);
	printf("In test read hid descr fn\n");
	// check that response in i2c buffer matches hid_descr exactly
	HIDDescLength = buffer[0] & 0xFF;
	TEST_ASSERT(HIDDescLength == hid_desc.wHIDDescLength);

	bcdVersion = buffer[3] << 8 | buffer[2];
	TEST_ASSERT(bcdVersion == hid_desc.bcdVersion);

	ReportDescLength = buffer[5] << 8 | buffer[4];
	TEST_ASSERT(ReportDescLength == hid_desc.wReportDescLength);

	ReportDescRegister = buffer[7] << 8 | buffer[6];
	TEST_ASSERT(ReportDescRegister == hid_desc.wReportDescRegister);

	InputRegister = buffer[9] << 8 | buffer[8];
	TEST_ASSERT(InputRegister == hid_desc.wInputRegister);

	MaxInputLength = buffer[11] << 8 | buffer[10];
	TEST_ASSERT(MaxInputLength == hid_desc.wMaxInputLength);

	OutputRegister = buffer[13] << 8 | buffer[12];
	TEST_ASSERT(OutputRegister == hid_desc.wOutputRegister);

	MaxOutputLength = buffer[15] << 8 | buffer[14];
	TEST_ASSERT(MaxOutputLength == hid_desc.wMaxOutputLength);

	CommandRegister = buffer[17] << 8 | buffer[16];
	TEST_ASSERT(CommandRegister == hid_desc.wCommandRegister);

	DataRegister = buffer[19] << 8 | buffer[18];
	TEST_ASSERT(DataRegister == hid_desc.wDataRegister);
	return EC_SUCCESS;

}

/* Emulate host by requesting for report descriptor */
static int test_read_report_descr(void)
{
	uint8_t buffer[sizeof(report_desc)];
	int same;

	buffer[0] = 0x00; // report_descr cmd reg lsb
	buffer[1] = 0x10; // report_descr cmd reg msb
	printf("In test read report descr:\n");
	same = i2c_hid_process(sizeof(buffer), buffer, send_response);
	TEST_ASSERT(same == 1);
	return EC_SUCCESS;

}

/* Emulate host by requesting for accel data as input report */
static int test_input_report(void)
{
// Host buffer received with get_report cmd
	uint8_t buffer[9];
	int response_len;
	struct motion_sensor_t *sensor;

	buffer[0] = 0x00; // cmd reg lsb
	buffer[1] = 0x30; // cmd reg msb
	buffer[2] = 0x11; // report type (1 for input report) & report ID
	buffer[3] = 0x02; // get_report cmd opcode
	buffer[4] = 0x00; // data reg same as cmd reg. lsb
	buffer[5] = 0x30; // data reg msb
	buffer[6] = 0x00; // report len lsb
	buffer[7] = 0x04; // report len msb. len=4 as odr is stored as int32_t
	buffer[8] = REPORT_ID_BASE_ACCEL; // report ID

// Create dummy sensor and set x,y,z values to return in input report
	sensor = &motion_sensors[0];
	sensor->xyz[X] = 4;
	sensor->xyz[Y] = 5;
	sensor->xyz[Z] = 6;
	hid_compile_dummy_input(sensor);
	response_len = i2c_hid_process(sizeof(buffer), buffer, send_response);
	printf("Input response len is: %d\n", response_len);
	TEST_ASSERT(response_len == sizeof(struct hid_accel_input_report)
	+ I2C_HID_HEADER_SIZE);
	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();
	RUN_TEST(test_read_report_descr);
	RUN_TEST(test_set_accel_sampling_freq);
	RUN_TEST(test_read_hid_descr);
	RUN_TEST(test_input_report);

	test_print_result();
}

