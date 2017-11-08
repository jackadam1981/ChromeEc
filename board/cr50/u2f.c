/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helpers to emulate a U2F HID dongle over the TPM transport */

#include "console.h"
#include "dcrypto.h"
#include "extension.h"
#include "flash.h"
#include "nvmem_vars.h"
#include "rbox.h"
#include "registers.h"
#include "signed_header.h"
#include "system.h"
#include "tpm_vendor_cmds.h"
#include "u2f.h"
#include "u2f_impl.h"
#ifdef HAVE_PRIVATE
#include "u2f_ssh.h"
#endif
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_EXTENSION, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ## args)

/* ---- physical presence (using the laptop power button) ---- */

static timestamp_t last_press;

/* how long do we keep the last button press as valid presence */
#define PRESENCE_TIMEOUT (10 * SECOND)

void power_button_record(void)
{
	if (ap_is_on() && rbox_powerbtn_is_pressed())
		last_press = get_time();
}

enum touch_state pop_check_presence(int consume)
{
	int recent = (get_time().val - PRESENCE_TIMEOUT) < last_press.val;

	CPRINTS("Presence:%d", recent);
	if (consume)
		last_press.val = 0;

	/* user physical presence on the power button */
	return recent ? POP_TOUCH_YES : POP_TOUCH_NO;
}

/* ---- non-volatile U2F parameters ---- */

/*
 * Current mode defining the behavior of the U2F feature.
 * Identical to the one defined on the host side by the enum U2fMode
 * in the chrome_device_policy.proto protobuf.
 */
enum u2f_mode {
	MODE_UNSET = 0,
	/* Feature disabled */
	MODE_DISABLED = 1,
	/* U2F as defined by the FIDO Alliance specification */
	MODE_U2F = 2,
	/* U2F plus extensions for individual attestation certificate */
	MODE_U2F_EXTENDED = 3,
};

static uint32_t salt[8];
static uint8_t u2f_mode = MODE_UNSET;
static const uint8_t k_salt = NVMEM_VAR_U2F_SALT;

static int load_state(void)
{
	const struct tuple *t_salt = getvar(&k_salt, sizeof(k_salt));

	if (!t_salt) {
		/* create random salt */
		if (!DCRYPTO_ladder_random(salt))
			return 0;
		if (setvar(&k_salt, sizeof(k_salt),
			   (const uint8_t *)salt, sizeof(salt)))
			return 0;
		/* really save the new variable to flash */
		writevars();
	} else {
		memcpy(salt, tuple_val(t_salt), sizeof(salt));
	}

	return 1;
}

static int use_u2f(void)
{
	/*
	 * TODO(b/62294740): Put board ID check here if needed
	 * if (!board_id_we_want)
	 *	return 0;
	 */

	if (u2f_mode == MODE_UNSET) {
		if (load_state())
			/* Start without extension enabled, host will set it */
			u2f_mode = MODE_U2F;
	}

	return u2f_mode >= MODE_U2F;
}

int use_g2f(void)
{
	return use_u2f() && u2f_mode == MODE_U2F_EXTENDED;
}

unsigned u2f_custom_dispatch(uint8_t ins, struct apdu apdu,
			     uint8_t *buf, unsigned *ret_len)
{
	if (ins == U2F_VENDOR_MODE) {
		if (apdu.p1) { /* Set mode */
			u2f_mode = apdu.p2;
		}
		/* return the current mode */
		buf[0] = use_u2f() ? u2f_mode : 0;
		*ret_len = 1;
		return U2F_SW_NO_ERROR;
	}
#ifdef HAVE_PRIVATE
	if (use_g2f())
		return ssh_dispatch(ins, apdu, buf, ret_len);
#endif
	return U2F_SW_INS_NOT_SUPPORTED;
}

/* ---- chip-specific U2F crypto ---- */

static int _derive_key(enum dcrypto_appid appid, const uint32_t input[8],
		       uint32_t output[8])
{
	struct APPKEY_CTX ctx;
	int result;

	/* Setup USR-based application key. */
	if (!DCRYPTO_appkey_init(appid, &ctx))
		return 0;
	result = DCRYPTO_appkey_derive(appid, input, output);

	DCRYPTO_appkey_finish(&ctx);
	return result;
}

