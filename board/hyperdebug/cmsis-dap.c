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

static const uint32_t DEFAULT_JTAG_CLOCK_HZ = 100000;
static const uint32_t OVERHEAD_CLOCK_CYCLES = 60;

/*
 * The CMSIS-DAP specification calls for identifying the USB interface by
 * looking for "CMSIS-DAP" in the string name, not by subclass/protocol.
 */
#define USB_SUBCLASS_CMSIS_DAP 0x00
#define USB_PROTOCOL_CMSIS_DAP 0x00

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
const uint16_t CAP_Swd = 0x0001;
const uint16_t CAP_Jtag = 0x0002;
const uint16_t CAP_SwoUart = 0x0004;
const uint16_t CAP_SwoManchester = 0x0008;
const uint16_t CAP_AtomicCommands = 0x0010;
const uint16_t CAP_TestDomainTimer = 0x0020;
const uint16_t CAP_SwoStreamingTrace = 0x0040;
const uint16_t CAP_UartCommunicationPort = 0x0080;
const uint16_t CAP_UsbComPort = 0x0100;

/* Bitfield response to vendor (Google) capabities request */
const uint32_t GOOG_CAP_I2c = 0x00000001;

/* Bitfield used in DAP_SWJ_Pins request */
const uint8_t PIN_SwClk_Tck = 0x01;
const uint8_t PIN_SwDio_Tms = 0x02;
const uint8_t PIN_Tdi = 0x04;
const uint8_t PIN_Tdo = 0x08;
const uint8_t PIN_Trst = 0x20;
const uint8_t PIN_Reset = 0x80;

/*
 * Incoming and outgoing byte streams.
 */

static struct queue const cmsis_dap_tx_queue;
static struct queue const cmsis_dap_rx_queue;

static uint8_t rx_buffer[256];
static uint8_t tx_buffer[256];

/*
 * JTAG state
 */
enum jtag_signal_t {
	JTAG_TCLK = 0,
	JTAG_TMS,
	JTAG_TDI,
	JTAG_TDO,
	JTAG_TRSTn,
	JTAG_INVALID
};

static int jtag_pins[JTAG_INVALID] = {
	GPIO_CN7_1, /* TCLK */
	GPIO_CN7_7, /* TMS */
	GPIO_CN7_3, /* TDI */
	GPIO_CN7_5, /* TDO */
	GPIO_CN7_16, /* TRSTn */
};
static int saved_pin_flags[JTAG_INVALID];
static bool jtag_enabled = false;
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
 * How many bytes of rx_buffer are filled with data "peeked" from incoming
 * stream.
 */
static size_t peek_c;

/*
 * Implementation of handler routines for each CMSIS-DAP command.
 */

/* Info command, used to discover which other commands are supported. */
static void dap_info(void)
{
	const char *CMSIS_DAP_VERSION_STR = "2.1.1";
	const uint16_t CAPABILITIES = CAP_Jtag;
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
		tx_buffer[1] = 2;
		memcpy(tx_buffer + 2, &CAPABILITIES, sizeof(CAPABILITIES));
		queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 4);
		break;
	default:
		tx_buffer[1] = 0;
		queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
		break;
	}
}

/* Informational command, to allow debugging device to indicate status. */
static void dap_host_status(void)
{
	if (queue_count(&cmsis_dap_rx_queue) < 3)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 3);
	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/* Establish JTAG connection, take control of JTAG pins. */
