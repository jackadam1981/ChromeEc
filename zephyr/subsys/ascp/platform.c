/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <ascp/ascp.h>

const uint8_t *get_ascp_pk_m()
{
	return (const uint8_t *)CONFIG_PLATFORM_EC_FINGERPRINT_ASCP_ADDR_PK_M;
}

const uint8_t *get_ascp_s_goog()
{
	return (const uint8_t *)CONFIG_PLATFORM_EC_FINGERPRINT_ASCP_ADDR_S_GOOG;
}

const uint8_t *get_ascp_pk_d()
{
	return (const uint8_t *)CONFIG_PLATFORM_EC_FINGERPRINT_ASCP_ADDR_PK_D;
}

const uint8_t *get_ascp_s_m()
{
	return (const uint8_t *)CONFIG_PLATFORM_EC_FINGERPRINT_ASCP_ADDR_S_M;
}

const uint8_t *get_ascp_pk_f()
{
	return (const uint8_t *)CONFIG_PLATFORM_EC_FINGERPRINT_ASCP_ADDR_PK_F;
}

const uint8_t *get_ascp_h_f()
{
	return (const uint8_t *)CONFIG_PLATFORM_EC_FINGERPRINT_ASCP_ADDR_H_F;
}

const uint8_t *get_ascp_s_d()
{
	return (const uint8_t *)CONFIG_PLATFORM_EC_FINGERPRINT_ASCP_ADDR_S_D;
}

const uint8_t *get_ascp_sk_f()
{
	return (const uint8_t *)CONFIG_PLATFORM_EC_FINGERPRINT_ASCP_ADDR_SK_F;
}

struct ascp_api ascp_api_instance = {
	.get_pk_m = get_ascp_pk_m,
	.get_s_goog = get_ascp_s_goog,
	.get_pk_d = get_ascp_pk_d,
	.get_s_m = get_ascp_s_m,
	.get_pk_f = get_ascp_pk_f,
	.get_h_f = get_ascp_h_f,
	.get_s_d = get_ascp_s_d,
	.get_sk_f = get_ascp_sk_f,
};
