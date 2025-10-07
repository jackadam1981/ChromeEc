/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <boot_param_platform.h>
#include <nvmem_vars.h>

#include "board_id.h"
#include "boot_param_platform_cr50.h"
#include "console.h"
#include "internal.h"
#include "tpm_nvmem_ops.h"

#define VERBOSE_BOOT_PARAM

#ifdef VERBOSE_BOOT_PARAM
#define verbose_log(s) __platform_log_str(s)
#else
#define verbose_log(s)
#endif

#define MAX_ECDSA_KEYGEN_ATTEMPTS 16

const char g_owner_data_var_name[] = {'B', 'P', 'O', 'D'};
const char g_auth_seed_var_name[] = {'A', 'U', 'T', 'H'};

/* Per-AP-boot configuration parameters */
struct ap_boot_config_s {
	/* [OUT] early entropy */
	uint8_t early_entropy[EARLY_ENTROPY_BYTES];
	/* [OUT] SessionKeySeed */
	uint8_t session_key_seed[KEY_SEED_BYTES];
	/* [OUT] AuthTokenKeySeed */
	uint8_t auth_token_key_seed[KEY_SEED_BYTES];
};
static struct ap_boot_config_s g_ap_boot_config = { 0 };
static bool g_ap_boot_config_valid = { 0 };

static inline void invalidate_ap_boot_config(void)
{
	memset(&g_ap_boot_config, 0, sizeof(struct ap_boot_config_s));
	if (setvar(g_auth_seed_var_name, sizeof(g_auth_seed_var_name),
		   NULL, 0) != 0) {
		verbose_log("erasing auth seed failed");
	}
	g_ap_boot_config_valid = false;
}

/* Static ECDSA key seed. Only one key is allowed to be used at any time. */
static uint8_t g_key_seed[P256_NBYTES] = { 0 };
static int g_key_seed_valid = { 0 };

static inline void invalidate_g_key_seed(void)
{
	memset(g_key_seed, 0, P256_NBYTES);
	g_key_seed_valid = 0;
}

/* Perform HKDF-SHA256(ikm, salt, info) */
bool __platform_hkdf_sha256(
	/* [IN] input key material */
	const struct slice_ref_s ikm,
	/* [IN] salt */
	const struct slice_ref_s salt,
	/* [IN] info */
	const struct slice_ref_s info,
	/* [IN/OUT] .size sets length for hkdf,
	 * .data is where the digest will be placed
	 */
	const struct slice_mut_s result
)
{
	int res = DCRYPTO_hkdf(result.data, result.size,
		salt.data, salt.size,
		ikm.data, ikm.size,
		info.data, info.size);
	return res != 0;
}

/* Implementation of HKDF-SHA512 algorithm. */
static int hkdf_sha512_extract(
	uint8_t *prk,
	const uint8_t *salt, size_t salt_len,
	const uint8_t *ikm, size_t ikm_len
)
{
	struct hmac_sha512_ctx ctx;

	if (prk == NULL)
		return 0;
	if (salt == NULL && salt_len > 0)
		return 0;
	if (ikm == NULL && ikm_len > 0)
		return 0;

	HMAC_SHA512_sw_init(&ctx, salt, salt_len);
	HMAC_SHA512_update(&ctx, ikm, ikm_len);
	memcpy(prk, HMAC_SHA512_final(&ctx), SHA512_DIGEST_SIZE);
	return 1;
}

static int hkdf_sha512_expand(
	uint8_t *okm, size_t okm_len,
	const uint8_t *prk,
	const uint8_t *info, size_t info_len
)
{
	uint8_t count = 1;
	const uint8_t *t = okm;
	size_t t_len = 0;
	uint32_t num_blocks = (okm_len / SHA512_DIGEST_SIZE) +
		(okm_len % SHA512_DIGEST_SIZE ? 1 : 0);

	if (okm == NULL || okm_len == 0)
		return 0;
	if (prk == NULL)
		return 0;
	if (info == NULL && info_len > 0)
		return 0;
	if (num_blocks > 255)
		return 0;

	while (okm_len > 0) {
		struct hmac_sha512_ctx ctx;
		const size_t block_size = okm_len < SHA512_DIGEST_SIZE ?
			okm_len : SHA512_DIGEST_SIZE;

		HMAC_SHA512_sw_init(&ctx, prk, SHA512_DIGEST_SIZE);
		HMAC_SHA512_update(&ctx, t, t_len);
		HMAC_SHA512_update(&ctx, info, info_len);
		HMAC_SHA512_update(&ctx, &count, sizeof(count));
		memcpy(okm, HMAC_SHA512_final(&ctx), block_size);

		t += t_len;
		t_len = SHA512_DIGEST_SIZE;
		count += 1;
		okm += block_size;
		okm_len -= block_size;
	}
	return 1;
}