static void dap_connect(void)
{
	if (peek_c < 2)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 2);
	if (rx_buffer[1] == 0 || rx_buffer[1] == 2) {
		tx_buffer[1] = 2;
		if (!jtag_enabled) {
			jtag_enabled = true;
			for (size_t i = 0; i < JTAG_INVALID; i++) {
				saved_pin_flags[i] =
					gpio_get_flags(jtag_pins[i]);
			}

			gpio_set_flags(jtag_pins[JTAG_TMS], GPIO_OUT_LOW);
			gpio_set_flags(jtag_pins[JTAG_TDI], GPIO_OUT_LOW);
			gpio_set_flags(jtag_pins[JTAG_TCLK], GPIO_OUT_LOW);
			gpio_set_flags(jtag_pins[JTAG_TRSTn],
				       GPIO_ODR_HIGH | GPIO_PULL_UP);
			gpio_set_flags(jtag_pins[JTAG_TDO],
				       GPIO_INPUT | GPIO_PULL_UP);
		}
	} else {
		tx_buffer[1] = 0;
	}
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/* Restore JTAG pins to previous configuration. */
static void dap_disconnect(void)
{
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 1);

	if (jtag_enabled) {
		jtag_enabled = false;
		for (size_t i = 0; i < JTAG_INVALID; i++) {
			gpio_set_flags(jtag_pins[i], saved_pin_flags[i]);
		}
	}

	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/* TODO: What does this command do? */
static void dap_transfer_configure(void)
{
	if (queue_count(&cmsis_dap_rx_queue) < 6)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 6);
	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/* Reset the GSC (using same pin as if blue button was pressed). */
static void dap_reset_target(void)
{
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 1);

	if (shield_reset_pin != GPIO_COUNT) {
		gpio_set_level(shield_reset_pin, false);
		usleep(100000);
		gpio_set_level(shield_reset_pin, true);
		tx_buffer[2] = 1;
	} else {
		tx_buffer[2] = 0;
	}
	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 3);
}

/* One-time setting of the output level of each JTAG signal. */
static void dap_swj_pins(void)
{
	if (queue_count(&cmsis_dap_rx_queue) < 7)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 7);

	uint32_t wait_us;
	memcpy(&wait_us, rx_buffer + 3, sizeof(wait_us));

	if ((rx_buffer[2] & PIN_SwClk_Tck))
		gpio_set_level(jtag_pins[JTAG_TCLK],
			       !!(rx_buffer[1] & PIN_SwClk_Tck));
	if ((rx_buffer[2] & PIN_SwDio_Tms))
		gpio_set_level(jtag_pins[JTAG_TMS],
			       !!(rx_buffer[1] & PIN_SwDio_Tms));
	if ((rx_buffer[2] & PIN_Tdi))
		gpio_set_level(jtag_pins[JTAG_TDI], !!(rx_buffer[1] & PIN_Tdi));
	if ((rx_buffer[2] & PIN_Trst))
		gpio_set_level(jtag_pins[JTAG_TRSTn],
			       !!(rx_buffer[1] & PIN_Trst));
	if ((rx_buffer[2] & PIN_Reset) && shield_reset_pin != GPIO_COUNT)
		gpio_set_level(shield_reset_pin, !!(rx_buffer[1] & PIN_Reset));

	usleep(wait_us);

	tx_buffer[1] = 0;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/* Set JTAG clock frequency. */
static void dap_swj_clock(void)
{
	uint32_t new_clock_hz, new_half_period_count;

	if (queue_count(&cmsis_dap_rx_queue) < 5)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 7);

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
		 * Empirically, it has been stablished that at least 60 CPU
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

/* Busy-wait half a JTAG clock cycle. */
static inline __attribute__((always_inline)) void half_clock_delay(void)
{
	STM32_TIM_CNT(3) = jtag_half_period_count;
	while (((int16_t)STM32_TIM_CNT(3)) >= 0)
		;
}

/* Clock data out on TMS. */
static void dap_swj_sequence(void)
{
	unsigned c = queue_count(&cmsis_dap_rx_queue);
	if (c < 2)
		return;
	unsigned int bit_count = rx_buffer[1] == 0 ? 256 : rx_buffer[1];
	if (c < 2 + (bit_count + 7) / 8)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, c);
	for (unsigned int i = 0; i < bit_count; i++) {
		gpio_set_level(jtag_pins[JTAG_TMS],
			       !!(rx_buffer[2 + i / 8] & (1 << (i % 8))));
		half_clock_delay();
		gpio_set_level(jtag_pins[JTAG_TCLK], true);
		half_clock_delay();
		gpio_set_level(jtag_pins[JTAG_TCLK], false);
	}
	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2);
}

