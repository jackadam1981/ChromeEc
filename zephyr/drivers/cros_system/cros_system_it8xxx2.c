/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/cros_system.h>
#include <logging/log.h>
#include <soc.h>

LOG_MODULE_REGISTER(cros_system, LOG_LEVEL_ERR);

static const char *cros_system_it8xxx2_get_chip_vendor(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "ite";
}

static uint32_t system_get_chip_id(void)
{
#ifdef IT83XX_CHIP_ID_3BYTES
	return (IT83XX_GCTRL_CHIPID1 << 16) | (IT83XX_GCTRL_CHIPID2 << 8) |
		IT83XX_GCTRL_CHIPID3;
#else
	return (IT83XX_GCTRL_CHIPID1 << 8) | IT83XX_GCTRL_CHIPID2;
#endif
}

static uint8_t system_get_chip_version(void)
{
	/* bit[3-0], chip version */
	return IT83XX_GCTRL_CHIPVER & 0x0F;
}

static char to_hex(int x)
{
	if (x >= 0 && x <= 9)
		return '0' + x;
	return 'a' + x - 10;
}

static const char *cros_system_it8xxx2_get_chip_name(const struct device *dev)
{
	ARG_UNUSED(dev);

	static char buf[8] = {'i', 't'};
	int num = (IS_ENABLED(IT83XX_CHIP_ID_3BYTES) ? 4 : 3);
	uint32_t chip_id = system_get_chip_id();

	for (int n = 2; num >= 0; n++, num--)
		buf[n] = to_hex(chip_id >> (num * 4) & 0xF);

	return buf;
}

static const char *cros_system_it8xxx2_get_chip_revision(const struct device
							 *dev)
{
	ARG_UNUSED(dev);

	static char buf[3];
	uint8_t rev = system_get_chip_version();

	buf[0] = to_hex(rev + 0xa);
	buf[1] = 'x';
	buf[2] = '\0';

	return buf;
}

static int cros_system_it8xxx2_get_reset_cause(const struct device *dev)
{
	ARG_UNUSED(dev);

	uint8_t raw_reset_cause = IT83XX_GCTRL_RSTS & 0x03;
	//uint8_t raw_reset_cause2 = IT83XX_GCTRL_SPCTRL4 & 0x07;
	/* TODO */

	/* Clear reset cause. */
	IT83XX_GCTRL_RSTS |= 0x03;
	IT83XX_GCTRL_SPCTRL4 |= 0x07;

	return raw_reset_cause;
}

static int cros_system_it8xxx2_init(const struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static int cros_system_it8xxx2_soc_reset(const struct device *dev)
{

	/*
	 * Writing invalid key to watchdog module triggers a soft or hardware
	 * reset. It depends on the setting of bit0 at ETWDUARTCR register.
	 */
	IT83XX_ETWD_ETWCFG |= 0x20;
	IT83XX_ETWD_EWDKEYR = 0x00;

	/* Spin and wait for reboot; should never return */
	while (1) {
		;
	}

	/* should never return */
	return 0;
}

static int cros_system_it8xxx2_hibernate(const struct device *dev,
					 uint32_t seconds,
					 uint32_t microseconds)
{
	/*
	 * TODO(b:):
	 */
	/* chip specific standby mode */
	//__enter_hibernate(seconds, microseconds);

	return 0;
}

static const struct cros_system_driver_api cros_system_driver_it8xxx2_api = {
	.get_reset_cause = cros_system_it8xxx2_get_reset_cause,
	.soc_reset = cros_system_it8xxx2_soc_reset,
	.hibernate = cros_system_it8xxx2_hibernate,
	.chip_vendor = cros_system_it8xxx2_get_chip_vendor,
	.chip_name = cros_system_it8xxx2_get_chip_name,
	.chip_revision = cros_system_it8xxx2_get_chip_revision,
};

DEVICE_DEFINE(cros_system_it8xxx2_0, "CROS_SYSTEM", cros_system_it8xxx2_init,
	      NULL, NULL, NULL, PRE_KERNEL_1,
	      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
	      &cros_system_driver_it8xxx2_api);