static int DCRYPTO_hkdf_sha512(
	uint8_t *okm, size_t okm_len,
	const uint8_t *salt, size_t salt_len,
	const uint8_t *ikm, size_t ikm_len,
	const uint8_t *info, size_t info_len
)
{
	int result;
	uint8_t prk[SHA512_DIGEST_SIZE];

	if (!hkdf_sha512_extract(prk, salt, salt_len, ikm, ikm_len))
		return 0;

	result = hkdf_sha512_expand(okm, okm_len, prk, info, info_len);
	always_memset(prk, 0, sizeof(prk));
	return result;
}

/* Perform HKDF-SHA512(ikm, salt, info) */
bool __platform_hkdf_sha512(
	/* [IN] input key material */
	const struct slice_ref_s ikm,
	/* [IN] salt */
	const struct slice_ref_s salt,
	/* [IN] info */
	const struct slice_ref_s info,
	/* [IN/OUT] .size sets length for hkdf,
	 * .data is where the digest will be placed
	 */
	const struct slice_mut_s result
)
{
	int res = DCRYPTO_hkdf_sha512(result.data, result.size,
		salt.data, salt.size,
		ikm.data, ikm.size,
		info.data, info.size);
	return res != 0;
}

/* Calculate SHA256 for the provided buffer */
bool __platform_sha256(
	/* [IN] data to hash */
	const struct slice_ref_s data,
	/* [OUT] resulting digest */
	uint8_t digest[DIGEST_BYTES]
)
{
	struct sha256_digest aligned_digest;

	/* SHA256_hw_hash produces big-endian hash */
	SHA256_hw_hash(data.data, data.size, &aligned_digest);
	memcpy(digest, aligned_digest.b8, DIGEST_BYTES);

	return true;
}

/* Generate salt for UDS crypto ladder */
static inline void generate_name_hash(struct sha256_digest *digest)
{
	const char *uds_salt_name = "CrOS UDS";

	SHA256_hw_hash(uds_salt_name, strlen(uds_salt_name), digest);

	/* Note that unlike DCRYPTO_appkey_init/derive we don't byteswap
	 * the resulting constant. We just need it to be different from
	 * PINWEAVER salt, which it is.
	 */
}

/* Derive a secret from key ladder.
 * We can't use DCRYPTO_appkey_init/derive since they are in fips module,
 * and we'd need to add the 7th const to dcrypto_app_names[] in app_keys.
 * Instead, copy the simplified DCRYPTO_appkey_init/derive logic here and
 * re-use PINWEAVER app_id for this case.
 */
static inline bool derive_secret(
	/* [IN] derivation input */
	const uint32_t input[8],
	/* [OUT] secret */
	uint8_t secret[DIGEST_BYTES]
)
{
	const enum dcrypto_appid app_id = PINWEAVER;
	uint32_t secret_buf[8]; /* intermediate buffer to ensure alignment */
	struct sha256_digest digest;
	int res;

	generate_name_hash(&digest);

	if (!dcrypto_ladder_compute_usr(app_id, digest.b32)) {
		verbose_log("dcrypto_ladder_compute_usr failed");
		return false;
	}

	res = dcrypto_ladder_derive(app_id, digest.b32, input, secret_buf);
	DCRYPTO_appkey_finish();
	if (!res) {
		verbose_log("dcrypto_ladder_derive failed");
		return false;
	}
	memcpy(secret, secret_buf, DIGEST_BYTES);
	memset(secret_buf, 0, DIGEST_BYTES); /* zeroize temp buf */
	return true;
}

/* Derive UDS from key ladder.
 */
static inline bool derive_uds(uint8_t uds[DIGEST_BYTES])
{
	const uint32_t uds_input[8] = {
		0xa5a5a5a5,
		0x5a5a5a5a,
		0xa5a5a5a5,
		0x5a5a5a5a,
		0xa5a5a5a5,
		0x5a5a5a5a,
		0xa5a5a5a5,
		0x5a5a5a5a,
	};

	return derive_secret(uds_input, uds);
}

