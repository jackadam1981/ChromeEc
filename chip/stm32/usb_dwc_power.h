/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_USB_DWC_POWER_H
#define __CROS_EC_USB_DWC_POWER_H

/* STM32 INA driver for Chrome EC */

#include "compile_time_macros.h"
#include "hooks.h"
#include "usb_descriptor.h"
#include "usb_dwc_hw.h"

/*
 * Command:
 *     +--------------+------------------------------------------+
 *     | command : 2B |                                          |
 *     +--------------+------------------------------------------+
 *
 *     command:	2 bytes
 *
 *     reset:	0x0000
 *
 *     +--------+
 *     | 0x0000 |
 *     +--------+
 *
 *     stop:	0x0001
 *
 *     +--------+
 *     | 0x0001 |
 *     +--------+
 *
 *     addina:	0x0002
 *
 *     +--------+--------------------------+-------------+--------------+-----------+-------------+--------+
 *     | 0x0002 | 1B: 4b: extender 4b: bus | 1B:INA type | 1B: INA addr | 1B: extra | 4B: voltage | 4B: Rs |
 *     +--------+--------------------------+-------------+--------------+-----------+-------------+--------+
 *
 *     start:	0x0003
 *
 *     +--------+----------------------+
 *     | 0x0003 | 4B: integration time |
 *     +--------+----------------------+
 *
 *     next:	0x0004
 *
 *     +--------+
 *     | 0x0004 |
 *     +--------+
 *
 *
 * Response:
 *     +-------------+----------+--------------------+------------------+
 *     | status : 1B | size: 1B | timestampread : 4B | payload : <= 58B |
 *     +-------------+----------+--------------------+------------------+
 *
 *     status: 1 byte status
 *         0x00: Success
 *         0x01: I2C Error
 *         0x02: Overflow
 *             This can happen if data aquisition is faster than USB reads.
 *         0x03: No configuration set.
 *         0x04: No active capture.
 *         0x80: Unknown error
 *
 *     size: 1 byte incoming INA reads count
 *
 *     timestamp: 4 byte timestamp associated with these samples
 *
 *     read payload: up to 58 bytes of data, 29x INA reads of current
 *
 */

enum usb_power_error {
	USB_POWER_SUCCESS	= 0x00,
	USB_POWER_I2C_ERROR	= 0x01,
	USB_POWER_OVERFLOW	= 0x02,
	USB_POWER_NOT_SETUP	= 0x03,
	USB_POWER_NOT_CAPTURING	= 0x04,
	USB_POWER_TIMEOUT	= 0x05,
	USB_POWER_BUSY		= 0x06,
	USB_POWER_READ_SIZE	= 0x07,
	USB_POWER_FULL		= 0x08,
	USB_POWER_UNKNOWN_ERROR	= 0x80,
};

enum usb_power_command {
	USB_POWER_CMD_RESET	= 0x0000,
	USB_POWER_CMD_STOP	= 0x0001,
	USB_POWER_CMD_ADDINA	= 0x0002,
	USB_POWER_CMD_START	= 0x0003,
	USB_POWER_CMD_NEXT	= 0x0004,
};

enum usb_power_states {
	USB_POWER_STATE_OFF	= 0,
	USB_POWER_STATE_SETUP,
	USB_POWER_STATE_CAPTURING,
};

#define USB_POWER_MAX_READ_COUNT 64
#define USB_POWER_MAX_CACHED 10

/*BUILD_ASSERT(USB_MAX_PACKET_SIZE == (1 + 1 + 4 + 2*USB_POWER_MAX_READ_COUNT));*/

struct usb_power_ina_cfg {
	/*
	 * Relevant config for INA usage.
	 */
	/* i2c bus. TODO(nsanders): specify what kind of index. */
	int port;
	/* 7-bit i2c addr */
	int addr;
	/* Servo INA board uses an I2C address entender. */
	int extender_mode;
	int extended_addr;

	/* Base voltage. mV */
	int mv;
	/* Shunt resistor. mOhm */
	int rs;
	int cal;
	int scale;
};


struct __attribute__ ((__packed__)) usb_power_report {
	uint8_t status;
	uint8_t size;
	uint32_t timestamp;
	uint16_t power[USB_POWER_MAX_READ_COUNT];
};


struct usb_power_state {
	/*
	 * The power data aquisition must be setup, then started, in order to
	 * return data.
	 * States are OFF, SETUP, and CAPTURING.
	 */
	int state;

	struct usb_power_ina_cfg ina_cfg[USB_POWER_MAX_READ_COUNT];
	int ina_count;
	int integration_us;

	/* Cached power reports for sending on USB. */
	struct usb_power_report reports[USB_POWER_MAX_CACHED];
	int reports_head;
	int reports_tail;

	/* Pointers to RAM. */
	uint8_t rx_buf[USB_MAX_PACKET_SIZE];
	uint8_t tx_buf[USB_MAX_PACKET_SIZE];
};


/*
 * Compile time Per-USB gpio configuration stored in flash.  Instances of this
 * structure are provided by the user of the USB gpio.  This structure binds
 * together all information required to operate a USB gpio.
 */
struct usb_power_config {
	/* In RAM state of the USB power interface. */
	struct usb_power_state *state;

	/* USB endpoint state.*/
	struct dwc_usb_ep *ep; 

	/* Interface and endpoint indicies. */
	int interface;
	int endpoint;

	/* Deferred function to call to handle power request. */
	const struct deferred_data *deferred;
	const struct deferred_data *deferred_cap;
};

/* USB command structure 
 *     addina
 *     | 0x0002 | 1B: 4b: extender 4b: bus | 1B:INA type | 1B: INA addr | 1B: extra | 4B: voltage | 4B: Rs |
 *
 *     start:   0x0003
 *     | 0x0003 | 4B: integration time |
 */