int u2f_origin_keypair(uint8_t *seed, p256_int *d,
		       p256_int *pk_x, p256_int *pk_y)
{
	uint32_t tmp[P256_NDIGITS];

	do {
		if (!DCRYPTO_ladder_random(seed))
			return EC_ERROR_UNKNOWN;
		memcpy(tmp, seed, sizeof(tmp));
		if (!_derive_key(U2F_ORIGIN, tmp, tmp))
			return EC_ERROR_UNKNOWN;
	} while (
	    !DCRYPTO_p256_key_from_bytes(pk_x, pk_y, d, (const uint8_t *)tmp));

	return EC_SUCCESS;
}

int u2f_origin_key(const uint8_t *seed, p256_int *d)
{
	uint32_t tmp[P256_NDIGITS];

	memcpy(tmp, seed, sizeof(tmp));
	if (!_derive_key(U2F_ORIGIN, tmp, tmp))
		return EC_ERROR_UNKNOWN;
	return DCRYPTO_p256_key_from_bytes(NULL, NULL, d,
					   (const uint8_t *)tmp) == 0;
}

int u2f_gen_kek(const uint8_t *origin, uint8_t *kek, size_t key_len)
{
	uint32_t buf[P256_NDIGITS];

	if (key_len != sizeof(buf))
		return EC_ERROR_UNKNOWN;
	if (!_derive_key(U2F_WRAP, salt, buf))
		return EC_ERROR_UNKNOWN;
	memcpy(kek, buf, key_len);

	return EC_SUCCESS;
}

int g2f_individual_keypair(p256_int *d, p256_int *pk_x, p256_int *pk_y)
{
	uint8_t buf[SHA256_DIGEST_SIZE];

	/* Incorporate HIK & diversification constant */
	if (!_derive_key(U2F_ATTEST, salt, (uint32_t *)buf))
		return EC_ERROR_UNKNOWN;

	/* Generate unbiased private key */
	while (!DCRYPTO_p256_key_from_bytes(pk_x, pk_y, d, buf)) {
		HASH_CTX sha;

		DCRYPTO_SHA256_init(&sha, 0);
		HASH_update(&sha, buf, sizeof(buf));
		memcpy(buf, HASH_final(&sha), sizeof(buf));
	}

	return EC_SUCCESS;
}

int g2f_ssh_keypair(int index, p256_int *d, p256_int *pk_x, p256_int *pk_y)
{
	uint8_t buf[SHA256_DIGEST_SIZE];
	const uint32_t *salt = ownerpin_salt();

	if (!salt)
		return EC_ERROR_UNKNOWN;

	if (!_derive_key(index, salt, (uint32_t *)buf))
		return EC_ERROR_UNKNOWN;

	while (!DCRYPTO_p256_key_from_bytes(pk_x, pk_y, d, buf)) {
		HASH_CTX sha;

		DCRYPTO_SHA256_init(&sha, 0);
		HASH_update(&sha, buf, sizeof(buf));
		memcpy(buf, HASH_final(&sha), sizeof(buf));
	}

	return EC_SUCCESS;
}


/* ---- Send/receive U2F APDU over TPM vendor commands ---- */

enum vendor_cmd_rc vc_u2f_apdu(enum vendor_cmd_cc code, void *body,
			       size_t cmd_size, size_t *response_size)
{
	unsigned retlen;

	if (!use_u2f()) { /* the feature is disabled */
		uint8_t *cmd = body;
		/* process it only if the host tries to enable the feature */
		if (cmd_size < 2 || cmd[1] != U2F_VENDOR_MODE) {
			*response_size = 0;
			return VENDOR_RC_NO_SUCH_COMMAND;
		}
	}

	/* Process U2F APDU */
	retlen = u2f_apdu_rcv(body, cmd_size, *response_size);

	*response_size = retlen;
	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_U2F_APDU, vc_u2f_apdu);

