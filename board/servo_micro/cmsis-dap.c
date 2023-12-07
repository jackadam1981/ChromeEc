/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "consumer.h"
#include "gpio.h"
#include "i2c.h"
#include "producer.h"
#include "queue.h"
#include "queue_policies.h"
#include "timer.h"
#include "usb-stream.h"
#include "usb_i2c.h"

#include <stddef.h>
#include <stdint.h>

static const uint32_t DEFAULT_JTAG_CLOCK_HZ = 100000;
static const uint32_t OVERHEAD_CLOCK_CYCLES = 50;

/*
 * The CMSIS-DAP specification calls for identifying the USB interface by
 * looking for "CMSIS-DAP" in the string name, not by subclass/protocol.
 */
#define USB_SUBCLASS_CMSIS_DAP USB_SUBCLASS_GOOGLE_I2C
#define USB_PROTOCOL_CMSIS_DAP USB_PROTOCOL_GOOGLE_I2C

/* CMSIS-DAP command bytes */
enum cmsis_dap_command_t {
	/* General commands */
	DAP_Info = 0x00,
	DAP_HostStatus = 0x01,
	DAP_Connect = 0x02,
	DAP_Disconnect = 0x03,
	DAP_TransferConfigure = 0x04,
	DAP_Transfer = 0x05,
	DAP_TransferBlock = 0x06,
	DAP_TransferAbort = 0x07,
	DAP_WriteAbort = 0x08,
	DAP_Delay = 0x09,
	DAP_ResetTarget = 0x0A,

	/* Commands used both for SWD and JTAG */
	DAP_SWJ_Pins = 0x10,
	DAP_SWJ_Clock = 0x11,
	DAP_SWJ_Sequence = 0x12,

	/* Commands used only with SWD */
	DAP_SWD_Configure = 0x13,

	/* Commands used only with JTAG */
	DAP_JTAG_Sequence = 0x14,
	DAP_JTAG_Configure = 0x15,
	DAP_JTAG_IdCode = 0x16,

	/* Commands used for UART tunnelling */
	DAP_SWO_Transport = 0x17,
	DAP_SWO_Mode = 0x18,
	DAP_SWO_Baudrate = 0x19,
	DAP_SWO_Control = 0x1A,
	DAP_SWO_Status = 0x1B,
	DAP_SWO_Data = 0x1C,

	/* Commands used to group other commands */
	DAP_QueueCommands = 0x7E,
	DAP_ExecuteCommands = 0x7F,

	/* Vendor-specific commands (reserved range 0x80 - 0x9F) */
	DAP_GOOG_Info = 0x80,
	DAP_GOOG_I2c = 0x81,

};

/* DAP Status Code */
enum cmsis_dap_status_t {
	STATUS_Ok = 0x00,
	STATUS_Error = 0xFF,
};

/* Parameter for info command */
enum cmsis_dap_info_subcommand_t {
	INFO_Vendor = 0x01,
	INFO_Product = 0x02,
	INFO_Serial = 0x03,
	INFO_Version = 0x04,
	INFO_DeviceVendor = 0x05,
	INFO_DeviceName = 0x06,
	INFO_Capabilities = 0xF0,
	INFO_SwoBufferSize = 0xFD,
	INFO_PacketCount = 0xFE,
	INFO_PacketSize = 0xFF,
};

/* Bitfield response to INFO_Capabilities */
const uint16_t CAP_Swd = BIT(0);
const uint16_t CAP_Jtag = BIT(1);
const uint16_t CAP_SwoUart = BIT(2);
const uint16_t CAP_SwoManchester = BIT(3);
const uint16_t CAP_AtomicCommands = BIT(4);
const uint16_t CAP_TestDomainTimer = BIT(5);
const uint16_t CAP_SwoStreamingTrace = BIT(6);
const uint16_t CAP_UartCommunicationPort = BIT(7);
const uint16_t CAP_UsbComPort = BIT(8);

enum connect_req_t {
	CONN_REQ_Default = 0,
	CONN_REQ_Swd = 1,
	CONN_REQ_Jtag = 2,
};

enum connect_resp_t {
	CONN_RESP_Failed = 0,
	CONN_RESP_Swd = 1,
	CONN_RESP_Jtag = 2,
};

/* Parameter for vendor (Google) info command */
enum goog_info_subcommand_t {
	GOOG_INFO_Capabilities = 0x00,
};

/* Bitfield response to vendor (Google) capabities request */
const uint32_t GOOG_CAP_I2c = BIT(0);

