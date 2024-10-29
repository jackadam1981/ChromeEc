/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "aardvark.h"
#include "include/platform.h"
#include "include/smbus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fcntl.h>
#include <gpiod.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH_SIZE 32
#define MAX_BLOCK_LEN 0x1000
#define I2C_BITRATE 400

/* 10ms timeout for gpiod wakeup */
#define GPIOD_WAIT_TIMEOUT_NS (10 * 1000 * 1000)
#define GPIOD_CONSUMER "um_ppm"

struct i2c_aardvark_dev {
	/* Aardvark handle. */
	Aardvark fd;

	uint8_t buffer[MAX_BLOCK_LEN + 1];

	pthread_mutex_t cmd_lock;
	pthread_mutex_t gpio_lock;
	struct gpiod_chip *chip;
	struct gpiod_line *line;

	bool cleaning_up;
};

#define CAST_FROM(v) (struct i2c_aardvark_dev *)(v)

int i2c_aardvark_read_byte(struct smbus_device *device, uint8_t chip_address)
{
	struct i2c_aardvark_dev *dev = CAST_FROM(device);
	int ret;
	uint8_t byte = 0;

	pthread_mutex_lock(&dev->cmd_lock);
	ret = aa_i2c_read(dev->fd, chip_address, AA_I2C_SIZED_READ, 1, &byte);
	if (ret < 0) {
		ELOG("[0x%02x]: Read byte failed: %d: %s", chip_address, ret,
		     aa_status_string(ret));
	}
	pthread_mutex_unlock(&dev->cmd_lock);

	return ret >= 0 ? byte : -1;
}

int i2c_aardvark_read_block(struct smbus_device *device, uint8_t chip_address,
			    uint8_t address, void *buf, size_t length)
{
	struct i2c_aardvark_dev *dev = CAST_FROM(device);
	int ret;
	u16 bytes_read = 0;

	if (length > MAX_BLOCK_LEN) {
		ELOG("Got length %d for block read > max %d", length,
		     MAX_BLOCK_LEN);
		return -1;
	}

	pthread_mutex_lock(&dev->cmd_lock);
	DLOG("[0x%02x]: Reading block at 0x%02x", chip_address, address);

	/* First write the address you want to read and then read from I2C.
	 * We ignore result from the first write because we always need to set
	 * stop bit to recover the bus.
	 *
	 * We use AA_I2C_SIZED_READ as the first byte is always the length.
	 */
	ret = aa_i2c_write(dev->fd, chip_address, AA_I2C_NO_STOP, 1, &address);
	if (ret < 0) {
		ELOG("[0x%02x]: Failed to write address before reading: %d",
		     ret);
	}
	ret = aa_i2c_read_ext(dev->fd, chip_address, AA_I2C_SIZED_READ_EXTRA1,
			      length + 1, dev->buffer, &bytes_read);

	if (ret != 0) {
		ELOG("[0x%02x]: Error reading addr 0x%02x: %d: %s",
		     chip_address, address, ret, aa_status_string(ret));
		length = 0;
	} else if (bytes_read < length + 1) {
		DLOG("[0x%02x]: I2C data read is truncated. Expected %d, got %d",
		     chip_address, length + 1, bytes_read);
		length = bytes_read - 1;
	}

	platform_memcpy(buf, &dev->buffer[1], length);

	DLOG_START("[0x%02x]: Reading data from %02x [", chip_address, address);
	for (int i = 0; i < length; ++i) {
		DLOG_LOOP("%02x, ", ((uint8_t *)buf)[i]);
	}
	DLOG_END("]");

	pthread_mutex_unlock(&dev->cmd_lock);
	return length;
}

