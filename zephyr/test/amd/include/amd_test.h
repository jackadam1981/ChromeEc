/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/devicetree.h>

/* All emulated GPIOS are on one device */
#define GPIO_DEVICE \
	DEVICE_DT_GET(DT_GPIO_CTLR(NAMED_GPIOS_GPIO_NODE(s0_pgood), gpios))
#define SLP_S3_PIN DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(slp_s3_l), gpios)
#define SLP_S5_PIN DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(slp_s5_l), gpios)
#define PGOOD_S0_PIN DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(s0_pgood), gpios)
#define PGOOD_S5_PIN DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(pg_pwr_s5), gpios)
#define PWRBTN_IN_PIN \
	DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(mech_pwr_btn_odl), gpios)
#define PWRBTN_OUT_PIN \
	DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(ec_soc_pwr_btn_l), gpios)
#define PROCHOT_PIN DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(prochot_odl), gpios)
#define LID_PIN DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(lid_open_ec), gpios)
#define STB_OUT_PIN DT_GPIO_PIN(NAMED_GPIOS_GPIO_NODE(ec_sfh_int_h), gpios)
