/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Motion sense hid module */

#include "accelgyro.h"
#include "atomic.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gesture.h"
#include "hid.h"
#include "hooks.h"
#include "host_command.h"
#include "hwtimer.h"
#include "lid_angle.h"
#include "lightbar.h"
#include "math_util.h"
#include "mkbp_event.h"
#include "motion_sense.h"
#include "motion_sense_hid.h"
#include "motion_lid.h"
#include "power.h"
#include "queue.h"
#include "tablet_mode.h"
#include "timer.h"
#include "task.h"
#include "util.h"

#define INVALID_ID		-1

/* Console output macros */
#define CPUTS(outstr) cputs(CC_MOTION_SENSE, outstr)
#define CPRINTS(format, args...) cprints(CC_MOTION_SENSE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_MOTION_SENSE, format, ## args)

/*
 * Adjustment in us to ec rate when calculating interrupt interval:
 * To be sure the EC will send an interrupt even if it finishes processing
 * events slightly earlier than the previous period.
 */
#define MOTION_SENSOR_INT_ADJUSTMENT_US 10

/* Reports (double buffered) */
static struct hid_accel_input_report input_reports[2];

/* Current active report buffer index */
int report_active_index;

/* Sensor odr */
uint32_t base_accel_odr;

/* Map feature report ID to report/ sensor ID
 * Feature reports are treated as Top Level Collections, each with a report ID.
 * A sensor has several features e.g. sampling rate, report interval etc. so
 * each feature report needs to be mapped to the sensor it applies to.
 * One sensor has several feature reports mapped to it.
 */
static int hid_get_sensorid_from_featureid(int feature_id)
{
	if (feature_id >= REPORT_ID_BASE_ACCEL_SAMPLING_RATE
	 && feature_id <= REPORT_ID_BASE_ACCEL_REPORTING_STATE) {
		return REPORT_ID_BASE_ACCEL;
	} else
		return INVALID_ID;
}

/* HID functions */
static struct hid_descriptor hid_desc = {
	.wHIDDescLength = 30,
	.bcdVersion = 0x0100,
	.wReportDescLength = sizeof(report_desc),
	.wReportDescRegister = REPORT_DESC_REGISTER,
	.wInputRegister = INPUT_REPORT_REGISTER,
	.wMaxInputLength = I2C_HID_HEADER_SIZE +
	 sizeof(struct hid_accel_input_report),
	.wOutputRegister = 0,
	.wMaxOutputLength = 0,
	.wCommandRegister = COMMAND_REGISTER,
	.wDataRegister = DATA_REGISTER,
	.wVendorID = 0x18d1,
	.wProductID = 0x5037,
	.wVersionID = 0x6776
};

BUILD_ASSERT(sizeof(hid_desc) == 30);

/* INPUT REPORT STRUCTURE: REQUEST BUFFER
 * host_buffer[0] = cmd reg lsb
 * host_buffer[1] = cmd reg msb
 * host_buffer[2] = rpt type & rpt id
 * host_buffer[3] = cmd opcode (get/ set report)
 * host_buffer[4] = data reg lsb
 * host_buffer[5] = data reg lsb

 * INPUT REPORT STRUCTURE: RESPONSE BUFFER
 * host_buffer[0] = rpt len lsb   makes up I2C_HID_HEADER_SIZE
 * host_buffer[1] = rpt len msb   makes up I2C_HID_HEADER_SIZE
 * host_buffer[2] = rpt id   struct hid_accel_input_report starts here
 * host_buffer[3] = sensor_state data
 * host_buffer[4] = sensor_event data
 * host_buffer[5] = x data lsb
 * host_buffer[6] = x data msb
 * host_buffer[7] = y data lsb
 * host_buffer[8] = y data msb
 * host_buffer[9] = z data lsb
 * host_buffer[10] = z data msb
 *
 * buffer: pointer to device buffer for receiving/ sending data
 * report_id: ID of the input report requested
 * data: pointer to input_reports[] array, which stores hid_accel_input_report
 * structs
 * data_len: length in bytes of the hid_accel_input_report struct
 */
size_t hid_fill_buffer(uint8_t *buffer, uint8_t report_id, const void *data,
			  size_t data_len)
{
	size_t response_len = I2C_HID_HEADER_SIZE + data_len;

	buffer[0] = response_len & 0xFF; /* report len lsb. I2C_HID_HEADER */
	buffer[1] = (response_len >> 8) & 0xFF; /* rpt len. I2C_HID_HEADER */
	memcpy(buffer + I2C_HID_HEADER_SIZE, data, data_len);
#ifdef CONFIG_HID_SENSOR_SPOOF_MODE
	memcpy(spoof_host_buffer + I2C_HID_HEADER_SIZE, data, data_len);
#endif
	return response_len;
}

