/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "consumer.h"
#include "producer.h"
#include "registers.h"
#include "task.h"
#include "usb_descriptor.h"
#include "util.h"

#ifndef __CROS_USB_I2C_H
#define __CROS_USB_I2C_H

/*
 * Currently, the following form of command are supported.
 *   - Write at most 254 bytes and read no more than 126 (0x7E) bytes. Fields
 *     are explained later.
 *   +------+------+----+----+-------------+
 *   | port | addr | wc | rc |    data     |
 *   +------+------+----+----+-------------+
 *   |  1B  |  1B  | 1B | 1B | < 255 bytes |
 *   +------+------+----+----+-------------+
 *
 *   - Write at most 254 bytes and read no more than 32765 (0x7FFE) bytes.
 *     Fields are explained later. 
 *   +------+------+----+----+-----+----------+-------------+
 *   | port | addr | wc | rc | rc1 | reserved |     data    |
 *   +------+------+----+----+----------------+-------------+
 *   |  1B  |  1B  | 1B | 1B |  1B |    1B    | < 255 bytes |
 *   +------+------+----+----+----------------+-------------+
 *
 *   - port: port address, 1 byte, i2c interface index.
 *
 *   - addr: slave address, 1 byte, i2c 7-bit bus address.
 *
 *   - wc: write count, 1 byte, zero based count of bytes to write. If write
 *         count exceeds 60 bytes (size of a USB packet), following packets are
 *         expected to continue the payload without header. Be sure the
 *         receiving side has enough buffer.
 *
 *   - rc: read count: 1 byte, zero based count of bytes to read. In the
 *         first protocol, this is limited to less than 0x80 because we use
 *         0x80 as the indicator bits for second protocol.
 *         
 *   - data: payload of data to write. See wc above for more information.
 *
 *   - rc1: read count 1, 1 byte, an extended version to allow reading more
 *          data. While the most significant bits is set in read count (0x80),
 *          indicating the need of extended bits for read count, rc and rc1
 *          will concatenate together. The final read count will be
 *          (rc1 << 7) | (rc & 0x7F) 
 *
 *   - reserved: reserved byte, 1 byte.
 *
 * Response:
 *     +-------------+---+---+--------------+
 *     | status : 2B | 0 | 0 | read payload |
 *     +-------------+---+---+--------------+
 *
 *     status: 2 byte status
 *         0x0000: Success
 *         0x0001: I2C timeout
 *         0x0002: Busy, try again
 *             This can happen if someone else has acquired the shared memory
 *             buffer that the I2C driver uses as /dev/null
 *         0x0003: Write count invalid (mismatch with merged payload)
 *         0x0004: Read count invalid (depends on the supported version)
 *         0x0005: The port specified is invalid.
 *         0x8000: Unknown error mask
 *             The bottom 15 bits will contain the bottom 15 bits from the EC
 *             error code.
 *
 *     read payload: Depends on the buffer size and implementation. Length will
 *             match requested read count
 */

enum usb_i2c_error {
	USB_I2C_SUCCESS             = 0x0000,
	USB_I2C_TIMEOUT             = 0x0001,
	USB_I2C_BUSY                = 0x0002,
	USB_I2C_WRITE_COUNT_INVALID = 0x0003,
	USB_I2C_READ_COUNT_INVALID  = 0x0004,
	USB_I2C_PORT_INVALID        = 0x0005,
	USB_I2C_UNKNOWN_ERROR       = 0x8000,
};


#define USB_I2C_CONFIG_BUFFER_SIZE \
	(CONFIG_USB_I2C_MAX_WRITE_COUNT > CONFIG_USB_I2C_MAX_READ_COUNT ?\
		(CONFIG_USB_I2C_MAX_WRITE_COUNT+4) :                     \
		(CONFIG_USB_I2C_MAX_READ_COUNT+4))


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

/*
 * Convenience macro for defining a USB I2C bridge driver.
 *
 * NAME is used to construct the names of the trampoline functions and the
 * usb_i2c_config struct, the latter is just called NAME.
 *
 * INTERFACE is the index of the USB interface to associate with this
 * I2C driver.
 *
 * INTERFACE_NAME is the index of the USB string descriptor (iInterface).
 *
 * ENDPOINT is the index of the USB bulk endpoint used for receiving and
 * transmitting bytes.
 */
#define USB_I2C_CONFIG(NAME,						\
		       INTERFACE,					\
		       INTERFACE_NAME,					\
		       ENDPOINT)					\
	static uint16_t							\
		CONCAT2(NAME, _buffer_)					\
			[USB_I2C_CONFIG_BUFFER_SIZE / 2];		\
	static void CONCAT2(NAME, _deferred_)(void);			\
	DECLARE_DEFERRED(CONCAT2(NAME, _deferred_));			\
	static struct queue const CONCAT2(NAME, _to_usb_);		\
	static struct queue const CONCAT3(usb_to_, NAME, _);		\
	USB_STREAM_CONFIG_FULL(CONCAT2(NAME, _usb_),			\
			       INTERFACE,				\
			       USB_CLASS_VENDOR_SPEC,			\
			       USB_SUBCLASS_GOOGLE_I2C,			\
			       USB_PROTOCOL_GOOGLE_I2C,			\
			       INTERFACE_NAME,				\
			       ENDPOINT,				\
			       USB_MAX_PACKET_SIZE,			\
			       USB_MAX_PACKET_SIZE,			\
			       CONCAT3(usb_to_, NAME, _),		\
			       CONCAT2(NAME, _to_usb_))			\
	struct usb_i2c_config const NAME = {				\
		.buffer    = CONCAT2(NAME, _buffer_),			\
		.deferred  = &CONCAT2(NAME, _deferred__data),		\
		.consumer  = {						\
			.queue = &CONCAT3(usb_to_, NAME, _),		\
			.ops   = &usb_i2c_consumer_ops,			\
		},							\
		.tx_queue = &CONCAT2(NAME, _to_usb_),			\
	};								\
	static struct queue const CONCAT2(NAME, _to_usb_) =		\
		QUEUE_DIRECT(CONFIG_USB_I2C_MAX_READ_COUNT+4, uint8_t,	\
		null_producer, CONCAT2(NAME, _usb_).consumer);		\
	static struct queue const CONCAT3(usb_to_, NAME, _) =		\
		QUEUE_DIRECT(CONFIG_USB_I2C_MAX_WRITE_COUNT+4, uint8_t,	\
		CONCAT2(NAME, _usb_).producer, NAME.consumer);		\
	static void CONCAT2(NAME, _deferred_)(void)			\
	{ usb_i2c_deferred(&NAME); }

/*
 * Handle I2C request in a deferred callback.
 */
void usb_i2c_deferred(struct usb_i2c_config const *config);

/*
 * These functions should be implemented by the board to provide any board
 * specific operations required to enable or disable access to the I2C device.
 */
int usb_i2c_board_enable(void);
void usb_i2c_board_disable(void);
#endif  /* __CROS_USB_I2C_H */
