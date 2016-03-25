/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "hooks.h"
#include "i2c.h"
#include "link_defs.h"
#include "registers.h"
#include "timer.h"
#include "usb_descriptor.h"
#include "usb_power.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)



static int usb_power_init_inas(struct usb_power_config const *config);


static int8_t usb_power_map_error(int error)
{
	switch (error) {
	case EC_SUCCESS:       return USB_POWER_SUCCESS;
	case EC_ERROR_TIMEOUT: return USB_POWER_TIMEOUT;
	case EC_ERROR_BUSY:    return USB_POWER_BUSY;
	default:               return USB_POWER_UNKNOWN_ERROR | (error & 0x7f);
	}
}

static uint16_t usb_power_read_packet(struct usb_power_config const *config)
{
	size_t   i;
	uint16_t bytes = btable_ep[config->endpoint].rx_count & 0x3ff;
	size_t   count = MAX((bytes + 1) / 2, USB_MAX_PACKET_SIZE / 2);

        CPRINTS("usb_power_read_packet");
	/*
	 * The USB peripheral doesn't support DMA access to its packet
	 * RAM so we have to copy messages out into a bounce buffer.
	 */
	for (i = 0; i < count; ++i)
		config->buffer[i] = config->rx_ram[i];

	/*
	 * RX packet consumed, mark the packet as VALID.  The master
	 * could queue up the next command while we process this power
	 * transaction and prepare the response.
	 */
	STM32_TOGGLE_EP(config->endpoint, EP_RX_MASK, EP_RX_VALID, 0);

	return bytes;
}

static void usb_power_write_packet(struct usb_power_config const *config,
				 uint8_t bytes)
{
	size_t  i;
	int count = (bytes + 1) / 2;
	CPRINTS("usb_power_write_packet(bytes=%d)", (int)bytes);


	/*
	 * Copy read bytes and status back out of bounce buffer and
	 * update TX packet state (mark as VALID for master to read).
	 */
	for (i = 0; i < count; ++i)
		config->tx_ram[i] = config->buffer[i];

	btable_ep[config->endpoint].tx_count = bytes;

	STM32_TOGGLE_EP(config->endpoint, EP_TX_MASK, EP_TX_VALID, 0);
}

static int rx_valid(struct usb_power_config const *config)
{
	return (STM32_USB_EP(config->endpoint) & EP_RX_MASK) == EP_RX_VALID;
}


static int tx_valid(struct usb_power_config const *config)
{
        return (STM32_USB_EP(config->endpoint) & EP_TX_MASK) == EP_TX_VALID;
}



#define LOWB(hword) ((hword) & 0xff)
#define HIGHB(hword) ((hword) >> 8)
#define HTOI(low, high) ((low) + (((int)(high) << 16)))


static size_t usb_power_write(struct usb_power_config const *config)
{
	struct usb_power_state *state = config->state;
	void *address = config->buffer;
	struct usb_power_report *r =
		config->state->reports + config->state->reports_tail;
	/* status + size + timestamps + voltage list */
	size_t bytes = 6 + 2*config->state->ina_count;

	CPRINTS("usb_power_write");

	if (sizeof(struct usb_power_report) != 64)
		CPRINTS("OMG %d != 64!", sizeof(struct usb_power_report));

	if (config->state->reports_head != config->state->reports_tail) {
		CPRINTS("usb_power_write idx=%d", config->state->reports_tail);
		memcpy((void*)address, r, bytes);

		state->reports_tail = (state->reports_tail + 1) % USB_POWER_MAX_CACHED;

		usb_power_write_packet(config, bytes);
		return bytes;
	}

	return 0;
}


