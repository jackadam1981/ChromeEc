/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Verify and jump to a RW image if power supply is not sufficient.
 */

#include "battery.h"
#include "charge_manager.h"
#include "chipset.h"
#include "clock.h"
#include "console.h"
#include "crc8.h"
#include "flash.h"
#include "hooks.h"
#include "host_command.h"
#include "rsa.h"
#include "rwsig.h"
#include "sha256.h"
#include "shared_mem.h"
#include "system.h"
#include "usb_pd.h"
#include "uart.h"
#include "version.h"
#include "vboot.h"
#include "vb21_struct.h"

#define CPRINTS(format, args...) cprints(CC_VBOOT,"VB " format, ## args)
#define CPRINTF(format, args...) cprintf(CC_VBOOT,"VB " format, ## args)

static int send_to_cr50_raw(const uint8_t *data, size_t size)
{
	uint64_t until = get_time().val + CR50_COMM_TIMEOUT;

	uart_flush_output();
	uart_clear_input();
	/* No traffic control, assuming Cr50 consumes stream much faster. */
	uart_put_raw(data, size);

	/* Wait for response from Cr50 */
	while (get_time().val < until) {
		int c = uart_getc();
		if (c != -1)
			return c;
		msleep(10);
	}
	return CR50_COMM_ERROR_TIMEOUT;
}

static int verify_hash(const uint8_t *hash, size_t size)
{
	struct {
		uint8_t preamble[CR50_UART_RX_BUFFER_SIZE];
		uint8_t packet[CR50_COMM_MAX_PACKET_SIZE];
	} __packed s;
	struct cr50_comm_packet *p = (struct cr50_comm_packet *)s.packet;

	/* compose stream = preamble + packet */
	memset(s.preamble, 0xec, sizeof(s.preamble));
	p->magic = CR50_PACKET_MAGIC;
	p->type = CR50_COMM_CMD_VERIFY_HASH;
	p->size = size;
	memcpy(p->data, hash, p->size);
	p->crc = crc8((uint8_t *)&p->type,
		      sizeof(p->type) + sizeof(p->size) + p->size);

	return send_to_cr50_raw((uint8_t *)&s,
				sizeof(s.preamble) + sizeof(*p) + p->size);
}

static int is_padding_valid(const uint8_t *data, uint32_t start, uint32_t end)
{
	const uint32_t *data32 = (const uint32_t *)data;
	int i;

	if (start > end)
		return EC_ERROR_INVAL;

	if (start % 4 || end % 4)
		return EC_ERROR_INVAL;

	for (i = start / 4; i < end / 4; i++) {
		if (data32[i] != 0xffffffff)
			return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

static int verify_rw(void)
{
	const uint8_t *data;
	size_t len;
	struct sha256_ctx ctx;
	uint8_t *hash;

	data = (const uint8_t *)(CONFIG_MAPPED_STORAGE_BASE
			+ CONFIG_EC_WRITABLE_STORAGE_OFF
			+ CONFIG_RW_STORAGE_OFF);
	len = ver_get_image_size(SYSTEM_IMAGE_RW);
	if (len == 0 || len > CONFIG_RW_SIZE) {
		CPRINTS("Invalid image size (%d)", len);
		return EC_ERROR_INVAL;
	}

	if (is_padding_valid(data, len, CONFIG_RW_SIZE)) {
		CPRINTS("Invalid padding");
		return EC_ERROR_INVAL;
	}

	/* Compute hash of the RW firmware */
	SHA256_init(&ctx);
	SHA256_update(&ctx, data, len);
	hash = SHA256_final(&ctx);

	return verify_hash(hash, sizeof(ctx.buf));
}

static int pd_comm_enabled;

static void enable_pd(void)
{
	CPRINTS("Enable PD comm");
	pd_comm_enabled = 1;
}

int vboot_need_pd_comm(void)
{
	return pd_comm_enabled;
}

static void verify_and_jump(void)
{
	int rv = verify_rw();

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
		CPRINTS("verify_rw failed (0x%x)", rv);
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
				|| battery_is_bad()) {
			CPRINTS("Battery not ready");
			set_boot_mode(BOOT_MODE_NO_RECOVERY);
			enable_pd();
			return;
		}
		set_boot_mode(BOOT_MODE_RECOVERY);
		return;
	}

	clock_enable_module(MODULE_FAST_CPU, 1);
	verify_and_jump();
	clock_enable_module(MODULE_FAST_CPU, 0);

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
