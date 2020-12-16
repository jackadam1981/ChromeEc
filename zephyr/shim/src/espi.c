/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <atomic.h>
#include <device.h>
#include <drivers/espi.h>
#include <logging/log.h>
#include <kernel.h>
#include <stdint.h>
#include <zephyr.h>

#include "chipset.h"
#include "common.h"
#include "espi.h"
#include "hooks.h"
#include "lpc.h"
#include "port80.h"
#include "zephyr_espi_shim.h"

LOG_MODULE_REGISTER(espi_shim, CONFIG_ESPI_LOG_LEVEL);

static struct host_packet lpc_packet;
static struct host_cmd_handler_args host_cmd_args;
static uint8_t host_cmd_flags; /* Flags from host command */
static uint8_t params_copy[EC_LPC_HOST_PACKET_SIZE] __aligned(4);
static int init_done;
static struct ec_lpc_host_args *lpc_host_args;

/*
 * A mapping of platform/ec signals to Zephyr virtual wires.
 *
 * This should be a macro which takes a parameter M, and does a
 * functional application of M to 2-tuples of (platform/ec signal,
 * zephyr vwire).
 */
#define VW_SIGNAL_TRANSLATION_LIST(M)                                 \
	M(VW_SLP_S3_L, ESPI_VWIRE_SIGNAL_SLP_S3)                      \
	M(VW_SLP_S4_L, ESPI_VWIRE_SIGNAL_SLP_S4)                      \
	M(VW_SLP_S5_L, ESPI_VWIRE_SIGNAL_SLP_S5)                      \
	M(VW_SUS_STAT_L, ESPI_VWIRE_SIGNAL_SUS_STAT)                  \
	M(VW_PLTRST_L, ESPI_VWIRE_SIGNAL_PLTRST)                      \
	M(VW_OOB_RST_WARN, ESPI_VWIRE_SIGNAL_OOB_RST_WARN)            \
	M(VW_OOB_RST_ACK, ESPI_VWIRE_SIGNAL_OOB_RST_ACK)              \
	M(VW_WAKE_L, ESPI_VWIRE_SIGNAL_WAKE)                          \
	M(VW_PME_L, ESPI_VWIRE_SIGNAL_PME)                            \
	M(VW_ERROR_FATAL, ESPI_VWIRE_SIGNAL_ERR_FATAL)                \
	M(VW_ERROR_NON_FATAL, ESPI_VWIRE_SIGNAL_ERR_NON_FATAL)        \
	M(VW_SLAVE_BTLD_STATUS_DONE, ESPI_VWIRE_SIGNAL_SLV_BOOT_DONE) \
	M(VW_SCI_L, ESPI_VWIRE_SIGNAL_SCI)                            \
	M(VW_SMI_L, ESPI_VWIRE_SIGNAL_SMI)                            \
	M(VW_HOST_RST_ACK, ESPI_VWIRE_SIGNAL_HOST_RST_ACK)            \
	M(VW_HOST_RST_WARN, ESPI_VWIRE_SIGNAL_HOST_RST_WARN)          \
	M(VW_SUS_ACK, ESPI_VWIRE_SIGNAL_SUS_ACK)                      \
	M(VW_SUS_WARN_L, ESPI_VWIRE_SIGNAL_SUS_WARN)                  \
	M(VW_SUS_PWRDN_ACK_L, ESPI_VWIRE_SIGNAL_SUS_PWRDN_ACK)        \
	M(VW_SLP_A_L, ESPI_VWIRE_SIGNAL_SLP_A)                        \
	M(VW_SLP_LAN, ESPI_VWIRE_SIGNAL_SLP_LAN)                      \
	M(VW_SLP_WLAN, ESPI_VWIRE_SIGNAL_SLP_WLAN)

/*
 * These two macros are intended to be used as as the M parameter to
 * the list above, generating case statements returning the
 * translation for the first parameter to the second, and the second
 * to the first, respectively.
 */
#define CASE_CROS_TO_ZEPHYR(A, B) \
	case A:                   \
		return B;
#define CASE_ZEPHYR_TO_CROS(A, B) CASE_CROS_TO_ZEPHYR(B, A)

/* Translate a platform/ec signal to a Zephyr signal */
static enum espi_vwire_signal signal_to_zephyr_vwire(enum espi_vw_signal signal)
{
	switch (signal) {
		VW_SIGNAL_TRANSLATION_LIST(CASE_CROS_TO_ZEPHYR);
	default:
		LOG_ERR("Invalid virtual wire signal (%d)", signal);
		return -1;
	}
}

