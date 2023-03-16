/* Copyright 2014 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_USB_SPI_H
#define __CROS_EC_USB_SPI_H

/* STM32 USB SPI driver for Chrome EC */

#include "compile_time_macros.h"
#include "hooks.h"
#include "usb_descriptor.h"
#include "usb_hw.h"

/*
 * This SPI flash programming interface is designed to talk to a Chromium OS
 * device over a Raiden USB connection.
 *
 * USB SPI Version 2:
 *
 *     USB SPI version 2 adds support for larger SPI transfers and reduces the
 *     number of USB packets transferred. This improves performance when
 *     writing or reading large chunks of memory from a device. A packet ID
 *     field is used to distinguish the different packet types. Additional
 *     packets have been included to query the device for its configuration
 *     allowing the interface to be used on platforms with different SPI
 *     limitations. It includes validation and a packet to recover from the
 *     situations where USB packets are lost.
 *
 *     The USB SPI hosts which support packet version 2 are backwards compatible
 *     and use the bInterfaceProtocol field to identify which type of target
 *     they are connected to.
 *
 *
 * Example: USB SPI request with 128 byte write and 0 byte read.
 *
 *      Packet #1 Host to Device:
 *           packet id   = USB_SPI_PKT_ID_CMD_TRANSFER_START
 *           write count = 128
 *           read count  = 0
 *           payload     = First 58 bytes from the write buffer,
 *                            starting at byte 0 in the buffer
 *           packet size = 64 bytes
 *
 *      Packet #2 Host to Device:
 *           packet id   = USB_SPI_PKT_ID_CMD_TRANSFER_CONTINUE
 *           data index  = 58
 *           payload     = Next 60 bytes from the write buffer,
 *                           starting at byte 58 in the buffer
 *           packet size = 64 bytes
 *
 *      Packet #3 Host to Device:
 *           packet id   = USB_SPI_PKT_ID_CMD_TRANSFER_CONTINUE
 *           data index  = 118
 *           payload     = Next 10 bytes from the write buffer,
 *                           starting at byte 118 in the buffer
 *           packet size = 14 bytes
 *
 *      Packet #4 Device to Host:
 *           packet id   = USB_SPI_PKT_ID_RSP_TRANSFER_START
 *           status code = status code from device
 *           payload     = 0 bytes
 *           packet size = 4 bytes
 *
 * Example: USB SPI request with 2 byte write and 100 byte read.
 *
 *      Packet #1 Host to Device:
 *           packet id   = USB_SPI_PKT_ID_CMD_TRANSFER_START
 *           write count = 2
 *           read count  = 100
 *           payload     = The 2 byte write buffer
 *           packet size = 8 bytes
 *
 *      Packet #2 Device to Host:
 *           packet id   = USB_SPI_PKT_ID_RSP_TRANSFER_START
 *           status code = status code from device
 *           payload     = First 60 bytes from the read buffer,
 *                            starting at byte 0 in the buffer
 *           packet size = 64 bytes
 *
 *      Packet #3 Device to Host:
 *           packet id   = USB_SPI_PKT_ID_RSP_TRANSFER_CONTINUE
 *           data index  = 60
 *           payload     = Next 40 bytes from the read buffer,
 *                           starting at byte 60 in the buffer
 *           packet size = 44 bytes
 *
 *
 * Message Packets:
 *
 * Command Start Packet (Host to Device):
 *
 *      Start of the USB SPI command, contains the number of bytes to write
 *      and read on SPI and up to the first 58 bytes of write payload.
 *      Longer writes will use the continue packets with packet id
 *      USB_SPI_PKT_ID_CMD_TRANSFER_CONTINUE to transmit the remaining data.
 *
 *     +----------------+------------------+-----------------+---------------+
 *     | packet id : 2B | write count : 2B | read count : 2B | w.p. : <= 58B |
 *     +----------------+------------------+-----------------+---------------+
 *
 *     packet id:     2 byte enum defined by packet_id_type
 *                    Valid values packet id = USB_SPI_PKT_ID_CMD_TRANSFER_START
 *
 *     write count:   2 byte, zero based count of bytes to write
 *
 *     read count:    2 byte, zero based count of bytes to read
 *                    UINT16_MAX indicates full duplex mode with a read count
 *                    equal to the write count.
 *
 *     write payload: Up to 58 bytes of data to write to SPI, the total
 *                    length of all TX packets must match write count.
 *                    Due to data alignment constraints, this must be an
 *                    even number of bytes unless this is the final packet.
 *
 *
 * Response Start Packet (Device to Host):
 *
 *      Start of the USB SPI response, contains the status code and up to
 *      the first 60 bytes of read payload. Longer reads will use the
 *      continue packets with packet id USB_SPI_PKT_ID_RSP_TRANSFER_CONTINUE
 *      to transmit the remaining data.
 *
 *     +----------------+------------------+-----------------------+
 *     | packet id : 2B | status code : 2B | read payload : <= 60B |
 *     +----------------+------------------+-----------------------+
 *
 *     packet id:     2 byte enum defined by packet_id_type
 *                    Valid values packet id = USB_SPI_PKT_ID_RSP_TRANSFER_START
 *
 *     status code: 2 byte status code
 *         0x0000: Success
 *         0x0001: SPI timeout
 *         0x0002: Busy, try again
 *             This can happen if someone else has acquired the shared memory
 *             buffer that the SPI driver uses as /dev/null
 *         0x0003: Write count invalid. The byte limit is platform specific
 *             and is set during the configure USB SPI response.
 *         0x0004: Read count invalid. The byte limit is platform specific
 *             and is set during the configure USB SPI response.
 *         0x0005: The SPI bridge is disabled.
 *         0x0006: The RX continue packet's data index is invalid. This
 *             can indicate a USB transfer failure to the device.
 *         0x0007: The RX endpoint has received more data than write count.
 *             This can indicate a USB transfer failure to the device.
 *         0x0008: An unexpected packet arrived that the device could not
 *             process.
 *         0x0009: The device does not support full duplex mode.
 *         0x000A: Requested EEPROM mode not supported
 *         0x8000: Unknown error mask
 *             The bottom 15 bits will contain the bottom 15 bits from the EC
 *             error code.
 *
 *     read payload: Up to 60 bytes of data read from SPI, the total
 *                   length of all RX packets must match read count
 *                   unless an error status was returned. Due to data
 *                   alignment constraints, this must be a even number
 *                   of bytes unless this is the final packet.
 *
 *
 * Continue Packet (Bidirectional):
 *
 *      Continuation packet for the writes and read buffers. Both packets
 *      follow the same format, a data index counts the number of bytes
 *      previously transferred in the USB SPI transfer and a payload of bytes.
 *
 *     +----------------+-----------------+-------------------------------+
 *     | packet id : 2B | data index : 2B | write / read payload : <= 60B |
 *     +----------------+-----------------+-------------------------------+
 *
 *     packet id:     2 byte enum defined by packet_id_type
 *                    The packet id has 2 values depending on direction:
 *                    packet id = USB_SPI_PKT_ID_CMD_TRANSFER_CONTINUE
 *                    indicates the packet is being transmitted from the host
 *                    to the device and contains SPI write payload.
 *                    packet id = USB_SPI_PKT_ID_RSP_TRANSFER_CONTINUE
 *                    indicates the packet is being transmitted from the device
 *                    to the host and contains SPI read payload.
 *
 *     data index:    The data index indicates the number of bytes in the
 *                    read or write buffers that have already been transmitted.
 *                    It is used to validate that no packets have been dropped
 *                    and that the prior packets have been correctly decoded.
 *                    This value corresponds to the offset bytes in the buffer
 *                    to start copying the payload into.
 *
 *     read and write payload:
 *                    Contains up to 60 bytes of payload data to transfer to
 *                    the SPI write buffer or from the SPI read buffer.
 *
 *
 * Command Get Configuration Packet (Host to Device):
 *
 *      Query the device to request it's USB SPI configuration indicating
 *      the number of bytes it can write and read.
 *
 *     +----------------+
 *     | packet id : 2B |
 *     +----------------+
 *
 *     packet id:     2 byte enum USB_SPI_PKT_ID_CMD_GET_USB_SPI_CONFIG
 *
 * Response Configuration Packet (Device to Host):
 *
 *      Response packet form the device to report the maximum write and
 *      read size supported by the device.
 *
 *     +----------------+----------------+---------------+----------------+
 *     | packet id : 2B | max write : 2B | max read : 2B | feature bitmap |
 *     +----------------+----------------+---------------+----------------+
 *
 *     packet id:         2 byte enum USB_SPI_PKT_ID_RSP_USB_SPI_CONFIG
 *
 *     max write count:   2 byte count of the maximum number of bytes
 *                        the device can write to SPI in one transaction.
 *
 *     max read count:    2 byte count of the maximum number of bytes
 *                        the device can read from SPI in one transaction.
 *
 *     feature bitmap:    Bitmap of supported features.
 *                        BIT(0): Full duplex SPI mode is supported
 *                        BIT(1): Multilane EEPROM modes are supported
 *                        BIT(1:15): Reserved for future use
 *
 * Command Restart Response Packet (Host to Device):
 *
 *      Command to restart the response transfer from the device. This enables
 *      the host to recover from a lost packet when reading the response
 *      without restarting the SPI transfer.
 *
 *     +----------------+
 *     | packet id : 2B |
 *     +----------------+
 *
 *     packet id:         2 byte enum USB_SPI_PKT_ID_CMD_RESTART_RESPONSE
 *
 * Command chip select Packet (Host to Device):
 *
 *     +----------------+-------------+
 *     | packet id : 2B | action : 2B |
 *     +----------------+-------------+
 *
 *     packet id:         2 byte enum USB_SPI_PKT_ID_CMD_CHIP_SELECT
 *
 *     action:            2 byte, current options:
 *                            0: Deassert chip select
 *                            1: Assert chip select
 *
 * Response chip select Packet (Device to Host):
 *
 *     +----------------+------------------+
 *     | packet id : 2B | status code : 2B |
 *     +----------------+------------------+
 *
 *     packet id:     2 byte enum USB_SPI_PKT_ID_RSP_CHIP_SELECT
 *
 *     status code: 2 byte status code
 *         0x0000: Success
 *         others: Error
 *
 * EEPROM Command Start Packet (Host to Device):
 *
 *      Start of the USB EEPROM SPI command, contains the number of bytes to
 *      write or read on SPI and up to the first 58 bytes of write payload.
 *      Longer writes will use the continue packets with packet id
 *      USB_SPI_PKT_ID_CMD_TRANSFER_CONTINUE to transmit the remaining data.
 *
 *      The reading or writing of the "main" data will be preceded by an
 *      short sequence of opcode, optional address, optional "alternate data",
 *      and optional 'dummy cycles" on the SPI bus.  Flags indicate how many
 *      bytes of each stage to send, and whether to use advanced features such
 *      as dual or quad signal lanes for each stage of the transfer".
 *
 *      The indicated number of opcode, address and alternate bytes will be
 *      the first in the "write payload".  The "count" field will contain the
 *      number of data bytes to be written/read after the opcode, address and
 *      alternate bytes.
 *
 *      Implementations will advertise whether they support dual, quad or octo
 *      modes, if none of these are supported, then support for "dummy cycles"
 *      are not guaranteed either, and callers should use one or two bytes of
 *      "alternate data" in place of dummy cycles.
 *
 *     +----------------+------------+------------+---------------+
 *     | packet id : 2B | count : 2B | flags : 4B | w.p. : <= 56B |
 *     +----------------+------------+------------+---------------+
 *
 *     packet id:     2 byte enum defined by packet_id_type
 *                    Valid values packet id =
 *                    USB_SPI_PKT_ID_CMD_EEPROM_TRANSFER_START
 *
 *     count:         2 byte, zero based count of bytes to read or write
 *
 *     flags:         4 byte, flags
 *          bits 0:1  opcode width
 *             bit 2  opcode double transfer rate
 *          bits 3:5  opcode length in bytes
 *          bits 6:7  address width
 *             bit 8  address double transfer rate
 *         bits 9:11  address length in bytes
 *        bits 12:13  alternate data width
 *            bit 14  alternate data double transfer rate
 *        bits 15:17  alternate data length in bytes
 *        bits 18:22  number of dummy cycles
 *        bits 23:24  data width
 *            bit 25  data double transfer rate
 *            bit 26  reserved, must be zero
 *            bit 27  write to be preceded by "write enable" (0x06)
 *            bit 28  write to be followed by polling of "busy bit" (0x05)
 *            bit 29  reserved, must be zero
 *            bit 31  read (0) / write (1)
 *
 *                    The "width" fields indicate whether each stage of the
 *                    transfer is to be using standard SPI uni-directional
 *                    signals (0), dual channel (1), quad (2) or octo (3).
 *
 *     write payload: Up to 56 bytes of data to write to SPI, the total length
 *                    of all TX packets must match opcode length + address
 *                    length + alternate data length + count, (the last one
 *                    only if bit 31 indicates a write operation). Due to data
 *                    alignment constraints, this must be an even number of
 *                    bytes unless this is the final packet.
 *
 *
 * USB Error Codes:
 *
 * send_command return codes have the following format:
 *
 *     0x00000:         Status code success.
 *     0x00001-0x0FFFF: Error code returned by the USB SPI device.
 *     0x10001-0x1FFFF: USB SPI Host error codes
 *     0x20001-0x20063  Lower bits store the positive value representation
 *                      of the libusb_error enum. See the libusb documentation:
 *                      http://libusb.sourceforge.net/api-1.0/group__misc.html
 */