/* Bitfield used in DAP_SWJ_Pins request */
const uint8_t PIN_SwClk_Tck = 0x01;
const uint8_t PIN_SwDio_Tms = 0x02;
const uint8_t PIN_Tdi = 0x04;
const uint8_t PIN_Tdo = 0x08;
const uint8_t PIN_Trst = 0x20;
const uint8_t PIN_Reset = 0x80;

/* Bitfield used in DAP_JTAG_Sequence request */
const uint8_t SEQ_NumBits = 0x3F;
const uint8_t SEQ_Tms = 0x40;
const uint8_t SEQ_CaptureTdo = 0x80;

/*
 * Incoming and outgoing byte streams.
 */

static struct queue const cmsis_dap_tx_queue;
static struct queue const cmsis_dap_rx_queue;

static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];

static bool jtag_enabled = false;
static bool swd_enabled = false;
static uint16_t jtag_half_period_count =
	CPU_CLOCK / DEFAULT_JTAG_CLOCK_HZ / 2 - OVERHEAD_CLOCK_CYCLES;

/*
 * A few routines mostly copied from usb_i2c.c.
 */

static int16_t usb_i2c_map_error(int error)
{
	switch (error) {
	case EC_SUCCESS:
		return USB_I2C_SUCCESS;
	case EC_ERROR_TIMEOUT:
		return USB_I2C_TIMEOUT;
	case EC_ERROR_BUSY:
		return USB_I2C_BUSY;
	default:
		return USB_I2C_UNKNOWN_ERROR | (error & 0x7fff);
	}
}

static void usb_i2c_execute(unsigned int expected_size)
{
	uint32_t count = queue_remove_units(&cmsis_dap_rx_queue, rx_buffer,
					    expected_size);
	uint16_t *i2c_buffer = (uint16_t *)&rx_buffer[0];
	/* Payload is ready to execute. */
	int portindex = (i2c_buffer[0] >> 0) & 0xf;
	uint16_t addr_flags = (i2c_buffer[0] >> 8) & 0x7f;
	int write_count = ((i2c_buffer[0] << 4) & 0xf00) |
			  ((i2c_buffer[1] >> 0) & 0xff);
	int read_count = (i2c_buffer[1] >> 8) & 0xff;
	int offset = 0; /* Offset for extended reading header. */

	i2c_buffer[0] = 0;
	i2c_buffer[1] = 0;

	if (read_count & 0x80) {
		read_count = ((i2c_buffer[2] & 0xff) << 7) |
			     (read_count & 0x7f);
		offset = 2;
	}

	if (!usb_i2c_board_is_enabled()) {
		i2c_buffer[0] = USB_I2C_DISABLED;
	} else if (!read_count && !write_count) {
		/* No-op, report as success */
		i2c_buffer[0] = USB_I2C_SUCCESS;
	} else if (write_count > CONFIG_USB_I2C_MAX_WRITE_COUNT ||
		   write_count != (count - 4 - offset)) {
		i2c_buffer[0] = USB_I2C_WRITE_COUNT_INVALID;
	} else if (read_count > CONFIG_USB_I2C_MAX_READ_COUNT) {
		i2c_buffer[0] = USB_I2C_READ_COUNT_INVALID;
	} else if (portindex >= i2c_ports_used) {
		i2c_buffer[0] = USB_I2C_PORT_INVALID;
	} else {
		int ret = i2c_xfer(i2c_ports[portindex].port, addr_flags,
				   (uint8_t *)(i2c_buffer + 2) + offset,
				   write_count, (uint8_t *)(i2c_buffer + 2),
				   read_count);
		i2c_buffer[0] = usb_i2c_map_error(ret);
	}
	queue_add_units(&cmsis_dap_tx_queue, i2c_buffer, read_count + 4);
}

/*
 * Implementation of handler routines for each CMSIS-DAP command.
 */

/* Info command, used to discover which other commands are supported. */
static void dap_info(size_t peek_c)
{
	const char *CMSIS_DAP_VERSION_STR = "2.1.1";
	const uint16_t CAPABILITIES = CAP_Jtag | CAP_Swd;
	struct usb_string_desc *sd = usb_serialno_desc;
	int i;

	if (peek_c < 2)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 2);
	switch (rx_buffer[1]) {
	case INFO_Serial:
		for (i = 0; i < CONFIG_SERIALNO_LEN && sd->_data[i]; i++)
			tx_buffer[2 + i] = sd->_data[i];
		tx_buffer[1] = i;
		queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2 + i);
		break;
	case INFO_Version:
		tx_buffer[1] = strlen(CMSIS_DAP_VERSION_STR) + 1;
		memcpy(tx_buffer + 2, CMSIS_DAP_VERSION_STR, tx_buffer[1]);
		queue_add_units(&cmsis_dap_tx_queue, tx_buffer,
				2 + tx_buffer[1]);
		break;
	case INFO_Capabilities:
		tx_buffer[1] = sizeof(CAPABILITIES);
		memcpy(tx_buffer + 2, &CAPABILITIES, sizeof(CAPABILITIES));
		queue_add_units(&cmsis_dap_tx_queue, tx_buffer,
				2 + tx_buffer[1]);
		break;
	default:
		tx_buffer[1] = 0;
		queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
		break;
	}
}

