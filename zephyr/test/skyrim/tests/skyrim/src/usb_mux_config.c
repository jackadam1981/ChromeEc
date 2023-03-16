/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "cros_cbi.h"
#include "emul/retimer/emul_anx7483.h"
#include "usbc/usb_muxes.h"
#include "test/usb_mux_config.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define ANX7483_EMUL0 EMUL_DT_GET(DT_NODELABEL(anx7483_port0))
#define ANX7483_EMUL1 EMUL_DT_GET(DT_NODELABEL(anx7483_port1))

int board_anx7483_c0_mux_set(const struct usb_mux *me, mux_state_t mux_state);
int board_anx7483_c1_mux_set(const struct usb_mux *me, mux_state_t mux_state);
int board_c1_ps8818_mux_set(const struct usb_mux *me, mux_state_t mux_state);
void setup_mux(void);

extern const struct anx7483_tuning_set anx7483_usb_enabled[];
extern const struct anx7483_tuning_set anx7483_dp_enabled[];
extern const struct anx7483_tuning_set anx7483_dock_noflip[];
extern const struct anx7483_tuning_set anx7483_dock_flip[];

extern const size_t anx7483_usb_enabled_count;
extern const size_t anx7483_dp_enabled_count;
extern const size_t anx7483_dock_noflip_count;
extern const size_t anx7483_dock_flip_count;

FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config, enum cbi_fw_config_field_id,
		uint32_t *);

static bool alt_retimer;
static int cros_cbi_get_fw_config_mock(enum cbi_fw_config_field_id field_id,
				       uint32_t *value)
{
	if (field_id != FW_IO_DB)
		return -EINVAL;

	*value = alt_retimer ? FW_IO_DB_PS8811_PS8818 : FW_IO_DB_NONE_ANX7483;
	return 0;
}

static void usb_mux_config_before(void *fixture)
{
	ARG_UNUSED(fixture);
	RESET_FAKE(cros_cbi_get_fw_config);

	cros_cbi_get_fw_config_fake.custom_fake = cros_cbi_get_fw_config_mock;
}

ZTEST_SUITE(usb_mux_config, NULL, NULL, usb_mux_config_before, NULL, NULL);

ZTEST(usb_mux_config, test_board_anx7483_c0_mux_set)
{
	int rv;
	struct usb_mux mux = {
		.usb_port = 0,
		.i2c_port = I2C_PORT_NODELABEL(i2c0_0),
		.i2c_addr_flags = 0x3e,
	};

	rv = board_anx7483_c0_mux_set(&mux, USB_PD_MUX_USB_ENABLED);
	zassert_ok(rv);
	rv = anx7483_emul_validate_tuning(ANX7483_EMUL0, anx7483_usb_enabled,
					  anx7483_usb_enabled_count);
	zexpect_ok(rv);

	rv = board_anx7483_c0_mux_set(&mux, USB_PD_MUX_DP_ENABLED);
	zassert_ok(rv);
	rv = anx7483_emul_validate_tuning(ANX7483_EMUL0, anx7483_dp_enabled,
					  anx7483_dp_enabled_count);
	zexpect_ok(rv);

	rv = board_anx7483_c0_mux_set(&mux, USB_PD_MUX_DOCK);
	zassert_ok(rv);
	rv = anx7483_emul_validate_tuning(ANX7483_EMUL0, anx7483_dock_noflip,
					  anx7483_dock_noflip_count);
	zexpect_ok(rv);

	rv = board_anx7483_c0_mux_set(
		&mux, USB_PD_MUX_DOCK | USB_PD_MUX_POLARITY_INVERTED);
	zassert_ok(rv);
	zassert_ok(rv);
	rv = anx7483_emul_validate_tuning(ANX7483_EMUL0, anx7483_dock_flip,
					  anx7483_dock_flip_count);
	zexpect_ok(rv);
}

ZTEST(usb_mux_config, test_board_anx7483_c1_mux_set)
{
	int rv;
	enum anx7483_eq_setting eq;

	struct usb_mux mux = {
		.usb_port = 1,
		.i2c_port = I2C_PORT_NODELABEL(i2c1_0),
		.i2c_addr_flags = 0x3e,
	};

	/* Test USB mux state. */
	rv = board_anx7483_c1_mux_set(&mux, USB_PD_MUX_USB_ENABLED);
	zassert_ok(rv);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_URX1, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_URX2, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_DRX1, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_DRX2, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	/* Test DP mux state. */
	rv = board_anx7483_c1_mux_set(&mux, USB_PD_MUX_DP_ENABLED);
	zassert_ok(rv);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_URX1, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_URX2, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_UTX1, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_UTX2, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	/* Test dock mux state. */
	rv = board_anx7483_c1_mux_set(&mux, USB_PD_MUX_DOCK);
	zassert_ok(rv);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_URX1, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_URX2, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_DRX1, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_UTX2, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	/* Test flipped dock mux state. */
	rv = board_anx7483_c1_mux_set(
		&mux, USB_PD_MUX_DOCK | USB_PD_MUX_POLARITY_INVERTED);
	zassert_ok(rv);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_URX1, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_URX2, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_UTX1, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);

	rv = anx7483_emul_get_eq(ANX7483_EMUL1, ANX7483_PIN_DRX2, &eq);
	zassert_ok(rv);
	zassert_equal(eq, ANX7483_EQ_SETTING_12_5DB);
}

ZTEST(usb_mux_config, board_c1_ps8818_mux_set)
{
	const struct gpio_dt_spec *c1 =
		GPIO_DT_FROM_NODELABEL(gpio_usb_c1_in_hpd);
	struct usb_mux mux = {
		.usb_port = 1,
	};
	int rv;

	rv = board_c1_ps8818_mux_set(&mux, 0);
	zassert_ok(rv);
	zassert_false(gpio_emul_output_get(c1->port, c1->pin));

	rv = board_c1_ps8818_mux_set(&mux, USB_PD_MUX_DP_ENABLED);
	zassert_ok(rv);
	zassert_true(gpio_emul_output_get(c1->port, c1->pin));
}

ZTEST(usb_mux_config, test_setup_mux)
{
	alt_retimer = false;
	setup_mux();
	zassert_equal(usb_mux_enable_alternative_fake.call_count, 0);

	alt_retimer = true;
	setup_mux();
	zassert_equal(usb_mux_enable_alternative_fake.call_count, 1);
}
