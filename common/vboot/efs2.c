/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Early Firmware Selection ver.2.
 *
 * Verify and jump to a RW image. Register boot mode to Cr50.
 */

#include "battery.h"
#include "chipset.h"
#include "clock.h"
#include "console.h"
#include "crc8.h"
#include "flash.h"
#include "sha256.h"
#include "system.h"
#include "usb_pd.h"
#include "uart.h"
#include "vboot.h"
#include "vboot_hash.h"

#define CPRINTS(format, args...) cprints(CC_VBOOT,"VB " format, ## args)
#define CPRINTF(format, args...) cprintf(CC_VBOOT,"VB " format, ## args)

static const char *boot_mode_to_string(uint8_t mode)
{
	static const char *boot_mode_str[] = {
		[BOOT_MODE_NORMAL] = "NORMAL",
		[BOOT_MODE_RECOVERY] = "RECOVERY",
		[BOOT_MODE_NO_BOOT] = "NO_BOOT",
		[BOOT_MODE_NO_BOOT_RECOVERY] = "NO_BOOT_REC",
	};
	if (mode < ARRAY_SIZE(boot_mode_str))
		return boot_mode_str[mode];
	return "UNDEF";
}

/*
 * Check whether a packet or a response was lost.
 */
static bool is_valid_cr50_response(enum cr50_comm_err code)
{
	return code != CR50_COMM_ERR_TIMEOUT
			&& CR50_COMM_SUCCESS <= code
			&& code <= CR50_COMM_ERR_UNDEFINED_CMD;
}

static void enable_packet_mode(bool enable)
{
	/*
	 * This can be done by set_flags(INPUT|PULL_UP). We won't need it until
	 * we find Cr50 need to initiate communication.
	 */
	gpio_set_level(GPIO_PACKET_MODE_EN, enable ? 1 : 0);
}

static enum cr50_comm_err send_to_cr50(const uint8_t *data, size_t size)
{
	timestamp_t until;
	uint16_t res = 0;
	int i;

	uart_flush_output();
	uart_clear_input();

	/* This will wake up (if it's sleeping) and interrupt Cr50. */
	enable_packet_mode(true);

	/*
	 * Send packet. No traffic control, assuming Cr50 consumes stream much
	 * faster. TX buffer shouldn't overflow because it's much bigger than
	 * the max packet size.
	 */
	uart_put_raw(data, size);
	uart_flush_output();

	until.val = get_time().val + CR50_COMM_TIMEOUT;

	/* Wait for response from Cr50 */
	for (i = 0; i < sizeof(enum cr50_comm_err); i++) {
		while (!timestamp_expired(until, NULL)) {
			int c = uart_getc();
			if (c != -1) {
				res = res | c << (i*8);
				break;
			}
			msleep(1);
		}
	}

	if (timestamp_expired(until, NULL))
		res = CR50_COMM_ERR_TIMEOUT;

	/* Exit packet mode */
	enable_packet_mode(false);

	if (res == CR50_COMM_ERR_TIMEOUT)
		CPRINTS("Timeout");
	else
		CPRINTS("Received 0x%04x", res);

	return res;
}

static enum cr50_comm_err cmd_to_cr50(enum cr50_comm_cmd cmd,
				      const uint8_t *data, size_t size)
{
	struct {
		uint8_t preamble[CR50_UART_RX_BUFFER_SIZE];
		uint8_t packet[CR50_COMM_MAX_PACKET_SIZE];
	} __packed s;
	struct cr50_comm_packet *p = (struct cr50_comm_packet *)s.packet;
	int retry = CR50_COMM_MAX_RETRY;
	int rv;

	/* compose a frame = preamble + packet */
	memset(s.preamble, CR50_COMM_PREAMBLE, sizeof(s.preamble));
	p->magic = CR50_PACKET_MAGIC;
	p->type = cmd;
	p->size = size;
	memcpy(p->data, data, size);
	p->crc = crc8((uint8_t *)&p->type,
		      sizeof(p->type) + sizeof(p->size) + size);

	do {
		rv = send_to_cr50((uint8_t *)&s,
				  sizeof(s.preamble) + sizeof(*p) + p->size);
		if (is_valid_cr50_response(rv))
			break;
		sleep(5);
	} while (--retry);

	return rv;
}

static enum cr50_comm_err verify_hash(void)
{
	const uint8_t *hash;
	int rv;

	/* Compute RW hash */
	rv = vboot_hash_sync(&hash);
	if (rv)
		return rv;

