/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "curve25519.h"
#include "gpio.h"
#include "hooks.h"
#include "sha256.h"
#include "spi.h"
#include "registers.h"
#include "rsa.h"
#include "timer.h"
#include "trng.h"
#include "util.h"
#include "watchdog.h"

#include "test/rsa2048-F4.h"

#include "gpio_list.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/* Interrupt line from the fingerprint senser */
void fps_event(enum gpio_signal signal)
{
	/* HACK: Forward interrupt to AP */
	gpio_set_level(GPIO_AP_INT, gpio_get_level(GPIO_FPS_INT));
	CPRINTS("FPS %d\n", gpio_get_level(GPIO_FPS_INT));
}

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	/* Fingerprint sensor */
	{ CONFIG_SPI_FP_PORT, 1, GPIO_SPI3_NSS }
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

/* Initialize board-specific configuraiton */
static void board_init(void)
{
	/* Set all SPI master signal pins to very high speed: pins B3/B4/B5 */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0x00000fc0;
	/* Enable clocks to SPI3 module (master) */
	STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI3;
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

static void print_a32(uint8_t *data, const char *name)
{
	uint32_t *buf32 = (void *)data; /* let's prey it's aligned ...*/
	ccprintf("%s = %08x %08x %08x %08x\n", name,
		buf32[0], buf32[1], buf32[2], buf32[3]);
}


static void test_diffie_hellman(void)
{
	uint8_t a_pubkey[32];
	uint8_t a_privkey[32];
	uint8_t b_pubkey[32];
	uint8_t b_privkey[32];
	uint8_t a_shared_secret[32];
	uint8_t b_shared_secret[32];

	init_trng();
	/*
	 * Create an ephemeral public/private key pair
	 * and pretend we share a secret with it.
	 */
	/* Alice: Generate the keypair */
	X25519_keypair(a_pubkey, a_privkey);
	/* Bob: Generate another keypair */
	X25519_keypair(b_pubkey, b_privkey);
	/* Bob: get the shared secret with Alice pubkey */
	X25519(b_shared_secret, b_privkey, a_pubkey);
	/* Alice: ditto */
	X25519(a_shared_secret, a_privkey, b_pubkey);

	print_a32(a_pubkey, "Alice public key");
	print_a32(a_privkey, "Alice private key");
	print_a32(b_pubkey, "Bob public key");
	print_a32(b_privkey, "Bob private key");
	print_a32(a_shared_secret, "Alice shared secret");
	print_a32(b_shared_secret, "Bob shared secret");
	ccprintf("CRYPTO: %s\n", memcmp(a_shared_secret, b_shared_secret, 32) ? "FAILED" : "OK");
}

static void test_x25519_speed(void)
{
	static const uint8_t scalar1[32] = {
		0xa5, 0x46, 0xe3, 0x6b, 0xf0, 0x52, 0x7c, 0x9d,
		0x3b, 0x16, 0x15, 0x4b, 0x82, 0x46, 0x5e, 0xdd,
		0x62, 0x14, 0x4c, 0x0a, 0xc1, 0xfc, 0x5a, 0x18,
		0x50, 0x6a, 0x22, 0x44, 0xba, 0x44, 0x9a, 0xc4,
	};
	static const uint8_t point1[32] = {
		0xe6, 0xdb, 0x68, 0x67, 0x58, 0x30, 0x30, 0xdb,
		0x35, 0x94, 0xc1, 0xa4, 0x24, 0xb1, 0x5f, 0x7c,
		0x72, 0x66, 0x24, 0xec, 0x26, 0xb3, 0x35, 0x3b,
		0x10, 0xa9, 0x03, 0xa6, 0xd0, 0xab, 0x1c, 0x4c,
	};
	uint8_t out[32];
	timestamp_t t0, t1;

	watchdog_reload();
	X25519(out, scalar1, point1);
	t0 = get_time();
	X25519(out, scalar1, point1);
	t1 = get_time();
	ccprintf("X25519 duration %ld us\n", t1.val - t0.val);
}

static void test_rsa_f4_speed(void)
{
	timestamp_t t0, t1;
	int good;
	uint32_t rsa_workbuf[3 * RSANUMBYTES/4];

	watchdog_reload();
	t0 = get_time();
	good = rsa_verify(rsa_key, sig, hash, rsa_workbuf);
	t1 = get_time();
	if (!good)
		ccprintf("RSA verify FAILED\n");
	ccprintf("RSA duration %ld us\n", t1.val - t0.val);
}

static void test_sha256_speed(void)
{
	timestamp_t t0, t1;
	struct sha256_ctx ctx;

	watchdog_reload();
	t0 = get_time();
	SHA256_init(&ctx);
	SHA256_update(&ctx, (const uint8_t *)(NULL), 4096);
	SHA256_final(&ctx);
	t1 = get_time();
	ccprintf("SHA256 duration %ld us\n", t1.val - t0.val);
}

static void test_trng_unalign(uint8_t *buf, size_t off, size_t len)
{
	memset(buf, 0xee, 1024);
	rand_bytes(buf+off, len);
	ccprintf("%u+%u %02x %02x %02x %02x  %02x %02x %02x %02x  %02x\n",
		off, len, buf[0], buf[1], buf[2], buf[3],
		buf[4], buf[5], buf[6], buf[7], buf[8]);
}

static void test_trng_speed(void)
{
	timestamp_t t0, t1;
	union {
		uint32_t buf32[256];
		uint8_t buf8[1024];
	} u;

	init_trng();
	ccprintf("RNG %08x %08x %08x\n", rand(), rand(), rand());
	t0 = get_time();
	rand_bytes(u.buf32, sizeof(u.buf32));
	t1 = get_time();
	ccprintf("1KB RNG duration %ld us (%08x %08x)\n", t1.val - t0.val,
		u.buf32[0], u.buf32[255]);
	test_trng_unalign(u.buf8, 1, 3);
	test_trng_unalign(u.buf8, 0, 2);
	test_trng_unalign(u.buf8, 0, 5);
	test_trng_unalign(u.buf8, 3, 3);
	test_trng_unalign(u.buf8, 2, 6);

	exit_trng();
}

static int command_speed_test(int argc, char **argv)
{
	test_diffie_hellman();
	test_x25519_speed();
	test_rsa_f4_speed();
	test_sha256_speed();
	test_trng_speed();

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(speed_test, command_speed_test,
                        "Test crypto speed", NULL);