/* Informational command, to allow debugging device to indicate status. */
static void dap_host_status(size_t peek_c)
{
	if (peek_c < 3)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 3);
	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/* Establish JTAG connection, take control of JTAG pins. */
static void dap_connect(size_t peek_c)
{
	if (peek_c < 2)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 2);
	switch (rx_buffer[1]) {
	case CONN_REQ_Default:
	case CONN_REQ_Jtag:
		tx_buffer[1] = CONN_RESP_Jtag;
		if (!jtag_enabled) {
			jtag_enabled = true;

			/* Disconnect DUT_SPI1 lines from PA0/1 */
			gpio_set_level(GPIO_SPI1_MUX_SEL, true);

			/* Turn PA0/1 into GPIO rather than UART */
			gpio_set_flags(GPIO_UART3_TX_SERVO_JTAG_TCK, GPIO_OUT_LOW);
			gpio_set_flags(GPIO_UART3_RX_JTAG_BUFFER_TO_SERVO_TDO, GPIO_INPUT);

			/* Configure buffers for output */
			gpio_set_level(GPIO_SERVO_JTAG_TMS_DIR, true);
			gpio_set_level(GPIO_SERVO_JTAG_TDI_DIR, true);
			gpio_set_level(GPIO_SERVO_JTAG_TRST_DIR, true);

			/* Configure signals feeding into above buffers */
			gpio_set_flags(GPIO_SERVO_JTAG_TMS, GPIO_OUT_LOW);
			gpio_set_flags(GPIO_SERVO_JTAG_TDI, GPIO_OUT_LOW);
			gpio_set_flags(GPIO_SERVO_JTAG_TRST_L, GPIO_OUT_HIGH);

			/* Enable JTAG buffers */
			gpio_set_level(GPIO_JTAG_BUFOUT_EN_L, false);
			gpio_set_level(GPIO_JTAG_BUFIN_EN_L, false);
		}
		break;
	case CONN_REQ_Swd:
		tx_buffer[1] = CONN_RESP_Swd;
		if (jtag_enabled) {
		}
		if (!swd_enabled) {
			swd_enabled = true;
			
			/* Turn PA0/1 into GPIO rather than UART */
			gpio_set_flags(GPIO_UART3_TX_SERVO_JTAG_TCK, GPIO_OUT_LOW);
			gpio_set_flags(GPIO_UART3_RX_JTAG_BUFFER_TO_SERVO_TDO, GPIO_INPUT);

			/* Configure buffers for input (TMS for now) */
			gpio_set_level(GPIO_SERVO_JTAG_TMS_DIR, false);
			gpio_set_level(GPIO_SERVO_JTAG_TDI_DIR, false);
			gpio_set_level(GPIO_SERVO_JTAG_TRST_DIR, false);

			/* Configure signals feeding into above buffers */
			gpio_set_flags(GPIO_SERVO_JTAG_TMS, GPIO_INPUT);
			gpio_set_flags(GPIO_SERVO_JTAG_TDI, GPIO_INPUT);
			gpio_set_flags(GPIO_SERVO_JTAG_TRST_L, GPIO_INPUT);

			/* Enable JTAG buffers */
			gpio_set_level(GPIO_JTAG_BUFOUT_EN_L, false);
			gpio_set_level(GPIO_JTAG_BUFIN_EN_L, false);
		}
		break;
	default:
		tx_buffer[1] = CONN_RESP_Failed;
	}
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/* Restore JTAG pins to previous configuration. */
static void dap_disconnect(size_t peek_c)
{
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 1);

	if (jtag_enabled) {
		jtag_enabled = false;

		/* Disable JTAG buffers */
		gpio_set_level(GPIO_JTAG_BUFOUT_EN_L, true);
		gpio_set_level(GPIO_JTAG_BUFIN_EN_L, true);

		/* Turn PA0/1 into GPIO rather than UART */
		gpio_set_flags(GPIO_UART3_TX_SERVO_JTAG_TCK, GPIO_ALTERNATE);
		gpio_set_flags(GPIO_UART3_RX_JTAG_BUFFER_TO_SERVO_TDO, GPIO_ALTERNATE);

		/* Re-connect DUT_SPI1 lines to PA0/1 */
		/* TODO: Probably should restore state as before enabling JTAG */
		gpio_set_level(GPIO_SPI1_MUX_SEL, false);
	}

	if (swd_enabled) {
		swd_enabled = false;

		/* Disable JTAG buffers */
		gpio_set_level(GPIO_JTAG_BUFOUT_EN_L, true);
		gpio_set_level(GPIO_JTAG_BUFIN_EN_L, true);

		/* Turn PA0/1 into GPIO rather than UART */
		gpio_set_flags(GPIO_UART3_TX_SERVO_JTAG_TCK, GPIO_ALTERNATE);
		gpio_set_flags(GPIO_UART3_RX_JTAG_BUFFER_TO_SERVO_TDO, GPIO_ALTERNATE);
	}
	
	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/* Busy-wait half a JTAG clock cycle. */
