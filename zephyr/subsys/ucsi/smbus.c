/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/platform.h"
#include "include/smbus.h"
#include "smbus_usermode.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <fcntl.h>
#include "i2c.h"
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(smbus, LOG_LEVEL_INF);

// 10ms timeout for gpiod wakeup
#define GPIOD_WAIT_TIMEOUT_NS (10 * 1000 * 1000)

/**
 * Internal structure for smbus implementation.
 */
struct smbus_device {
	const struct i2c_dt_spec *i2c;
	const struct gpio_dt_spec *irq_gpios;
	struct gpio_callback gpio_cb;
	uint8_t port;
	uint8_t addr;

	volatile bool cleaning_up;
	struct platform_condvar *pdc_condvar;
	struct platform_mutex *gpio_lock;
};

static struct smbus_device smbus_dev;

#define CAST_FROM(v) (struct smbus_device *)(v)

static int smbus_read_byte_nolock(int port, uint8_t addr, uint8_t *buf)
{
	return i2c_xfer_unlocked(port, addr, NULL, 0, buf, 1, I2C_XFER_SINGLE);
}

static int __smbus_read_byte(struct smbus_device *device, uint8_t chip_address)
{
	struct smbus_device *dev = CAST_FROM(device);
	uint8_t buf;
	int rv;

	i2c_lock(dev->port, 1);
	rv = smbus_read_byte_nolock(dev->port, dev->addr, &buf);
	i2c_lock(dev->port, 0);

	if (rv)
		return -1;

	return buf;
}

static int __smbus_read_block(struct smbus_device *device, uint8_t addr,
			    uint8_t cmd, void *data, size_t length)
{
	struct smbus_device *dev = CAST_FROM(device);
	uint8_t read_len;
	int rv;

	// Block read will read at most 32 bytes.
	if (length > SMBUS_MAX_BLOCK_SIZE) {
		ELOG("length (%d) > max (%d) for block read", length,
		     SMBUS_MAX_BLOCK_SIZE);
		return -1;
	}

	i2c_lock(dev->port, 1);

	/* First read the size. */
	rv = i2c_xfer_unlocked(dev->port, addr, &cmd, 1, &read_len, 1,
			       I2C_XFER_START);
	if (rv)
		goto smbus_read_block_done;

	if (read_len > SMBUS_MAX_BLOCK_SIZE) {
		DLOG("Got size (%d) > max (%d) for block read", read_len,
		     SMBUS_MAX_BLOCK_SIZE);
		goto smbus_read_block_done;
	}
	if (read_len > length) {
		DLOG("Got size %d > expected size (%d) for block read",
		     read_len, length);
		goto smbus_read_block_done;
	}

	DLOG("Reading block at 0x%02x (%d bytes) from addr=0x02x port=%d", cmd,
	     read_len, addr, dev->port);

	rv = i2c_xfer_unlocked(dev->port, addr, NULL, 0, data, read_len,
			       I2C_XFER_STOP);

smbus_read_block_done:
	i2c_lock(dev->port, 0);

	return rv;
}

static int __smbus_write_block(struct smbus_device *device, uint8_t addr,
			     uint8_t cmd, void *buf, size_t length)
{
	struct smbus_device *dev = CAST_FROM(device);
	int rv;

	DLOG_START("[0x%02x]: Sending data to %02x [", addr, cmd);
	for (int i = 0; i < length; ++i) {
		DLOG_LOOP("%02x, ", ((uint8_t *)buf)[i]);
	}
	DLOG_END("]");

	i2c_lock(dev->port, 1);
	rv = i2c_xfer_unlocked(dev->port, addr, &cmd, 1, NULL, 0,
			       I2C_XFER_START);
	if (rv)
		goto smbus_write_block_done;
	rv = i2c_xfer_unlocked(dev->port, addr, buf, length, NULL, 0,
			       I2C_XFER_STOP);

smbus_write_block_done:
	i2c_lock(dev->port, 0);

	return rv;
}

static int __smbus_read_ara(struct smbus_device *device, uint8_t ara_address)
{
	struct smbus_device *dev = CAST_FROM(device);
	uint8_t ara_byte_result;
	int rv;

	i2c_lock(dev->port, 1);

	/* Read ARA (0xC). */
	rv = smbus_read_byte_nolock(dev->port, ara_address, &ara_byte_result);
	if (rv)
		rv = -1;
	else
		/* ARA will have 8 bits with top 7 bits of chip address. */
		rv = ara_byte_result >> 1;

	i2c_lock(dev->port, 0);

	return rv;
}

