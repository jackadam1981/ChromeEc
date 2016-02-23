/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Detect what adapter is connected */

#include "charge_manager.h"
#include "registers.h"
#include "timer.h"

void enable_usb(void)
{
	STM32_RCC_APB1ENR |= STM32_RCC_PB1_USB;
}

static uint16_t detect_type(uint16_t det_type)
{
	STM32_USB_BCDR &= 0;
	usleep(1);
	STM32_USB_BCDR |= (STM32_USB_BCDR_BCDEN | det_type);
	usleep(1);
	STM32_USB_BCDR &= ~(STM32_USB_BCDR_BCDEN | det_type);
	return STM32_USB_BCDR;
}


int get_device_type(void)
{
	uint16_t pdet_result;

	if (!(detect_type(STM32_USB_BCDR_DCDEN) & STM32_USB_BCDR_DCDET))
		return CHARGE_SUPPLIER_PD;

	pdet_result = detect_type(STM32_USB_BCDR_PDEN);
	/* TODO: add support for detecting proprietary chargers. */
	if (pdet_result & STM32_USB_BCDR_PDET) {
		if (detect_type(STM32_USB_BCDR_SDEN) & STM32_USB_BCDR_SDET)
			return CHARGE_SUPPLIER_BC12_DCP;
		else
			return CHARGE_SUPPLIER_BC12_CDP;
	} else if (pdet_result & STM32_USB_BCDR_PS2DET)
		return CHARGE_SUPPLIER_PROPRIETARY;
	else
		return CHARGE_SUPPLIER_BC12_SDP;
}