static inline __attribute__((always_inline)) void half_clock_delay(void)
{
	/* Set counter value.  Timer will immediately begin counting down. */
	STM32_TIM_CNT(3) = jtag_half_period_count;
	/*
	 * Wait for counter value to wrap around zero.  Worst case, counting
	 * down from 32767 at a 104Mhz clock frequency will finish in 315us.
	 */
	while (((int16_t)STM32_TIM_CNT(3)) >= 0)
		;
}

/* Configure parameters for DAP_Transfer family of requests. */
static void dap_transfer_configure(size_t peek_c)
{
	if (peek_c < 6)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 6);

	/*
	 * This file does not offer support for the DAP_Transfer family of
	 * requests, and OpenOCD does not seem to issue any requests (at least
	 * not when operating on a RISC-V OpenTitan code.
	 *
	 * OpenOCD still sends this configuration request as part of its setup
	 * sequence, we can safely ignore the parameters given, and report
	 * success to the caller.
	 */

	ccprintf("DAP_Transfer_Configure idle cycles: %d, wait retry: %d, match retry: %d\n",
		 rx_buffer[1],
		 rx_buffer[2] + rx_buffer[3] * 256,
		 rx_buffer[4] + rx_buffer[5] * 256);

	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

static void dap_transfer(size_t peek_c)
{
	int num_transfers, offset, c, tx_offset;
	
	if (peek_c < 3)
		return;

	c = queue_count(&cmsis_dap_rx_queue);

	/* Check whether a complete request is in queue. */
	queue_peek_units(&cmsis_dap_rx_queue, rx_buffer, 0, c);
	num_transfers = rx_buffer[2];
	offset = 3;
	for (size_t i = 0; i < num_transfers; i++) {
		uint8_t header = rx_buffer[offset];
		offset += 1;
		if ((header & 0x02) == 0 || (header & 0x30) != 0) {
			offset += 4;
		}
		if (offset > c) {
			/* We do not yet have all bytes of the request. */
			return;
		}
	}

	/* We have a complete request, mark as removed from the queue. */
	queue_advance_head(&cmsis_dap_rx_queue, offset);

	offset = 3;
	tx_offset = 2;
	tx_buffer[1] = num_transfers;
	ccprintf("DAP_Transfer[%d]\n", num_transfers);
	for (size_t i = 0; i < num_transfers; i++) {
		uint8_t header = rx_buffer[offset];
		ccprintf("  header %02x\n", header);
		offset += 1;
		if ((header & 0x02) != 0) {
			// Read request
			uint8_t ack = 0;
			
			uint8_t swd_header = 0x01
				| (header & 0x01 ? 0x02 : 0)
				| 0x04
				| ((header & 0x0C) << 1)
				| 0x80;

			if (!!(swd_header & 0x02)
			    ^ !(swd_header & 0x04)
			    ^ !(swd_header & 0x08)
			    ^ !(swd_header & 0x10)) {
				// Parity
				swd_header |= 0x20;
			}

			ccprintf("  swd_header 0x%02x\n", swd_header);
			// Enable drive on TMS
			gpio_set_level(GPIO_SERVO_JTAG_TMS_DIR, true);
			gpio_set_flags(GPIO_SERVO_JTAG_TMS, GPIO_OUT_HIGH);

			for (unsigned int i = 0; i < 8; i++) {
				gpio_set_level(GPIO_SERVO_JTAG_TMS,
					       !!(swd_header & (1 << (i % 8))));
				half_clock_delay();
				gpio_set_level(GPIO_UART3_TX_SERVO_JTAG_TCK, true);
				half_clock_delay();
				gpio_set_level(GPIO_UART3_TX_SERVO_JTAG_TCK, false);
			}

			// Disable drive on TMS (should have had pullup)
			gpio_set_flags(GPIO_SERVO_JTAG_TMS, GPIO_INPUT);
			gpio_set_level(GPIO_SERVO_JTAG_TMS_DIR, false);

			for (int i = 0; i < 4; i++) {
				half_clock_delay();
				gpio_set_level(GPIO_UART3_TX_SERVO_JTAG_TCK, true);
				half_clock_delay();
				ack |= !!gpio_get_level(GPIO_SERVO_JTAG_TMS) << i;
				gpio_set_level(GPIO_UART3_TX_SERVO_JTAG_TCK, false);
			}
			ccprintf("  ack 0x%02x\n", ack);

			tx_buffer[tx_offset] = ack;
			tx_offset++;

			for (int b = 0; b < 4; b++) {
				uint8_t byte = 0;
				for (int i = 0; i < 8; i++) {
					half_clock_delay();
					gpio_set_level(GPIO_UART3_TX_SERVO_JTAG_TCK, true);
					half_clock_delay();
					byte |= !!gpio_get_level(GPIO_SERVO_JTAG_TMS) << i;
					gpio_set_level(GPIO_UART3_TX_SERVO_JTAG_TCK, false);
				}
				tx_buffer[tx_offset] = byte;
				tx_offset++;
				ccprintf("  data 0x%02x\n", byte);
			}

			half_clock_delay();
			gpio_set_level(GPIO_UART3_TX_SERVO_JTAG_TCK, true);
			half_clock_delay();
			//partity = !!gpio_get_level(GPIO_SERVO_JTAG_TMS) << i;
			gpio_set_level(GPIO_UART3_TX_SERVO_JTAG_TCK, false);
		}

		if ((header & 0x02) == 0 || (header & 0x30) != 0) {
			offset += 4;
		}
		if (offset > c) {
			/* We do not yet have all bytes of the request. */
			return;
		}
	}


	
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/* Reset the GSC (using same pin as if blue button was pressed). */
static void dap_reset_target(size_t peek_c)
{
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 1);

	tx_buffer[2] = 0; /* Unable to reset target */
	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 3);
}