/* Translate a Zephyr vwire to a platform/ec signal */
static enum espi_vw_signal zephyr_vwire_to_signal(enum espi_vwire_signal vwire)
{
	switch (vwire) {
		VW_SIGNAL_TRANSLATION_LIST(CASE_ZEPHYR_TO_CROS);
	default:
		LOG_ERR("Invalid zephyr vwire (%d)", vwire);
		return -1;
	}
}

/*
 * Bit field for each signal which can have an interrupt enabled.
 * Note the interrupt is always enabled, it just depends whether we
 * route it to the power_signal_interrupt handler or not.
 */
static atomic_t signal_interrupt_enabled;

/* To be used with VW_SIGNAL_TRASLATION_LIST */
#define CASE_CROS_TO_BIT(A, _) CASE_CROS_TO_ZEPHYR(A, BIT(A - VW_SIGNAL_START))

/* Convert from an EC signal to the corresponding interrupt enabled bit. */
static uint32_t signal_to_interrupt_bit(enum espi_vw_signal signal)
{
	switch (signal) {
		VW_SIGNAL_TRANSLATION_LIST(CASE_CROS_TO_BIT);
	default:
		return 0;
	}
}

/* Callback for vwire received */
static void espi_vwire_handler(const struct device *dev,
			       struct espi_callback *cb,
			       struct espi_event event)
{
	int ec_signal = zephyr_vwire_to_signal(event.evt_details);

	if (IS_ENABLED(CONFIG_PLATFORM_EC_POWERseQ) &&
	    (signal_interrupt_enabled & signal_to_interrupt_bit(ec_signal))) {
		power_signal_interrupt(ec_signal);
	}
}

static void handle_host_write(uint32_t data);

static void espi_peripheral_handler(const struct device *dev,
				    struct espi_callback *cb,
				    struct espi_event event)
{
	uint16_t event_type = event.evt_details;

	if (IS_ENABLED(CONFIG_PLATFORM_EC_PORT80) &&
	    event_type == ESPI_PERIPHERAL_DEBUG_PORT80) {
		port_80_write(event.evt_data);
	}

	if (IS_ENABLED(CONFIG_PLATFORM_EC_HOSTCMD) &&
	    event_type == ESPI_PERIPHERAL_EC_HOST_CMD) {
		handle_host_write(event.evt_data);
	}
}

#define ESPI_DEV DT_LABEL(DT_NODELABEL(espi0))
static const struct device *espi_dev;

int zephyr_shim_setup_espi(void)
{
	static struct {
		struct espi_callback cb;
		espi_callback_handler_t handler;
		enum espi_bus_event event_type;
	} callbacks[] = {
		{
			.handler = espi_vwire_handler,
			.event_type = ESPI_BUS_EVENT_VWIRE_RECEIVED,
		},
		{
			.handler = espi_peripheral_handler,
			.event_type = ESPI_BUS_PERIPHERAL_NOTIFICATION,
		},
	};

	struct espi_cfg cfg = {
		.io_caps = ESPI_IO_MODE_SINGLE_LINE,
		.channel_caps = ESPI_CHANNEL_VWIRE | ESPI_CHANNEL_PERIPHERAL |
				ESPI_CHANNEL_OOB,
		.max_freq = 20,
	};

	espi_dev = device_get_binding(ESPI_DEV);
	if (!espi_dev) {
		LOG_ERR("Failed to find device %s", ESPI_DEV);
		return -1;
	}

	/* Configure eSPI */
	if (espi_config(espi_dev, &cfg)) {
		LOG_ERR("Failed to configure eSPI device");
		return -1;
	}

	/* Setup callbacks */
	for (size_t i = 0; i < ARRAY_SIZE(callbacks); i++) {
		espi_init_callback(&callbacks[i].cb, callbacks[i].handler,
				   callbacks[i].event_type);
		espi_add_callback(espi_dev, &callbacks[i].cb);
	}

	return 0;
}

int espi_vw_set_wire(enum espi_vw_signal signal, uint8_t level)
{
	return espi_send_vwire(espi_dev, signal_to_zephyr_vwire(signal), level);
}

int espi_vw_get_wire(enum espi_vw_signal signal)
{
	uint8_t level;

	if (espi_receive_vwire(espi_dev, signal_to_zephyr_vwire(signal),
			       &level) < 0) {
		LOG_ERR("Encountered error receiving virtual wire signal");
		return 0;
	}

	return level;
}