#define USB_SPI_FULL_DUPLEX_ENABLED (UINT16_MAX)

#define USB_SPI_PAYLOAD_SIZE_V2_START (58)

#define USB_SPI_PAYLOAD_SIZE_V2_RESPONSE (60)

#define USB_SPI_PAYLOAD_SIZE_V2_CONTINUE (60)

#define USB_SPI_PAYLOAD_SIZE_V2_ERROR (60)

#define USB_SPI_PAYLOAD_SIZE_EEPROM_START (56)

#define USB_SPI_MIN_PACKET_SIZE (2)

/*
 * Values used in spi_device_t.usb_flags
 */

/* Is the USB host allowed to operate on SPI device. */
#define USB_SPI_ENABLED (BIT(0))
/* Use board specific SPI driver when forwarding to this device. */
#define USB_SPI_CUSTOM_SPI_DEVICE (BIT(1))
/* This SPI device supports dual lane mode. */
#define USB_SPI_EEPROM_DUAL_SUPPORT (BIT(2))
/* This SPI device supports four lane mode. */
#define USB_SPI_EEPROM_QUAD_SUPPORT (BIT(3))
/* This SPI device supports eight lane mode. */
#define USB_SPI_EEPROM_OCTO_SUPPORT (BIT(4))

enum packet_id_type {
	/* Request USB SPI configuration data from device. */
	USB_SPI_PKT_ID_CMD_GET_USB_SPI_CONFIG = 0,
	/* USB SPI configuration data from device. */
	USB_SPI_PKT_ID_RSP_USB_SPI_CONFIG = 1,
	/*
	 * Start a USB SPI transfer specifying number of bytes to write,
	 * read and deliver first packet of data to write.
	 */
	USB_SPI_PKT_ID_CMD_TRANSFER_START = 2,
	/* Additional packets containing write payload. */
	USB_SPI_PKT_ID_CMD_TRANSFER_CONTINUE = 3,
	/*
	 * Request the device restart the response enabling us to recover
	 * from packet loss without another SPI transfer.
	 */
	USB_SPI_PKT_ID_CMD_RESTART_RESPONSE = 4,
	/*
	 * First packet of USB SPI response with the status code
	 * and read payload if it was successful.
	 */
	USB_SPI_PKT_ID_RSP_TRANSFER_START = 5,
	/* Additional packets containing read payload. */
	USB_SPI_PKT_ID_RSP_TRANSFER_CONTINUE = 6,
	/*
	 * Request assertion or deassertion of chip select
	 */
	USB_SPI_PKT_ID_CMD_CHIP_SELECT = 7,
	/* Response to above request. */
	USB_SPI_PKT_ID_RSP_CHIP_SELECT = 8,
	/*
	 * Start a USB EEPROM SPI transfer.
	 */
	USB_SPI_PKT_ID_CMD_EEPROM_TRANSFER_START = 9,
};

