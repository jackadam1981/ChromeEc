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

#define USB_I2C_CONFIG(NAME, INTERFACE, INTERFACE_NAME, ENDPOINT)              \
	static struct queue const i2c_to_usb;                     \
	static struct queue const usb_to_i2c;                   \
	USB_STREAM_CONFIG_FULL(i2c_usb, INTERFACE,                \
			       USB_CLASS_VENDOR_SPEC, USB_SUBCLASS_GOOGLE_I2C, \
			       USB_PROTOCOL_GOOGLE_I2C, INTERFACE_NAME,        \
			       ENDPOINT, USB_MAX_PACKET_SIZE,                  \
			       USB_MAX_PACKET_SIZE, usb_to_i2c, \
			       i2c_to_usb, 1, 0); \
	static struct queue const i2c_to_usb =                    \
		QUEUE_DIRECT(USB_I2C_READ_BUFFER, uint8_t, null_producer,      \
			     i2c_usb.consumer);                   \
	static struct queue const usb_to_i2c =                  \
		QUEUE_DIRECT(USB_I2C_WRITE_BUFFER, uint8_t,                    \
			     i2c_usb.producer, null_consumer);    \