/* ---- certificates storage ---- */

#define SSH_CERT_COUNT 2
#define SSH_CERT_SIZE 0x800
#define SSH_CERT_OFFSET 0x3000
/* each certificate is divided into 2x 1KB blocks */
#define CERT_BLOCK_SIZE 1024
#define CERT_BLOCK_WORDS (CERT_BLOCK_SIZE / sizeof(uint32_t))

/*
 * Provide access to two 2K flash pages at offset 0x3000 from the base of each
 * RO regions. Page 1 is mapped to 0x43000 and page 2 - to 0x73000. Each page
 * is 0x800 in size.
 */
static const void* flash_cert_addr(uint32_t cert_num)
{
	return (const void *)(CONFIG_PROGRAM_MEMORY_BASE + SSH_CERT_OFFSET +
			      cert_num * CFG_FLASH_HALF);
}

uint16_t flash_cert_read(uint8_t cert_num, uint8_t blockno, void *out,
			 size_t *out_len)
{
	const uint32_t *p = flash_cert_addr(cert_num);
	uint32_t len;

	if (cert_num >= SSH_CERT_COUNT)
		return U2F_SW_RECORD_NOT_FOUND;

	/* its length is embedded in the first word of the certificate data */
	len = p[0];
	len += 4;
	if (len > SSH_CERT_SIZE)
		return U2F_SW_RECORD_NOT_FOUND;
	/* keep only the portion in the requested 1KB block */
	if (len < blockno * CERT_BLOCK_SIZE)
		return U2F_SW_RECORD_NOT_FOUND;
	len -= blockno * CERT_BLOCK_SIZE;
	len = MIN(CERT_BLOCK_SIZE, len);

	memcpy(out, p + blockno * CERT_BLOCK_SIZE, len);
	*out_len = len;

	return U2F_SW_NO_ERROR;
}

uint16_t flash_cert_write(uint8_t cert_num, uint8_t blockno, const void *buffer,
			  size_t buffer_size)
{
	uint16_t rc = U2F_SW_NO_ERROR;
	uint32_t cert_addr = (uint32_t)flash_cert_addr(cert_num);

	if (cert_num >= SSH_CERT_COUNT)
		return U2F_SW_RECORD_NOT_FOUND;
	if (buffer_size > SSH_CERT_SIZE)
		return 0x6F01;

	GREG32(GLOBALSEC, FLASH_REGION6_BASE_ADDR) = (uint32_t) cert_addr;
	GREG32(GLOBALSEC, FLASH_REGION6_SIZE) = SSH_CERT_SIZE - 1;
	GWRITE_FIELD(GLOBALSEC, FLASH_REGION6_CTRL, EN, 1);
	GWRITE_FIELD(GLOBALSEC, FLASH_REGION6_CTRL, RD_EN, 1);
	GWRITE_FIELD(GLOBALSEC, FLASH_REGION6_CTRL, WR_EN, 1);

	if (blockno == 0) {
		/*
		 * Assume the host is writing the blocks in sequential order,
		 * and erase the full 2kB page (block 0 and block 1),
		 * else it will be caught by the verification below.
		 */
		if (flash_physical_erase(cert_addr - CONFIG_PROGRAM_MEMORY_BASE,
					  SSH_CERT_SIZE)) {
			CPRINTF("Failed to erase certificate at 0x%x\n",
				cert_addr);
			rc = U2F_SW_FILE_FULL;
			goto lock_and_exit;
		}
	} else {
		int i;
		uint32_t *ptr = (uint32_t *)(cert_addr + blockno * CERT_BLOCK_WORDS);
		/* Verify that this half of the page was erased before. */
		for (i = 0; i < CERT_BLOCK_WORDS; i++, ptr++)
			if (*ptr != 0xFFFFFFFF) {
				rc = U2F_SW_FILE_FULL;
				goto lock_and_exit;
			}
	}

	if (buffer && buffer_size) {
		if (flash_physical_write(cert_addr - CONFIG_PROGRAM_MEMORY_BASE,
					buffer_size, buffer)) {
			CPRINTF("Failed to write certificate at 0x%x\n",
				cert_addr);
			rc = U2F_SW_FILE_FULL;
			goto lock_and_exit;
		}
	}

lock_and_exit:
	GWRITE_FIELD(GLOBALSEC, FLASH_REGION6_CTRL, WR_EN, 0);
	return rc;
}