enum feature_bitmap {
	/* Indicates the platform supports full duplex mode. */
	USB_SPI_FEATURE_FULL_DUPLEX_SUPPORTED = BIT(0),
	/* Indicates support for USB_SPI_PKT_ID_CMD_EEPROM_TRANSFER_START. */
	USB_SPI_FEATURE_EEPROM_SUPPORTED = BIT(1),
	/*
	 * Indicates that chip and any MUXes support bidirectional data on the
	 * two SPI data lines.
	 */
	USB_SPI_FEATURE_DUAL_MODE_SUPPORTED = BIT(2),
	/*
	 * Indicates that chip and any MUXes support bidirectional data on the
	 * "hold" and "write protect" lines.
	 */
	USB_SPI_FEATURE_QUAD_MODE_SUPPORTED = BIT(3),
	/* Indicates support for eight-line bidirectional data. */
	USB_SPI_FEATURE_OCTO_MODE_SUPPORTED = BIT(4),
};

struct usb_spi_response_configuration_v2 {
	uint16_t packet_id;
	uint16_t max_write_count;
	uint16_t max_read_count;
	uint16_t feature_bitmap;
} __packed;

struct usb_spi_command_v2 {
	uint16_t packet_id;
	uint16_t write_count;
	/* UINT16_MAX Indicates readback all on halfduplex compliant devices. */
	uint16_t read_count;
	uint8_t data[USB_SPI_PAYLOAD_SIZE_V2_START];
} __packed;

