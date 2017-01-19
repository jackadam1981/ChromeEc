/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helpers to emulate a U2F HID dongle over the TPM transport */

#include "console.h"
#include "dcrypto.h"
#include "extension.h"
#include "nvmem_vars.h"
#include "rbox.h"
#include "registers.h"
#include "signed_header.h"
#include "system.h"
#include "tpm_vendor_cmds.h"
#include "u2f_impl.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_EXTENSION, format, ## args)

/* Maximum U2F message payload */
#define U2F_MAX_BCNT (57 + 17 * 59)

/* ---- physical presence (using the laptop power button) ---- */

static timestamp_t last_press;

/* how long do we keep the last button press as valid presence */
#define PRESENCE_TIMEOUT (10 * SECOND)

void power_button_record(void)
{
	if (rbox_powerbtn_is_pressed())
		last_press = get_time();
}

enum touch_state pop_check_presence(int consume)
{
	int recent = (get_time().val - last_press.val) < PRESENCE_TIMEOUT;

	CPRINTS("Presence:%d", recent);
	if (consume)
		last_press.val = 0;

	/* user physical presence on the power button */
	return recent ? POP_TOUCH_YES : POP_TOUCH_NO;
}

/* ---- non-volatile U2F parameters ---- */

#define FLAG_U2F_ENABLE    (1 << 0)
#define FLAG_G2F_ENABLE    (1 << 1)
#define FLAG_UNINITIALIZED (1 << 7)

/* default U2F state on blank machines : /TBD/ Enable U2F and extensions */
#define FLAG_DEFAULT (FLAG_U2F_ENABLE | FLAG_G2F_ENABLE)

static uint32_t salt[8];
static uint8_t flags = FLAG_UNINITIALIZED;

static void create_state(uint8_t new_flags)
{
	const uint8_t k_flags = NVMEM_VAR_U2F_FLAGS;
	const uint8_t k_salt = NVMEM_VAR_U2F_SALT;
	/* create random salt */
	if (DCRYPTO_ladder_random(salt))
		return;
	if (setvar(&k_salt, sizeof(k_salt),
		   (const uint8_t *)salt, sizeof(salt)))
		return;
	if (setvar(&k_flags, sizeof(k_flags), &new_flags, sizeof(new_flags)))
		return;
	/* we are ready */
	flags = new_flags;
	CPRINTS("U2F new state %x", new_flags);
}

static void load_state(void)
{
	const uint8_t k_flags = NVMEM_VAR_U2F_FLAGS;
	const uint8_t k_salt = NVMEM_VAR_U2F_SALT;
	const struct tuple *t_flags = getvar(&k_flags, sizeof(k_salt));
	const struct tuple *t_salt = getvar(&k_salt, sizeof(k_salt));

	if (!t_flags)
		create_state(FLAG_DEFAULT);

	flags = *tuple_val(t_flags);
	memcpy(salt, tuple_val(t_salt), sizeof(salt));
}

static int use_u2f(void)
{
	if (flags & FLAG_UNINITIALIZED)
		load_state();

	return flags & FLAG_U2F_ENABLE;
}

int use_g2f(void)
{
	return use_u2f() && flags & FLAG_G2F_ENABLE;
}

static int command_u2f(int argc, char *argv[])
{
	char *e;

	if (argc > 1) {
		uint8_t new_flags;

		new_flags = strtoi(argv[1], &e, 0);
		if (new_flags != flags)
			create_state(new_flags);
	}
	ccprintf("U2F flags %x salt %.32h\n", flags, salt);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(u2f, command_u2f, "U2F",  "Get/set U2F state");

/* ---- chip-specific U2F crypto ---- */

int u2f_origin_keypair(uint8_t *seed, p256_int *d,
		       p256_int *pk_x, p256_int *pk_y)
{
	uint32_t tmp[8];

	do {
		if (DCRYPTO_ladder_random(seed))
			return EC_ERROR_UNKNOWN;
		memcpy(tmp, seed, sizeof(tmp));
		if (kl_derive_origin(tmp, tmp))
			return EC_ERROR_UNKNOWN;
	} while (
	    !DCRYPTO_p256_key_from_bytes(pk_x, pk_y, d, (const uint8_t *)tmp));

	return EC_SUCCESS;
}

int u2f_origin_key(const uint8_t *seed, p256_int *d)
{
	uint32_t tmp[8];

	memcpy(tmp, seed, sizeof(tmp));
	if (kl_derive_origin(tmp, tmp))
		return EC_ERROR_UNKNOWN;
	return DCRYPTO_p256_key_from_bytes(NULL, NULL, d,
					   (const uint8_t *)tmp) == 0;
}

int u2f_gen_kek(const uint8_t *origin, uint8_t *kek, size_t key_len)
{
	uint32_t buf[8];

	if (key_len != sizeof(buf))
		return EC_ERROR_UNKNOWN;
	if (kl_derive_wrap(salt, buf))
		return EC_ERROR_UNKNOWN;
	memcpy(kek, buf, key_len);

	return EC_SUCCESS;
}

int g2f_individual_keypair(p256_int *d, p256_int *pk_x, p256_int *pk_y)
{
	uint8_t buf[32];

	/* Incorporate HIK & diversification constant */
	if (kl_derive_attest(salt, (uint32_t *)buf))
		return EC_ERROR_UNKNOWN;

	/* Generate unbiased private key */
	while (!DCRYPTO_p256_key_from_bytes(pk_x, pk_y, d, buf)) {
		HASH_CTX sha;

		DCRYPTO_SHA256_init(&sha, 0);
		HASH_update(&sha, buf, sizeof(buf));
		memcpy(buf, HASH_final(&sha), SHA_DIGEST_MAX_BYTES);
	}

	return EC_SUCCESS;
}

#define SN_VERSION 0x02

void board_get_serial(p256_int *n)
{
	const struct SignedHeader *ro_hdr =
	    (const struct SignedHeader *)get_program_memory_addr(
		system_get_ro_image_copy());
	uint32_t tmp[8] = {0};
	uint8_t category;
	uint32_t id = GREG32(PMU, CHIP_ID);

	switch ((id >> 28) & 0x0f) {
	case 0x03: /* B1 */
		category = 0x00;
		break;
	case 0x04: /* B2 */
		category = 0x01;
		break;
	case 0x01: /* FPGA */
		category = 0xFF;
		break;
	default:
		category = 0x00;
	}

	tmp[7] = ro_hdr->keyid;
	tmp[6] = GREG32(FUSE, DEV_ID0);
	tmp[5] = GREG32(FUSE, DEV_ID1);
	tmp[4] = (SN_VERSION << 16) | (category << 24);

	p256_from_bin((const uint8_t *)tmp, n);
}

/* ---- Send/receive U2F APDU over TPM vendor commands ---- */

enum vendor_cmd_rc vc_u2f_apdu(enum vendor_cmd_cc code, void *body,
			       size_t cmd_size, size_t *response_size)
{
	uint16_t retlen;
	static uint8_t tmp_tx_buf[U2F_MAX_BCNT];

	if (!use_u2f())
		return VENDOR_RC_NO_SUCH_COMMAND;

	/* Process U2F APDU */
	retlen = u2f_apdu_rcv((const uint8_t *)body, cmd_size, tmp_tx_buf);
	if (retlen)
		memcpy(body, tmp_tx_buf, retlen);

	*response_size = retlen;
	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_U2F_APDU, vc_u2f_apdu);
