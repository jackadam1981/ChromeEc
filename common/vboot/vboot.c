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
#include "console.h"
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

#define CR50_UART	CONFIG_CONSOLE_UART
#define CR50_UART_IRQ	NPCX_IRQ_UART

//static struct sha256_ctx __ctx;
static uint8_t *__hash;

static int has_matrix_keyboard(void)
{
	return 0;
}

static int is_efs_supported(void)
{
#ifdef CONFIG_VBOOT_EFS
	return 1;
#else
	return 0;
#endif
}

static int is_low_power_ap_boot_supported(void)
{
	return 0;
}

static int verify_slot(enum system_image_copy_t slot)
{
#if 0
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

	__hash = vboot_get_hash(data, len, &__ctx);
	rv = vboot_verify(__hash, key, sig);
	if (rv) {
		CPRINTS("Invalid data (%d)", rv);
		return EC_ERROR_INVAL;
	}
#endif

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

static int send_to_cr50(uint8_t *data, uint64_t timeout)
{
	uint64_t until = get_time().val + timeout;
	uart_clear_input();
	uart_puts(data);
	while (get_time().val < until) {
		int c = uart_getc();
		if (c != -1)
			return c == 0xce ? EC_SUCCESS : EC_ERROR_UNKNOWN;
		msleep(10);
	}
	return EC_ERROR_TIMEOUT;
}

static int handshake_cr50_open(void)
{
	/* EC sends 63 '0xec' */
	uint8_t handshaker[32];
	memset(handshaker, 0xec, sizeof(handshaker));
	handshaker[sizeof(handshaker) - 1] = 0;
	/* CPRINTS("Handshake opening"); */
	/* We'll get reset by cr50 if this is an illegal timing */
	return send_to_cr50(handshaker, 200 * MSEC);
}

static int handshake_cr50_close(void)
{
	uint8_t handshaker[32];
	memset(handshaker, 0xec, sizeof(handshaker));
	handshaker[sizeof(handshaker) - 1] = 0;
	/* CPRINTS("Handshake closing"); */
	return send_to_cr50(handshaker, 200 * MSEC);
}

static void toggle_ccd_mode(void)
{
	int level = gpio_get_level(GPIO_CCD_MODE_ODL);
	gpio_set_level(GPIO_CCD_MODE_ODL, 1);
	gpio_set_level(GPIO_CCD_MODE_ODL, 0);
	msleep(10);
	gpio_set_level(GPIO_CCD_MODE_ODL, 1);
	msleep(10);
	gpio_set_level(GPIO_CCD_MODE_ODL, level);
}

static void send_identity_to_cr50(enum system_image_copy_t slot)
{
	/* +1 for a nul character. This fits version string too. */
	uint8_t buf[SHA256_DIGEST_SIZE+1];
	const struct image_data *image_data;
	//int32_t version = ver_get_num_commits(slot);

	/* Clear Tx buffer. Console task hasn't started yet. */
	uart_flush_output();

	/* Wake up cr50. cr50 has to clear its uart, too. */
	toggle_ccd_mode();

	if (handshake_cr50_open()) {
		CPRINTS("Failed to open handshake with cr50");
		return;
	}

	/* send version */
	image_data = system_get_image_data(slot);
	if (!image_data)
		goto exit;

	memset(buf, 0, sizeof(buf));
	memcpy(buf, image_data->version, sizeof(image_data->version));
	if (send_to_cr50(buf, 200 * MSEC))
		goto exit;

	/* send hash */
	memset(buf, 0, sizeof(buf));
	memcpy(buf, __hash, sizeof(__hash));
	if (send_to_cr50(buf, 200 * MSEC))
		goto exit;

exit:
	if (handshake_cr50_close())
		CPRINTS("Failed to close handshake with cr50");
}


int command_cr50(int argc, char *argv[])
{
	//toggle_ccd_mode();
	send_identity_to_cr50(SYSTEM_IMAGE_RW_A);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(cr50, command_cr50, NULL, "Toggle CCD_MODE_L");

static int verify_and_jump(void)
{
	enum system_image_copy_t slot;
	int rv;

	/* 1. Decide which slot to try */
	slot = system_get_active_copy();

	/* 2. Verify the slot */
	rv = verify_slot(slot);
	if (rv) {
		if (rv == EC_ERROR_VBOOT_KEY)
			/* Key error. The other slot isn't worth trying. */
			return rv;
		slot = system_get_update_copy();
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

	/* 3. Send identity to cr50 */
	send_identity_to_cr50(slot);

	/* 4. Jump (and reboot) */
	rv = system_run_image_copy(slot);
	CPRINTS("Failed to jump (%d)", rv);

	return rv;
}

/* Request more power: charging battery or more powerful AC adapter */
static void request_power(void)
{
	CPRINTS("%s", __func__);
}

static void request_recovery(void)
{
	CPRINTS("%s", __func__);
	//led_critical();
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
		if (battery_is_present() || has_matrix_keyboard()) {
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

	if (!is_efs_supported()) {
		if (is_low_power_ap_boot_supported())
			/* If a device supports this feature, AP's boot power
			 * threshold should be set low. That will let EC-RO
			 * boot AP and softsync take care of RW verification. */
			return;
		request_power();
		return;
	}

	/* If successful, this won't return. */
	verify_and_jump();

	/* Failed to jump. Need recovery. */
	request_recovery();
}