struct usb_spi_response_v2 {
	uint16_t packet_id;
	uint16_t status_code;
	uint8_t data[USB_SPI_PAYLOAD_SIZE_V2_RESPONSE];
} __packed;

struct usb_spi_continue_v2 {
	uint16_t packet_id;
	uint16_t data_index;
	uint8_t data[USB_SPI_PAYLOAD_SIZE_V2_CONTINUE];
} __packed;

enum chip_select_flags {
	/* Indicates chip select should be asserted. */
	USB_SPI_CHIP_SELECT = BIT(0)
};

struct usb_spi_chip_select_command {
	uint16_t packet_id;
	uint16_t flags;
} __packed;

struct usb_spi_chip_select_response {
	uint16_t packet_id;
	uint16_t status_code;
} __packed;

struct usb_spi_eeprom_command {
	uint16_t packet_id;
	uint16_t count;
	uint32_t flags;
	uint8_t data[USB_SPI_PAYLOAD_SIZE_EEPROM_START];
} __packed;

/*
 * Mask of the flags that are handled by logic in sub_spi.c, and not passed to
 * SPI drivers through usb_spi_board_transaction().
 */
#define EEPROM_FLAGS_NONBOARD 0xF0000000UL

#define EEPROM_FLAG_WRITE_ENABLE_POS 28U
#define EEPROM_FLAG_WRITE_ENABLE (0x1UL << EEPROM_FLAG_WRITE_ENABLE_POS)

#define EEPROM_FLAG_POLL_POS 29U
#define EEPROM_FLAG_POLL (0x1UL << EEPROM_FLAG_POLL_POS)

#define EEPROM_FLAG_READ_WRITE_POS 31U
#define EEPROM_FLAG_READ_WRITE_MSK (0x1UL << EEPROM_FLAG_READ_WRITE_POS)
#define EEPROM_FLAG_READ_WRITE_READ 0
#define EEPROM_FLAG_READ_WRITE_WRITE (0x1UL << EEPROM_FLAG_READ_WRITE_POS)

struct usb_spi_packet_ctx {
	union {
		uint8_t bytes[USB_MAX_PACKET_SIZE];
		uint16_t packet_id;
		struct usb_spi_command_v2 cmd_start;
		struct usb_spi_eeprom_command cmd_eeprom_start;
		struct usb_spi_continue_v2 cmd_continue;
		struct usb_spi_response_configuration_v2 rsp_config;
		struct usb_spi_response_v2 rsp_start;
		struct usb_spi_continue_v2 rsp_continue;
		struct usb_spi_chip_select_command cmd_cs;
		struct usb_spi_chip_select_response rsp_cs;
	} __packed;
	/*
	 * By storing the number of bytes in the header and knowing that the
	 * USB data packets are all 64B long, we are able to use the header
	 * size to store the offset of the buffer and it's size without
	 * duplicating variables that can go out of sync.
	 */
	size_t header_size;
	/* Number of bytes in the packet. */
	size_t packet_size;
};

