/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

 /* Header for motion_sense_hid.c */
#include <stdint.h>

#include "motion_sense.h"


#ifndef __packed
#define __packed
#endif


/* 2 bytes for length . Report ID is part of the data/ input report struct*/
#define I2C_HID_HEADER_SIZE		2

/* Report IDs for sensor input reports. Although they map to the indices in
 *motion_sensors[], we can't use 0 as ID for devices with TLCs
 */
#define REPORT_ID_BASE_ACCEL			0x01
#define REPORT_ID_LID_ACCEL			0x02
#define REPORT_ID_BASE_GYRO			0x03
#define REPORT_ID_BASE_MAG			0x04
#define REPORT_ID_LID_LIGHT			0x05

/* Report ID for feature reports. Need this in order to know which
 *features to set. Strictly speaking the features are not TLCs but there's
 *no other way of knowing how to index the feature unless the Usage Page
 *is part of the host request.
 */
#define REPORT_ID_BASE_ACCEL_SAMPLING_RATE		0x01
#define REPORT_ID_BASE_ACCEL_POWER_STATE		0x02
#define REPORT_ID_BASE_ACCEL_CHANGE_SENSITIVITY		0x03
#define REPORT_ID_BASE_ACCEL_SENSOR_STATUS		0x04
#define REPORT_ID_BASE_ACCEL_REPORT_INTERVAL		0x05
#define REPORT_ID_BASE_ACCEL_REPORTING_STATE		0x06