/* Derive versioned seed from key ladder.
 */
BUILD_ASSERT(DIGEST_BYTES == KEY_SEED_BYTES);
static inline bool derive_versioned_seed(
	/* [IN] seed version */
	uint32_t version,
	/* [OUT] seed */
	uint8_t seed[KEY_SEED_BYTES]
)
{
	uint32_t seed_input[8] = {
		0x26262626,
		0x62626262,
		0x26262626,
		0x62626262,
		0x26262626,
		0x62626262,
		0x26262626,
		0x00000000,
	};
	seed_input[7] = version;

	return derive_secret(seed_input, seed);
}

/* Get data that changes with owner clear */
static inline bool get_hidden_owner_data(uint8_t owner_data[DIGEST_BYTES])
{
	const struct tuple *var;

	var = getvar(g_owner_data_var_name, sizeof(g_owner_data_var_name));
	if (var) {
		/* Variable exists, let's get owner data. */
		if (var->val_len != DIGEST_BYTES) {
			verbose_log("wrong boot_param owner data size");
			freevar(var);
			return false;
		}
		memcpy(owner_data, tuple_val(var), DIGEST_BYTES);
		freevar(var);
		return true;
	}

	/* No owner data yet, let's create it. */
	if (!fips_trng_bytes(owner_data, DIGEST_BYTES)) {
		verbose_log("generating boot_param owner data failed");
		return false;
	}
	if (setvar(g_owner_data_var_name, sizeof(g_owner_data_var_name),
		   owner_data, DIGEST_BYTES) != 0) {
		verbose_log("setting boot_param var failed");
		return false;
	}
	return true;
}

/* Get DICE config */
bool __platform_get_dice_config(
	/* [OUT] DICE config */
	struct dice_config_s *cfg
)
{
	struct board_id bid;

	/* Just in case, pre-set all fields to zeroes.
	 * Future-proofing in case we add new fields to dice_config_s
	 * w/o updating platform code in cr50.
	 */
	memset(cfg, 0, sizeof(struct dice_config_s));

	/* Cr50 can't provide APROV status.
	 * Always return SettingNotProvisioned = 33
	 */
	cfg->aprov_status = 33;

	/* Cr50 can't provide GSCVD info.
	 * Leave sec_ver and code_digest at 0:
	 * cfg->sec_ver = 0;
	 * memset(cfg->code_digest, 0, DIGEST_BYTES);
	 */

	if (!derive_uds(cfg->uds))
		return false;

	if (!get_hidden_owner_data(cfg->hidden_digest))
		return false;

	if (!get_tpm_pcr_value(0, cfg->pcr0)) {
		verbose_log("getting pcr0 failed");
		return false;
	}

	if (!get_tpm_pcr_value(10, cfg->pcr10)) {
		verbose_log("getting pcr10 failed");
		return false;
	}

	cfg->gsc_type = BOOT_PARAM_GSC_TYPE_H1B3X;
	if (read_board_id(&bid) == EC_SUCCESS) {
		cfg->board_id_flags = bid.flags;
		cfg->board_id_type = bid.type;
	} else {
		cfg->board_id_flags = 0;
		cfg->board_id_type = 0;
	}


	return true;
}

/* Generate per-AP boot configuration */
static inline bool ensure_ap_boot_config(void)
{
	const struct tuple *var;

	if (g_ap_boot_config_valid)
		return true;

	var = getvar(g_auth_seed_var_name, sizeof(g_auth_seed_var_name));
	if (!var) {
		/*
		 * There must have been a hard reset, let's generate new
		 * entropy.
		 */
		if (!fips_trng_bytes(&g_ap_boot_config,
				     sizeof(struct ap_boot_config_s))) {
			verbose_log("generating boot config failed");
			return false;
		}
		if (setvar(g_auth_seed_var_name, sizeof(g_auth_seed_var_name),
			   (const uint8_t *)&g_ap_boot_config,
			   sizeof(g_ap_boot_config))) {
			verbose_log("storing boot config failed");
			return false;
		}
	} else {
		if (var->val_len != sizeof(g_ap_boot_config)) {
			ccprintf("unexpected boot config size %d\n",
				 var->val_len);
			freevar(var);
			/*
			 * Immediately generating a new value might hide a
			 * problem, better return an error: presumably TPM
			 * would be reset as a result and this variable will
			 * be regenerated.
			 */
			return false;
		}
		memcpy(&g_ap_boot_config, tuple_val(var),
		       sizeof(g_ap_boot_config));
		freevar(var);
	}

	g_ap_boot_config_valid = true;
	return true;
}

