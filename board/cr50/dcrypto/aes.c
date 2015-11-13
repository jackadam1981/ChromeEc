/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dcrypto.h"
#include "internal.h"

#include "registers.h"

static void set_control_register(
	unsigned mode, unsigned key_size, unsigned encrypt)
{
	GWRITE_FIELD(KEYMGR, AES_CTRL, RESET, CTRL_NO_SOFT_RESET);
	GWRITE_FIELD(KEYMGR, AES_CTRL, KEYSIZE, key_size);
	GWRITE_FIELD(KEYMGR, AES_CTRL, CIPHER_MODE, mode);
	GWRITE_FIELD(KEYMGR, AES_CTRL, ENC_MODE, encrypt);
	GWRITE_FIELD(KEYMGR, AES_CTRL, CTR_ENDIAN, CTRL_CTR_BIG_ENDIAN);
	GWRITE_FIELD(KEYMGR, AES_CTRL, ENABLE, CTRL_ENABLE);
}

static int wait_read_data(volatile uint32_t *addr)
{
	int empty;
	int count = 20;     /* Wait these many ~cycles. */

	do {
		empty = REG32(addr);
		count--;
	} while (count && empty);

	return empty ? 0 : 1;
}

int DCRYPTO_aes_init(const uint8_t *key, uint32_t key_len, const uint8_t *iv,
		enum cipher_mode c_mode, enum encrypt_mode e_mode)
{
	int i;
	const struct access_helper *p;
	uint32_t key_mode;

	switch (key_len) {
	case 128:
		key_mode = 0;
		break;
	case 192:
		key_mode = 1;
		break;
	case 256:
		key_mode = 2;
		break;
	default:
		/* Invalid key length specified. */
		return 0;
	}
	set_control_register(c_mode, key_mode, e_mode);

	/* Initialize hardware with AES key */
	p = (struct access_helper *) key;
	for (i = 0; i < (key_len >> 5); i++)
		GR_KEYMGR_AES_KEY(i) = p[i].udata;
	/* Trigger key expansion. */
	GREG32(KEYMGR, AES_KEY_START) = 1;

	/* Wait for key expansion. */
	if (!wait_read_data(GREG32_ADDR(KEYMGR, AES_KEY_START))) {
		/* Should not happen. */
		return 0;
	}

	/* Initialize IV for modes that require it. */
	if (iv) {
		p = (struct access_helper *) iv;
		for (i = 0; i < 4; i++)
			GR_KEYMGR_AES_CTR(i) = p[i].udata;
	}
	return 1;
}

int DCRYPTO_aes_block(const uint8_t *in, uint8_t *out)
{
	int i;
	struct access_helper *outw;
	const struct access_helper *inw = (struct access_helper *) in;

	/* Write plaintext. */
	for (i = 0; i < 4; i++)
		GREG32(KEYMGR, AES_WFIFO_DATA) = inw[i].udata;

	/* Wait for the result. */
	if (!wait_read_data(GREG32_ADDR(KEYMGR, AES_RFIFO_EMPTY))) {
		/* Should not happen, ciphertext not ready. */
		return 0;
	}

	/* Read ciphertext. */
	outw = (struct access_helper *) out;
	for (i = 0; i < 4; i++)
		outw[i].udata = GREG32(KEYMGR, AES_RFIFO_DATA);
	return 1;
}

void DCRYPTO_aes_write_iv(const uint8_t *iv)
{
	int i;
	const uint32_t *p = (const uint32_t *) iv;

	for (i = 0; i < 4; i++)
		GR_KEYMGR_AES_CTR(i) = p[i];
}

void DCRYPTO_aes_read_iv(uint8_t *iv)
{
	int i;
	uint32_t *p = (uint32_t *) iv;

	for (i = 0; i < 4; i++)
		p[i] = GR_KEYMGR_AES_CTR(i);
}

#ifdef CRYPTO_TEST_SETUP

#include "backdoor.h"
#include "hooks.h"
#include "uart.h"

static void aes_command_handler(const void *cmd_body,
				size_t cmd_size,
				void *cmd_response,
				size_t *response_size)
{
	const uint8_t *key;
	uint16_t key_len;
	uint8_t iv_len;
	const uint8_t *iv;
	enum cipher_mode c_mode;
	enum encrypt_mode e_mode;
	uint8_t *cmd = (uint8_t *)cmd_body;
	uint8_t *response = (uint8_t *)cmd_response;
	int16_t data_len;
	unsigned max_data_len = *response_size;
	unsigned actual_cmd_size;

	*response_size = 0;

	/*
	 * Command structure, shared out of band with the test driver running
	 * on the host:
	 *
	 * field       |    size  |              note
	 * ================================================================
	 * mode        |    1     | 0 - decrypt, 1 - encrypt
	 * cipher_mode |    1     | ECB = 0, CTR = 1, CBC = 2, GCM = 3
	 * key_len     |    1     | key size in bytes (16, 24 or 32)
	 * key         | key len  | key to use
	 * iv_len      |   1      | either 0 or 16
	 * iv          | 0 or 16  | as defined by iv_len
	 * text_len    |    2     | size of the text to process, big endian
	 * text        | text_len | text to encrypt/decrypt
	 */
	e_mode = *cmd++;
	c_mode = *cmd++;
	key_len = *cmd++;

	if ((key_len != 16) && (key_len != 24) && (key_len != 32)) {
		uart_printf("Invalid key len %d\n", key_len * 8);
		return;
	}
	key = cmd;
	cmd += key_len;
	key_len *= 8;
	iv_len = *cmd++;
	if (iv_len && (iv_len != 16)) {
		uart_printf("Invalid vector len %d\n", iv_len);
		return;
	}
	iv = cmd;
	cmd += iv_len;
	data_len = *cmd++;
	data_len = data_len * 256 + *cmd++;

	/*
	 * We know that the receive buffer is at least this big, i.e. all the
	 * preceding fields are guaranteed to fit.
	 *
	 * Now is a good time to verify overall sanity of the received
	 * payload: does the actual size match the added up sizes of the
	 * pieces.
	 */
	actual_cmd_size = cmd - (const uint8_t *)cmd_body + data_len;
	if (actual_cmd_size != cmd_size) {
		uart_printf("Command size mismatch: %d != %d (data len %d)\n",
			    actual_cmd_size, cmd_size, data_len);
		return;
	}

	if (((data_len + 15) & ~15) > max_data_len) {
		uart_printf("Response buffer too small\n");
		return;
	}

	if (!DCRYPTO_aes_init(key, key_len, iv, c_mode, e_mode)) {
		uart_printf("Initialization failed\n");
		return;
	}

	/*
	 * The only thing we return is the result of the operation, its size
	 * accumulated in *response_size.
	 */
	while (data_len > 0) {
		DCRYPTO_aes_block(cmd, response);
		cmd += 16;
		response += 16;
		data_len -= 16;
		*response_size += 16;
	}
}

static void hook_install(void)
{
	backdoor_register_handler(0, aes_command_handler);
}
DECLARE_HOOK(HOOK_INIT, hook_install, HOOK_PRIO_LAST);
#endif   /* CRYPTO_TEST_SETUP */