int i2c_aardvark_write_block(struct smbus_device *device, uint8_t chip_address,
			     uint8_t address, void *buf, size_t length)
{
	struct i2c_aardvark_dev *dev = CAST_FROM(device);
	int ret;

	if (length > MAX_BLOCK_LEN) {
		ELOG("Got length %d for block write, max %d", length,
		     MAX_BLOCK_LEN);
		return -1;
	}

	DLOG_START("[0x%02x]: Sending data to %02x [", chip_address, address);
	for (int i = 0; i < length; ++i) {
		DLOG_LOOP("%02x, ", ((uint8_t *)buf)[i]);
	}
	DLOG_END("]");

	pthread_mutex_lock(&dev->cmd_lock);

	dev->buffer[0] = address;
	dev->buffer[1] = length;
	platform_memcpy(&dev->buffer[2], buf, length);

	ret = aa_i2c_write(dev->fd, chip_address, AA_I2C_NO_FLAGS, length + 2,
			   dev->buffer);

	if (ret < 0) {
		ELOG("[0x%02x]: Error writing to addr 0x%02x, length %d: %d: %s",
		     chip_address, address, length, ret, aa_status_string(ret));
	}

	pthread_mutex_unlock(&dev->cmd_lock);
	return ret;
}

int i2c_aardvark_stream_write(struct smbus_device *device, uint8_t chip_address,
			      void *buf, size_t length)
{
	struct i2c_aardvark_dev *dev = CAST_FROM(device);
	int ret;

	if (length > 0xffff) {
		ELOG("[0x%02x]: Streaming write lengh exceeds u16. Use chunks.",
		     chip_address);
		return -1;
	}

	pthread_mutex_lock(&dev->cmd_lock);
	DLOG("[0x%02x]: Streaming data of length %d", chip_address, length);
	ret = aa_i2c_write(dev->fd, chip_address, AA_I2C_NO_FLAGS, length, buf);
	pthread_mutex_unlock(&dev->cmd_lock);

	return ret;
}

int i2c_aardvark_read_ara(struct smbus_device *device, uint8_t ara_address)
{
	ELOG("ARA for Aardvark I2C is not supported!");
	return -1;
}

int i2c_aardvark_block_for_interrupt(struct smbus_device *device)
{
	struct i2c_aardvark_dev *dev = CAST_FROM(device);
	int ret = 0;
	bool cleaning_up = false;
	struct timespec ts;

	if (!(dev->chip && dev->line)) {
		ELOG("Gpio not initialized for polling.");
		return -1;
	}

	if (dev->cleaning_up) {
		return -1;
	}

	ts.tv_sec = 0;
	ts.tv_nsec = GPIOD_WAIT_TIMEOUT_NS;

	DLOG("Polling for I2C interrupt.");

	do {
		pthread_mutex_lock(&dev->gpio_lock);

		ret = gpiod_line_event_wait(dev->line, &ts);
		cleaning_up = dev->cleaning_up;

		pthread_mutex_unlock(&dev->gpio_lock);

		/* If we're cleaning up, exit out with an error. */
		if (cleaning_up) {
			ret = -1;
			break;
		}

		/* Either error or result will break here. Otherwise, continue.
		 */
		if (ret == 1 || ret == -1) {
			break;
		}
	} while (true);

	/* Got an event. Clear the event before forwarding interrupt. */
	if (ret == 1) {
		struct gpiod_line_event event;
		DLOG("Got I2C interrupt!");

		/* First clear the line event. */
		if (gpiod_line_event_read(dev->line, &event) == -1) {
			ELOG("Failed to read line event.");
			ret = -1;
		}
	} else {
		DLOG("I2C polling resulted in ret %d", ret);
	}

	return ret == 1 ? 0 : -1;
}

void i2c_aardvark_cleanup(struct smbus_driver *driver)
{
	if (driver->dev) {
		struct i2c_aardvark_dev *dev = CAST_FROM(driver->dev);

		if (dev->fd) {
			aa_close(dev->fd);
		}

		dev->cleaning_up = true;
		pthread_mutex_lock(&dev->gpio_lock);
		pthread_mutex_unlock(&dev->gpio_lock);

		platform_free(driver->dev);
		driver->dev = NULL;
	}
}