enum usb_spi_error {
	USB_SPI_SUCCESS = 0x0000,
	USB_SPI_TIMEOUT = 0x0001,
	USB_SPI_BUSY = 0x0002,
	USB_SPI_WRITE_COUNT_INVALID = 0x0003,
	USB_SPI_READ_COUNT_INVALID = 0x0004,
	USB_SPI_DISABLED = 0x0005,
	/* The RX continue packet's data index is invalid. */
	USB_SPI_RX_BAD_DATA_INDEX = 0x0006,
	/* The RX endpoint has received more data than write count. */
	USB_SPI_RX_DATA_OVERFLOW = 0x0007,
	/* An unexpected packet arrived on the device. */
	USB_SPI_RX_UNEXPECTED_PACKET = 0x0008,
	/* The device does not support full duplex mode. */
	USB_SPI_UNSUPPORTED_FULL_DUPLEX = 0x0009,
	/* The device does not support dual/quad wire mode. */
	USB_SPI_UNSUPPORTED_EEPROM_MODE = 0x000A,
	USB_SPI_UNKNOWN_ERROR = 0x8000,
};

enum usb_spi_request {
	USB_SPI_REQ_ENABLE = 0x0000,
	USB_SPI_REQ_DISABLE = 0x0001,
};

/*
 * To optimize for speed, we want to fill whole packets for each transfer
 * This is done by setting the read and write counts to the payload sizes
 * of the smaller start packet + N * continue packets.
 *
 * If a platform has a small maximum SPI transfer size, it can be optimized
 * by setting these limits to the maximum transfer size.
 */
#ifdef CONFIG_USB_SPI_BUFFER_SIZE
#define USB_SPI_BUFFER_SIZE CONFIG_USB_SPI_BUFFER_SIZE
#else
#define USB_SPI_BUFFER_SIZE \
	(USB_SPI_PAYLOAD_SIZE_V2_START + (4 * USB_SPI_PAYLOAD_SIZE_V2_CONTINUE))
#endif
#define USB_SPI_MAX_WRITE_COUNT USB_SPI_BUFFER_SIZE
#define USB_SPI_MAX_READ_COUNT USB_SPI_BUFFER_SIZE

/* Protocol uses two-byte length fields.  Larger buffer makes no sense. */
BUILD_ASSERT(USB_SPI_BUFFER_SIZE <= 65536);

struct usb_spi_transfer_ctx {
	/* Address of transfer buffer. */
	uint8_t *buffer;
	/* Number of bytes in the transfer. */
	size_t transfer_size;
	/* Number of bytes transferred. */
	size_t transfer_index;
};

enum usb_spi_mode {
	/* No tasks are required. */
	USB_SPI_MODE_IDLE = 0,
	/* Indicates the device needs to send it's USB SPI configuration.*/
	USB_SPI_MODE_SEND_CONFIGURATION,
	/* Indicates the device needs to respond to chip select. */
	USB_SPI_MODE_SEND_CHIP_SELECT_RESPONSE,
	/* Indicates we device needs start the SPI transfer. */
	USB_SPI_MODE_START_SPI,
	/* Indicates we should start a transfer response. */
	USB_SPI_MODE_START_RESPONSE,
	/* Indicates we need to continue a transfer response. */
	USB_SPI_MODE_CONTINUE_RESPONSE,
};

struct usb_spi_state {
	/*
	 * The SPI bridge must be enabled both locally and by the host to allow
	 * access to the SPI device.  The enabled_host flag is set and cleared
	 * by sending USB_SPI_REQ_ENABLE and USB_SPI_REQ_DISABLE to the device
	 * control endpoint.  The enabled_device flag is set by calling
	 * usb_spi_enable.
	 */
	uint8_t enabled_host;
	uint8_t enabled_device;

	/*
	 * The current enabled state.  This is only updated in the deferred
	 * callback.  Whenever either of the host or device specific enable
	 * flags is changed the deferred callback is queued, and it will check
	 * their combined state against this flag.  If the combined state is
	 * different, then one of usb_spi_board_enable or usb_spi_board_disable
	 * is called and this flag is updated.  This ensures that the board
	 * specific state update routines are only called from the deferred
	 * callback.
	 */
	uint8_t enabled;

	/*
	 * The index of the SPI port currently receiving forwarded transactions,
	 * default is zero.
	 */
	uint8_t current_spi_device_idx;

	/* Mark the current operating mode. */
	enum usb_spi_mode mode;

	/*
	 * Stores the status code response for the transfer, delivered in the
	 * header for the first response packet. Error code is cleared during
	 * first RX packet and set if a failure occurs.
	 */
	uint16_t status_code;

