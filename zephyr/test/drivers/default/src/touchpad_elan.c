#include "usb_hid_touchpad.h"
#include "test/drivers/test_state.h"
#include "timer.h"
#include "gpio/gpio_int.h"

#include <zephyr/device.h>
#include <zephyr/devicetree/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define TOUCHPAD_INT DT_NODELABEL(gpio_touchpad_int)
#define TOUCHPAD_INT_PORT DEVICE_DT_GET(DT_GPIO_CTLR(TOUCHPAD_INT, gpios))
#define TOUCHPAD_INT_PIN DT_GPIO_PIN(TOUCHPAD_INT, gpios)

FAKE_VOID_FUNC(set_touchpad_report, struct usb_hid_touchpad_report*);
FAKE_VOID_FUNC(board_touchpad_reset);

static void *touchpad_elan_setup(void)
{
	return NULL;
}

static void touchpad_elan_before(void *fixture)
{
	RESET_FAKE(set_touchpad_report);
	RESET_FAKE(board_touchpad_reset);
}

/* verify that interrupt triggers read report */
ZTEST_USER(touchpad_elan, read_report)
{
	gpio_emul_input_set(TOUCHPAD_INT_PORT, TOUCHPAD_INT_PIN, 1);
	gpio_emul_input_set(TOUCHPAD_INT_PORT, TOUCHPAD_INT_PIN, 0);
	msleep(100);

	zassert_equal(set_touchpad_report_fake.call_count, 1);
}

ZTEST_SUITE(touchpad_elan, drivers_predicate_post_main, touchpad_elan_setup, touchpad_elan_before, NULL, NULL);