static int init_interrupt(struct i2c_aardvark_dev *dev, int gpio_chip,
			  int gpio_line)
{
	struct gpiod_chip *chip = NULL;
	struct gpiod_line *line = NULL;
	char filename[MAX_PATH_SIZE];

	if (pthread_mutex_init(&dev->gpio_lock, NULL) != 0) {
		ELOG("Failed to init gpio mutex");
		return -1;
	}

	if (pthread_mutex_init(&dev->cmd_lock, NULL) != 0) {
		ELOG("Failed to init command lock");
		return -1;
	}

	/* Request gpiochip and lines */
	snprintf(filename, MAX_PATH_SIZE - 1, "/dev/gpiochip%d", gpio_chip);
	chip = gpiod_chip_open(filename);
	if (!chip) {
		ELOG("Failed to open %s", filename);
		goto cleanup;
	}

	line = gpiod_chip_get_line(chip, gpio_line);
	if (!line) {
		ELOG("Failed to get line %d", gpio_line);
		goto cleanup;
	}

	if (gpiod_line_request_falling_edge_events(line, GPIOD_CONSUMER) != 0) {
		ELOG("Failed to set line config.");
		goto cleanup;
	}

	dev->chip = chip;
	dev->line = line;

	return 0;

cleanup:
	if (line) {
		gpiod_line_release(line);
	}

	if (chip) {
		gpiod_chip_close(chip);
	}

	return -1;
}

struct smbus_driver *i2c_aardvark_open(int port, uint8_t chip_address,
				       int gpio_chip, int gpio_line,
				       uint8_t transport)
{
	struct i2c_aardvark_dev *dev = NULL;
	struct smbus_driver *drv = NULL;
	Aardvark handle;
	AardvarkExt aa_ext;
	int bitrate;

	if (transport != SMBUS_TRANSPORT_I2C) {
		ELOG("Only I2C transport is supported at this time.");
		return NULL;
	}

	DLOG("Opening port %d", port);

	handle = aa_open_ext(port, &aa_ext);
	if (handle <= 0) {
		ELOG("Could not open Aardvark on port %d: %d: %s", port, handle,
		     aa_status_string(handle));
		return NULL;
	}

	/* Configure the aardvark port before starting. */
	aa_configure(handle, AA_CONFIG_SPI_I2C);
	//aa_i2c_pullup(handle, AA_I2C_PULLUP_BOTH);
	aa_target_power(handle, AA_TARGET_POWER_BOTH);
	bitrate = aa_i2c_bitrate(handle, I2C_BITRATE);
	DLOG("I2C bit-rate for port %d set to %d KHz", port, bitrate);

	dev = platform_calloc(1, sizeof(struct i2c_aardvark_dev));
	if (!dev) {
		goto handle_error;
	}

	dev->fd = handle;

	/* Initialize the gpio lines */
	if (init_interrupt(dev, gpio_chip, gpio_line) == -1) {
		ELOG("Failed to initialize gpio for interrupt.");
		goto handle_error;
	}

	drv = platform_calloc(1, sizeof(struct smbus_driver));
	if (!drv) {
		goto handle_error;
	}

	drv->dev = (struct smbus_device *)dev;
	drv->read_byte = i2c_aardvark_read_byte;
	drv->read_block = i2c_aardvark_read_block;
	drv->write_block = i2c_aardvark_write_block;
	drv->stream_write = i2c_aardvark_stream_write;
	drv->read_ara = i2c_aardvark_read_ara;
	drv->block_for_interrupt = i2c_aardvark_block_for_interrupt;
	drv->cleanup = i2c_aardvark_cleanup;

	/* Make sure chip address is valid before returning. */
	if (drv->read_byte(drv->dev, chip_address) < 0) {
		ELOG("Could not read byte at given chip address.");
		goto handle_error;
	}

	return drv;

handle_error:
	aa_close(handle);

	free(dev);
	free(drv);

	return NULL;
}