/* Get GSC boot parameters */
bool __platform_get_gsc_boot_param(
	/* [OUT] early entropy */
	uint8_t early_entropy[EARLY_ENTROPY_BYTES],
	/* [OUT] SessionKeySeed */
	uint8_t session_key_seed[KEY_SEED_BYTES],
	/* [OUT] AuthTokenKeySeed */
	uint8_t auth_token_key_seed[KEY_SEED_BYTES]
)
{
	if (!ensure_ap_boot_config())
		return false;

	memcpy(early_entropy,
	       g_ap_boot_config.early_entropy,
	       EARLY_ENTROPY_BYTES);
	memcpy(session_key_seed,
	       g_ap_boot_config.session_key_seed,
	       KEY_SEED_BYTES);
	memcpy(auth_token_key_seed,
	       g_ap_boot_config.auth_token_key_seed,
	       KEY_SEED_BYTES);

	return true;
}

/* Get current versioned seed */
bool __platform_get_cur_versioned_seed(
	/* [OUT] seed */
	uint8_t seed[KEY_SEED_BYTES],
	/* [OUT] version */
	uint32_t *version
)
{
	/* Hard-code initial version */
	const uint32_t cur_seed_version = 2;

	*version = cur_seed_version;
	return derive_versioned_seed(cur_seed_version, seed);
}

/*
 * This function is used Generate ECDSA P-256 key from the DRBG output.
 *
 * We can't just call DCRYPTO_p256_key_from_bytes() on the unadjusted
 * DRBG bytes because it adds 1 to the provided seed. And we need to
 * follow an exact keygen sequence (which doesn't add 1) so that AP FW
 * can generate the same CDI key from CDI.
 *
 * We can't change DCRYPTO_p256_key_from_bytes() - it is used for EK,
 * and we can't add a new function to dcrypto - it would change the u2f
 * certified binary. So, we have to create a custom function here.
 *
 * Instead of recreating the steps of DCRYPTO_p256_key_from_bytes() locally,
 * this custom function subtracts 1 from the seed before passing it to
 * DCRYPTO_p256_key_from_bytes(). Adding a big-endian subtraction cycle
 * should result in smaller code than copy-pasting the entire logic.
 *
 * We don't need to check that the original value is not 0 because
 * DCRYPTO_p256_key_from_bytes() will fail if it sees all 0xFF.
 *
 * Having an adjust_drbg_bytes() step after hmac_drbg_generate()
 * is chosen over wrapping DCRYPTO_p256_key_from_bytes() in a
 * custom function because DCRYPTO_p256_key_from_bytes() is called
 * from multiple places: __platform_ecdsa_p256_keygen_hmac_drbg,
 * __platform_ecdsa_p256_sign, and __platform_ecdsa_p256_get_pub_key.
 * Doing the sub-1 correction in all of them would require making a local
 * copy of g_key_seed (since otherwise we'd be decrementing it on every
 * use). Having an adjust_drbg_bytes step in one place allows us to
 * avoid this extra copy.
 */
static inline void adjust_drbg_bytes(
	/* [IN/OUT] bytes from DRBG, used as a big-endian */
	uint8_t key_bytes[P256_NBYTES]
)
{
	int i;

	for (i = P256_NBYTES - 1; i >= 0; i--) {
		if (key_bytes[i] == 0) {
			key_bytes[i] = 0xFF;
		} else {
			key_bytes[i]--;
			return;
		}
	}
}