/*
 * Do a JTAG transaction, consisting of one or more sequences of clocking data
 * on TDI (between 1 and 64 bits), while keeping TMS at a particular level.
 */
static void dap_jtag_sequence(void)
{
	unsigned int tdo_cnt = 0;
	int c = queue_count(&cmsis_dap_rx_queue);
	if (c < 3)
		return;

	/* Check whether a complete request is in queue. */
	queue_peek_units(&cmsis_dap_rx_queue, rx_buffer, 0, c);
	size_t num_sequences = rx_buffer[1];
	size_t offset = 2;
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

	/*
	 * As an optimization, resolve the IO port addresses and masks of
	 * frequently used GPIOs.
	 */
	volatile uint32_t *const jtag_clk_bsrr =
		&STM32_GPIO_BSRR(gpio_list[jtag_pins[JTAG_TCLK]].port);
	const uint32_t jtag_clk_mask_set = gpio_list[jtag_pins[JTAG_TCLK]].mask;
	const uint32_t jtag_clk_mask_clear =
		gpio_list[jtag_pins[JTAG_TCLK]].mask << 16;

	volatile uint32_t *const jtag_tms_bsrr =
		&STM32_GPIO_BSRR(gpio_list[jtag_pins[JTAG_TMS]].port);
	const uint32_t jtag_tms_mask_set = gpio_list[jtag_pins[JTAG_TMS]].mask;
	const uint32_t jtag_tms_mask_clear = gpio_list[jtag_pins[JTAG_TMS]].mask
					     << 16;

	volatile uint32_t *const jtag_tdi_bsrr =
		&STM32_GPIO_BSRR(gpio_list[jtag_pins[JTAG_TDI]].port);
	const uint32_t jtag_tdi_mask_set = gpio_list[jtag_pins[JTAG_TDI]].mask;
	const uint32_t jtag_tdi_mask_clear = gpio_list[jtag_pins[JTAG_TDI]].mask
					     << 16;

	volatile uint16_t *const jtag_tdo_idr =
		&STM32_GPIO_IDR(gpio_list[jtag_pins[JTAG_TDO]].port);
	const uint16_t jtag_tdo_mask = gpio_list[jtag_pins[JTAG_TDO]].mask;

	memset(tx_buffer + 1, 0, sizeof(tx_buffer) - 1);
	const uint8_t *ptr = rx_buffer + 2;
	const uint8_t *const end = rx_buffer + offset;
	while (ptr < end) {
		uint8_t header = *ptr++;
		if (header & 0x40) {
			*jtag_tms_bsrr = jtag_tms_mask_set;
		} else {
			*jtag_tms_bsrr = jtag_tms_mask_clear;
		}
		bool capture_tdo = !!(header & 0x80);
		unsigned int bit_count = header & 0x3F;
		if (bit_count == 0)
			bit_count = 0x40;
		for (unsigned int i = 0; i < bit_count; i++) {
			if (ptr[i / 8] & (1 << (i % 8))) {
				*jtag_tdi_bsrr = jtag_tdi_mask_set;
			} else {
				*jtag_tdi_bsrr = jtag_tdi_mask_clear;
			}
			half_clock_delay();
			*jtag_clk_bsrr = jtag_clk_mask_set;
			uint32_t tdo_val = !!(*jtag_tdo_idr & jtag_tdo_mask);
			if (capture_tdo) {
				tx_buffer[2 + tdo_cnt / 8] |=
					tdo_val << (tdo_cnt % 8);
				tdo_cnt++;
			} else {
				/* No-op to spend time comparable to memory
				 * access above. */
				tx_buffer[2 + tdo_cnt / 8] &=
					~(0xFF << (tdo_cnt % 8));
			}
			half_clock_delay();
			*jtag_clk_bsrr = jtag_clk_mask_clear;
		}
		ptr += (bit_count + 7) / 8;
	}

	tx_buffer[1] = STATUS_Ok;
	queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 2 + (tdo_cnt + 7) / 8);
}