	/* Stores the content from the USB packets */
	struct usb_spi_packet_ctx receive_packet;
	struct usb_spi_packet_ctx transmit_packet;

#ifdef CONFIG_SPI_EEPROM_SUPPORT
	/*
	 * Flags describing if and how multi-lane (dual/quad), double transfer
	 * rate, and other advanced EEPROM protocol features are to be used.
	 */
	uint32_t eeprom_flags;
#endif

	/*
	 * Context structures representing the progress receiving the SPI
	 * write data and transmitting the SPI read data.
	 */
	struct usb_spi_transfer_ctx spi_write_ctx;
	struct usb_spi_transfer_ctx spi_read_ctx;
};

/*
 * Compile time Per-USB gpio configuration stored in flash.  Instances of this
 * structure are provided by the user of the USB gpio.  This structure binds
 * together all information required to operate a USB gpio.
 */
struct usb_spi_config {
	/* In RAM state of the USB SPI bridge. */
	struct usb_spi_state *state;

	/* Interface and endpoint indices. */
	int interface;
	int endpoint;

	/* Deferred function to call to handle SPI request. */
	const struct deferred_data *deferred;

	/* Pointers to USB endpoint buffers. */
	usb_uint *ep_rx_ram;
	usb_uint *ep_tx_ram;

	/* Flags. See USB_SPI_CONFIG_FLAGS_* for definitions */
	uint32_t flags;
};

/*
 * Use when you want the SPI subsystem to be enabled even when the USB SPI
 * endpoint is not enabled by the host. This means that when this firmware
 * enables SPI, then the HW SPI module is enabled (i.e. SPE bit is set) until
 * this firmware disables the SPI module; it ignores the host's enables state.
 */
#define USB_SPI_CONFIG_FLAGS_IGNORE_HOST_SIDE_ENABLE BIT(0)

/*
 * Convenience macro for defining a USB SPI bridge driver.
 *
 * NAME is used to construct the names of the trampoline functions and the
 * usb_spi_config struct, the latter is just called NAME.
 *
 * INTERFACE is the index of the USB interface to associate with this
 * SPI driver.
 *
 * ENDPOINT is the index of the USB bulk endpoint used for receiving and
 * transmitting bytes.
 *
 * FLAGS encodes different run-time control parameters. See
 * USB_SPI_CONFIG_FLAGS_* for definitions.
 */
#define USB_SPI_CONFIG(NAME, INTERFACE, ENDPOINT, FLAGS)                    \
	static uint16_t CONCAT2(NAME,                                       \
				_buffer_)[(USB_SPI_BUFFER_SIZE + 1) / 2];   \
	static usb_uint CONCAT2(                                            \
		NAME, _ep_rx_buffer_)[USB_MAX_PACKET_SIZE / 2] __usb_ram;   \
	static usb_uint CONCAT2(                                            \
		NAME, _ep_tx_buffer_)[USB_MAX_PACKET_SIZE / 2] __usb_ram;   \
	static void CONCAT2(NAME, _deferred_)(void);                        \
	DECLARE_DEFERRED(CONCAT2(NAME, _deferred_));                        \
	struct usb_spi_state CONCAT2(NAME, _state_) = {                     \
		.enabled_host = 0,                                          \
		.enabled_device = 0,                                        \
		.enabled = 0,                                               \
		.current_spi_device_idx = 0,                                \
		.spi_write_ctx.buffer = (uint8_t *)CONCAT2(NAME, _buffer_), \
		.spi_read_ctx.buffer = (uint8_t *)CONCAT2(NAME, _buffer_),  \
	};                                                                  \
	struct usb_spi_config const NAME = {                                \
		.state = &CONCAT2(NAME, _state_),                           \
		.interface = INTERFACE,                                     \
		.endpoint = ENDPOINT,                                       \
		.deferred = &CONCAT2(NAME, _deferred__data),                \
		.ep_rx_ram = CONCAT2(NAME, _ep_rx_buffer_),                 \
		.ep_tx_ram = CONCAT2(NAME, _ep_tx_buffer_),                 \
		.flags = FLAGS,                                             \
	};                                                                  \
	const struct usb_interface_descriptor USB_IFACE_DESC(INTERFACE) = { \
		.bLength = USB_DT_INTERFACE_SIZE,                           \
		.bDescriptorType = USB_DT_INTERFACE,                        \
		.bInterfaceNumber = INTERFACE,                              \
		.bAlternateSetting = 0,                                     \
		.bNumEndpoints = 2,                                         \
		.bInterfaceClass = USB_CLASS_VENDOR_SPEC,                   \
		.bInterfaceSubClass = USB_SUBCLASS_GOOGLE_SPI,              \
		.bInterfaceProtocol = USB_PROTOCOL_GOOGLE_SPI,              \
		.iInterface = USB_STR_SPI_NAME,                             \
	};                                                                  \
	const struct usb_endpoint_descriptor USB_EP_DESC(INTERFACE, 0) = {  \
		.bLength = USB_DT_ENDPOINT_SIZE,                            \
		.bDescriptorType = USB_DT_ENDPOINT,                         \
		.bEndpointAddress = 0x80 | ENDPOINT,                        \
		.bmAttributes = 0x02 /* Bulk IN */,                         \
		.wMaxPacketSize = USB_MAX_PACKET_SIZE,                      \
		.bInterval = 10,                                            \
	};                                                                  \
	const struct usb_endpoint_descriptor USB_EP_DESC(INTERFACE, 1) = {  \
		.bLength = USB_DT_ENDPOINT_SIZE,                            \
		.bDescriptorType = USB_DT_ENDPOINT,                         \
		.bEndpointAddress = ENDPOINT,                               \
		.bmAttributes = 0x02 /* Bulk OUT */,                        \
		.wMaxPacketSize = USB_MAX_PACKET_SIZE,                      \
		.bInterval = 0,                                             \
	};                                                                  \
	static void CONCAT2(NAME, _ep_tx_)(void)                            \
	{                                                                   \
		usb_spi_tx(&NAME);                                          \
	}                                                                   \
	static void CONCAT2(NAME, _ep_rx_)(void)                            \
	{                                                                   \
		usb_spi_rx(&NAME);                                          \
	}                                                                   \
	static void CONCAT2(NAME, _ep_event_)(enum usb_ep_event evt)        \
	{                                                                   \
		usb_spi_event(&NAME, evt);                                  \
	}                                                                   \
	USB_DECLARE_EP(ENDPOINT, CONCAT2(NAME, _ep_tx_),                    \
		       CONCAT2(NAME, _ep_rx_), CONCAT2(NAME, _ep_event_));  \
	static int CONCAT2(NAME, _interface_)(usb_uint * rx_buf,            \
					      usb_uint * tx_buf)            \
	{                                                                   \
		return usb_spi_interface(&NAME, rx_buf, tx_buf);            \
	}                                                                   \
	USB_DECLARE_IFACE(INTERFACE, CONCAT2(NAME, _interface_));           \
	static void CONCAT2(NAME, _deferred_)(void)                         \
	{                                                                   \
		usb_spi_deferred(&NAME);                                    \
	}