/* One-time setting of the output level of each JTAG signal. */
static void dap_swj_pins(size_t peek_c)
{
	uint8_t pin_value;
	uint8_t pin_mask;
	uint32_t wait_us;

	if (peek_c < 7)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 7);

	pin_value = rx_buffer[1];
	pin_mask = rx_buffer[2];
	memcpy(&wait_us, rx_buffer + 3, sizeof(wait_us));

	if ((pin_mask & PIN_SwClk_Tck))
		gpio_set_level(GPIO_UART3_TX_SERVO_JTAG_TCK,
			       !!(pin_value & PIN_SwClk_Tck));
	if ((pin_mask & PIN_SwDio_Tms))
		gpio_set_level(GPIO_SERVO_JTAG_TMS,
			       !!(pin_value & PIN_SwDio_Tms));
	if ((pin_mask & PIN_Tdi))
		gpio_set_level(GPIO_SERVO_JTAG_TDI, !!(pin_value & PIN_Tdi));
	if ((pin_mask & PIN_Trst))
		gpio_set_level(GPIO_SERVO_JTAG_TRST_L, !!(pin_value & PIN_Trst));
	/*
	if ((pin_mask & PIN_Reset) && shield_reset_pin != GPIO_COUNT)
		gpio_set_level(shield_reset_pin, !!(pin_value & PIN_Reset));
	*/
	if (wait_us)
		usleep(wait_us);

	tx_buffer[1] = 0;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/* Set JTAG clock frequency. */