struct __attribute__ ((__packed__)) usb_power_command_start {
	uint16_t command;
	uint32_t integration_us;
};
struct __attribute__ ((__packed__)) usb_power_command_addina {
	uint16_t command;
	uint8_t port;
	uint8_t type;
	uint8_t addr;
	uint8_t extra;
	uint32_t mv;
	uint32_t rs;
};
union usb_power_command_data {
	uint16_t command;
	struct usb_power_command_start start;
	struct usb_power_command_addina addina;
}; 


/*
 * Convenience macro for defining a USB INA Power driver.
 *
 * NAME is used to construct the names of the trampoline functions and the
 * usb_power_config struct, the latter is just called NAME.
 *
 * INTERFACE is the index of the USB interface to associate with this
 * driver.
 *
 * ENDPOINT is the index of the USB bulk endpoint used for receiving and
 * transmitting bytes.
 */
#define USB_POWER_CONFIG(NAME,						\
		       INTERFACE,					\
		       ENDPOINT)					\
	static void CONCAT2(NAME, _deferred_)(void);			\
	DECLARE_DEFERRED(CONCAT2(NAME, _deferred_));			\
	static void CONCAT2(NAME, _deferred_cap_)(void);		\
	DECLARE_DEFERRED(CONCAT2(NAME, _deferred_cap_));		\
	struct usb_power_state CONCAT2(NAME, _state_) = {		\
		.state = USB_POWER_STATE_OFF,				\
		.ina_count = 0,						\
		.integration_us = 0,					\
		.reports_head = 0,					\
		.reports_tail = 0,					\
	};								\
	static struct dwc_usb_ep CONCAT2(NAME, _ep_ctl) = {		\
		.max_packet = USB_MAX_PACKET_SIZE,			\
		.tx_fifo = ENDPOINT,					\
		.out_pending = 0,					\
		.out_data = 0,						\
		.out_databuffer = 0,					\
		.out_databuffer_max = 0,				\
		.in_packets = 0,					\
		.in_pending = 0,					\
		.in_data = 0,						\
		.in_databuffer = 0,					\
		.in_databuffer_max = 0,					\
	};								\
	struct usb_power_config const NAME = {				\
		.state     = &CONCAT2(NAME, _state_),			\
		.ep        = &CONCAT2(NAME, _ep_ctl),			\
		.interface = INTERFACE,					\
		.endpoint  = ENDPOINT,					\
		.deferred  = &CONCAT2(NAME, _deferred__data),		\
		.deferred_cap  = &CONCAT2(NAME, _deferred_cap__data),	\
	};								\
	const struct usb_interface_descriptor				\
	USB_IFACE_DESC(INTERFACE) = {					\
		.bLength            = USB_DT_INTERFACE_SIZE,		\
		.bDescriptorType    = USB_DT_INTERFACE,			\
		.bInterfaceNumber   = INTERFACE,			\
		.bAlternateSetting  = 0,				\
		.bNumEndpoints      = 2,				\
		.bInterfaceClass    = USB_CLASS_VENDOR_SPEC,		\
		.bInterfaceSubClass = USB_SUBCLASS_GOOGLE_POWER,	\
		.bInterfaceProtocol = USB_PROTOCOL_GOOGLE_POWER,	\
		.iInterface         = 0,				\
	};								\
	const struct usb_endpoint_descriptor				\
	USB_EP_DESC(INTERFACE, 0) = {					\
		.bLength          = USB_DT_ENDPOINT_SIZE,		\
		.bDescriptorType  = USB_DT_ENDPOINT,			\
		.bEndpointAddress = 0x80 | ENDPOINT,			\
		.bmAttributes     = 0x02 /* Bulk IN */,			\
		.wMaxPacketSize   = USB_MAX_PACKET_SIZE,		\
		.bInterval        = 10,					\
	};								\
	const struct usb_endpoint_descriptor				\
	USB_EP_DESC(INTERFACE, 1) = {					\
		.bLength          = USB_DT_ENDPOINT_SIZE,		\
		.bDescriptorType  = USB_DT_ENDPOINT,			\
		.bEndpointAddress = ENDPOINT,				\
		.bmAttributes     = 0x02 /* Bulk OUT */,		\
		.wMaxPacketSize   = USB_MAX_PACKET_SIZE,		\
		.bInterval        = 0,					\
	};								\
	static void CONCAT2(NAME, _ep_tx_)   (void) { usb_power_tx   (&NAME); } \
	static void CONCAT2(NAME, _ep_rx_)   (void) { usb_power_rx   (&NAME); } \
	static void CONCAT2(NAME, _ep_reset_)(void) { usb_power_reset(&NAME); } \
	USB_DECLARE_EP(ENDPOINT,					\
		       CONCAT2(NAME, _ep_tx_),				\
		       CONCAT2(NAME, _ep_rx_),				\
		       CONCAT2(NAME, _ep_reset_));			\
	static void CONCAT2(NAME, _deferred_)(void)			\
	{ usb_power_deferred(&NAME); }					\
	static void CONCAT2(NAME, _deferred_cap_)(void)			\
	{ usb_power_deferred_cap(&NAME); }


/*
 * Handle power request in a deferred callback.
 */
void usb_power_deferred(struct usb_power_config const *config);
void usb_power_deferred_cap(struct usb_power_config const *config);

/*
 * These functions are used by the trampoline functions defined above to
 * connect USB endpoint events with the generic USB GPIO driver.
 */
void usb_power_tx(struct usb_power_config const *config);
void usb_power_rx(struct usb_power_config const *config);
void usb_power_reset(struct usb_power_config const *config);




#endif /* __CROS_EC_USB_DWC_POWER_H */

