/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Implementation of the RW firmware signature verification and jump.
 */

#include "console.h"
#include "ec_commands.h"
#include "rsa.h"
#include "sha256.h"
#include "shared_mem.h"
#include "system.h"
#include "usb_pd.h"
#include "util.h"
#include "vb21_struct.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/* RW firmware reset vector */
static uint32_t * const rw_rst =
	(uint32_t *)(CONFIG_PROGRAM_MEMORY_BASE + CONFIG_RW_MEM_OFF + 4);

void check_rw_signature(void)
{
	struct sha256_ctx ctx;
	int res, i;
	const struct rsa_public_key *key;
	const uint8_t *sig;
	uint8_t *hash;
	uint32_t *rsa_workbuf;
	uint8_t *rwdata = (uint8_t *)CONFIG_PROGRAM_MEMORY_BASE
		      + CONFIG_RW_MEM_OFF;
	int good = 0;

	int rwlen;
#ifdef CONFIG_RWSIG_TYPE_RWSIG
	const struct vb21_packed_key *vb21_key;
	const struct vb21_signature *vb21_sig;
#endif

	/* Only the Read-Only firmware needs to do the signature check */
	if (system_get_image_copy() != SYSTEM_IMAGE_RO)
		return;

	/* Check if we have a RW firmware flashed */
	if (*rw_rst == 0xffffffff)
		return;

	CPRINTS("Verifying RW image...");

	/* Large buffer for RSA computation : could be re-use afterwards... */
	res = shared_mem_acquire(3 * RSANUMBYTES, (char **)&rsa_workbuf);
	if (res) {
		CPRINTS("No memory for RW verification");
		return;
	}

#ifdef CONFIG_RWSIG_TYPE_USBPD1
	key = (const struct rsa_public_key *)CONFIG_RO_PUBKEY_ADDR;
	sig = (const uint8_t *)CONFIG_RW_SIG_ADDR;
	rwlen = CONFIG_RW_SIZE - CONFIG_RW_SIG_SIZE;
#elif defined(CONFIG_RWSIG_TYPE_RWSIG)
	vb21_key = (const struct vb21_packed_key *)CONFIG_RO_PUBKEY_ADDR;
	vb21_sig = (const struct vb21_signature *)CONFIG_RW_SIG_ADDR;

	if (vb21_key->c.magic != VB21_MAGIC_PACKED_KEY) {
		CPRINTS("Invalid packed_key VB2 magic signature.");
		goto out;
	}

	if (vb21_key->key_size != sizeof(struct rsa_public_key)) {
		CPRINTS("Invalid VB2 key size.");
		goto out;
	}

	key = (const struct rsa_public_key *)
		((const uint8_t *)vb21_key + vb21_key->key_offset);

	if (vb21_sig->c.magic != VB21_MAGIC_SIGNATURE) {
		CPRINTS("Invalid signature VB2 magic signature.");
		goto out;
	}

	if (vb21_sig->sig_size != RSANUMBYTES) {
		CPRINTS("Invalid VB2 signature size.");
		goto out;
	}

	/*
	 * TODO(crbug.com/690773): We could verify other parameters such
	 * as sig_alg/hash_alg actually matches what we build for.
	 */

	if (vb21_key->sig_alg != vb21_sig->sig_alg ||
		vb21_key->hash_alg != vb21_sig->hash_alg) {
		CPRINTS("Mismatching key algorithms");
		goto out;
	}

	sig = (const uint8_t *)vb21_sig + vb21_sig->sig_offset;
	rwlen = vb21_sig->data_size;
#endif

	/*
	 * Check that unverified RW region is actually filled with zeros.
	 *
	 * TODO(crbug.com/p/62798): This can be optimized further by doing
	 * 32-bit wide memory accesses.
	 */
	for (i = rwlen; i < CONFIG_RW_SIZE - CONFIG_RW_SIG_SIZE; i++) {
		if (rwdata[i] != 0xff) {
			CPRINTS("Invalid padding outside of signed region.");
			goto out;
		}
	}

	/* SHA-256 Hash of the RW firmware */
	SHA256_init(&ctx);
	SHA256_update(&ctx, rwdata, rwlen);
	hash = SHA256_final(&ctx);

	good = rsa_verify(key, sig, hash, rsa_workbuf);
out:
	if (good) {
		CPRINTS("RW image verified");
		/* Jump to the RW firmware */
		system_run_image_copy(SYSTEM_IMAGE_RW);
	} else {
		CPRINTS("RSA verify FAILED");
		pd_log_event(PD_EVENT_ACC_RW_FAIL, 0, 0, NULL);
		/* RW firmware is invalid : do not jump there */
		if (system_is_locked())
			system_disable_jump();
	}
	shared_mem_release(rsa_workbuf);
}