static void dap_swj_clock(size_t peek_c)
{
	uint32_t new_clock_hz, new_half_period_count;

	if (peek_c < 5)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 5);

	memcpy(&new_clock_hz, rx_buffer + 1, sizeof(new_clock_hz));

	if (!new_clock_hz) {
		tx_buffer[1] = STATUS_Error;
	} else {
		new_half_period_count = CPU_CLOCK / new_clock_hz / 2;

		/*
		 * At this point, new_half_period_count contains the number of
		 * CPU clock cycles for a half JTAG clock period.  This will be
		 * used in a wait loop in the bit banging logic.
		 *
		 * Empirically, it has been stablished that at least 50 CPU
		 * clock cycles are used by execution of GPIO manipulations
		 * involved in clock toggling and data shifting, so we subtract
		 * that from the number of cycles that will be "burned" in each
		 * clock phaze while generating the waveform.
		 */
		if (new_half_period_count <= OVERHEAD_CLOCK_CYCLES) {
			/*
			 * Requested speed as at or above the limit, run with no
			 * delay at all.
			 */
			new_half_period_count = 0;
		} else {
			new_half_period_count -= OVERHEAD_CLOCK_CYCLES;
		}

		if (new_half_period_count >= 0x8000) {
			/*
			 * Requested clock is too slow.  Out of range for a
			 * signed 16-bit countdown timer.
			 */
			tx_buffer[1] = STATUS_Error;
		} else {
			jtag_half_period_count = new_half_period_count;
			tx_buffer[1] = STATUS_Ok;
		}
	}
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/* Clock data out on TMS. */
static void dap_swj_sequence(size_t peek_c)
{
	unsigned int bit_count;
	unsigned c;
	if (peek_c < 2)
		return;
	bit_count = rx_buffer[1] == 0 ? 256 : rx_buffer[1];
	c = queue_count(&cmsis_dap_rx_queue);
	if (c < 2 + (bit_count + 7) / 8)
		return;
	ccprintf("SWJ_Sequence[%d]\n", bit_count);
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, c);

	if (swd_enabled) {
		// Enable drive on TMS
		gpio_set_level(GPIO_SERVO_JTAG_TMS_DIR, true);
		gpio_set_flags(GPIO_SERVO_JTAG_TMS, GPIO_OUT_HIGH);
	}
	
	for (unsigned int i = 0; i < bit_count; i++) {
		gpio_set_level(GPIO_SERVO_JTAG_TMS,
			       !!(rx_buffer[2 + i / 8] & (1 << (i % 8))));
		half_clock_delay();
		gpio_set_level(GPIO_UART3_TX_SERVO_JTAG_TCK, true);
		half_clock_delay();
		gpio_set_level(GPIO_UART3_TX_SERVO_JTAG_TCK, false);
	}

	if (swd_enabled) {
		// Disable drive on TMS
		gpio_set_flags(GPIO_SERVO_JTAG_TMS, GPIO_INPUT);
		gpio_set_level(GPIO_SERVO_JTAG_TMS_DIR, false);
	}

	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/*
 * Do a JTAG transaction, consisting of one or more sequences of clocking data
 * on TDI (between 1 and 64 bits), while keeping TMS at a particular level.
 */
