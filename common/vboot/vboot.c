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

static int verify_slot(enum system_image_copy_t slot)
{
	const struct vb21_packed_key *vb21_key;
	const struct vb21_signature *vb21_sig;
	const struct rsa_public_key *key;
	const uint8_t *sig;
	const uint8_t *data;
	int len;
	int rv;

	CPRINTS("Verifying %s", system_image_copy_t_to_string(slot));

	vb21_key = (const struct vb21_packed_key *)(
			CONFIG_MAPPED_STORAGE_BASE +
			CONFIG_EC_PROTECTED_STORAGE_OFF +
			CONFIG_RO_PUBKEY_STORAGE_OFF);
	rv = vb21_is_packed_key_valid(vb21_key);
	if (rv) {
		CPRINTS("Invalid key (%d)", rv);
		return EC_ERROR_VBOOT_KEY;
	}
	key = (const struct rsa_public_key *)
		((const uint8_t *)vb21_key + vb21_key->key_offset);

	if (slot == SYSTEM_IMAGE_RW_A) {
		data = (const uint8_t *)(CONFIG_MAPPED_STORAGE_BASE +
				CONFIG_EC_WRITABLE_STORAGE_OFF +
				CONFIG_RW_A_STORAGE_OFF);
		vb21_sig = (const struct vb21_signature *)(
				CONFIG_MAPPED_STORAGE_BASE +
				CONFIG_EC_WRITABLE_STORAGE_OFF +
				CONFIG_RW_A_SIGN_STORAGE_OFF);
	} else {
		data = (const uint8_t *)(CONFIG_MAPPED_STORAGE_BASE +
				CONFIG_EC_WRITABLE_STORAGE_OFF +
				CONFIG_RW_B_STORAGE_OFF);
		vb21_sig = (const struct vb21_signature *)(
				CONFIG_MAPPED_STORAGE_BASE +
				CONFIG_EC_WRITABLE_STORAGE_OFF +
				CONFIG_RW_B_SIGN_STORAGE_OFF);
	}

	rv = vb21_is_signature_valid(vb21_sig, vb21_key);
	if (rv) {
		CPRINTS("Invalid signature (%d)", rv);
		return EC_ERROR_INVAL;
	}
	sig = (const uint8_t *)vb21_sig + vb21_sig->sig_offset;
	len = vb21_sig->data_size;

	if (vboot_is_padding_valid(data, len,
				   CONFIG_RW_SIZE - CONFIG_RW_SIG_SIZE)) {
		CPRINTS("Invalid padding");
		return EC_ERROR_INVAL;
	}

	rv = vboot_verify(data, len, key, sig);
	if (rv) {
		CPRINTS("Invalid data (%d)", rv);
		return EC_ERROR_INVAL;
	}

	CPRINTS("Verified %s", system_image_copy_t_to_string(slot));

	return EC_SUCCESS;
}

static int hc_verify_slot(struct host_cmd_handler_args *args)
{
	const struct ec_params_efs_verify *p = args->params;
	enum system_image_copy_t slot;

	switch (p->region) {
	case EC_FLASH_REGION_ACTIVE:
		slot = system_get_active_copy();
		break;
	case EC_FLASH_REGION_UPDATE:
		slot = system_get_update_copy();
		break;
	default:
		return EC_RES_INVALID_PARAM;
	}
	return verify_slot(slot) ? EC_RES_ERROR : EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_EFS_VERIFY, hc_verify_slot, EC_VER_MASK(0));

/**
 * Send raw byte stream to Cr50
 *
 * @param data
 * @param timeout
 * @return
 */
