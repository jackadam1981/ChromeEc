#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include <ap_power/ap_power_events.h>
#include <charge_manager.h>
#include <extpower.h>
#include <keyboard_protocol.h>
#include <nissa_hdmi.h>
#include <system.h>
#include <usb_pd.h>
#include <usb_pd_tcpm.h>

LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

extern const struct ec_response_keybd_config nereid_kb_legacy;

FAKE_VOID_FUNC(nissa_configure_hdmi_rails);
FAKE_VOID_FUNC(nissa_configure_hdmi_vcc);
FAKE_VALUE_FUNC(int, cbi_get_board_version, uint32_t *);

FAKE_VALUE_FUNC(enum ec_error_list, sm5803_is_acok, int, bool *);
FAKE_VALUE_FUNC(bool, sm5803_check_vbus_level, int, enum vbus_level);
FAKE_VOID_FUNC(sm5803_disable_low_power_mode, int);
FAKE_VOID_FUNC(sm5803_enable_low_power_mode, int);
FAKE_VALUE_FUNC(enum ec_error_list, sm5803_vbus_sink_enable, int, int);
FAKE_VALUE_FUNC(enum ec_error_list, sm5803_set_vbus_disch, int, int);
FAKE_VOID_FUNC(sm5803_hibernate, int);

FAKE_VALUE_FUNC(enum ec_error_list, charger_set_otg_current_voltage, int, int);
FAKE_VALUE_FUNC(enum ec_error_list, charger_enable_otg_power, int, int);
FAKE_VALUE_FUNC(int, charger_is_sourcing_otg_power, int);
FAKE_VOID_FUNC(extpower_handle_update, int);
FAKE_VOID_FUNC(charger_discharge_on_ac, int);
FAKE_VALUE_FUNC(int, charge_manager_get_active_charge_port);

uint8_t board_get_charger_chip_count(void)
{
	return 2;
}

static void test_before(void *fixture)
{
	RESET_FAKE(nissa_configure_hdmi_rails);
	RESET_FAKE(nissa_configure_hdmi_vcc);
	RESET_FAKE(cbi_get_board_version);

	RESET_FAKE(sm5803_is_acok);
	RESET_FAKE(sm5803_check_vbus_level);
	RESET_FAKE(sm5803_disable_low_power_mode);
	RESET_FAKE(sm5803_enable_low_power_mode);
	RESET_FAKE(sm5803_vbus_sink_enable);
	RESET_FAKE(sm5803_set_vbus_disch);
	RESET_FAKE(sm5803_hibernate);

	RESET_FAKE(charger_set_otg_current_voltage);
	RESET_FAKE(charger_enable_otg_power);
	RESET_FAKE(charger_is_sourcing_otg_power);
	RESET_FAKE(extpower_handle_update);
}

ZTEST_SUITE(nereid, NULL, NULL, test_before, NULL, NULL);

ZTEST(nereid, test_keyboard_config)
{
	zassert_equal_ptr(board_vivaldi_keybd_config(), &nereid_kb_legacy);
}

static int cbi_get_board_version_1(uint32_t *version)
{
	*version = 1;
	return 0;
}

static int cbi_get_board_version_2(uint32_t *version)
{
	*version = 2;
	return 0;
}

ZTEST(nereid, test_hdmi_power)
{
	/* Board version less than 2 configures both */
	cbi_get_board_version_fake.custom_fake = cbi_get_board_version_1;
	nissa_configure_hdmi_power_gpios();
	zassert_equal(nissa_configure_hdmi_vcc_fake.call_count, 1);
	zassert_equal(nissa_configure_hdmi_rails_fake.call_count, 1);

	/* Later versions only enable core rails */
	cbi_get_board_version_fake.custom_fake = cbi_get_board_version_2;
	nissa_configure_hdmi_power_gpios();
	zassert_equal(nissa_configure_hdmi_vcc_fake.call_count, 1);
	zassert_equal(nissa_configure_hdmi_rails_fake.call_count, 2);
}

static enum ec_error_list sm5803_is_acok_fake_no(int chgnum, bool *acok)
{
	*acok = false;
	return EC_SUCCESS;
}

static enum ec_error_list sm5803_is_acok_fake_yes(int chgnum, bool *acok)
{
	*acok = true;
	return EC_SUCCESS;
}

ZTEST(nereid, test_extpower_is_present)
{
	/* Errors are not-OK */
	sm5803_is_acok_fake.return_val = EC_ERROR_UNKNOWN;
	zassert_false(extpower_is_present());
	zassert_equal(sm5803_is_acok_fake.call_count, 2);

	/* When neither charger is connected, we check both and return no. */
	sm5803_is_acok_fake.custom_fake = sm5803_is_acok_fake_no;
	zassert_false(extpower_is_present());
	zassert_equal(sm5803_is_acok_fake.call_count, 4);

	/* If one is connected, AC is present */
	sm5803_is_acok_fake.custom_fake = sm5803_is_acok_fake_yes;
	zassert_true(extpower_is_present());
	zassert_equal(sm5803_is_acok_fake.call_count, 5);
}

