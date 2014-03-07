/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Tiny charger configuration */

#include "common.h"
#include "registers.h"
#include "util.h"

static void hardware_init(void)
{
	/* Pin usage */
	/*
	 * PA1  (ANALOG - ADC_IN1) : CC sense 
	 * PA2  (ANALOG - ADC_IN2) : Current sense
	 * PA3  (ANALOG - ADC_IN3) : Voltage sense
	 * PA4  (OUT - OD GPIO)    : PD TX enable
	 * PA5  (AF0 - SPI1_SCK)   : TX clock in
	 * PA6  (AF0 - SPI1_MISO)  : PD TX
	 * PA7  (AF5 - TIM17_CH1)  : PD RX
	 * PA9  (AF1 - UART1_TX)   : [DEBUG] UART TX
	 * PA10 (AF1 - UART1_RX)   : [DEBUG] UART RX
	 * PA13 (OUT - GPIO)       : voltage select[0]
	 * PA14 (OUT - GPIO)       : voltage select[1]
	 * PB1  (AF0 - TIM14_CH1)  : TX clock out
	 * PF0  (OUT - GPIO)       : LM5050 FET driver off
	 * PF1  (OUT - GPIO)       : discharge FET
	 */

	/* --- dev board only --- */
	/*
	 * User button : PA0 (IN)
	 * Blue LED    : PB6 (OUT)
	 * Green LED   : PB7 (OUT)
	 */
}

int main(void)
{
	hardware_init();
	while (1) {
	}
}