/*
 * Handle SPI request in a deferred callback.
 */
void usb_spi_deferred(struct usb_spi_config const *config);

/*
 * Set the enable state for the USB-SPI bridge.
 *
 * The bridge must be enabled from both the host and device side
 * before the SPI bus is usable.  This allows the bridge to be
 * available for host tools to use without forcing the device to
 * disconnect or disable whatever else might be using the SPI bus.
 */
void usb_spi_enable(struct usb_spi_config const *config, int enabled);

/*
 * These functions are used by the trampoline functions defined above to
 * connect USB endpoint events with the generic USB GPIO driver.
 */
void usb_spi_tx(struct usb_spi_config const *config);
void usb_spi_rx(struct usb_spi_config const *config);
void usb_spi_event(struct usb_spi_config const *config, enum usb_ep_event evt);
int usb_spi_interface(struct usb_spi_config const *config, usb_uint *rx_buf,
		      usb_uint *tx_buf);

/*
 * These functions should be implemented by the board to provide any board
 * specific operations required to enable or disable access to the SPI device.
 */
void usb_spi_board_enable(struct usb_spi_config const *config);
void usb_spi_board_disable(struct usb_spi_config const *config);

/*
 * In order to facilitate special SPI busses not covered by standard EC
 * drivers, setting the USB_SPI_CUSTOM_SPI_DEVICE_MASK bit of spi_device->port
 * will cause the USB to SPI forwarding logic to invoke this method rather
 * than the standard spi_transaction_async().
 */
int usb_spi_board_transaction(const struct spi_device_t *spi_device,
			      uint32_t eeprom_flags, const uint8_t *txdata,
			      int txlen, uint8_t *rxdata, int rxlen);

/*
 * Flags to use in usb_spi_board_transaction_async() for advanced EEPROM
 * communication, when supported.
 */

/* Data width during the opcode stage. */
#define EEPROM_FLAG_OPCODE_WIDTH_POS 0U
#define EEPROM_FLAG_OPCODE_WIDTH_MSK (0x3UL << EEPROM_FLAG_OPCODE_WIDTH_POS)
#define EEPROM_FLAG_OPCODE_WIDTH_1WIRE (0x0UL << EEPROM_FLAG_OPCODE_WIDTH_POS)
#define EEPROM_FLAG_OPCODE_WIDTH_2WIRE (0x1UL << EEPROM_FLAG_OPCODE_WIDTH_POS)
#define EEPROM_FLAG_OPCODE_WIDTH_4WIRE (0x2UL << EEPROM_FLAG_OPCODE_WIDTH_POS)
#define EEPROM_FLAG_OPCODE_WIDTH_8WIRE (0x3UL << EEPROM_FLAG_OPCODE_WIDTH_POS)

/* Transmit opcode bits at both rising and falling clock edges. */
#define EEPROM_FLAG_OPCODE_DTR_POS 2U
#define EEPROM_FLAG_OPCODE_DTR (0x1UL << EEPROM_FLAG_OPCODE_DTR_POS)

/* Number of bytes of opcode (0-4). */
#define EEPROM_FLAG_OPCODE_LEN_POS 3U
#define EEPROM_FLAG_OPCODE_LEN_MSK (0x7UL << EEPROM_FLAG_OPCODE_LEN_POS)

