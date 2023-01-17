/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* HyperDebug board configuration */

#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
#include "hwtimer.h"
#include "i2c.h"
#include "queue_policies.h"
#include "registers.h"
#include "spi.h"
#include "task.h"
#include "timer.h"
#include "usart-stm32l5.h"
#include "usb-stream.h"
#include "usb_hw.h"
#include "usb_spi.h"

#include <stdio.h>

#define OCTOSPI_CLOCK (16000000UL)
#define SPI_CLOCK (16000000UL)

/* SPI devices, default to 250 kb/s for all. */
struct spi_device_t spi_devices[] = {
	{ .name = "SPI2",
	  .port = 1,
	  .div = 5,
	  .gpio_cs = GPIO_CN9_25,
	  .usb_flags = USB_SPI_ENABLED },
	{ .name = "QSPI",
	  .port = -1 /* OCTOSPI */,
	  .div = 63,
	  .gpio_cs = GPIO_CN10_6,
	  .usb_flags = USB_SPI_ENABLED | USB_SPI_CUSTOM_SPI_DEVICE },
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

/*
 * Find spi device by name or by number.  Returns an index into spi_devices[],
 * or on error a negative value.
 */
static int find_spi_by_name(const char *name)
{
	int i;
	char *e;
	i = strtoi(name, &e, 0);

	if (!*e && i < spi_devices_used)
		return i;

	for (i = 0; i < spi_devices_used; i++) {
		if (!strcasecmp(name, spi_devices[i].name))
			return i;
	}

	/* SPI device not found */
	return -1;
}

static void print_spi_info(int index)
{
	uint32_t bits_per_second;

	if (spi_devices[index].usb_flags & USB_SPI_CUSTOM_SPI_DEVICE) {
		// OCTOSPI as 8 bit prescaler, dividing clock by 1..256.
		bits_per_second = OCTOSPI_CLOCK / (spi_devices[index].div + 1);
	} else {
		// Other SPIs have prescaler by power of two 2, 4, 8, ..., 256.
		bits_per_second = SPI_CLOCK / (2 << spi_devices[index].div);
	}

	ccprintf("  %d %s %d bps\n", index, spi_devices[index].name,
		 bits_per_second);

	/* Flush console to avoid truncating output */
	cflush();
}

/*
 * Get information about one or all SPI ports.
 */
static int command_spi_info(int argc, const char **argv)
{
	int i;

	/* If a SPI target is specified, print only that one */
	if (argc == 3) {
		int index = find_spi_by_name(argv[2]);
		if (index < 0) {
			ccprintf("SPI device not found\n");
			return EC_ERROR_PARAM2;
		}

		print_spi_info(index);
		return EC_SUCCESS;
	}

	/* Otherwise print them all */
	for (i = 0; i < spi_devices_used; i++) {
		print_spi_info(i);
	}

	return EC_SUCCESS;
}

static int command_spi_set_speed(int argc, const char **argv)
{
	int index;
	uint32_t desired_speed;
	char *e;
	if (argc < 5)
		return EC_ERROR_PARAM_COUNT;

	index = find_spi_by_name(argv[3]);
	if (index < 0)
		return EC_ERROR_PARAM3;

	desired_speed = strtoi(argv[4], &e, 0);
	if (*e)
		return EC_ERROR_PARAM4;

	if (spi_devices[index].usb_flags & USB_SPI_CUSTOM_SPI_DEVICE) {
		/*
		 * Find prescaler value by division, rounding up in order to get
		 * slightly slower speed than requested, if it cannot be matched
		 * exactly.
		 */
		spi_devices[index].div =
			(OCTOSPI_CLOCK + desired_speed - 1) / desired_speed - 1;
		STM32_OCTOSPI_DCR2 = spi_devices[index].div;
	} else {
		int divisor = 7;
		/*
		 * Find the smallest divisor that result in a speed not faster
		 * than what was requested.
		 */
		while (divisor > 0) {
			if (SPI_CLOCK / (2 << (divisor - 1)) > desired_speed) {
				/* One step further would make the clock too
				 * fast, stop here. */
				break;
			}
			divisor--;
		}

		/*
		 * Re-initialize spi controller to apply the new clock divisor.
		 */
		spi_enable(&spi_devices[index], 0);
		spi_devices[index].div = divisor;
		spi_enable(&spi_devices[index], 1);
	}

	return EC_SUCCESS;
}

static int command_spi_set(int argc, const char **argv)
{
	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;
	if (!strcasecmp(argv[2], "speed"))
		return command_spi_set_speed(argc, argv);
	return EC_ERROR_PARAM2;
}

static int command_spi(int argc, const char **argv)
{
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;
	if (!strcasecmp(argv[1], "info"))
		return command_spi_info(argc, argv);
	if (!strcasecmp(argv[1], "set"))
		return command_spi_set(argc, argv);
	return EC_ERROR_PARAM1;
}
DECLARE_CONSOLE_COMMAND_FLAGS(spi, command_spi,
			      "info [PORT]"
			      "\nset speed PORT BPS",
			      "SPI bus manipulation", CMD_FLAG_RESTRICTED);
