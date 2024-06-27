/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#define USB_I2C_WRITE_BUFFER (CONFIG_USB_I2C_MAX_WRITE_COUNT + 4)
/* If read payload is larger or equal to 128 bytes, header contains rc1 */
#define USB_I2C_READ_BUFFER                            \
	((CONFIG_USB_I2C_MAX_READ_COUNT < 128) ?       \
		 (CONFIG_USB_I2C_MAX_READ_COUNT + 4) : \
		 (CONFIG_USB_I2C_MAX_READ_COUNT + 6))

#define USB_I2C_BUFFER_SIZE                                                 \
	(USB_I2C_READ_BUFFER > USB_I2C_WRITE_BUFFER ? USB_I2C_READ_BUFFER : \
						      USB_I2C_WRITE_BUFFER)
enum usb_i2c_error {
	USB_I2C_SUCCESS = 0x0000,
	USB_I2C_TIMEOUT = 0x0001,
	USB_I2C_BUSY = 0x0002,
	USB_I2C_WRITE_COUNT_INVALID = 0x0003,
	USB_I2C_READ_COUNT_INVALID = 0x0004,
	USB_I2C_PORT_INVALID = 0x0005,
	USB_I2C_DISABLED = 0x0006,
	USB_I2C_MISSING_HANDLER = 0x0007,
	USB_I2C_UNSUPPORTED_COMMAND = 0x0008,
	USB_I2C_UNKNOWN_ERROR = 0x8000,
};

/*
 * Compile time Per-USB gpio configuration stored in flash.  Instances of this
 * structure are provided by the user of the USB i2c.  This structure binds
 * together all information required to operate a USB i2c.
 */
struct usb_i2c_config {
	uint16_t *buffer;

	/* Deferred function to call to handle SPI request. */
	const struct deferred_data *deferred;

	struct consumer const consumer;
	struct queue const *tx_queue;
};

extern struct consumer_ops const usb_i2c_consumer_ops;

#define USB_I2C_CONFIG(NAME, INTERFACE, INTERFACE_NAME, ENDPOINT)              \
	static uint16_t CONCAT2(NAME, _buffer_)[USB_I2C_BUFFER_SIZE / 2];      \
	static void CONCAT2(NAME, _deferred_)(void);                           \
	DECLARE_DEFERRED(i2c_deferred_);                           \
	static struct queue const CONCAT2(NAME, _to_usb_);                     \
	static struct queue const CONCAT3(usb_to_, NAME, _);                   \
	USB_STREAM_CONFIG_FULL(usb_i2c, INTERFACE,                \
			       USB_CLASS_VENDOR_SPEC, USB_SUBCLASS_GOOGLE_I2C, \
			       USB_PROTOCOL_GOOGLE_I2C, INTERFACE_NAME,        \
			       ENDPOINT, USB_MAX_PACKET_SIZE,                  \
			       USB_MAX_PACKET_SIZE, usb_to_i2c_, \
			       i2c_to_usb_, 1, 0);                  \
	struct usb_i2c_config const NAME = {				\
		.buffer    = CONCAT2(NAME, _buffer_),			\
		.deferred  = &CONCAT2(NAME, _deferred__data),		\
		.consumer  = {						\
			.queue = &CONCAT3(usb_to_, NAME, _),		\
			.ops   = &usb_i2c_consumer_ops,			\
		},							\
		.tx_queue = &CONCAT2(NAME, _to_usb_),			\
	};                              \
	static struct queue const CONCAT2(NAME, _to_usb_) =                    \
		QUEUE_DIRECT(USB_I2C_READ_BUFFER, uint8_t, null_producer,      \
			     CONCAT2(usb_, NAME).consumer);                   \
	static struct queue const CONCAT3(usb_to_, NAME, _) =                  \
		QUEUE_DIRECT(USB_I2C_WRITE_BUFFER, uint8_t,                    \
			     CONCAT2(usb_, NAME).producer, NAME.consumer);    \
	static void CONCAT2(NAME, _deferred_)(void)                            \
	{                                                                      \
		usb_i2c_deferred(&NAME);                                       \
	}

/*
 * Handle I2C request in a deferred callback.
 */
void usb_i2c_deferred(struct usb_i2c_config const *config);

/**
 * Check if the I2C device is enabled
 *
 * @return 1 if enabled, 0 if disabled.
 */
int usb_i2c_board_is_enabled(void);

/*
 * Special i2c address to use when the client is required to execute some
 * command which does not directly involve the i2c controller driver.
 */
#define USB_I2C_CMD_ADDR_FLAGS 0x78