/* Generate ECDSA P-256 key using HMAC-DRBG initialized by the seed */
bool __platform_ecdsa_p256_keygen_hmac_drbg(
	/* [IN] key seed */
	const uint8_t seed[DIGEST_BYTES],
	/* [OUT] ECDSA key handle */
	const void **key
)
{
	p256_int d;
	struct drbg_ctx drbg;
	enum dcrypto_result result = DCRYPTO_FAIL;
	size_t attempt = 0;

	*key = NULL;

	hmac_drbg_init(&drbg, seed, DIGEST_BYTES,
		NULL, 0, NULL, 0,
		HMAC_DRBG_DO_NOT_AUTO_RESEED);

	g_key_seed_valid = 0;
	do {
		result = hmac_drbg_generate(&drbg,
			g_key_seed, sizeof(g_key_seed),
			NULL, 0);
		if (result != DCRYPTO_OK) {
			verbose_log("hmac_drbg_generate failed");
			drbg_exit(&drbg);
			invalidate_g_key_seed();
			return false;
		}
		/* See the description of adjust_drbg_bytes() above */
		adjust_drbg_bytes(g_key_seed);
		result = DCRYPTO_p256_key_from_bytes(NULL, NULL,
			&d, g_key_seed);
		attempt++;
	} while (result == DCRYPTO_RETRY &&
		 attempt < MAX_ECDSA_KEYGEN_ATTEMPTS);
	drbg_exit(&drbg);
	if (result != DCRYPTO_OK) {
		verbose_log("DCRYPTO_p256_key_from_bytes failed");
		invalidate_g_key_seed();
		return false;
	}

	g_key_seed_valid = 1;
	*key = g_key_seed;
	return true;
}

/* Implementation of DRBG used by open-dice called by pvmfw.
 * This DRBG is used for CDI key/ID generation to ensure that
 * GSC and pvmfw independently calculate the same values.
 */
#define KV_SIZE 64

static void update_k(
	uint8_t k[KV_SIZE],
	const uint8_t v[KV_SIZE],
	uint8_t in2,
	const uint8_t *in3,
	unsigned int in3_len)
{
	struct hmac_sha512_ctx ctx;

	HMAC_SHA512_sw_init(&ctx, k, KV_SIZE);
	HMAC_SHA512_update(&ctx, v, KV_SIZE);
	HMAC_SHA512_update(&ctx, &in2, 1);
	/* HMAC_SHA512_update() internally checks for
	 * (in3 == NULL || in3_len == 0), no needto check here.
	 */
	HMAC_SHA512_update(&ctx, in3, in3_len);
	memcpy(k, HMAC_SHA512_final(&ctx), KV_SIZE);
}

static void update_v(const uint8_t k[KV_SIZE], uint8_t v[KV_SIZE])
{

	struct hmac_sha512_ctx ctx;

	HMAC_SHA512_sw_init(&ctx, k, KV_SIZE);
	HMAC_SHA512_update(&ctx, v, KV_SIZE);
	memcpy(v, HMAC_SHA512_final(&ctx), KV_SIZE);
}

/* Generate ECDSA P-256 key using HMAC-SHA512-DRBG used by open-dice.
 * This HKDF is used for CDI key/ID generation to ensure that
 * GSC and pvmfw independently calculate the same values.
 */
bool __platform_ecdsa_p256_keygen_hmac_sha512_opendice_drbg(
	/* [IN] key seed */
	const uint8_t seed[DIGEST_BYTES],
	/* [OUT] ECDSA key handle */
	const void **key
)
{
	uint8_t v[KV_SIZE];
	uint8_t k[KV_SIZE];
	p256_int d;
	enum dcrypto_result result = DCRYPTO_FAIL;
	size_t attempt = 0;

	memset(v, 1, KV_SIZE);
	memset(k, 0, KV_SIZE);

	update_k(k, v, 0x00, seed, DIGEST_BYTES);
	update_v(k, v);
	update_k(k, v, 0x01, seed, DIGEST_BYTES);

	*key = NULL;
	g_key_seed_valid = 0;
	do {
		update_v(k, v);
		update_v(k, v);
		memcpy(g_key_seed, v, P256_NBYTES);
		update_k(k, v, 0x00, NULL, 0);

		/* See the description of adjust_drbg_bytes() above */
		adjust_drbg_bytes(g_key_seed);
		result = DCRYPTO_p256_key_from_bytes(NULL, NULL,
						     &d, g_key_seed);
		attempt++;
	} while (result == DCRYPTO_RETRY &&
		 attempt < MAX_ECDSA_KEYGEN_ATTEMPTS);

	if (result != DCRYPTO_OK) {
		verbose_log("DCRYPTO_p256_key_from_bytes failed");
		invalidate_g_key_seed();
		return false;
	}

	g_key_seed_valid = 1;
	*key = g_key_seed;
	return true;
}