static int usb_power_read(struct usb_power_config const *config)
{
	/*
	 * If there is a USB packet waiting we process it and generate a
	 * response.
	 */
	uint8_t count		= usb_power_read_packet(config);
	uint16_t command	= config->buffer[0];
	uint8_t result		= USB_POWER_SUCCESS;

	struct usb_power_state *state = config->state;

        CPRINTS("usb_power_read");

	// State machine.
	if (command == USB_POWER_CMD_RESET) {
		state->state = USB_POWER_STATE_OFF;
		//usb_power_reset();
		state->reports_head = 0;
		state->reports_tail = 0;
		CPRINTS("[RESET] STATE -> OFF");
		/* No status return on reset */
		return EC_SUCCESS;
	} else if (command == USB_POWER_CMD_STOP) {
		if (state->state == USB_POWER_STATE_CAPTURING) {
			state->state = USB_POWER_STATE_SETUP;
			state->reports_head = 0;
			state->reports_tail = 0;
			CPRINTS("[STOP] STATE: CAPTURING -> SETUP");
		} else {
			CPRINTS("[STOP] Error not capturing.");
			result = USB_POWER_NOT_CAPTURING;
		}
		/* No status return on stop */
		return EC_SUCCESS;
	} else if (command == USB_POWER_CMD_START) {
		if (state->state == USB_POWER_STATE_SETUP) {
			if (count != 6) {
				CPRINTS("[START] Error count %d is not 6", (int)count);
				result = USB_POWER_READ_SIZE;
			} else {
				int integration_ms = HTOI(config->buffer[1], config->buffer[2]);
				if (integration_ms != 0) {
					state->integration_ms = integration_ms;
					usb_power_init_inas(config);

					state->state = USB_POWER_STATE_CAPTURING;
					CPRINTS("[START] STATE: SETUP -> CAPTURING %dms", integration_ms);
					hook_call_deferred(config->deferred_cap, state->integration_ms * 1000);
				} else {
					CPRINTS("[START] integration_ms cannot be 0");
					result = USB_POWER_UNKNOWN_ERROR;
				}
			}
		} else {
			CPRINTS("[START] Error not setup.");
			result = USB_POWER_NOT_SETUP;
		}
	} else if (command == USB_POWER_CMD_ADDINA) {
		if ((state->state == USB_POWER_STATE_OFF) ||
		    (state->state == USB_POWER_STATE_SETUP)) {
			if (count != 14) {
				CPRINTS("[ADDINA] Error count %d is not 6", (int)count);
				result = USB_POWER_READ_SIZE;
			} else if (state->ina_count >= USB_POWER_MAX_READ_COUNT) {
				CPRINTS("[ADDINA] Error INA list full");
				result = USB_POWER_FULL;
			} else {
				int port = LOWB(config->buffer[1]);
				//int type = HIGHB(config->buffer[1]);
				int addr = LOWB(config->buffer[2]);
				//int extra = HIGHB(config->buffer[2]);
				int mv = HTOI(config->buffer[3], config->buffer[4]);
				int rs = HTOI(config->buffer[5], config->buffer[6]);
				struct usb_power_ina_cfg *ina = state->ina_cfg + state->ina_count;

				if (state->state == USB_POWER_STATE_OFF) {
					state->state = USB_POWER_STATE_SETUP;
					state->ina_count = 0;
					CPRINTS("[ADDINA] STATE: OFF -> SETUP");
				} else {
					CPRINTS("[ADDINA] STATE: SETUP -> SETUP");
				}

				ina->port = port;
				ina->addr = addr << 1; // 7 to 8 bit addr.
				ina->extender_mode = 0;
				ina->extended_addr = 0;
				ina->mv = mv;
				ina->rs = rs;
				state->ina_count += 1;
				CPRINTS("[ADDINA] port:%d addr:0x%02x mv:%d rs:%d count:%d",
				        port, addr, mv, rs, state->ina_count);
			}
		} else {
			CPRINTS("[ADDINA] Error.");
			result = USB_POWER_NOT_SETUP;
		}
	} else {
		CPRINTS("[ERROR] Unknown command 0x%04x", (int)command);
		result = USB_POWER_UNKNOWN_ERROR;
	}

	usb_power_map_error(0);

	config->buffer[0] = result;
	usb_power_write_packet(config, 1);

	return EC_SUCCESS;
}

void usb_power_tx(struct usb_power_config const *config)
{
	STM32_TOGGLE_EP(config->endpoint, EP_TX_MASK, EP_TX_NAK, 0);

	hook_call_deferred(config->deferred, 0);
}

void usb_power_rx(struct usb_power_config const *config)
{
	STM32_TOGGLE_EP(config->endpoint, EP_RX_MASK, EP_RX_NAK, 0);

	hook_call_deferred(config->deferred, 0);
}


void usb_power_deferred(struct usb_power_config const *config)
{
        CPRINTS("usb_power_deferred");
        if (!rx_valid(config) && usb_power_read(config))
                STM32_TOGGLE_EP(config->endpoint, EP_RX_MASK, EP_RX_VALID, 0);

        if (!tx_valid(config) && usb_power_write(config))
                STM32_TOGGLE_EP(config->endpoint, EP_TX_MASK, EP_TX_VALID, 0);
}

void usb_power_reset(struct usb_power_config const *config)
{
	int endpoint = config->endpoint;

	btable_ep[endpoint].tx_addr  = usb_sram_addr(config->tx_ram);
	btable_ep[endpoint].tx_count = 0;

	btable_ep[endpoint].rx_addr  = usb_sram_addr(config->rx_ram);
	btable_ep[endpoint].rx_count =
		0x8000 | ((USB_MAX_PACKET_SIZE / 32 - 1) << 10);

	STM32_USB_EP(endpoint) = ((endpoint <<  0) | /* Endpoint Addr*/
				  (2        <<  4) | /* TX NAK */
				  (0        <<  9) | /* Bulk EP */
				  (3        << 12)); /* RX Valid */
}