int espi_vw_enable_wire_int(enum espi_vw_signal signal)
{
	atomic_or(&signal_interrupt_enabled, signal_to_interrupt_bit(signal));
	return 0;
}

int espi_vw_disable_wire_int(enum espi_vw_signal signal)
{
	atomic_and(&signal_interrupt_enabled, ~signal_to_interrupt_bit(signal));
	return 0;
}

uint8_t *lpc_get_memmap_range(void)
{
	uint32_t lpc_memmap = NULL;

	if (espi_read_lpc_request(espi_dev, EACPI_GET_SHARED_MEMORY,
				  &lpc_memmap) != 0) {
		LOG_ERR("Get lpc_memmap failed!\n");
	}

	return (uint8_t *)lpc_memmap;
}

/**
 * Update the level-sensitive wake signal to the AP.
 *
 * @param wake_events	Currently asserted wake events
 */
static void lpc_update_wake(host_event_t wake_events)
{
	/*
	 * Mask off power button event, since the AP gets that through a
	 * separate dedicated GPIO.
	 */
	wake_events &= ~EC_HOST_EVENT_MASK(EC_HOST_EVENT_POWER_BUTTON);

	/* Signal is asserted low when wake events is non-zero */
	gpio_set_level(NAMED_GPIO(ec_pch_wake_odl), !wake_events);
}

static void lpc_generate_smi(void)
{
	host_event_t smi;

	espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SMI, 1);
	udelay(65);
	espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SMI, 0);
	udelay(65);
	espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SMI, 1);

	smi = lpc_get_host_events_by_type(LPC_HOST_EVENT_SMI);
	if (smi)
		LOG_INF("smi 0x%016llx", smi);
}

static void lpc_generate_sci(void)
{
	host_event_t sci;

	espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SCI, 1);
	udelay(65);
	espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SCI, 0);
	udelay(65);
	espi_send_vwire(espi_dev, ESPI_VWIRE_SIGNAL_SCI, 1);

	sci = lpc_get_host_events_by_type(LPC_HOST_EVENT_SCI);
	if (sci)
		LOG_INF("sci 0x%016llx", sci);
}

void lpc_update_host_event_status(void)
{
	uint32_t enable;
	uint32_t status;
	int need_sci = 0;
	int need_smi = 0;

	if (!init_done)
		return;

	/* Disable PMC1 interrupt while updating status register */
	enable = 0;
	espi_write_lpc_request(espi_dev, ECUSTOM_HOST_SUBS_INTERRUPT_EN,
			       &enable);

	espi_read_lpc_request(espi_dev, EACPI_READ_STS, &status);
	if (lpc_get_host_events_by_type(LPC_HOST_EVENT_SMI)) {
		/* Only generate SMI for first event */
		if (!(status & EC_LPC_STATUS_SMI_PENDING))
			need_smi = 1;

		status |= EC_LPC_STATUS_SMI_PENDING;
		espi_write_lpc_request(espi_dev, EACPI_WRITE_STS, &status);
	} else {
		status &= ~EC_LPC_STATUS_SMI_PENDING;
		espi_write_lpc_request(espi_dev, EACPI_WRITE_STS, &status);
	}

	espi_read_lpc_request(espi_dev, EACPI_READ_STS, &status);
	if (lpc_get_host_events_by_type(LPC_HOST_EVENT_SCI)) {
		/* Generate SCI for every event */
		need_sci = 1;

		status |= EC_LPC_STATUS_SCI_PENDING;
		espi_write_lpc_request(espi_dev, EACPI_WRITE_STS, &status);
	} else {
		status &= ~EC_LPC_STATUS_SCI_PENDING;
		espi_write_lpc_request(espi_dev, EACPI_WRITE_STS, &status);
	}

	*(host_event_t *)host_get_memmap(EC_MEMMAP_HOST_EVENTS) =
		lpc_get_host_events();

	enable = 1;
	espi_write_lpc_request(espi_dev, ECUSTOM_HOST_SUBS_INTERRUPT_EN,
			       &enable);

	/* Process the wake events. */
	lpc_update_wake(lpc_get_host_events_by_type(LPC_HOST_EVENT_WAKE));

	/* Send pulse on SMI signal if needed */
	if (need_smi)
		lpc_generate_smi();

	/* ACPI 5.0-12.6.1: Generate SCI for SCI_EVT=1. */
	if (need_sci)
		lpc_generate_sci();
}

