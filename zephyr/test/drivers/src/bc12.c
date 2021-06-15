/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_emul.h>

#include "emul/emul_pi3usb9201.h"

#include "timer.h"
#include "usb_charge.h"
#include "battery.h"
#include "extpower.h"
#include "stubs.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(test_drivers_bc12, LOG_LEVEL_DBG);

#define EMUL_LABEL DT_NODELABEL(pi3usb9201_emul)

#define PI3USB9201_ORD DT_DEP_ORD(EMUL_LABEL)

/* Control_1 regiter bit definitions */
#define PI3USB9201_REG_CTRL_1_INT_MASK BIT(0)
#define PI3USB9201_REG_CTRL_1_MODE_SHIFT 1
#define PI3USB9201_REG_CTRL_1_MODE_MASK (0x7 << \
					 PI3USB9201_REG_CTRL_1_MODE_SHIFT)

/* Control_2 regiter bit definitions */
#define PI3USB9201_REG_CTRL_2_AUTO_SW BIT(1)
#define PI3USB9201_REG_CTRL_2_START_DET BIT(3)

/* Host status register bit definitions */
#define PI3USB9201_REG_HOST_STS_BC12_DET BIT(0)
#define PI3USB9201_REG_HOST_STS_DEV_PLUG BIT(1)
#define PI3USB9201_REG_HOST_STS_DEV_UNPLUG BIT(2)

enum pi3usb9201_mode {
	PI3USB9201_POWER_DOWN,
	PI3USB9201_SDP_HOST_MODE,
	PI3USB9201_DCP_HOST_MODE,
	PI3USB9201_CDP_HOST_MODE,
	PI3USB9201_CLIENT_MODE,
	PI3USB9201_RESERVED_1,
	PI3USB9201_RESERVED_2,
	PI3USB9201_USB_PATH_ON,
};

enum pi3usb9201_client_sts {
	CHG_OTHER = 0,
	CHG_2_4A,
	CHG_2_0A,
	CHG_1_0A,
	CHG_RESERVED,
	CHG_CDP,
	CHG_SDP,
	CHG_DCP,
};

#define GPIO_BATT_PRES_ODL_PATH DT_PATH(named_gpios, ec_batt_pres_odl)
#define GPIO_BATT_PRES_ODL_PORT DT_GPIO_PIN(GPIO_BATT_PRES_ODL_PATH, gpios)

#define GPIO_ACOK_OD_PATH DT_PATH(named_gpios, acok_od)
#define GPIO_ACOK_OD_PORT DT_GPIO_PIN(GPIO_ACOK_OD_PATH, gpios)

/*
 * The driver test app seems to have USBC_PORT_COUNT=2 but missing the task for
 * port 1, so change port count to 1 here.
 * TODO: fix the app so count and tasks agree.
 */
__override uint8_t board_get_usb_pd_port_count(void)
{
	return 1;
}

/*
 * PI3USB9201 is a dual-role BC1.2 charger detector/advertiser used on dual-role
 * USB-C ports. It can be programmed to operate in host mode or client mode
 * through I2C. When operating as a host, PI3USB9201 enables BC1.2 SDP/CDP/DCP
 * advertisement to the attached USB devices via the D+/- connection. When
 * operating as a client, PI3USB9201 starts BC1.2 detection to detect the
 * attached host type. In both host mode and client mode, the detection results
 * are reported through I2C to the controller.
 */