static bool get_ecdsa_points(
	/* [IN] ECDSA key handle */
	const void *key,
	/* [OUT] x as p256_int, can pass NULL if not required */
	p256_int *x,
	/* [OUT] y as p256_int, can pass NULL if not required */
	p256_int *y,
	/* [OUT] d as p256_int, must be non-NULL */
	p256_int *d
)
{
	enum dcrypto_result result = DCRYPTO_FAIL;

	if (key != g_key_seed || !g_key_seed_valid) {
		verbose_log("__platform_ecdsa_p256_get_pub_key: invalid key");
		return false;
	}
	result = DCRYPTO_p256_key_from_bytes(x, y, d, g_key_seed);
	if (result != DCRYPTO_OK) {
		verbose_log("DCRYPTO_p256_key_from_bytes failed");
		return false;
	}
	return true;
}

/* Generate ECDSA P-256 signature: 64 bytes (R | S) */
bool __platform_ecdsa_p256_sign(
	/* [IN] ECDSA key handle */
	const void *key,
	/* [IN] data to sign */
	const struct slice_ref_s data,
	/* [OUT] resulting signature */
	uint8_t signature[ECDSA_SIG_BYTES]
)
{
	struct sha256_digest digest;
	p256_int d, h, r, s;
	struct drbg_ctx ctx;
	enum dcrypto_result result = DCRYPTO_FAIL;

	if (!get_ecdsa_points(key, NULL, NULL, &d))
		return false;

	/* SHA256_hw_hash produces big-endian hash */
	SHA256_hw_hash(data.data, data.size, &digest);
	p256_from_bin(digest.b8, &h);

	hmac_drbg_init_rfc6979(&ctx, &d, &h);
	result = dcrypto_p256_ecdsa_sign(&ctx, &d, &h, &r, &s);
	drbg_exit(&ctx);

	if (result != DCRYPTO_OK) {
		verbose_log("dcrypto_p256_ecdsa_sign failed");
		return false;
	}

	p256_to_bin(&r, signature);
	p256_to_bin(&s, signature + ECDSA_POINT_BYTES);

	return true;
}

/* Get ECDSA public key X, Y */
bool __platform_ecdsa_p256_get_pub_key(
	/* [IN] ECDSA key handle */
	const void *key,
	/* [OUT] public key structure */
	struct ecdsa_public_s *pub_key
)
{
	p256_int x, y, d;

	if (!get_ecdsa_points(key, &x, &y, &d))
		return false;

	p256_to_bin(&x, pub_key->x);
	p256_to_bin(&y, pub_key->y);

	return true;
}

/* Free ECDSA key handle */
void __platform_ecdsa_p256_free(
	/* [IN] ECDSA key handle */
	const void *key
)
{
	invalidate_g_key_seed();
}

/* Check if APROV status allows making 'normal' boot mode decision */
bool __platform_aprov_status_allows_normal(
	/* [IN] APROV status */
	uint32_t aprov_status
)
{
	/*
	 * For Cr50 ARPOV status is ignored,
	 * so this method always returns true.
	 */
	return true;
}

/* Print error string to log */
void __platform_log_str(
	/* [IN] string to print */
	const char *str
)
{
	ccprintf("%s\n", str);
	cflush();
}

/* memcpy */
void __platform_memcpy(void *dest, const void *src, size_t size)
{
	memcpy(dest, src, size);
}

/* memset */
void __platform_memset(void *dest, uint8_t fill, size_t size)
{
	memset(dest, fill, size);
}

/* memcmp */
int __platform_memcmp(const void *str1, const void *str2, size_t size)
{
	return memcmp(str1, str2, size);
}

/* Handler for Owner Clear event.
 * Called by _plat__OwnerClearCallback.
 */
void boot_param_handle_owner_clear(void)
{
	if (setvar(g_owner_data_var_name,
		   sizeof(g_owner_data_var_name),
		   NULL, 0) != 0) {
		verbose_log("clearing boot_param var failed");
	}
}

/* Handler for TPM Startup event.
 * Called by _plat__StartupCallback.
 */
void boot_param_handle_tpm_startup(bool shall_reset_state)
{
	invalidate_g_key_seed();
	if (shall_reset_state)
		invalidate_ap_boot_config();
}