static int send_to_cr50_raw(const uint8_t *data, size_t size)
{
	uint64_t until = get_time().val + CR50_COMM_TIMEOUT;

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

static int send_to_cr50(const uint8_t *data, uint8_t size)
{
	struct {
		uint8_t preamble[CR50_UART_RX_BUFFER_SIZE];
		uint8_t packet[CR50_COMM_MAX_PACKET_SIZE];
	} __packed s;
	struct cr50_comm_packet *p = (struct cr50_comm_packet *)s.packet;

	/* compose stream = preamble + packet */
	memset(s.preamble, 0xec, sizeof(s.preamble));
	p->magic = CR50_PACKET_MAGIC;
	p->type = CR50_CMD_FW_VERSION;
	p->size = size;
	memcpy(p->data, data, p->size);
	p->crc = crc8((uint8_t *)&p->type,
		      sizeof(p->type) + sizeof(p->size) + p->size);

	return send_to_cr50_raw((uint8_t *)&s,
				sizeof(s.preamble) + sizeof(*p) + p->size);
}

static int check_rollback(enum system_image_copy_t slot)
{
	int32_t version = ver_get_num_commits(slot);
	int rv;

	if (version < 0)
		return EC_ERROR_UNKNOWN;
	/* TODO: uncomment it */
	// CPRINTS("RW-%d version %d", slot, version);

	/* Clear Tx buffer. Console task hasn't started yet. */
	uart_flush_output();

	/* send version */
	rv = send_to_cr50((uint8_t *)&version, sizeof(version));
	if (rv != CR50_COMM_SUCCESS) {
		CPRINTS("Rollback check failed (0x%x)", rv);
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

static int verify_and_jump(void)
{
	enum system_image_copy_t slot;
	int rv;

	/* 1. Decide which slot to try */
	slot = system_get_active_copy();

	/* 2. Verify the slot */
	rv = verify_slot(slot);
	if (rv) {
		enum system_image_copy_t fallback;
		if (rv == EC_ERROR_VBOOT_KEY)
			/* Key error. The other slot isn't worth trying. */
			return rv;

		fallback = system_get_update_copy();

		/* If there is no fallback slot, we're done. */
		if (fallback == slot)
			return rv;

		slot = fallback;
		/* TODO(chromium:767050): Skip reading key again. */
		rv = verify_slot(slot);
		if (rv)
			/* Both slots failed */
			return rv;

		/* Proceed with the other slot. If this slot isn't expected, AP
		 * will catch it and request recovery after a few attempts. */
		if (system_set_active_copy(slot))
			CPRINTS("Failed to activate %s",
				system_image_copy_t_to_string(slot));
	}

	/* 3. Send version to cr50 for rollback protection */
	rv = check_rollback(slot);
	if (rv)
		return rv;

	/* 4. Jump (and reboot) */
	rv = system_run_image_copy(slot);
	CPRINTS("Failed to jump (0x%x)", rv);

	return rv;
}

__overridable void led_critical(void)
{
	/* no-op */
}

/* Request more power: charging battery or more powerful AC adapter */
static void request_power(void)
{
	CPRINTS("%s", __func__);
}

static void request_recovery(void)
{
	CPRINTS("%s", __func__);
	led_critical();
}

static int is_manual_recovery(void)
{
	return host_is_event_set(EC_HOST_EVENT_KEYBOARD_RECOVERY);
}

static int pd_comm_enabled;

int vboot_need_pd_comm(void)
{
	return pd_comm_enabled;
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
		CPRINTS("Manual recovery");
		if (battery_is_present()
				|| IS_ENABLED(CONFIG_HAS_MATRIX_KEYBOARD)) {
			request_power();
			return;
		}
		/* We don't request_power because we don't want to assume all
		 * devices support a non type-c charger. We open up a security
		 * hole by allowing EC-RO to do PD negotiation but attackers
		 * don't gain meaningful advantage on devices without a matrix
		 * keyboard */
		CPRINTS("Enable PD comm");
		pd_comm_enabled = 1;
		return;
	}

	clock_enable_module(MODULE_FAST_CPU, 1);
	/* If successful, this won't return. */
	verify_and_jump();
	clock_enable_module(MODULE_FAST_CPU, 0);

	/*
	 * Failed to jump.
	 *
	 * We proceed in RO to boot AP (using a traditional path). AP will run
	 * softsync and fix the invalid or corrupted RW.
	 *
	 * If this is a Chromebox, it'll stay in S5 requesting recovery with a
	 * USB-C adapter or boot to recovery mode with a barrel jack adapter.
	 */
	if (!battery_is_present())
		request_recovery();
}

DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, vboot_main, HOOK_PRIO_LAST);