/*
 * SSH extension PIN management
 *
 * only 1 try as we have no persistent counter, should be used with the pin
 * tied to the machine rather than user-entered pin (to avoid human error).
 */

struct ownerpin {
	/*
	 * Some entropy per PIN.
	 * Idea is that if PIN got reset, so did this entropy
	 * and dependent data should be crypted with this entropy.
	 */
	uint32_t salt[SHA256_DIGEST_WORDS];
	/*
	 * hashed/MACed PIN.
	 * Using the salt (and keyladder) as key(s).
	 * We compare against inputs indirectly, using hash/MAC.
	 */
	uint32_t hash[SHA256_DIGEST_WORDS];
};

static const uint8_t k_pin = NVMEM_VAR_U2F_PIN;
/* locally cached (properly aligned) version of the stored ownerpin */
static struct ownerpin pin_cache;

static const struct ownerpin* get_ownerpin(void)
{
	const struct tuple *t_pin = getvar(&k_pin, sizeof(k_pin));

	if (t_pin) {
		memcpy(&pin_cache, tuple_val(t_pin), sizeof(struct ownerpin));
		return &pin_cache;
	}
	return NULL;
}

const uint32_t* ownerpin_salt(void)
{
	const struct ownerpin *pin = get_ownerpin();

	return pin ? pin->salt : NULL;
}

static int ownerpin_hash(const uint32_t* salt, const void *data,
			 size_t data_len, void *dst)
{
	HASH_CTX sha;

	DCRYPTO_SHA256_init(&sha, 0);
	HASH_update(&sha, data, data_len);
	HASH_update(&sha, salt, SHA256_DIGEST_SIZE);

	return _derive_key(U2F_SSH, (const uint32_t *)HASH_final(&sha), dst) ?
			EC_SUCCESS : EC_ERROR_UNKNOWN;
}

int ownerpin_init(const void* data, size_t data_len)
{
	struct ownerpin pin;
	int err, i;

	/* Wipe certificates */
	for (i = 0; i < SSH_CERT_COUNT; i++)
		flash_cert_write(i, 0, NULL, 0);

	/* Pick and save a random salt associated with this PIN */
	if (!DCRYPTO_ladder_random(pin.salt))
		return EC_ERROR_UNKNOWN;
	err = ownerpin_hash(pin.salt, data, data_len, pin.hash);
	err |= setvar(&k_pin, sizeof(k_pin), (const void *)&pin, sizeof(pin));
	err |= writevars();

	return err;
}

int ownerpin_check(const void* data, size_t data_len)
{
	const struct ownerpin *pin = get_ownerpin();
	uint32_t hash[SHA256_DIGEST_WORDS];
	int result;

	/* PIN does not exist / was locked out by deletion */
	if (!pin)
		return -1;

	if (ownerpin_hash(pin->salt, data, data_len, hash) != EC_SUCCESS)
		return -1;
	result = safe_memcmp(hash, pin->hash, sizeof(hash));
	if (result) { /* Invalid PIN, delete it */
		setvar(&k_pin, sizeof(k_pin), NULL, 0);
		writevars(); /* commit immediatly */
	}
	return result;
}

int ownerpin_change(const void* data, size_t data_len)
{
	const struct ownerpin *pin = get_ownerpin();
	struct ownerpin npin;
	int err;

	/* PIN does not exist / was locked out by deletion */
	if (!pin)
		return -1;

	memcpy(npin.salt, pin->salt, sizeof(pin->salt));
	err = ownerpin_hash(pin->salt, data, data_len, npin.hash);
	err |= setvar(&k_pin, sizeof(k_pin), (const void*)&npin, sizeof(npin));
	err |= writevars();

	return err;
}