static void dap_jtag_sequence(size_t peek_c)
{
	/*
	 * As an optimization, resolve the IO port addresses and masks of
	 * frequently used GPIOs.
	 */
	volatile uint32_t *const jtag_clk_bsrr =
		&STM32_GPIO_BSRR(gpio_list[GPIO_UART3_TX_SERVO_JTAG_TCK].port);
	const uint32_t jtag_clk_mask_set = gpio_list[GPIO_UART3_TX_SERVO_JTAG_TCK].mask;
	const uint32_t jtag_clk_mask_clear =
		gpio_list[GPIO_UART3_TX_SERVO_JTAG_TCK].mask << 16;

	volatile uint32_t *const jtag_tms_bsrr =
		&STM32_GPIO_BSRR(gpio_list[GPIO_SERVO_JTAG_TMS].port);
	const uint32_t jtag_tms_mask_set = gpio_list[GPIO_SERVO_JTAG_TMS].mask;
	const uint32_t jtag_tms_mask_clear = gpio_list[GPIO_SERVO_JTAG_TMS].mask
					     << 16;

	volatile uint32_t *const jtag_tdi_bsrr =
		&STM32_GPIO_BSRR(gpio_list[GPIO_SERVO_JTAG_TDI].port);
	const uint32_t jtag_tdi_mask_set = gpio_list[GPIO_SERVO_JTAG_TDI].mask;
	const uint32_t jtag_tdi_mask_clear = gpio_list[GPIO_SERVO_JTAG_TDI].mask
					     << 16;

	volatile uint16_t *const jtag_tdo_idr =
		&STM32_GPIO_IDR(gpio_list[GPIO_UART3_RX_JTAG_BUFFER_TO_SERVO_TDO].port);
	const uint16_t jtag_tdo_mask = gpio_list[GPIO_UART3_RX_JTAG_BUFFER_TO_SERVO_TDO].mask;


	int c;
	size_t num_sequences;
	size_t offset;
	const uint8_t *ptr;
	const uint8_t *end;
	uint8_t *tx_ptr;

	if (peek_c < 3)
		return;
	c = queue_count(&cmsis_dap_rx_queue);

	/* Check whether a complete request is in queue. */
	queue_peek_units(&cmsis_dap_rx_queue, rx_buffer, 0, c);
	num_sequences = rx_buffer[1];
	offset = 2;
	for (size_t i = 0; i < num_sequences; i++) {
		uint8_t header = rx_buffer[offset];
		unsigned int bit_count = header & 0x3F;
		if (bit_count == 0)
			bit_count = 0x40;
		offset += 1 + (bit_count + 7) / 8;
		if (offset > c) {
			/* We do not yet have all bytes of the request. */
			return;
		}
	}

	/* We have a complete request, mark as removed from the queue. */
	queue_advance_head(&cmsis_dap_rx_queue, offset);
	/* Prepare output buffer for being populated one bit at a time. */
	memset(tx_buffer + 1, 0, sizeof(tx_buffer) - 1);

	/* Clock should be low already, but make sure. */
	*jtag_clk_bsrr = jtag_clk_mask_clear;

	/*
	 * Iterate over the list of "sequences", each having a one-byte header
	 * specifying how many bits in the sequence, what the value of TMS
	 * during this sequence, and whether to record TDO during this sequence.
	 */
	ptr = rx_buffer + 2;
	end = rx_buffer + offset;
	tx_ptr = tx_buffer + 2;
	while (ptr < end) {
		bool capture_tdo;
		unsigned int bit_count;
		/* Consume and decode header byte for this one "sequence". */
		uint8_t header = *ptr++;
		*jtag_tms_bsrr = header & SEQ_Tms ? jtag_tms_mask_set :
						    jtag_tms_mask_clear;
		capture_tdo = !!(header & SEQ_CaptureTdo);
		bit_count = (((header - 1) & SEQ_NumBits) + 1);

		/*
		 * With TMS set at a given value, clock 1 - 64 bits of data on
		 * TDI/TDO.
		 */
		for (unsigned int i = 0; i < bit_count; i++) {
			uint32_t tdo_val;
			*jtag_tdi_bsrr = ptr[i / 8] & (1 << (i % 8)) ?
						 jtag_tdi_mask_set :
						 jtag_tdi_mask_clear;
			half_clock_delay();
			*jtag_clk_bsrr = jtag_clk_mask_set;
			tdo_val = !!(*jtag_tdo_idr & jtag_tdo_mask);
			if (capture_tdo) {
				tx_ptr[i / 8] |= tdo_val << (i % 8);
			} else {
				/*
				 * Spend time comparable to memory access above.
				 *
				 * Statement below clears all the bits in the
				 * current output byte which are "ahead" of
				 * where we would be placing the next sampled
				 * bits, that is, into the bit range that was
				 * already zero'ed by memset().
				 */
				tx_ptr[i / 8] &= ~(0xFF << (i % 8));
			}
			half_clock_delay();
			*jtag_clk_bsrr = jtag_clk_mask_clear;
		}
		/* Consume the data bytes of this one "sequence". */
		ptr += (bit_count + 7) / 8;
		if (capture_tdo)
			tx_ptr += (bit_count + 7) / 8;
	}

	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, tx_ptr - tx_buffer);
}

static void dap_swd_configure(size_t peek_c)
{
	if (peek_c < 2)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 2);

	ccprintf("SWD configuration: %02x\n", rx_buffer[1]);

	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}


/* Vendor command (HyperDebug): Discover Google-specific capabilities. */
static void dap_goog_info(size_t peek_c)
{
	const uint16_t CAPABILITIES = GOOG_CAP_I2c;

	if (peek_c < 2)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 2);
	switch (rx_buffer[1]) {
	case GOOG_INFO_Capabilities:
		queue_remove_unit(&cmsis_dap_rx_queue, rx_buffer);
		tx_buffer[1] = sizeof(CAPABILITIES);
		memcpy(tx_buffer + 2, &CAPABILITIES, sizeof(CAPABILITIES));
		queue_add_units(&cmsis_dap_tx_queue, tx_buffer,
				2 + tx_buffer[1]);
		break;
	}
}