/* Report descriptor for motion sensors*/
/* Usage ID, Usage page. Implementing sensors as separate TLCs. Max size 64kb */
static const uint8_t report_desc[] = {
// input reports (transmit)
	0x05, 0x20,				/* Usage Page (Sensors) */
	0x09, 0x73,				/* Usage Sensor Type (3D Acc) */
	// Report ID for accel
	0x85, REPORT_ID_BASE_ACCEL,		/* Report ID (3DAccel) */
	0x19, 0x01,				/* HID_USAGE_MIN_8 */
	0x29, 0x02,				/* HID_USAGE_MAX_8 */
	0xA1, 0x01,				/* (Application: Accel TLC) */
	// 1. Sensor state
	0x0A, 0x01, 0x02,			/* HID_USAGE_SENSOR_STATE */
	0x15, 0,				/* HID_LOGICAL_MIN_8 */
	0x25, 6,				/* HID_LOGICAL_MAX_8*/
	0x075, 8,				/* HID_REPORT_SIZE */
	0x95, 1,				/* HID_REPORT_COUNT */
	0xA1, 0x02,				/* HID_COLLECTION, (Logical) */
	0x0A, 0x00, 0x08,			/* SENSOR_STATE_UNKNOWN*/
	0x0A, 0x01, 0x08,			/* SENSOR_STATE_READY*/
	0x0A, 0x02, 0x08,			/* SENSOR_STATE_NOT_AVAILABLE*/
	0x0A, 0x03, 0x08,			/* SENSOR_STATE_NO_DATA */
	0x0A, 0x04, 0x08,			/* SENSOR_STATE_INITIALIZING*/
	0x0A, 0x05, 0x08,			/* SENSOR_STATE_ACCESS_DENIED,*/
	0x0A, 0x06, 0x08,			/* SENSOR_STATE_ERROR*/
	0x81, 0x03,				/* HID_INPUT(Const_Arr_Abs) */
	0xC0,					/* HID_END_COLLECTION*/
	// 2. Sensor event
	0x0A, 0x02, 0x02,				/*USAGE_SENSOR_EVENT */
	0x15, 0,				/* HID_LOGICAL_MIN_8 */
	0x25, 16,				/* HID_LOGICAL_MAX_8*/
	0x075, 8,				/* HID_REPORT_SIZE */
	0x95, 1,				/* HID_REPORT_COUNT */
	0xA1, 0x02,				/* HID_COLLECTION, (Logical) */
	0x0A, 0x10, 0x08,				/* UNKNOWN */
	0x0A, 0x11, 0x08,				/* STATE_CHANGED */
	0x0A, 0x12, 0x08,				/* PROPERTY_CHANGED */
	0x0A, 0x13, 0x08,				/* DATA_UPDATED */
	0x0A, 0x14, 0x08,				/* POLL_RESPONSE */
	0x0A, 0x15, 0x08,				/* CHANGE_SENST*/
	0x0A, 0x16, 0x08,				/* MAX_REACHED */
	0x0A, 0x17, 0x08,				/* MIN_REACHED */
	0x0A, 0x18, 0x08,				/* HIGH_THRES_CROSS_UP*/
	0x0A, 0x19, 0x08,				/* HIGH_THRE_CROSS_DWN*/
	0x0A, 0x1A, 0x08,				/* LOW_THRES_CROSS_UP*/
	0x0A, 0x1B, 0x08,				/* LOW_THRES_CROSS_DWN*/
	0x0A, 0x1C, 0x08,				/* ZERO_THRES_CROSS_UP*/
	0x0A, 0x1D, 0x08,				/* ZERO_THRE_CROSS_DWN*/
	0x0A, 0x1E, 0x08,				/* PERIOD_EXCEEDED */
	0x0A, 0x1F, 0x08,				/* FREQ_EXCEEDED*/
	0x0A, 0x20, 0x08,				/* COMPLEX_TRIGGER */
	0x81, 0x03,				/* HID_INPUT(Const_Arr_Abs) */
	0xC0,					/* HID_END_COLLECTION*/
	// 3. X, Y, Z axis accel readings
	0x0A, 0x53, 0x04,			/* MOTION_ACCELERATION_X_AXIS*/
	0x0A, 0x54, 0x04,			/* ACCEL_Y_AXIS */
	0x0A, 0x55, 0x04,			/* ACCEL_Z_AXIS*/
	0x16, 0x01, 0x80,			/* LOGICAL_MINIMUM (-32767)*/
	0x2A, 0xFF, 0x7F,			/* LOGICAL_MAXIMUM (32767)*/
	0x075, 16,				/* HID_REPORT_SIZE */
	0x95, 3,				/* HID_REPORT_COUNT */
	0x55, 0x0E,				/* HID_UNIT_EXPONENT*/
	0x81, 0x02,				/* HID_INPUT(Const_Arr_Abs) */

// feature reports (xmit/receive)
	// 1. Reporting state
	0x0A, 0x16, 0x03,			/*PROPERTY_REPORTING_STATE*/
	0x85, REPORT_ID_BASE_ACCEL_REPORTING_STATE,
	0x15, 0,				/* HID_LOGICAL_MIN_8 */
	0x25, 5,				/* HID_LOGICAL_MAX_8*/
	0x075, 8,				/* HID_REPORT_SIZE */
	0x95, 1,				/* HID_REPORT_COUNT */
	0xA1, 0x02,				/* HID_COLLECTION, (Logical) */
	0x0A, 0x40, 0x08,				/* NO_EVENTS */
	0x0A, 0x41, 0x08,				/* ALL_EVENTS*/
	0x0A, 0x42, 0x08,				/* THRESHOLD_EVENTS*/
	0x0A, 0x43, 0x08,				/* NO_EVENTS_WAKE*/
	0x0A, 0x44, 0x08,				/* ALL_EVENTS_WAKE*/
	0x0A, 0x45, 0x08,				/* THRES_EVENTS_WAKE*/
	0xB1, 0x02,				/* HID_FEATURE(Data_Arr_Abs)*/
	0xC0,					/* HID_END_COLLECTION*/
	// 2. Power state
	0x0A, 0x19, 0x03,			/*SENSOR_PROPERTY_POWER_STATE*/
	0x85, REPORT_ID_BASE_ACCEL_POWER_STATE, /* Report ID */
	0x15, 0,				/* HID_LOGICAL_MIN_8 */
	0x25, 5,				/* HID_LOGICAL_MAX_8*/
	0x075, 8,				/* HID_REPORT_SIZE */
	0x95, 1,				/* HID_REPORT_COUNT */
	0xA1, 0x02,				/* HID_COLLECTION, (Logical) */
	0x0A, 0x50, 0x08,				/* UNDEFINED */
	0x0A, 0x51, 0x08,				/* D0_FULL_POWER */
	0x0A, 0x52, 0x08,				/* D1_LOW_POWER*/
	0x0A, 0x53, 0x08,				/* D2_STDBY_WITH_WAKE*/
	0x0A, 0x54, 0x08,				/* D3_SLEEP_WITH_WAKE */
	0x0A, 0x55, 0x08,				/* D4_POWER_OFF */
	0xB1, 0x02,				/* HID_FEATURE(Data_Arr_Abs)*/
	0xC0,					/* HID_END_COLLECTION*/
	// 3. Change sensitivity
	0x0A, 0x0F, 0x03,			/* PROPERTY_CHANGE_SENSIT_ABS*/
	0x85, REPORT_ID_BASE_ACCEL_CHANGE_SENSITIVITY, /* Report ID */
	0x15, 0,				/* HID_LOGICAL_MIN_8 */
	0x26, 0xFF, 0xFF,				/* LOGICAL_MAX_16*/
	0x75, 16,				/* HID_REPORT_SIZE */
	0x95, 1,				/* HID_REPORT_COUNT*/
	0x55, 0x0E,				/* HID_UNIT_EXPONENT*/
	0xB1, 0x02,				/* HID_FEATURE(Data_Arr_Abs)*/
	// 4. Sensor status
	0x0A, 0x03, 0x03,			/* PROPERTY_SENSOR_STATUS */
	0x85, REPORT_ID_BASE_ACCEL_SENSOR_STATUS,
	0x15, 0,				/* HID_LOGICAL_MIN_8 */
	0x55, 0xFF, 0xFF, 0xFF, 0xFF,		/* HID_LOGICAL_MAX_32 */
	0x75, 32,				/* HID_REPORT_SIZE */
	0xB1, 0x02,				/* HID_FEATURE(Data_Arr_Abs)*/
	// 5. Sampling rate/ odr
	0x0A, 0x17, 0x03,			/* PROPERTY_SAMPLING_RATE*/
	0x85, REPORT_ID_BASE_ACCEL_SAMPLING_RATE, /* Report ID */
	0x15, 0,				/* HID_LOGICAL_MIN_8 */
	0x55, 0xFF, 0xFF, 0xFF, 0xFF,		/* HID_LOGICAL_MAX_32 */
	0x75, 32,				/* HID_REPORT_SIZE */
	0x95, 1,				/* HID_REPORT_COUNT*/
	0x55, 0,				/* HID_UNIT_EXPONENT*/
	0xB1, 0x02,				/* HID_FEATURE(Data_Arr_Abs)*/

};

