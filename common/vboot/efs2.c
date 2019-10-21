/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Verify and jump to a RW image
 */

#include "charge_state.h"
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

static int send_to_cr50_raw(const uint8_t *data, size_t size)
{
	timestamp_t until;
	uint16_t res = 0;
	int i;

	CPRINTS("Sending packet");
	uart_flush_output();
	uart_clear_input();

	/*
	 * Send packet. No traffic control, assuming Cr50 consumes stream much
	 * faster. TX buffer shouldn't overflow because it's much bigger than
	 * the max packet size.
	 */
	uart_put_raw(data, size);
	uart_flush_output();

	until.val = get_time().val + CR50_COMM_TIMEOUT;

	/* Wait for response from Cr50 */
	for (i = 0; i < sizeof(enum cr50_comm_res); i++) {
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
		res = CR50_COMM_ERROR_TIMEOUT;

	/* Exit packet mode */
	gpio_set_level(GPIO_PACKET_MODE_EN, 0);

	CPRINTS("Received 0x%04x", res);

	return res;
}

static int verify_hash(void)
{
	const uint8_t *hash;
	struct {
		uint8_t preamble[CR50_UART_RX_BUFFER_SIZE];
		uint8_t packet[CR50_COMM_MAX_PACKET_SIZE];
	} __packed s;
	struct cr50_comm_packet *p = (struct cr50_comm_packet *)s.packet;
	int rv;

	/* Compute RW hash */
	rv = vboot_hash_sync(&hash);
	if (rv)
		return rv;

	CPRINTS("Enabling packet mode");
	cflush();

	/* This will wake up (if it's sleeping) and interrupt Cr50. */
	gpio_set_level(GPIO_PACKET_MODE_EN, 1);

	/* compose a frame = preamble + packet */
	memset(s.preamble, CR50_COMM_PREAMBLE, sizeof(s.preamble));
	p->magic = CR50_PACKET_MAGIC;
	p->type = CR50_COMM_CMD_VERIFY_HASH;
	p->size = SHA256_DIGEST_SIZE;
	memcpy(p->data, hash, p->size);
	p->crc = crc8((uint8_t *)&p->type,
		      sizeof(p->type) + sizeof(p->size) + p->size);

	return send_to_cr50_raw((uint8_t *)&s,
				sizeof(s.preamble) + sizeof(*p) + p->size);
}

static int pd_comm_enabled;

static void enable_pd(void)
{
	CPRINTS("Enable PD comm");
	pd_comm_enabled = 1;
}

int vboot_allow_usb_pd(void)
{
	return pd_comm_enabled;
}

static void verify_and_jump(void)
{
	int rv = verify_hash();

	switch (rv) {
	case CR50_COMM_ERROR_HASH_MISMATCH:
		/* Cr50 should have set NO_BOOT. */
		CPRINTS("Hash mismatch");
		enable_pd();
		break;
	case CR50_COMM_SUCCESS:
		rv = system_run_image_copy(SYSTEM_IMAGE_RW);
		CPRINTS("Failed to jump (0x%x)", rv);
		break;
	default:
		CPRINTS("verify_hash failed (0x%x)", rv);
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

static int set_boot_mode(enum boot_mode mode)
{
	struct {
		uint8_t preamble[CR50_UART_RX_BUFFER_SIZE];
		uint8_t packet[CR50_COMM_MAX_PACKET_SIZE];
	} __packed s;
	struct cr50_comm_packet *p = (struct cr50_comm_packet *)s.packet;

	/* compose stream = preamble + packet */
	memset(s.preamble, 0xec, sizeof(s.preamble));
	p->magic = CR50_PACKET_MAGIC;
	p->type = CR50_COMM_CMD_SET_BOOT_MODE;
	p->size = 1;
	memcpy(p->data, &mode, p->size);
	p->crc = crc8((uint8_t *)&p->type,
		      sizeof(p->type) + sizeof(p->size) + p->size);

	return send_to_cr50_raw((uint8_t *)&s,
				sizeof(s.preamble) + sizeof(*p) + p->size);
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

	if (!(flash_get_protect() & EC_FLASH_PROTECT_GPIO_ASSERTED) && 0) {
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
		int soc;

		CPRINTS("Manual recovery");
		if (!IS_ENABLED(CONFIG_BATTERY)
				&& !IS_ENABLED(HAS_TASK_KEYSCAN)) {
			/*
			 * For Chromeboxes, we relax security by allowing PD in
			 * RO. Attackers don't gain meaningful advantage on
			 * built-in-keyboard-less systems.
			 */
			set_boot_mode(BOOT_MODE_RECOVERY);
			enable_pd();
			return;
		}

		/*
		 * If battery is drained or bad, we will boot in NO_RECOVERY to
		 * inform the user of the problem.
		 *
		 * TODO: Defer the judgment after the battery is initialized.
		 */
		if (battery_state_of_charge_abs(&soc) || soc < 20
				|| charge_battery_is_bad()) {
			CPRINTS("Battery not ready");
			set_boot_mode(BOOT_MODE_NO_RECOVERY);
			enable_pd();
			return;
		}
		set_boot_mode(BOOT_MODE_RECOVERY);
		return;
	}

	verify_and_jump();

	/*
	 * Failed to jump.
	 *
	 * If a battery isn't charged, we'll hang out in S5 until it's charged.
	 * If a battery is disconnected, the system would boot on PD power (
	 * because PD is unlocked). Either way, a Chromebook will proceed in RO.
	 * We don't need to indicate failure (by LED). EC Software Sync will fix
	 * the bad RW (using the traditional path).
	 */
	if (!IS_ENABLED(CONFIG_BATTERY))
		/*
		 * If this is a Chromebox, it'll stay in S5, requesting recovery
		 * with a USB-C adapter, or boot to recovery mode with a barrel
		 * jack adapter. We need to indicate it on the LED.
		 */
		led_critical();

	CPRINTS("Exit");
}
