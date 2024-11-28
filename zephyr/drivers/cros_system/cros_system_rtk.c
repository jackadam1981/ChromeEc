/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "drivers/cros_system.h"
#include "system.h"

#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/drivers/watchdog.h>

#include "stdint.h"
#include "reg/reg_system.h"
#include "reg/reg_wdt.h"
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(cros_system, LOG_LEVEL_ERR);

#undef IS_BIT_SET
#define IS_BIT_SET(reg, bit) (((reg) >> (bit)) & (0x1))

#define RTK_SCCON_REG_BASE	((SYSTEM_Type *)(DT_REG_ADDR(DT_NODELABEL(sccon))))
#define RTK_WDT_REG_BASE	((WDT_Type *)(DT_REG_ADDR(DT_NODELABEL(wdog))))
#define RTK_VIVO_BACKUP0_REG	*((uint32_t*)0x40104ff8)
#define RTK_VIVO_BACKUP1_REG	*((uint32_t*)0x40104ffc)

/* Driver data */
struct cros_system_rtk_data {
	int reset; /* reset cause */
};

/* Driver convenience defines */
#define DRV_CONFIG(dev) ((const struct cros_system_rtk_config *)(dev)->config)
#define DRV_DATA(dev) ((struct cros_system_rtk_data *)(dev)->data)

static const struct device *const watchdog =
	DEVICE_DT_GET(DT_CHOSEN(cros_ec_watchdog));

/* Soc specific system local functions */
static int system_rtk_watchdog_stop(void)
{
	if (IS_ENABLED(CONFIG_WATCHDOG)) {
		const struct device *wdt_dev =
			DEVICE_DT_GET(DT_NODELABEL(wdog));
		if (!device_is_ready(wdt_dev)) {
			LOG_ERR("device %s not ready", wdt_dev->name);
			return -ENODEV;
		}

		wdt_disable(wdt_dev);
	}

	return 0;
}

static const char *cros_system_rtk_get_chip_vendor(const struct device *dev)
{
	ARG_UNUSED(dev);

	return "rtk";
}

static uint32_t system_get_chip_id(void)
{
	return 0x5915;
}

static uint8_t system_get_chip_version(void)
{
	return 0xB;
}

static const char *cros_system_rtk_get_chip_name(const struct device *dev)
{
	ARG_UNUSED(dev);

	static char buf[8] = { 'r', 't', 's' };
	uint32_t chip_id = system_get_chip_id();
	int num = 4;

	for (int n = 3; num >= 0; n++, num--)
		snprintf(buf + n, (sizeof(buf) - n), "%x",
			 chip_id >> (num * 4) & 0xF);

	return buf;
}

static const char *
cros_system_rtk_get_chip_revision(const struct device *dev)
{
	ARG_UNUSED(dev);

	static char buf[3];
	uint8_t rev = system_get_chip_version();

	snprintf(buf, sizeof(buf), "%cx", rev + 'a');

	return buf;
}

static int cros_system_rtk_get_reset_cause(const struct device *dev)
{
	struct cros_system_rtk_data *data = DRV_DATA(dev);

	return data->reset;
}

static int cros_system_rtk_init(const struct device *dev)
{
	struct cros_system_rtk_data *data = DRV_DATA(dev);
	WDT_Type *wdt_reg = RTK_WDT_REG_BASE;
	uint32_t vivo_reg0 = RTK_VIVO_BACKUP0_REG;
	uint32_t vivo_reg1 = RTK_VIVO_BACKUP1_REG;
	/* check reset cause */
	data->reset = UNKNOWN_RST;

	/* is the WDT reset */
	if (IS_BIT_SET(wdt_reg->STS, WDT_STS_RSTFLAG_Pos)) {
		data->reset = WATCHDOG_RST;
		/* Clear watchdog reset status initially */
		wdt_reg->CTRL |= BIT(WDT_CTRL_CLRRSTFLAG_Pos);
	}
	else if ((vivo_reg0^vivo_reg1) == UINT32_MAX) {
		/* VIN3 (GPIO115) connect to power button */
		if(vivo_reg1 & BIT(SYSTEM_VIVOCTRL_VIN3STS_Pos)) {
			data->reset = POWERUP;
		}
	}
	return 0;
}

static int cros_system_rtk_soc_reset(const struct device *dev)
{
	ARG_UNUSED(dev);

	WDT_Type *wdt_reg = RTK_WDT_REG_BASE;

	/* Disable interrupts to avoid task swaps during reboot */
	interrupt_disable_all();

	/*
	 * Set minimal watchdog timeout - 10 millisecond.
	 * RTK WDT can be set for lower value, but we are limited by
	 * Zephyr API.
	 */
	struct wdt_timeout_cfg minimal_timeout = { .window.max = 10 };
	/* Setup watchdog */
	wdt_install_timeout(watchdog, &minimal_timeout);
	/* Apply the changes (the driver will reload watchdog) */
	// wdt_setup(watchdog, 0);
	wdt_reg->CTRL = (WDT_CTRL_RSTEN_Msk);
	wdt_reg->CTRL |= WDT_CTRL_EN_Msk;
	/* Spin and wait for reboot */
	while (1)
		;

	/* Should never return */
	return 0;
}

/*
 * Fake wake ISR handler, needed for pins that do not have a handler.
 */
void wake_isr(enum gpio_signal signal)
{
}

static int cros_system_rtk_hibernate(const struct device *dev,
					 uint32_t seconds,
					 uint32_t microseconds)
{
	/* Disable interrupt first */
	interrupt_disable_all();
	/* Stop the watchdog */
	system_rtk_watchdog_stop();

	return 0;
}

static const struct cros_system_driver_api cros_system_driver_rtk_api = {
	.get_reset_cause = cros_system_rtk_get_reset_cause,
	.soc_reset = cros_system_rtk_soc_reset,
	.hibernate = cros_system_rtk_hibernate,
	.chip_vendor = cros_system_rtk_get_chip_vendor,
	.chip_name = cros_system_rtk_get_chip_name,
	.chip_revision = cros_system_rtk_get_chip_revision,
};
#if CONFIG_CROS_SYSTEM_REALTEK_INIT_PRIORITY >= \
	CONFIG_PLATFORM_EC_SYSTEM_PRE_INIT_PRIORITY
#error "CROS_SYSTEM must initialize before the SYSTEM_PRE initialization"
#endif
static struct cros_system_rtk_data cros_system_rtk_data_0;
DEVICE_DEFINE(cros_system_rtk_0, "CROS_SYSTEM", cros_system_rtk_init,
	      NULL, &cros_system_rtk_data_0, NULL, PRE_KERNEL_1,
	      CONFIG_CROS_SYSTEM_REALTEK_INIT_PRIORITY,
	      &cros_system_driver_rtk_api);