	CPRINTS("Verifying hash");
	return cmd_to_cr50(CR50_COMM_CMD_VERIFY_HASH, hash, SHA256_DIGEST_SIZE);
}

static enum cr50_comm_err set_boot_mode(uint8_t mode)
{
	enum cr50_comm_err rv;

	CPRINTS("Setting boot mode to %s(%d)", boot_mode_to_string(mode), mode);
	rv = cmd_to_cr50(CR50_COMM_CMD_SET_BOOT_MODE,
			 &mode, sizeof(enum boot_mode));
	if (rv != CR50_COMM_SUCCESS)
		CPRINTS("Failed to set boot mode");
	return rv;
}

static int pd_comm_enabled;

static void enable_pd(void)
{
	CPRINTS("Enable USB-PD");
	pd_comm_enabled = 1;
}

int vboot_allow_usb_pd(void)
{
	return pd_comm_enabled;
}

static void verify_and_jump(void)
{
	enum cr50_comm_err rv = verify_hash();

	switch (rv) {
	case CR50_COMM_ERR_HASH_MISMATCH:
		/* Cr50 should have set NO_BOOT. */
		CPRINTS("Hash mismatch");
		enable_pd();
		break;
	case CR50_COMM_SUCCESS:
		rv = system_run_image_copy(SYSTEM_IMAGE_RW);
		CPRINTS("Failed to jump (0x%x)", rv);
		break;
	default:
		CPRINTS("Failed to verify RW (0x%x)", rv);
	}
}

/* Request more power: charging battery or more powerful AC adapter */
static void request_power(void)
{
	CPRINTS("%s", __func__);
}

static int is_manual_recovery(void)
{
	return host_is_event_set(EC_HOST_EVENT_KEYBOARD_RECOVERY);
}

static bool is_battery_ready(void)
{
	/* TODO: Add battery check (https://crbug.com/1045216) */
	return true;
}

__overridable void led_critical(void)
{
}

void vboot_main(void)
{
	CPRINTS("Main");

	if (system_is_in_rw()) {
		/*
		 * We come here and immediately return. LED shows power shortage
		 * but it will be immediately corrected if the adapter can
		 * provide enough power.
		 */
		CPRINTS("Already in RW. Wait for power...");
		request_power();
		return;
	}

	if (!(flash_get_protect() & EC_FLASH_PROTECT_GPIO_ASSERTED)) {
		/*
		 * If hardware WP is disabled, PD communication is enabled.
		 * We can return and wait for more power.
		 * Note: If software WP is disabled, we still perform EFS even
		 * though PD communication is enabled.
		 */
		CPRINTS("HW-WP not asserted.");
		request_power();
		return;
	}

	if (is_manual_recovery()) {
		CPRINTS("In recovery mode");
		if (!IS_ENABLED(CONFIG_BATTERY)
				&& !IS_ENABLED(HAS_TASK_KEYSCAN)) {
			/*
			 * For Chromeboxes, we relax security by allowing PD in
			 * RO. Attackers don't gain meaningful advantage on
			 * built-in-keyboard-less systems.
			 *
			 * Alternatively, we can use NO_BOOT_RECOVERY to show a
			 * firmware screen, strictly requiring BJ adapter,
			 * keeping PD disabled. We can simply fall-through.
			 */
			set_boot_mode(BOOT_MODE_RECOVERY);
			enable_pd();
			return;
		}

		/*
		 * If battery is drained or bad, we will boot in NO_RECOVERY to
		 * inform the user of the problem.
		 */
		if (!is_battery_ready()) {
			CPRINTS("Battery not ready or bad");
			if (set_boot_mode(BOOT_MODE_NO_BOOT_RECOVERY) ==
					CR50_COMM_SUCCESS)
				enable_pd();
			return;
		}

		/* We can enter recovery mode. */
		set_boot_mode(BOOT_MODE_RECOVERY);
		return;
	}

	verify_and_jump();

	/* Verification failed. Make LED indicate so. */
	led_critical();

	/*
	 * EFS failed. EC-RO may be able to boot AP if:
	 *
	 *   - Battery is charged or
	 *   - PD may be enabled by NO_BOOT or
	 *   - PD may be enabled by battery removal or
	 *   - BJ adapter is plugged.
	 *
	 * Once AP boots, software sync will fix the mismatch. If that's the
	 * reason of the failure, we won't come back here next time.
	 */
	CPRINTS("Exit");
}