#define INA231_REG_CONF 0
#define INA231_REG_RSHV 1
#define INA231_REG_BUSV 2
#define INA231_REG_PWR  3
#define INA231_REG_CURR 4
#define INA231_REG_CAL  5
#define INA231_REG_EN   6


#define INA231_CONF_AVG(val) (((int)(val & 0x7)) << 9)
#define INA231_CONF_BUS_TIME(val) (((int)(val & 0x7)) << 6)
#define INA231_CONF_SHUNT_TIME(val) (((int)(val & 0x7)) << 3)
#define INA231_CONF_MODE(val) (((int)(val & 0x7)) << 0)
#define INA231_MODE_OFF		0x0
#define INA231_MODE_SHUNT	0x5
#define INA231_MODE_BUS		0x6
#define INA231_MODE_BOTH	0x7

/* Lazy log implementation */
static int ilog2(int val) {
	int l = 0;
	val = val >> 1;
	while (val > 0) {
		val = val >> 1;
		l++;
	}
	return l;
}

static int usb_power_init_inas(struct usb_power_config const *config)
{
	struct usb_power_state *state = config->state;
	int i;

	/* Set up i2c mux. */
	i2c_write8(0, 0xe0, 0, 5);

	if (config->state->state != USB_POWER_STATE_SETUP) {
		CPRINTS("[ERROR] usb_power_init_inas while not SETUP");
		return -1;
	}
	for (i = 0; i < state->ina_count; i++) {
		int value;
		int ret;
		int shunt_time = 0x4;
		int avg = 0;
		struct usb_power_ina_cfg *ina = state->ina_cfg + i;

		if (state->integration_ms < 0) {
			shunt_time = 0; // 140uS
			shunt_time = 1; // 204uS
			shunt_time = 2; // 332uS
			shunt_time = 3; // 588uS
			avg = 0;
		} else {
			shunt_time = MIN(7, ilog2(state->integration_ms));
			avg = MIN(7, ilog2(state->integration_ms/8) / 2);
		}
		CPRINTS("[CAP] shunt:%d avg:%d ms:%d", shunt_time, avg, state->integration_ms);

		// INA231
		// Conversion time 1.1ms, shunt only, no average.
		value = INA231_CONF_MODE(INA231_MODE_SHUNT) |
			INA231_CONF_SHUNT_TIME(shunt_time) |
			INA231_CONF_BUS_TIME(shunt_time) |
			INA231_CONF_AVG(avg);
		ret = i2c_write16(ina->port, ina->addr, INA231_REG_CONF, value);
		if (ret != EC_SUCCESS)
			CPRINTS("[CAP] usb_power_init_inas FAIL: %d", ret);
	}
	return 0;
}

static int usb_power_get_samples(struct usb_power_config const *config) {
	uint64_t time = get_time().val;
	struct usb_power_state *state = config->state;
	struct usb_power_report *r = state->reports + state->reports_head;
	int i;
	int overflow = 0;

	// TODO(nsanders): Would we prefer to evict oldest?
	if (((state->reports_head + 1) % USB_POWER_MAX_CACHED) == state->reports_tail) {
		overflow = 1;
		// Goodbye oldest entry.
		state->reports_tail = (state->reports_tail + 1) % USB_POWER_MAX_CACHED;
	}

	r->status = USB_POWER_SUCCESS;
	r->size = state->ina_count;
	r->timestamp = time & 0xffffffff;
	for (i = 0; i < state->ina_count; i++) {
		int value;
		int ret;
		struct usb_power_ina_cfg *ina = state->ina_cfg + i;

		// INA231
		ret = i2c_read16(ina->port, ina->addr, INA231_REG_RSHV, &value);
		r->voltage[i] = value;
		if (ret != EC_SUCCESS)
			CPRINTS("[CAP] usb_power_get_samples FAIL: %d", ret);

		{
		int uV = ((int)(r->voltage[i]) * 25) / 10;
		int mA = (uV / ina->rs);
		CPRINTS("[CAP] %d (%d,0x%02x): %dmV / %dmO = %dmA",
                        i, ina->port, ina->addr, uV/1000, ina->rs, mA);
		}
	}

	// Mark this slot as used.
	state->reports_head = (state->reports_head + 1) % USB_POWER_MAX_CACHED;

	if (overflow)
		return USB_POWER_OVERFLOW;
	else
		return 0;
}


void usb_power_deferred_cap(struct usb_power_config const *config)
{
	// Exit if we have stopped capturing in the meantime.
	if (config->state->state != USB_POWER_STATE_CAPTURING)
		return;

	CPRINTS("[CAP] usb_power_deferred_cap");
	usb_power_get_samples(config);
	hook_call_deferred(config->deferred, 0);

	// Double check that we have stopped capturing.
	if (config->state->state == USB_POWER_STATE_CAPTURING)
		hook_call_deferred(config->deferred_cap, config->state->integration_ms * 1000);
}