/* Vendor command (HyperDebug): I2C forwarding. */
static void dap_goog_i2c(size_t peek_c)
{
	unsigned int expected_size;

	if (peek_c < 5)
		return;

	/*
	 * The first four bytes of the packet (following the CMSIS-DAP one-byte
	 * header) will describe its expected size.
	 */
	if (rx_buffer[4] & 0x80)
		expected_size = 6;
	else
		expected_size = 4;

	/* write count */
	expected_size += (((size_t)rx_buffer[1] & 0xf0) << 4) | rx_buffer[3];

	if (queue_count(&cmsis_dap_rx_queue) >= expected_size + 1) {
		queue_remove_unit(&cmsis_dap_rx_queue, rx_buffer);
		queue_add_unit(&cmsis_dap_tx_queue, rx_buffer);
		usb_i2c_execute(expected_size);
	}
}

/* Map from CMSIS-DAP command byte to handler routine. */
static void (*dispatch_table[256])(size_t peek_c) = {
	[DAP_Info] = dap_info,
	[DAP_GOOG_Info] = dap_goog_info,
	[DAP_GOOG_I2c] = dap_goog_i2c,
	[DAP_HostStatus] = dap_host_status,
	[DAP_Connect] = dap_connect,
	[DAP_Disconnect] = dap_disconnect,
	[DAP_TransferConfigure] = dap_transfer_configure,
	[DAP_Transfer] = dap_transfer,
	[DAP_ResetTarget] = dap_reset_target,
	[DAP_SWJ_Pins] = dap_swj_pins,
	[DAP_SWJ_Clock] = dap_swj_clock,
	[DAP_SWJ_Sequence] = dap_swj_sequence,
	[DAP_JTAG_Sequence] = dap_jtag_sequence,
	[DAP_SWD_Configure] = dap_swd_configure,
};

/* Dispatch incoming request according to table above. */
static void cmsis_dap_dispatch(void)
{
	/* Peek at the incoming data. */
	size_t peek_c = queue_peek_units(&cmsis_dap_rx_queue, rx_buffer, 0, 8);
	if (peek_c < 1) {
		/* Not enough data to start decoding request. */
		return;
	}

	if (dispatch_table[rx_buffer[0]]) {
		/* First byte of response is always same as command byte. */
		tx_buffer[0] = rx_buffer[0];
		//ccprintf("CMSIS-DAP request %02x\n", rx_buffer[0]);
		/* Invoke handler routine. */
		dispatch_table[rx_buffer[0]](peek_c);
	} else {
		ccprintf("CMSIS-DAP unrecognized request %02x\n", rx_buffer[0]);
		/*
		 * Unrecognized command.  The CMSIS-DAP protocol does not allow
		 * us to know the size of the data of a command in general, nor
		 * is there any command-independent means for sending "not
		 * understood".  The code below discards all queued incoming
		 * data, and sends no reply. */
		queue_advance_head(&cmsis_dap_rx_queue,
				   queue_count(&cmsis_dap_rx_queue));
	}
}

/*
 * Main entry point for handling CMSIS-DAP requests received via USB.
 */
void cmsis_dap_task(void *unused)
{
	while (true) {
		/* Wait for cmsis_dap_written() to wake up this task. */
		task_wait_event(0);
		/* Dispatch CMSIS request, if fully received. */
		cmsis_dap_dispatch();
	}
}

/*
 * Declare USB interface for CMSIS-DAP.
 */
USB_STREAM_CONFIG_FULL(cmsis_dap_usb, USB_IFACE_CMSIS_DAP,
		       USB_CLASS_VENDOR_SPEC, USB_SUBCLASS_CMSIS_DAP,
		       USB_PROTOCOL_CMSIS_DAP, USB_STR_CMSIS_DAP_NAME,
		       USB_EP_CMSIS_DAP, USB_MAX_PACKET_SIZE,
		       USB_MAX_PACKET_SIZE, cmsis_dap_rx_queue,
		       cmsis_dap_tx_queue);

static void cmsis_dap_written(struct consumer const *consumer, size_t count)
{
	task_wake(TASK_ID_CMSIS_DAP);
}

struct consumer_ops const cmsis_dap_consumer_ops = {
	.written = cmsis_dap_written,
};

struct consumer const cmsis_dap_consumer = {
	.queue = &cmsis_dap_rx_queue,
	.ops = &cmsis_dap_consumer_ops,
};

static struct queue const cmsis_dap_tx_queue = QUEUE_DIRECT(
	sizeof(tx_buffer), uint8_t, null_producer, cmsis_dap_usb.consumer);

static struct queue const cmsis_dap_rx_queue = QUEUE_DIRECT(
	sizeof(rx_buffer), uint8_t, cmsis_dap_usb.producer, cmsis_dap_consumer);
