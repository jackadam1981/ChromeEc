/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helpers to emulate a U2F HID dongle over the TPM transport */

#include "console.h"
#include "dcrypto.h"
#include "extension.h"
#include "fips_rand.h"
#include "new_nvmem.h"
#include "nvmem_vars.h"
#include "rbox.h"
#include "registers.h"
#include "signed_header.h"
#include "system.h"
#include "tpm_nvmem_ops.h"
#include "tpm_vendor_cmds.h"
#include "u2f.h"
#include "u2f_impl.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_EXTENSION, format, ## args)

/* ---- physical presence (using the laptop power button) ---- */

static timestamp_t last_press;

/* how long do we keep the last button press as valid presence */
#define PRESENCE_TIMEOUT (10 * SECOND)

void power_button_record(void)
{
	if (ap_is_on() && rbox_powerbtn_is_pressed()) {
		last_press = get_time();
#ifdef CR50_DEV
		CPRINTS("record pp");
#endif
	}
}

enum touch_state pop_check_presence(int consume)
{
	int recent = ((last_press.val  > 0) &&
		((get_time().val - last_press.val) < PRESENCE_TIMEOUT));

#ifdef CR50_DEV
	if (recent)
		CPRINTS("User presence: consumed %d", consume);
#endif
	if (consume)
		last_press.val = 0;

	/* user physical presence on the power button */
	return recent ? POP_TOUCH_YES : POP_TOUCH_NO;
}

/* ---- non-volatile U2F state ---- */

static const uint8_t k_salt = NVMEM_VAR_G2F_SALT;
static const uint8_t k_salt_deprecated = NVMEM_VAR_U2F_SALT;

/* Can't include TPM2 headers, so just define constant locally. */
#define HR_NV_INDEX (1U << 24)

/* Wipe old U2F keys. */
bool u2f_zeroize_old(void)
{
	const uint32_t u2fobjs[] = { TPM_HIDDEN_U2F_KEK | HR_NV_INDEX,
				     TPM_HIDDEN_U2F_KH_SALT | HR_NV_INDEX, 0 };

	/* Delete NVMEM_VAR_G2F_SALT. */
	setvar(&k_salt, sizeof(k_salt), NULL, 0);
	/* Remove U2F keys and wipe all deleted objects. */
	nvmem_erase_tpm_data_selective(u2fobjs);
	return true;
}

bool u2f_load_old_state(bool create, uint32_t **p_salt, uint32_t **p_salt_kek,
			uint32_t **p_salt_kh)
{
	/* legacy u2f state */
	static uint32_t salt[8];
	static uint32_t salt_kek[8];
	static uint32_t salt_kh[8];

	const struct tuple *t_salt = getvar(&k_salt, sizeof(k_salt));

	if (!t_salt) {
		/* Delete the old salt if present, no-op if not. */
		setvar(&k_salt_deprecated, sizeof(k_salt_deprecated), NULL, 0);

		/* If we just check presence of old keys, stop here. */
		if (!create)
			return false;

		/* Otherwise create U2F keys in old style. */
		if (!fips_rand_bytes(salt, sizeof(salt)))
			return false;
		if (setvar(&k_salt, sizeof(k_salt), (const uint8_t *)salt,
			   sizeof(salt)))
			return false;
	} else {
		memcpy(salt, tuple_val(t_salt), sizeof(salt));
		freevar(t_salt);
	}

	*p_salt = salt;

	/**
	 * If creation is not needed, don't spend time further, as keys will
	 * be loaded / created on actual use.
	 */
	if (!create) {
		*p_salt_kek = NULL;
		*p_salt_kh = NULL;
		return true;
	}

	/**
	 * Since k_salt was present or created, we are in non-FIPS, load or
	 * create other keys using old method.
	 */
	if (read_tpm_nvmem_hidden(TPM_HIDDEN_U2F_KEK, sizeof(salt_kek),
				  salt_kek) == TPM_READ_NOT_FOUND) {
		/*
		 * Not found means that we have not used u2f before,
		 * or not used it with updated fw that resets kek seed
		 * on TPM clear.
		 */
		if (t_salt) { /* Note that memory has been freed already!. */
			/*
			 * We have previously used u2f, and may have
			 * existing registrations; we don't want to
			 * invalidate these, so preserve the existing
			 * seed as a one-off. It will be changed on
			 * next TPM clear.
			 */
			memcpy(salt_kek, salt, sizeof(salt_kek));
		} else {
			/*
			 * We have never used u2f before - generate
			 * new seed.
			 */
			if (!fips_rand_bytes(salt_kek, sizeof(salt_kek)))
				return false;
		}
		if (write_tpm_nvmem_hidden(TPM_HIDDEN_U2F_KEK, sizeof(salt_kek),
					   salt_kek,
					   1 /* commit */) != TPM_WRITE_CREATED)
			return false;
	}
	*p_salt_kek = salt_kek;

	if (read_tpm_nvmem_hidden(TPM_HIDDEN_U2F_KH_SALT, sizeof(salt_kh),
				  salt_kh) == TPM_READ_NOT_FOUND) {
		/*
		 * We have never used U2F before - generate
		 * new entropy for DRBG.
		 */
		fips_trng_bytes(salt_kh, sizeof(salt_kh));
		if (write_tpm_nvmem_hidden(TPM_HIDDEN_U2F_KH_SALT,
					   sizeof(salt_kh), salt_kh,
					   1 /* commit */) != TPM_WRITE_CREATED)
			return false;
	}
	*p_salt_kh = salt_kh;
	return true;
}