/* Fill input report struct with data so that hid_fill_buffer can use it*/
int hid_compile_input(int report_id)
{
	struct motion_sensor_t *sensor;
	int i;
	struct hid_accel_input_report input;

	sensor = hid_host_sensor_id_to_real_sensor(report_id);
	ccprintf("In hid compile input after spoofing x, y, z %d, %d, %d\n",
	sensor->xyz[X], sensor->xyz[Y], sensor->xyz[Z]);
	if (sensor == NULL)
		return EC_RES_INVALID_PARAM;

	mutex_lock(&g_sensor_mutex);

	input.report_id = report_id;
	input.sensor_state = 0;
	input.sensor_event = 0;
	input.x = sensor->xyz[X];
	input.y = sensor->xyz[Y];
	input.z = sensor->xyz[Z];
	ccprintf("In hid compile input: Get sensor x, y, z %d, %d, %d\n",
	input.x, input.y, input.z);
	for (i = 0; i < 2 ; i++)
		input_reports[i] = input; /* input reports are x2 buffered */
	mutex_unlock(&g_sensor_mutex);
	return 0;

}


#ifdef CONFIG_HID_SENSOR_SPOOF_MODE
void send_response(int len)
{
	ccprintf("Send_response should be implemented at chip level");
}

struct motion_sensor_t *get_spoof_sensor(void)
{
	return spoof_sensor;
}

/* Dummy fn for tests. Real fn defined at chip level*/
void spoof_send_response(uint8_t *buffer, int is_hid_desc)
{
	ccprintf("Send response buffer in spoof mode for tests");
	if (is_hid_desc == 1) {
		ccprintf("Retrieve HID_DESC_REGISTER in spoof mode");
		memcpy(buffer, &hid_desc, sizeof(hid_desc));
	}
	/* Else it's the report descriptor */
	else if (is_hid_desc == 0) {
		ccprintf("Retrieve REPORT_DESC_REGISTER in spoof mode");
		memcpy(buffer, &report_desc, sizeof(report_desc));
	} else {
		ccprintf("Spoof send_response fn does nothing");
	}
}

int get_new_odr(struct motion_sensor_t *sensor)
{
	/* Check if SET_REPORT sets the new sensor odr */
	return sensor->config[SENSOR_CONFIG_AP].odr;
}
#endif

/* Extracts report data from |buffer| into |data|.
 *
 * |buffer| is expected to contain the values written to the command register
 * followed by the values written to the data register, upon receiving a
 * SET_REPORT command, in the following byte sequence format:
 *
 *   00 30 - command register address (0x3000)
 *   xx    - report type and ID
 *   03    - SET_REPORT
 *   00 30 - data register address (0x3000)
 *   xx xx - length
 *   xx    - report ID
 *   xx... - report data
 *
 * Note that command register and data register have the same address. Also,
 * any report ID >= 15 requires an extra byte after the SET_REPORT byte, which
 * is not supported here as we don't have any report ID >= 15.
 *
 * In summary, we expect |buffer| contains at least 10 bytes where the report
 * data starts at buffer[9]. If |buffer| contains the incorrect number bytes,
 * we ignore the report.
 */
int hid_extract_report(uint64_t len, uint8_t *buffer, void *data,
			   uint64_t data_len)
{
	if (len == 9 + data_len)
		memcpy(data, buffer + 9, data_len);
#ifdef CONFIG_HID_SENSOR_SPOOF_MODE
		memcpy(data, spoof_host_buffer + 9, data_len);
#endif
		ccprintf("Data set in extract report%d\n", *((int *)data));
		return *((int *)data);
}

/* Function to map hid report IDs to motion sensor. */
struct motion_sensor_t
	*hid_host_sensor_id_to_real_sensor(int report_id)
{
	struct motion_sensor_t *sensor;
	int report_id_mapped;

	if (report_id > motion_sensor_count || report_id == 0)
		return NULL;
	/* As we can't use HID report ID of 0, we map to motion_sensors[] by
	 * subtracting 1 from the report ID
	 */
	report_id_mapped = report_id - 1;
	ccprintf("Report ID afer mapping %d\n", report_id_mapped);
#ifdef CONFIG_HID_SENSOR_SPOOF_MODE
	sensor = get_spoof_sensor();
#else
	sensor = &motion_sensors[report_id_mapped];
	/* if sensor is powered and initialized, return match */
#ifdef SENSOR_ACTIVE
	if (SENSOR_ACTIVE(sensor) && (sensor->state == SENSOR_INITIALIZED))
		return sensor;
#endif /* defined(SENSOR_ACTIVE) */
#endif
	/* If no match then the EC currently doesn't support ID received. */
	return sensor;
}


