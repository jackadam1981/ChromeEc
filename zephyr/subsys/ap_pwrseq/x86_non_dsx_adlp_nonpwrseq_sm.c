/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <x86_non_dsx_common_pwrseq_sm_handler.h>
#include <x86_non_dsx_adlp_pwrseq_sm.h>
#include "drivers/sensor.h"
#include <sys/atomic.h>

LOG_MODULE_DECLARE(ap_pwrseq, 4);

#define pp3300_s5_on_dev DEVICE_DT_GET(DT_NODELABEL(cmp_pp3300_s5_high))
#define pp3300_s5_off_dev DEVICE_DT_GET(DT_NODELABEL(cmp_pp3300_s5_low))

#define pp1p05_on_dev DEVICE_DT_GET(DT_NODELABEL(cmp_pp1p05_high))
#define pp1p05_off_dev DEVICE_DT_GET(DT_NODELABEL(cmp_pp1p05_low))

atomic_t pp3300_a_pgood = 0;
atomic_t pp1p05_pgood = 0;

extern const struct chipset_pwrseq_config chip_cfg;

void g3s5_action_handler(void)
{
	enable_power_rail(GPIO_NET_NAME(EC_VR_EN_PP5000_A), 1);
	/*Is there a delay?*/
	enable_power_rail(GPIO_NET_NAME(EC_VR_EN_PP3300_A), 1);

	power_wait_signals(IN_PGOOD_ALL_CORE);

	dsw_pwrok_pass_thru_handler();
}

int generate_pch_pwrok_handler(void)
{
	int pch_pok;

	/* Enable PCH_PWROK, gated by VRRDY. */
	pch_pok = gpio_get_lvl(GPIO_NET_NAME(PCH_PWROK));
	if (pch_pok == 0) {
		k_msleep(chip_cfg.pch_pwrok_delay_ms);
		gpio_set_lvl(GPIO_NET_NAME(PCH_PWROK), 1);
		LOG_DBG("Set PCH_PWROK\n");
	}

	return 0;
}

int intel_x86_get_pg_ec_dsw_pwrok(void)
{
	/*
	 * The PP3300_A rail is an input to generate DPWROK.  Assuming that
	 * power is good if voltage is at least 80% of nominal level.  We cannot
	 * read the ADC values during an interrupt, therefore, this power good
	 * value is updated via ADC threshold interrupts.
	 */
	return pp3300_a_pgood;
}

void dsw_pwrok_pass_thru_handler(void)
{
	int in_sig_val =  intel_x86_get_pg_ec_dsw_pwrok();

	if (in_sig_val != gpio_get_lvl(GPIO_NET_NAME(EC_PCH_DSW_PWROK))) {
		if (in_sig_val)
			k_msleep(10);

		gpio_set_lvl(GPIO_NET_NAME(EC_PCH_DSW_PWROK), in_sig_val);
	}
}

int intel_x86_get_pg_ec_all_sys_pwrgd(void)
{
	if (power_has_signals(IN_PCH_SLP_S3_DEASSERTED) == 0) {
		LOG_DBG("SLP_S3 is 0");
		return 0;
	}
	if (gpio_get_lvl(GPIO_NET_NAME(VR_EC_ALL_SYS_PWRGD)) == 0) {
		LOG_DBG("ALL_SYS_PWRGD is 0");
		return 0;
	}
	if (pp1p05_pgood == 0){
		LOG_DBG("PP1050_PROC is 0");
		return 0;
	}
	return 1;
}

int power_signal_gpio_is_asserted(const struct power_signal_gpio_info *s)
{
	if (s->power_sig == X86_DSW_PWROK)
		return intel_x86_get_pg_ec_dsw_pwrok();

	if (s->power_sig == X86_ALL_SYS_PGOOD)
		return intel_x86_get_pg_ec_all_sys_pwrgd();

	return gpio_get_lvl(s->net_name) ==
		!!(s->flags & POWER_SIGNAL_ACTIVE_STATE);
}

static void pp3300_a_pgood_cb(const struct device *dev, const struct sensor_trigger *trigger)
{
	struct sensor_value val;

	if (dev == pp3300_s5_on_dev) {
		atomic_set_bit(&pp3300_a_pgood, 0);

		/* Disable this interrupt while it's asserted. */
		val.val1 = false;
		sensor_attr_set(pp3300_s5_on_dev, SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT, &val);

		/* Enable the voltage low interrupt. */
		val.val1 = true;
		sensor_attr_set(pp3300_s5_off_dev, SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT, &val);
	}
	else if (dev == pp3300_s5_off_dev) {
		atomic_clear_bit(&pp3300_a_pgood, 0);

		/* Disable this interrupt while it's asserted. */
		val.val1 = false;
		sensor_attr_set(pp3300_s5_off_dev, SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT, &val);

		/* Enable the voltage high interrupt. */
		val.val1 = true;
		sensor_attr_set(pp3300_s5_on_dev, SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT, &val);
	}
	power_update_signals();
}

static void pp1p05_good_cb(const struct device *dev, const struct sensor_trigger *trigger)
{
	struct sensor_value val;

	if (dev == pp1p05_on_dev) {
		atomic_set_bit(&pp1p05_pgood, 0);

		/* Disable this interrupt while it's asserted. */
		val.val1 = false;
		sensor_attr_set(pp1p05_on_dev, SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT, &val);

		/* Enable the voltage low interrupt. */
		val.val1 = true;
		sensor_attr_set(pp1p05_off_dev, SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT, &val);
	}
	else if (dev == pp1p05_off_dev) {
		atomic_clear_bit(&pp1p05_pgood, 0);

		/* Disable this interrupt while it's asserted. */
		val.val1 = false;
		sensor_attr_set(pp1p05_off_dev, SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT, &val);

		/* Enable the voltage high interrupt. */
		val.val1 = true;
		sensor_attr_set(pp1p05_on_dev, SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT, &val);
	}
	power_update_signals();
}

static int baseboard_init(const struct device *device)
{

	struct sensor_value val;
	struct sensor_trigger trig = {
		.type = SENSOR_TRIG_THRESHOLD,
		.chan = SENSOR_CHAN_VOLTAGE
	};

	sensor_trigger_set(pp3300_s5_on_dev, &trig, pp3300_a_pgood_cb);
	val.val1 = true;
	sensor_attr_set(pp3300_s5_on_dev, SENSOR_CHAN_VOLTAGE,
		SENSOR_ATTR_ALERT, &val);
	sensor_trigger_set(pp3300_s5_off_dev, &trig, pp3300_a_pgood_cb);

	sensor_trigger_set(pp1p05_on_dev, &trig, pp1p05_good_cb);
	sensor_attr_set(pp1p05_on_dev, SENSOR_CHAN_VOLTAGE,
		SENSOR_ATTR_ALERT, &val);
	sensor_trigger_set(pp1p05_off_dev, &trig, pp1p05_good_cb);
	return 0;
}
SYS_INIT(baseboard_init, POST_KERNEL, 55);