int hid_command_process(int len, uint8_t *buffer,
			void (*send_response)(int len));

/* HID feature report. Struct maps to report descriptor feature report fields */
struct hid_accel_feature_report {
	uint8_t report_id;
	uint8_t polling_interval;
	uint8_t sensor_state;
	uint8_t power_state;
	uint16_t change_sensitivity;
	uint32_t sensor_status;
	uint32_t odr; // Sampling rate
};

/* HID input report. Struct maps to report descriptor input report fields */
struct hid_accel_input_report {
	uint8_t report_id; // maps to sensor id
	uint8_t sensor_state;
	uint8_t sensor_event;
	uint16_t x;
	uint16_t y;
	uint16_t z;
};

int hid_compile_input(int report_id);


int hid_extract_report(uint64_t len, uint8_t *buffer,
			void *data, uint64_t data_len);

/**
 * Function to process HID message once they have been reassembled into the
 * i2c buffer.
 *
 * @param len:  length of the incoming message.
 * @param buffer: address of the incoming message.
 * @param send_response: Function to call to send a respone.
 *    Using same buffer, response has len bytes.
 */
void i2c_hid_process(int len, uint8_t *buffer, void(*send_response)(int len));

struct motion_sensor_t
	*hid_host_sensor_id_to_real_sensor(int report_id);

#ifdef CONFIG_HID_SENSOR_SPOOF_MODE
int get_new_odr(struct motion_sensor_t *sensor);
struct motion_sensor_t *get_spoof_sensor(void);
void send_response(int len);
void spoof_send_response(uint8_t *buffer, int is_hid_desc);
extern struct motion_sensor_t *spoof_sensor;
extern uint8_t spoof_host_buffer[512];
#endif /* defined(CONFIG_HID_SENSOR_SPOOF_MODE) */