static void test_bc12_pi3usb9201(void)
{
	const struct device *batt_pres_dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_BATT_PRES_ODL_PATH, gpios));
	const struct device *acok_dev =
		DEVICE_DT_GET(DT_GPIO_CTLR(GPIO_ACOK_OD_PATH, gpios));

	struct i2c_emul *emul =
		pi3usb9201_emul_get(PI3USB9201_ORD);

	int val;

	/* Pretend we have battery and AC so charging works normally. */
	zassert_ok(gpio_emul_input_set(batt_pres_dev,
				       GPIO_BATT_PRES_ODL_PORT, 0), NULL);
	zassert_equal(BP_YES, battery_is_present(), NULL);
	zassert_ok(gpio_emul_input_set(acok_dev, GPIO_ACOK_OD_PORT, 1), NULL);
	msleep(CONFIG_EXTPOWER_DEBOUNCE_MS + 1);
	zassert_equal(1, extpower_is_present(), NULL);

	/* Wait long enough for TCPMv2 to be idle. */
	msleep(2000);

	/*
	 * Pretend that the USB-C Port Manager (TCPMv2) has set the port data
	 * role to disconnected.
	 */
	task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_CC_OPEN);
	msleep(1);
	/*
	 * Expect the pi3usb9201 driver to configure power down mode and mask
	 * interrupts.
	 */
	val = PI3USB9201_POWER_DOWN << PI3USB9201_REG_CTRL_1_MODE_SHIFT;
	val |= PI3USB9201_REG_CTRL_1_INT_MASK;
	zassert_equal(pi3usb9201_emul_get_reg(emul, PI3USB9201_REG_CTRL_1),
		      val, NULL);

	/*
	 * Pretend that the USB-C Port Manager (TCPMv2) has set the port data
	 * role to DFP.
	 */
	task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_DR_DFP);
	msleep(1);
	/*
	 * Expect the pi3usb9201 driver to configure CDP host mode and unmask
	 * interrupts.
	 */
	val = PI3USB9201_CDP_HOST_MODE << PI3USB9201_REG_CTRL_1_MODE_SHIFT;
	zassert_equal(pi3usb9201_emul_get_reg(emul, PI3USB9201_REG_CTRL_1),
		      val, NULL);

	/* Pretend that a device has been plugged in. */
	msleep(500);
	pi3usb9201_emul_set_reg(emul, PI3USB9201_REG_HOST_STS,
				PI3USB9201_REG_HOST_STS_DEV_PLUG);
	task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_BC12);
	msleep(1);
	/* Expect the pi3usb9201 driver to configure SDP host mode. */
	val = PI3USB9201_SDP_HOST_MODE << PI3USB9201_REG_CTRL_1_MODE_SHIFT;
	zassert_equal(pi3usb9201_emul_get_reg(emul, PI3USB9201_REG_CTRL_1),
		      val, NULL);
	pi3usb9201_emul_set_reg(emul, PI3USB9201_REG_HOST_STS, 0);

	/* Pretend that a device has been unplugged. */
	msleep(500);
	pi3usb9201_emul_set_reg(emul, PI3USB9201_REG_HOST_STS,
				PI3USB9201_REG_HOST_STS_DEV_UNPLUG);
	task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_BC12);
	msleep(1);
	/* Expect the pi3usb9201 driver to configure CDP host mode. */
	val = PI3USB9201_CDP_HOST_MODE << PI3USB9201_REG_CTRL_1_MODE_SHIFT;
	zassert_equal(pi3usb9201_emul_get_reg(emul, PI3USB9201_REG_CTRL_1),
		      val, NULL);
	pi3usb9201_emul_set_reg(emul, PI3USB9201_REG_HOST_STS, 0);

	/*
	 * Pretend that the USB-C Port Manager (TCPMv2) has set the port data
	 * role to UFP and decided charging from the port is allowed.
	 */
	msleep(500);
	task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_DR_UFP);
	charge_manager_update_dualrole(USBC_PORT_C0, CAP_DEDICATED);
	msleep(1);
	/*
	 * Expect the pi3usb9201 driver to configure client mode and start
	 * detection.
	 */
	val = PI3USB9201_CLIENT_MODE << PI3USB9201_REG_CTRL_1_MODE_SHIFT;
	zassert_equal(pi3usb9201_emul_get_reg(emul, PI3USB9201_REG_CTRL_1),
		      val, NULL);
	zassert_equal(pi3usb9201_emul_get_reg(emul, PI3USB9201_REG_CTRL_2),
		      PI3USB9201_REG_CTRL_2_START_DET, NULL);

	/* Pretend that detection completed. */
	msleep(500);
	pi3usb9201_emul_set_reg(emul, PI3USB9201_REG_CLIENT_STS, 1 << CHG_CDP);
	task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_BC12);
	msleep(1);
	/* Expect the pi3usb9201 driver to clear the start bit. */
	zassert_equal(pi3usb9201_emul_get_reg(emul, PI3USB9201_REG_CTRL_2),
		      0, NULL);
	pi3usb9201_emul_set_reg(emul, PI3USB9201_REG_CLIENT_STS, 0);
	/*
	 * Expect the charge manager to select the detected BC1.2 CDP supplier.
	 */
	zassert_equal(charge_manager_get_active_charge_port(),
		      USBC_PORT_C0, NULL);
	zassert_equal(charge_manager_get_supplier(),
		      CHARGE_SUPPLIER_BC12_CDP, NULL);
	zassert_equal(charge_manager_get_charger_current(),
		      USB_CHARGER_MAX_CURR_MA, NULL);
	zassert_equal(charge_manager_get_charger_voltage(),
		      USB_CHARGER_VOLTAGE_MV, NULL);

	/*
	 * Pretend that the USB-C Port Manager (TCPMv2) has set the port data
	 * role to disconnected.
	 */
	msleep(500);
	task_set_event(TASK_ID_USB_CHG_P0, USB_CHG_EVENT_CC_OPEN);
	msleep(1);
	/*
	 * Expect the pi3usb9201 driver to configure power down mode and mask
	 * interrupts.
	 */
	val = PI3USB9201_POWER_DOWN << PI3USB9201_REG_CTRL_1_MODE_SHIFT;
	val |= PI3USB9201_REG_CTRL_1_INT_MASK;
	zassert_equal(pi3usb9201_emul_get_reg(emul, PI3USB9201_REG_CTRL_1),
		      val, NULL);
	/* Expect the charge manager to have no active supplier. */
	zassert_equal(charge_manager_get_active_charge_port(),
		      CHARGE_PORT_NONE, NULL);
	zassert_equal(charge_manager_get_supplier(),
		      CHARGE_SUPPLIER_NONE, NULL);
	zassert_equal(charge_manager_get_charger_current(), 0, NULL);
	zassert_equal(charge_manager_get_charger_voltage(), 0, NULL);
}

void test_suite_bc12(void)
{
	ztest_test_suite(bc12,
			 ztest_user_unit_test(test_bc12_pi3usb9201));
	ztest_run_test_suite(bc12);
}