ZTEST(nereid, test_board_check_extpower)
{
	/* Initial state is stable not-present */
	sm5803_is_acok_fake.custom_fake = sm5803_is_acok_fake_no;
	board_check_extpower();
	RESET_FAKE(extpower_handle_update);

	/* Unchanged state does nothing */
	board_check_extpower();
	zassert_equal(extpower_handle_update_fake.call_count, 0);

	/* Changing the state triggers extpower_handle_update() */
	sm5803_is_acok_fake.custom_fake = sm5803_is_acok_fake_yes;
	board_check_extpower();
	zassert_equal(extpower_handle_update_fake.call_count, 1);
}

ZTEST(nereid, test_board_hibernate)
{
	board_hibernate();
	zassert_equal(sm5803_hibernate_fake.call_count, 2);
}

ZTEST(nereid, test_board_vconn_control)
{
	const struct gpio_dt_spec *cc1 = GPIO_DT_FROM_NODELABEL(gpio_en_usb_c0_cc1_vconn);
	const struct gpio_dt_spec *cc2 = GPIO_DT_FROM_NODELABEL(gpio_en_usb_c0_cc2_vconn);

	/* Both off initially */
	gpio_pin_set_dt(cc1, 0);
	gpio_pin_set_dt(cc2, 0);

	/* Port 1 isn't managed through this function */
	board_pd_vconn_ctrl(1, USBPD_CC_PIN_1, 1);
	zassert_false(gpio_emul_output_get(cc1->port, cc1->pin));

	/* We can enable or disable CC1 */
	board_pd_vconn_ctrl(0, USBPD_CC_PIN_1, 1);
	zassert_true(gpio_emul_output_get(cc1->port, cc1->pin));
	board_pd_vconn_ctrl(0, USBPD_CC_PIN_1, 0);
	zassert_false(gpio_emul_output_get(cc1->port, cc1->pin));

	/* .. or CC2 */
	board_pd_vconn_ctrl(0, USBPD_CC_PIN_2, 1);
	zassert_true(gpio_emul_output_get(cc2->port, cc2->pin));
	board_pd_vconn_ctrl(0, USBPD_CC_PIN_2, 0);
	zassert_false(gpio_emul_output_get(cc2->port, cc2->pin));
}

ZTEST(nereid, test_pd_check_vbus_level)
{
	/* pd_check_vbus_level delegates directly to sm5803_check_vbus_level */
	pd_check_vbus_level(1, VBUS_PRESENT);
	zassert_equal(sm5803_check_vbus_level_fake.call_count, 1);
	zassert_equal(sm5803_check_vbus_level_fake.arg0_val, 1);
	zassert_equal(sm5803_check_vbus_level_fake.arg1_val, VBUS_PRESENT);
}

ZTEST(nereid, test_chargers_suspend)
{
	ap_power_ev_send_callbacks(AP_POWER_RESUME);
	zassert_equal(sm5803_disable_low_power_mode_fake.call_count, 2);

	ap_power_ev_send_callbacks(AP_POWER_SUSPEND);
	zassert_equal(sm5803_enable_low_power_mode_fake.call_count, 2);
}

ZTEST(nereid, test_set_active_charge_port)
{
	/* Asking for an invalid port is an error */
	zassert_equal(board_set_active_charge_port(3), EC_ERROR_INVAL);

	/* A port that's sourcing won't sink */
	charger_is_sourcing_otg_power_fake.return_val = true;
	zassert_equal(board_set_active_charge_port(0), EC_ERROR_INVAL);
	RESET_FAKE(charger_is_sourcing_otg_power);

	/* Enabling a port disables the other one then enables it */
	charge_manager_get_active_charge_port_fake.return_val = 1;
	zassert_ok(board_set_active_charge_port(0));
	zassert_equal(sm5803_vbus_sink_enable_fake.call_count, 2);
	zassert_equal(sm5803_vbus_sink_enable_fake.arg0_history[0], 1);
	zassert_equal(sm5803_vbus_sink_enable_fake.arg1_history[0], 0);
	zassert_equal(sm5803_vbus_sink_enable_fake.arg0_history[1], 0);
	zassert_equal(sm5803_vbus_sink_enable_fake.arg1_history[1], 1);
	/* It also temporarily requested discharge on AC */
	zassert_equal(charger_discharge_on_ac_fake.call_count, 2);
	zassert_equal(charger_discharge_on_ac_fake.arg0_history[0], 1);
	zassert_equal(charger_discharge_on_ac_fake.arg0_history[1], 0);
	RESET_FAKE(charger_discharge_on_ac);

	/* Requesting no port skips the enable step */
	RESET_FAKE(sm5803_vbus_sink_enable);
	zassert_ok(board_set_active_charge_port(CHARGE_PORT_NONE));
	zassert_equal(sm5803_vbus_sink_enable_fake.call_count, 2);
	zassert_equal(sm5803_vbus_sink_enable_fake.arg0_history[0], 0);
	zassert_equal(sm5803_vbus_sink_enable_fake.arg1_history[0], 0);
	zassert_equal(sm5803_vbus_sink_enable_fake.arg0_history[1], 1);
	zassert_equal(sm5803_vbus_sink_enable_fake.arg1_history[1], 0);

	/* Errors bubble up */
	sm5803_vbus_sink_enable_fake.return_val = EC_ERROR_UNKNOWN;
	zassert_equal(board_set_active_charge_port(0), EC_ERROR_UNKNOWN);
}