void i2c_hid_process(int data_len, uint8_t *buffer,
			void (*send_response)(int len))
{
	int invalid_reg;
	size_t response_len;
	uint16_t reg;

	invalid_reg = -1;
	if (data_len < 2)
		reg = INPUT_REPORT_REGISTER;
	else
		reg = *((uint16_t *)buffer);
	ccprintf("Processing  %x - %d\n", reg, data_len);
	switch (reg) {
	case EC_ACPI_HID_DESCRIPTOR_ADDR:
	/* Return HID descr to incompatible types when assigning to typehost */
		ccprintf("Retrieve HID_DESC_REGISTER %x\n", reg);
		memcpy(buffer, &hid_desc, sizeof(hid_desc));
#ifdef CONFIG_HID_SENSOR_SPOOF_MODE
		spoof_send_response(buffer, 1);
#endif
		send_response(sizeof(hid_desc));
		ccprintf("spoof_host_buffer[0] %d\n", spoof_host_buffer[0]);
		break;

	case REPORT_DESC_REGISTER:
		/* Return Report descr to host */
		ccprintf("Retrieve REPORT_DESC_REGISTER %x\n", reg);
		memcpy(buffer, &report_desc, sizeof(report_desc));
#ifdef CONFIG_HID_SENSOR_SPOOF_MODE
		spoof_send_response(buffer, 0);
#endif
		send_response(sizeof(report_desc));
		break;

	case INPUT_REPORT_REGISTER:
	/* Return input report to host */
	/* Need to add code to check if reset is pending. Not sure of GPIO */
		hid_compile_input(REPORT_ID_BASE_ACCEL);
		response_len = hid_fill_buffer(buffer, REPORT_ID_BASE_ACCEL,
				(void *)&input_reports[0],
				sizeof(struct hid_accel_input_report));
		ccprintf("&input_reports[0] x:%d\n", input_reports[0].x);
		ccprintf("&input_reports[0] y:%d\n", input_reports[0].y);
		ccprintf("&input_reports[0] z:%d\n", input_reports[0].z);

		send_response(response_len);
		break;

	case COMMAND_REGISTER:
	/* Process cmd from host */
		ccprintf("Inside i2c hid process fn: data set is: %.*h\n",
		data_len, buffer);
		hid_command_process(data_len, buffer, send_response);
		break;
	default:
		/* Ignore invalid register. */
		memcpy(buffer, &invalid_reg, sizeof(invalid_reg));
	}
}

int hid_command_process(int len, uint8_t *buffer,
			void (*send_response)(int len))
{
	uint8_t command = buffer[3] & 0x0F;
	uint8_t report_type_id = buffer[2];
	uint8_t rpt_id = report_type_id & 0x0F;
	size_t response_len;
	uint8_t sensor_rpt_id = 0;
	uint32_t data_set;
	struct motion_sensor_t *sensor;
	int ret = EC_RES_INVALID_PARAM;

	switch (command) {
	case I2C_HID_CMD_SET_POWER:
		ccprintf("I2C-HID: SET_POWER %s\n",
			 buffer[2] & 0x3 ? "Sleep" : "ON");

		/* Set Power is a NOOP for now. */
		send_response(0);
		break;
	case I2C_HID_CMD_RESET:
		ccprintf("I2C-HID: RESET\n");

		/* Reset is a NOOP for now. */
		send_response(0);
		break;
	/* For both input and feature reports */
	case I2C_HID_CMD_GET_REPORT:
		ccprintf("I2C-HID: command get_report (%04x)\n", rpt_id);
		switch (rpt_id) {
		case REPORT_ID_BASE_ACCEL:
			hid_compile_input(REPORT_ID_BASE_ACCEL);

			response_len = hid_fill_buffer(
					buffer, REPORT_ID_BASE_ACCEL,
					(void *)&input_reports[0],
					sizeof(struct hid_accel_input_report));
			ccprintf("&input_reports[0] x, y, z: %d %d %d\n",
				input_reports[0].x,
				input_reports[0].y,
				input_reports[0].z);
			return response_len;
		default:
			response_len = 2;
			buffer[0] = response_len;
			buffer[1] = 0;
			break;
		}
#ifdef CONFIG_HID_SENSOR_SPOOF_MODE
		spoof_send_response(buffer, -1);
#endif
		send_response(response_len);
		break;
	case I2C_HID_CMD_SET_REPORT:
		ccprintf("I2C-HID: command set_report (%04x)\n", rpt_id);
		switch (rpt_id) {
		case REPORT_ID_BASE_ACCEL_SAMPLING_RATE: {
			data_set = hid_extract_report(len, buffer,
			&base_accel_odr, sizeof(base_accel_odr));
			sensor_rpt_id = hid_get_sensorid_from_featureid(rpt_id);
			sensor =
			hid_host_sensor_id_to_real_sensor(sensor_rpt_id);
			ccprintf("New ODR to set %d\n", data_set);
			ccprintf("ODR place holder var: %d\n", base_accel_odr);
			ccprintf("Sensor report id: %d\n", sensor_rpt_id);

			if (sensor == NULL)
				return EC_RES_INVALID_PARAM;
			/* Set new data rate */
			set_odr(sensor, &data_set, 1);
			break;
		}
		default:
			ret = EC_RES_INVALID_PARAM;
		}

		send_response(ret * -1);
		break;

	default:
		ccprintf("I2C-HID: unknown command %d\n", command);

		send_response(EC_RES_INVALID_PARAM * -1);
	}
	return 0;
}

