/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "charger.h"
#include "charger/isl923x_public.h"

int extpower_is_present(void)
{
	int rv;
	bool acok;

	/* raa489000_is_acok() is part of the common isl923x driver. */
	rv = raa489000_is_acok(0, &acok);
	if ((rv == EC_SUCCESS) && acok) {
		return 1;
	}

	return 0;
}

#include <soc.h>
#define LED_PIN 2
#define RTCRST_PIN 5

static int brox_sign_of_life(void)
{
	uintptr_t a_gpdr = 0x00f01601;
	uintptr_t a_gpcr = 0x00f01610 + LED_PIN;
	uintptr_t a_gpotr = 0x00f01671;
	uintptr_t h_gpdr = 0x00f01608;
	uintptr_t h_gpcr = 0x00f01648 + RTCRST_PIN;
	uintptr_t h_gpotr = 0x00f01678;
	uint8_t temp;

	/* LED_1 */
	/* Push-pull */
	ECREG(a_gpotr) &= ~BIT(LED_PIN);
	/* Set high to start */
	ECREG(a_gpdr) |= BIT(LED_PIN);
	/* Set output */
	ECREG(a_gpcr) = (ECREG(a_gpcr) | GPCR_PORT_PIN_MODE_OUTPUT) &
			~GPCR_PORT_PIN_MODE_INPUT;

	/* RTCRST */
	/* Push-pull */
	ECREG(h_gpotr) &= ~BIT(RTCRST_PIN);
	/* Set high to start */
	ECREG(h_gpdr) |= BIT(RTCRST_PIN);
	/* Set output */
	ECREG(h_gpcr) = (ECREG(h_gpcr) | GPCR_PORT_PIN_MODE_OUTPUT) &
			~GPCR_PORT_PIN_MODE_INPUT;

	/* No-ops to add a little delay */
	temp = ECREG(a_gpdr);
	temp = ECREG(a_gpdr);
	temp = ECREG(a_gpdr);
	temp = ECREG(a_gpdr);
	temp = ECREG(a_gpdr);
	temp = ECREG(a_gpdr);
	temp = ECREG(a_gpdr);
	temp = ECREG(a_gpdr);
	temp = ECREG(a_gpdr);
	temp = ECREG(a_gpdr);

	/* Set low to generate a pulse */
	ECREG(a_gpdr) &= ~BIT(LED_PIN);
	ECREG(h_gpdr) &= ~BIT(RTCRST_PIN);

	return 0;
}
SYS_INIT(brox_sign_of_life, EARLY, 0);