/* Vendor command (HyperDebug): Discover Google-specific capabilities. */
static void dap_goog_info(void)
{
	const uint32_t CAPABILITIES = GOOG_CAP_I2c;

	if (peek_c < 2)
		return;
	queue_remove_units(&cmsis_dap_rx_queue, rx_buffer, 2);
	switch (rx_buffer[1]) {
	case 0:
		queue_remove_unit(&cmsis_dap_rx_queue, rx_buffer);
		tx_buffer[1] = 4;
		memcpy(tx_buffer + 2, &CAPABILITIES, sizeof(CAPABILITIES));
		queue_add_units(&cmsis_dap_tx_queue, tx_buffer, 6);
		break;
	}
}

/* Vendor command (HyperDebug): I2C forwarding. */
static void dap_goog_i2c(void)
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
static void (*dispatch_table[256])(void) = {
	[DAP_Info] = dap_info,
	[DAP_GOOG_Info] = dap_goog_info,
	[DAP_GOOG_I2c] = dap_goog_i2c,
	[DAP_HostStatus] = dap_host_status,
	[DAP_Connect] = dap_connect,
	[DAP_Disconnect] = dap_disconnect,
	[DAP_TransferConfigure] = dap_transfer_configure,
	[DAP_ResetTarget] = dap_reset_target,
	[DAP_SWJ_Pins] = dap_swj_pins,
	[DAP_SWJ_Clock] = dap_swj_clock,
	[DAP_SWJ_Sequence] = dap_swj_sequence,
	[DAP_JTAG_Sequence] = dap_jtag_sequence,
};

/*
 * Main entry point for handling CMSIS-DAP requests received via USB.
 */
static void cmsis_dap_deferred(void)
{
	/* Peek at the incoming data. */
	peek_c = queue_peek_units(&cmsis_dap_rx_queue, rx_buffer, 0, 5);
	if (peek_c < 1) {
		/* Not enough data to start decoding request. */
		return;
	}

	if (dispatch_table[rx_buffer[0]]) {
		/* First byte of response is always same as command byte. */
		tx_buffer[0] = rx_buffer[0];
		/* Invoke handler routine. */
		dispatch_table[rx_buffer[0]]();
	} else {
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
DECLARE_DEFERRED(cmsis_dap_deferred);

static int command_jtag_set_pins(int argc, const char **argv)
{
	int new_pins[JTAG_INVALID];

	if (argc < 7)
		return EC_ERROR_PARAM_COUNT;

	for (int i = 0; i < JTAG_INVALID; i++) {
		new_pins[i] = gpio_find_by_name(argv[2]);
		if (new_pins[i] == GPIO_COUNT)
			return EC_ERROR_PARAM2 + i;
	}

	/* No errors parsing command line, now apply the new settings. */
	for (int i = 0; i < JTAG_INVALID; i++)
		jtag_pins[i] = new_pins[i];

	return EC_SUCCESS;
}

static int command_jtag(int argc, const char **argv)
{
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;
	if (!strcasecmp(argv[1], "set-pins"))
		return command_jtag_set_pins(argc, argv);
	return 0;
}
DECLARE_CONSOLE_COMMAND_FLAGS(jtag, command_jtag, "",
			      "set-pins <TCLK> <TMS> <TDI> <TDO> <TRSTn>",
			      CMD_FLAG_RESTRICTED);

/*
 * Declare USB interface for CMSIS-DAP.
 */
USB_STREAM_CONFIG_FULL(cmsis_dap_usb, USB_IFACE_CMSIS_DAP,
		       USB_CLASS_VENDOR_SPEC, USB_SUBCLASS_CMSIS_DAP,
		       USB_PROTOCOL_CMSIS_DAP, USB_STR_CMSIS_DAP_NAME,
		       USB_EP_CMSIS_DAP, USB_MAX_PACKET_SIZE,
		       USB_MAX_PACKET_SIZE, cmsis_dap_rx_queue,
		       cmsis_dap_tx_queue, 0, 1);

static void cmsis_dap_written(struct consumer const *consumer, size_t count)
{
	hook_call_deferred(&cmsis_dap_deferred_data, 0);
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
