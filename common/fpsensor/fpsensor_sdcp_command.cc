/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "crypto/cleanse_wrapper.h"
#include "ec_commands.h"
#include "fpsensor/fpsensor.h"
#include "fpsensor/fpsensor_auth_crypto.h"
#include "fpsensor/fpsensor_auth_secrets.h"
#include "host_command.h"
#include "openssl/mem.h"
#include "system.h"
#include "util.h"

#include <array>

static enum ec_status fp_command_sdcp_claim(struct host_cmd_handler_args *args)
{
	if (sizeof(ec_response_fp_sdcp_claim) > args->response_max) {
		return EC_RES_RESPONSE_TOO_BIG;
	}

	auto *r = static_cast<ec_response_fp_sdcp_claim *>(args->response);
	args->response_size = sizeof(ec_response_fp_sdcp_claim);

	auto ret = fp_sdcp_command(r);

	if (ret < 0) {
		return EC_RES_ERROR;
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_SDCP_CLAIM, fp_command_sdcp_claim,
		     EC_VER_MASK(0));

void print_bytes(const char *label, const uint8_t *bytes, size_t len)
{
	printk("%s: ", label);
	for (size_t i = 0; i < len; ++i) {
		printk("%02x", bytes[i]);
	}
	printk("\n");
}

static int command_fp_sdcp_command(int argc, const char **argv)
{
	struct ec_response_fp_sdcp_claim res;
	int ret = fp_sdcp_command(&res);
	if (ret != EC_SUCCESS) {
		printk("Failure\n");
		return ret;
	}
	print_bytes("pk_m", res.pk_m, sizeof(res.pk_m));
	print_bytes("s_goog", res.s_goog, sizeof(res.s_goog));
	print_bytes("pk_d", res.pk_d, sizeof(res.pk_d));
	print_bytes("s_m", res.s_m, sizeof(res.s_m));
	print_bytes("pk_f", res.pk_f, sizeof(res.pk_f));
	print_bytes("h_f", res.h_f, sizeof(res.h_f));
	print_bytes("s_d", res.s_d, sizeof(res.s_d));

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(fpsdcp, command_fp_sdcp_command, "", "");

static enum ec_status
fp_command_sdcp_establish(struct host_cmd_handler_args *args)
{
	const auto *params =
		static_cast<const ec_params_fp_sdcp_establish *>(args->params);
	if (params->pk_g[0] != 0x04) {
		return EC_RES_INVALID_PARAM;
	}
	bssl::UniquePtr<EC_KEY> public_key = create_ec_key_from_pubkey(
		*reinterpret_cast<const fp_elliptic_curve_public_key *>(
			&params->pk_g[1]));
	if (public_key == nullptr) {
		return EC_RES_INVALID_PARAM;
	}

	CleanseWrapper<std::array<uint8_t, 32> > sk_f{};
	auto ret = fp_sdcp_sk_f(sk_f.data(), sk_f.size());
	if (ret != EC_SUCCESS) {
		return EC_RES_ERROR;
	}
	bssl::UniquePtr<EC_KEY> private_key =
		create_ec_key_from_privkey(sk_f.data(), sk_f.size());
	if (private_key == nullptr) {
		return EC_RES_ERROR;
	}

	ret = generate_ecdh_shared_secret_without_kdf(*private_key, *public_key,
						      pairing_key);
	if (ret != EC_SUCCESS) {
		return EC_RES_ERROR;
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_SDCP_ESTABLISH, fp_command_sdcp_establish,
		     EC_VER_MASK(0));