static void host_command_init(void)
{
	uint32_t shm_mem_host_cmd;

	espi_read_lpc_request(espi_dev, ECUSTOM_HOST_CMD_GET_PARAM_MEMORY,
			      &shm_mem_host_cmd);
	lpc_host_args = (struct ec_lpc_host_args *)shm_mem_host_cmd;

	/* We support LPC args and version 3 protocol */
	*(lpc_get_memmap_range() + EC_MEMMAP_HOST_CMD_FLAGS) =
		EC_HOST_CMD_FLAG_LPC_ARGS_SUPPORTED |
		EC_HOST_CMD_FLAG_VERSION_3;

	/* Sufficiently initialized */
	init_done = 1;

	lpc_update_host_event_status();
}

DECLARE_HOOK(HOOK_INIT, host_command_init, HOOK_PRIO_INIT_LPC);

static void lpc_send_response(struct host_cmd_handler_args *args)
{
	uint8_t *out;
	uint32_t data;
	int size = args->response_size;
	int csum;
	int i;

	/* Ignore in-progress on LPC since interface is synchronous anyway */
	if (args->result == EC_RES_IN_PROGRESS)
		return;

	/* Handle negative size */
	if (size < 0) {
		args->result = EC_RES_INVALID_RESPONSE;
		size = 0;
	}

	/* New-style response */
	lpc_host_args->flags = (host_cmd_flags & ~EC_HOST_ARGS_FLAG_FROM_HOST) |
			       EC_HOST_ARGS_FLAG_TO_HOST;

	lpc_host_args->data_size = size;

	csum = args->command + lpc_host_args->flags +
	       lpc_host_args->command_version + lpc_host_args->data_size;

	for (i = 0, out = (uint8_t *)args->response; i < size; i++, out++)
		csum += *out;

	lpc_host_args->checksum = (uint8_t)csum;

	/* Fail if response doesn't fit in the param buffer */
	if (size > EC_PROTO2_MAX_PARAM_SIZE)
		args->result = EC_RES_INVALID_RESPONSE;

	/* Write result to the data byte.  This sets the TOH status bit. */
	data = args->result;
	espi_write_lpc_request(espi_dev, ECUSTOM_HOST_CMD_SEND_RESULT, &data);
}

static void lpc_send_response_packet(struct host_packet *pkt)
{
	uint32_t data;
	/* Ignore in-progress on LPC since interface is synchronous anyway */
	if (pkt->driver_result == EC_RES_IN_PROGRESS)
		return;

	/* Write result to the data byte.  This sets the TOH status bit. */
	data = pkt->driver_result;
	espi_write_lpc_request(espi_dev, ECUSTOM_HOST_CMD_SEND_RESULT, &data);
}

static void handle_host_write(uint32_t data)
{
	uint32_t shm_mem_host_cmd;
	/*
	 * Read the command byte.  This clears the FRMH bit in
	 * the status byte.
	 */
	host_cmd_args.command = data & 0xff;

	host_cmd_args.result = EC_RES_SUCCESS;
	host_cmd_args.send_response = lpc_send_response;
	host_cmd_flags = lpc_host_args->flags;

	/* See if we have an old or new style command */
	if (host_cmd_args.command == EC_COMMAND_PROTOCOL_3) {
		espi_read_lpc_request(espi_dev,
				      ECUSTOM_HOST_CMD_GET_PARAM_MEMORY,
				      &shm_mem_host_cmd);

		lpc_packet.send_response = lpc_send_response_packet;

		lpc_packet.request = (const void *)shm_mem_host_cmd;
		lpc_packet.request_temp = params_copy;
		lpc_packet.request_max = sizeof(params_copy);
		/* Don't know the request size so pass in the entire buffer */
		lpc_packet.request_size = EC_LPC_HOST_PACKET_SIZE;

		lpc_packet.response = (void *)shm_mem_host_cmd;
		lpc_packet.response_max = EC_LPC_HOST_PACKET_SIZE;
		lpc_packet.response_size = 0;

		lpc_packet.driver_result = EC_RES_SUCCESS;

		host_packet_receive(&lpc_packet);
		return;

	} else {
		/* Old style command, now unsupported */
		host_cmd_args.result = EC_RES_INVALID_COMMAND;
	}

	/* Hand off to host command handler */
	host_command_received(&host_cmd_args);
}