/* Data width during the address stage. */
#define EEPROM_FLAG_ADDR_WIDTH_POS 6U
#define EEPROM_FLAG_ADDR_WIDTH_MSK (0x3UL << EEPROM_FLAG_ADDR_WIDTH_POS)
#define EEPROM_FLAG_ADDR_WIDTH_1WIRE (0x0UL << EEPROM_FLAG_ADDR_WIDTH_POS)
#define EEPROM_FLAG_ADDR_WIDTH_2WIRE (0x1UL << EEPROM_FLAG_ADDR_WIDTH_POS)
#define EEPROM_FLAG_ADDR_WIDTH_4WIRE (0x2UL << EEPROM_FLAG_ADDR_WIDTH_POS)
#define EEPROM_FLAG_ADDR_WIDTH_8WIRE (0x3UL << EEPROM_FLAG_ADDR_WIDTH_POS)

/* Transmit address bits at both rising and falling clock edges. */
#define EEPROM_FLAG_ADDR_DTR_POS 8U
#define EEPROM_FLAG_ADDR_DTR (0x1UL << EEPROM_FLAG_ADDR_DTR_POS)

/* Number of bytes of address (0-4). */
#define EEPROM_FLAG_ADDR_LEN_POS 9U
#define EEPROM_FLAG_ADDR_LEN_MSK (0x7UL << EEPROM_FLAG_ADDR_LEN_POS)

/* Data width during the "alternate bytes" stage. */
#define EEPROM_FLAG_ALT_WIDTH_POS 12U
#define EEPROM_FLAG_ALT_WIDTH_MSK (0x3UL << EEPROM_FLAG_ALT_WIDTH_POS)
#define EEPROM_FLAG_ALT_WIDTH_1WIRE (0x0UL << EEPROM_FLAG_ALT_WIDTH_POS)
#define EEPROM_FLAG_ALT_WIDTH_2WIRE (0x1UL << EEPROM_FLAG_ALT_WIDTH_POS)
#define EEPROM_FLAG_ALT_WIDTH_3WIRE (0x2UL << EEPROM_FLAG_ALT_WIDTH_POS)
#define EEPROM_FLAG_ALT_WIDTH_4WIRE (0x3UL << EEPROM_FLAG_ALT_WIDTH_POS)

/* Transmit alternate bits at both rising and falling clock edges. */
#define EEPROM_FLAG_ALT_DTR_POS 14U
#define EEPROM_FLAG_ALT_DTR (0x1UL << EEPROM_FLAG_ALT_DTR_POS)

/* Number of bytes of alternate data (0-4). */
#define EEPROM_FLAG_ALT_LEN_POS 15U
#define EEPROM_FLAG_ALT_LEN_MSK (0x7UL << EEPROM_FLAG_ALT_LEN_POS)

/* Number of dummy clock cycles (0-31). */
#define EEPROM_FLAG_DUMMY_CYCLES_POS 18U
#define EEPROM_FLAG_DUMMY_CYCLES_MSK (0x1FUL << EEPROM_FLAG_DUMMY_CYCLES_POS)

/* Data width during the data stage. */
#define EEPROM_FLAG_DATA_WIDTH_POS 23U
#define EEPROM_FLAG_DATA_WIDTH_MSK (0x3UL << EEPROM_FLAG_DATA_WIDTH_POS)
#define EEPROM_FLAG_DATA_WIDTH_1WIRE (0x0UL << EEPROM_FLAG_DATA_WIDTH_POS)
#define EEPROM_FLAG_DATA_WIDTH_2WIRE (0x1UL << EEPROM_FLAG_DATA_WIDTH_POS)
#define EEPROM_FLAG_DATA_WIDTH_4WIRE (0x2UL << EEPROM_FLAG_DATA_WIDTH_POS)
#define EEPROM_FLAG_DATA_WIDTH_8WIRE (0x3UL << EEPROM_FLAG_DATA_WIDTH_POS)

/* Transmit data bits at both rising and falling clock edges. */
#define EEPROM_FLAG_DATA_DTR_POS 25U
#define EEPROM_FLAG_DATA_DTR (0x1UL << EEPROM_FLAG_DATA_DTR_POS)

/*
 * Mask of the flags that cannot be ignored.  This is basically any flags
 * which call for wires to switch direction, or data being clocked on both
 * rising and falling edges.  As long as none of these are present, then the
 * remaining flags specifying the length of opcode/address can be ignored, as
 * the entire data buffer can be transmitted as a sequence of bytes, without
 * the controller knowing which parts are to be interpreted as
 * opcode/address/data.
 */
#define EEPROM_FLAGS_REQUIRING_SUPPORT                               \
	(EEPROM_FLAG_OPCODE_WIDTH_MSK | EEPROM_FLAG_OPCODE_DTR |     \
	 EEPROM_FLAG_ADDR_WIDTH_MSK | EEPROM_FLAG_ADDR_DTR |         \
	 EEPROM_FLAG_ALT_WIDTH_MSK | EEPROM_FLAG_ALT_DTR |           \
	 EEPROM_FLAG_DUMMY_CYCLES_MSK | EEPROM_FLAG_DATA_WIDTH_MSK | \
	 EEPROM_FLAG_DATA_DTR)

#endif /* __CROS_EC_USB_SPI_H */
