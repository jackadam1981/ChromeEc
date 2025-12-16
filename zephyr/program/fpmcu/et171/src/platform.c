/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "fpsensor/fpsensor.h"
#include "write_protect.h"

#define MT_DATA_OFF DT_REG_ADDR(DT_NODELABEL(mt_data))
struct mt_data {
	uint32_t size;
	uint8_t data[];
};

int fp_vendor_command(uint32_t param, uint8_t *buf, size_t buf_size)
{
	const struct mt_data *mt_data =
		(struct mt_data *)(CONFIG_FLASH_BASE_ADDRESS + MT_DATA_OFF);
	if (mt_data->size > buf_size) {
		return -EC_RES_RESPONSE_TOO_BIG;
	}
	memcpy(buf, (uint8_t *)mt_data->data, mt_data->size);

	return mt_data->size;
}

/* TODO(b/432682921): Change based on the final solution of WP state. */
int write_protect_is_asserted_custom(void)
{
	return 0;
}

int get_sdcp_claim(struct ec_response_fp_sdcp_claim *res)
{
	memcpy(res->pk_m, (uint8_t *)0x80000138, sizeof(res->pk_m));
	memcpy(res->s_goog, (uint8_t *)0x80000179, sizeof(res->s_goog));
	memcpy(res->pk_d, (uint8_t *)0x80000035, sizeof(res->pk_d));
	memcpy(res->s_m, (uint8_t *)0x80000076, sizeof(res->s_m));
	// 0x90000000 is mirroring 0x00000000 in order to avoid NULL dereference
	memcpy(res->pk_f, (uint8_t *)0x90000060, sizeof(res->pk_f));
	memcpy(res->h_f, (uint8_t *)0x90000000, sizeof(res->h_f));
	memcpy(res->s_d, (uint8_t *)0x90000020, sizeof(res->s_d));

	return EC_SUCCESS;
}

int get_sdcp_sk_f(uint8_t *buf, size_t buf_size)
{
	memcpy(buf, (uint8_t *)0x900000A0, buf_size);

	return EC_SUCCESS;
}
