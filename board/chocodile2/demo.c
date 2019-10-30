/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "ina2xx.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#include "usb_pd.h"
#include "usb_pd_tcpc.h"

#include "registers.h"

/* Console output macros */
#define CPUTS(outstr) cputs(10, outstr)
#define CPRINTS(format, args...) cprints(10, format, ## args)

#define ALPHA "zyxwvutsrqponmlkjihgfedcba9876543210123456789"\
		"abcdefghijklmnopqrstuvwxyz"
static char *itoa(int value, char *result, int base)
{
	char *ptr;
	char *ptr1;
	char tmp_char;
	int tmp_value;

	/* check that the base if valid */
	if (base < 2 || base > 36) {
		*result = '\0';
		return result;
	}

	ptr = result;
	ptr1 = result;

	do {
		tmp_value = value;
		value /= base;
		*ptr++ = ALPHA[35 + (tmp_value - value * base)];
	} while (value);

	/* Apply negative sign */
	if (tmp_value < 0)
		*ptr++ = '-';

	*ptr-- = '\0';
	while (ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr-- = *ptr1;
		*ptr1++ = tmp_char;
	}

	return result;
}
#if 0
int started = 0;

void tcpc_alert(int port)
{
	int cc1, cc2;
	CPRINTS("tcpc_alert(%d)", port);
	
	/* check CC lines */
	tcpc_get_cc(port, &cc1, &cc2);
	if ((cc1 == 5 || cc2 == 5) && !started) {
		started = 1;
		CPRINTS("  starting bist mode 2");
		tcpc_set_rx_enable(port, 1);
		tcpc_transmit(port, TCPC_TX_HARD_RESET, 0,
			      NULL);
	}
}
#endif
void demo_task(void)
{
	uint32_t v = 0;
	char snum[20];

	STM32_RCC_APBENR1 |= STM32_RCC_UCPD1EN;

	while (1) {
		task_wait_event(1000000);
		itoa(v, snum, 16);
		gpio_set_level(GPIO_DEBUG_LED_G_ODL, !!(v & 0x01));

		CPRINTS("tick %04x", v);

		v++;
	}
}
