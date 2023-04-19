/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Krabby board-specific USB-C configuration */

#include "charge_manager.h"
#include "console.h"
#include "driver/tcpm/it83xx_pd.h"
#include "driver/usb_mux/tusb1064.h"
#include "i2c.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "variant_db_detection.h"
#include "zephyr_adc.h"

#define CPRINTSUSB(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

int tusb1064_mux_1_board_init(const struct usb_mux *me)
{
	int rv;

	rv = i2c_write8(me->i2c_port, me->i2c_addr_flags,
			TUSB1064_REG_DP1DP3EQ_SEL,
			TUSB1064_DP1EQ(TUSB1064_DP_EQ_RX_8_9_DB) |
				TUSB1064_DP3EQ(TUSB1064_DP_EQ_RX_5_4_DB));
	if (rv)
		return rv;

	/* Enable EQ_OVERRIDE so the gain registers are used */
	return i2c_update8(me->i2c_port, me->i2c_addr_flags,
			   TUSB1064_REG_GENERAL, REG_GENERAL_EQ_OVERRIDE,
			   MASK_SET);
}

#ifdef CONFIG_USB_PD_TCPM_ITE_ON_CHIP
const struct cc_para_t *board_get_cc_tuning_parameter(enum usbpd_port port)
{
	const static struct cc_para_t
		cc_parameter[CONFIG_USB_PD_ITE_ACTIVE_PORT_COUNT] = {
			{
				.rising_time =
					IT83XX_TX_PRE_DRIVING_TIME_1_UNIT,
				.falling_time =
					IT83XX_TX_PRE_DRIVING_TIME_2_UNIT,
			},
			{
				.rising_time =
					IT83XX_TX_PRE_DRIVING_TIME_1_UNIT,
				.falling_time =
					IT83XX_TX_PRE_DRIVING_TIME_2_UNIT,
			},
		};

	return &cc_parameter[port];
}
#endif

void board_reset_pd_mcu(void)
{
	/*
	 * C0 & C1: TCPC is embedded in the EC and processes interrupts in the
	 * chip code (it83xx/intc.c)
	 */
}

#ifdef CONFIG_USB_PD_VBUS_MEASURE_ADC_EACH_PORT
enum adc_channel board_get_vbus_adc(int port)
{
	if (port == 0) {
		return ADC_VBUS_C0;
	}
	if (port == 1) {
		return ADC_VBUS_C1;
	}
	CPRINTSUSB("Unknown vbus adc port id: %d", port);
	return ADC_VBUS_C0;
}
#endif /* CONFIG_USB_PD_VBUS_MEASURE_ADC_EACH_PORT */
