/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "fff_function_signature.pch"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "printf.h"
#include "uart.h"

DEFINE_FFF_GLOBALS;

/* printf.h */
DEFINE_FAKE_VALUE_FUNC(int, vfnprintf, vfnprintf_addchar_t, void *,
		       const char *, va_list);

/* uart.h */
DEFINE_FAKE_VALUE_FUNC(int, uart_tx_char_raw, void *, int);
DEFINE_FAKE_VOID_FUNC(uart_tx_start);

static void fake_reset_rule_before(const struct ztest_unit_test *test,
				   void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

	/* printf.h */
	printk("resetting vfnprintf\n");
//	RESET_FAKE(vfnprintf);
	memset((void*)&vfnprintf_fake, 0,
	       sizeof(vfnprintf_fake) - sizeof(vfnprintf_fake.custom_fake) -
	       sizeof(vfnprintf_fake.custom_fake_seq));
	printk("resetting custom_fake\n");
	vfnprintf_fake.custom_fake = NULL;
	printk("resetting custom_fake_seq\n");
	vfnprintf_fake.custom_fake_seq = NULL;
	vfnprintf_fake.arg_history_len = FFF_ARG_HISTORY_LEN;

	/* uart.h */
	printk("resetting uart_tx_char_raw\n");
	RESET_FAKE(uart_tx_char_raw);
	printk("resetting uart_tx_start\n");
	RESET_FAKE(uart_tx_start);
}

ZTEST_RULE(fake_reset, fake_reset_rule_before, nullptr);