void pdc_interrupt(enum gpio_signal signal)
{
	platform_condvar_signal(smbus_dev.pdc_condvar);
}

static int __smbus_block_for_interrupt(struct smbus_device *device)
{
	struct smbus_device *dev = CAST_FROM(device);
	int ret = 0;
	bool cleaning_up = false;
	struct timespec ts;

	if (dev->cleaning_up) {
		return -1;
	}

	ts.tv_sec = 0;
	ts.tv_nsec = GPIOD_WAIT_TIMEOUT_NS;

	DLOG("Polling for smbus interrupt.");

	do {
		platform_mutex_lock(dev->gpio_lock);

		platform_condvar_wait(dev->pdc_condvar, dev->gpio_lock);
		cleaning_up = dev->cleaning_up;

		platform_mutex_unlock(dev->gpio_lock);

		// If we're cleaning up, exit out with an error.
		if (cleaning_up) {
			ret = -1;
			break;
		}

		// Either error or result will break here. Otherwise, continue.
		if (ret == 1 || ret == -1) {
			break;
		}
	} while (true);

	// Got an event. Clear the event before forwarding interrupt.
	if (ret == 1) {
		DLOG("Got SMBUS interrupt!");
	} else {
		DLOG("Smbus polling resulted in ret %d", ret);
	}

	return ret == 1 ? 0 : -1;
}

static void __smbus_cleanup(struct smbus_driver *driver)
{
	if (driver->dev) {
		struct smbus_device *dev = CAST_FROM(driver->dev);

		dev->cleaning_up = true;
	}
}

static void __interrupt_callback(const struct device *dev,
				 struct gpio_callback *cb, uint32_t pins)
{
	platform_condvar_signal(smbus_dev.pdc_condvar);
}

static int init_interrupt(struct smbus_device *dev)
{
	int rv;

	dev->gpio_lock = platform_mutex_init();
	if (!dev->gpio_lock) {
		ELOG("Failed to init gpio mutex");
		return -1;
	}

	dev->pdc_condvar = platform_condvar_init();
	if (!dev->pdc_condvar) {
		ELOG("Failed to init pdc_condvar");
		return -1;
	}

	rv = gpio_pin_configure_dt(dev->irq_gpios, GPIO_INPUT);
	if (rv < 0) {
		LOG_ERR("Unable to configure GPIO");
		return rv;
	}

	gpio_init_callback(&dev->gpio_cb, __interrupt_callback,
			   BIT(dev->irq_gpios->pin));

	rv = gpio_add_callback(dev->irq_gpios->port, &dev->gpio_cb);
	if (rv < 0) {
		LOG_ERR("Unable to add callback");
		return rv;
	}

	rv = gpio_pin_interrupt_configure_dt(dev->irq_gpios,
					     GPIO_INT_EDGE_FALLING);
	if (rv < 0) {
		LOG_ERR("Unable to configure interrupt");
		return rv;
	}

	return 0;
}

struct smbus_driver *smbus_open(const struct i2c_dt_spec *i2c,
				const struct gpio_dt_spec *irq_gpios)
{
	static struct smbus_driver drv;

	smbus_dev.i2c = i2c;
	smbus_dev.irq_gpios = irq_gpios;
	smbus_dev.port = i2c_get_port_from_device(i2c->bus);
	smbus_dev.addr = i2c->addr;

	// Initialize the gpio lines
	if (init_interrupt(&smbus_dev) == -1) {
		ELOG("Failed to initialize gpio for interrupt.");
		return NULL;
	}

	drv.dev = &smbus_dev;
	drv.read_byte = __smbus_read_byte;
	drv.read_block = __smbus_read_block;
	drv.write_block = __smbus_write_block;
	drv.read_ara = __smbus_read_ara;
	drv.block_for_interrupt = __smbus_block_for_interrupt;
	drv.cleanup = __smbus_cleanup;

	// Make sure chip address is valid before returning.
	if (__smbus_read_byte(&smbus_dev, i2c->addr) < 0) {
		ELOG("Could not read byte at given chip address.");
		return NULL;
	}

	return &drv;
}
