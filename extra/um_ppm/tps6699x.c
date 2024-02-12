/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/pd_driver.h"
#include "include/platform.h"
#include "include/ppm.h"
#include "ppm_common.h"
#include "tps6699x.h"

#define SMBUS_MAX_BLOCK_SIZE 32

struct tps6699x_device {
	/* LPM smbus driver. */
	struct smbus_driver *smbus;

	/* PPM driver (common implementation). */
	struct ucsi_ppm_driver *ppm;

	/* Re-usable task output buffer for active command. */
	uint8_t task_output_buffer[SMBUS_MAX_BLOCK_SIZE];

	/* Configuration for this driver. */
	struct pd_driver_config *driver_config;

	/* Number of active ports from |GET_CAPABILITIES|. */
	uint8_t active_port_count;

	/* Task running interrupt handling. */
	struct task_handle *lpm_interrupt_task;

	/* Lock used for executing tasks. */
	struct platform_mutex *task_lock;

	/* Signal that a task has completed. */
	struct platform_condvar *task_condvar;
};

#define CAST_FROM(v) (struct tps6699x_device *)(v)

/* Non-exhaustive set of smbus registers for TPS6699x. */
enum tps6699x_smbus_registers {
	TPSREG_VENDOR_ID = 0x00,
	TPSREG_DEVICE_ID = 0x01,
	TPSREG_CMD1 = 0x08,
	TPSREG_DATA1 = 0x09,
	TPSREG_VERSION = 0x0F,
	TPSREG_CMD2 = 0x10,
	TPSREG_DATA2 = 0x11,
	TPSREG_IRQ_EVENT1 = 0x14,
	TPSREG_IRQ_EVENT2 = 0x15,
	TPSREG_IRQ_MASK1 = 0x16,
	TPSREG_IRQ_MASK2 = 0x17,
	TPSREG_IRQ_CLEAR1 = 0x18,
	TPSREG_IRQ_CLEAR2 = 0x19,
	TPSREG_BOOT_FLAGS = 0x2D,
	TPSREG_DEVICE_INFO = 0x2F,
};

enum tps6699x_smbus_commands {
	/* Control tasks */
	TPSCMD_GAID, /* Cold reset */

	/* Firmware Update Tasks */
	TPSCMD_TFUs, /* Enter TFU Mode. */
	TPSCMD_TFUc, /* Complete Phase. */
	TPSCMD_TFUd, /* Data Phase. */
	TPSCMD_TFUe, /* Exit. */
	TPSCMD_TFUi, /* Initiate update. */
	TPSCMD_TFUq, /* Query status. */

	TPSCMD_UCSI, /* All UCSI commands. */

	/* For counting only (not a valid command). */
	TPSCMD_MAX_COUNT,
};

#define CMD_SIZE 4

/* Map of tps6699x_smbus_commands to actual values. */
char *tpscmd_map[TPSCMD_MAX_COUNT] = {
	"GAID", "TFUs", "TFUc", "TFUd", "TFUe", "TFUi", "TFUq", "UCSI",
};
const char kEmptyCmd[] = { 0, 0, 0, 0 };
const char *kInvalidCmd = "!CMD";

#define TASK_RESULT_MASK(b) ((b) & 0xF)
enum tps6699x_task_result {
	TASK_SUCCESS = 0,
	TASK_TIMEOUT_ABORT = 0x1,
	TASK_RESERVED1 = 0x2,
	TASK_REJECTED = 0x3,
	TASK_REJECTED_RXBUFLOCK = 0x4,
};

/* Convert a given port to a chip address.
 *
 * @param dev: Internal data.
 * @param port: 1-indexed port number. 0 will give default port.
 *
 * @return Chip address for i2c.
 */
static uint8_t port_to_chip_address(struct tps6699x_device *dev, uint8_t port)
{
	if (port > dev->active_port_count) {
		ELOG("Attempted to access invalid port %d. Active port count = %d",
		     port, dev->active_port_count);
		return 0;
	}

	if (port > 0) {
		port = port - 1;
	}

	return dev->driver_config->port_address_map[port];
}

static char *task_string_from_command(uint8_t command)
{
	if (command >= TPSCMD_MAX_COUNT) {
		ELOG("Task out of range: %d", command);
		return NULL;
	}

	return tpscmd_map[command];
}

static bool are_same_cmd(const char *cmd_a, const char *cmd_b)
{
	for (int i = 0; i < CMD_SIZE; ++i) {
		if (cmd_a[i] != cmd_b[i]) {
			return false;
		}
	}

	return true;
}

static int tps6699x_smbus_read_register(struct tps6699x_device *dev,
					uint8_t port, uint8_t smbus_register,
					uint8_t *out, size_t out_length)
{
	uint8_t chip_address = port_to_chip_address(dev, port);
	if (chip_address == 0) {
		return -1;
	}

	return dev->smbus->read_block(dev->smbus->dev, chip_address,
				      smbus_register, out, out_length);
}

static int tps6699x_smbus_write_register(struct tps6699x_device *dev,
					 uint8_t port, uint8_t smbus_register,
					 uint8_t *data, size_t data_length)
{
	uint8_t chip_address = port_to_chip_address(dev, port);
	if (chip_address == 0) {
		return -1;
	}

	return dev->smbus->write_block(dev->smbus->dev, chip_address,
				       smbus_register, data, data_length);
}

/* Runs a 4CC task.
 *
 * @param dev: Internal data.
 * @param port: Which port to operate on.
 * @param task: Which task to run.
 * @param data_in: What to write to DATAx for task.
 * @param data_in_length: Length of data to write. If 0, skip the data_in step.
 * @param data_out: What to read from DATAx after task completes.
 * @param data_out_length: Length of data to read. If 0, skip the data_out step.
 *
 * @return -1 on error, bytes read from DATAx after task completes otherwise.
 */
static int tps6699x_smbus_run_task(struct tps6699x_device *dev, uint8_t port,
				   uint8_t task, uint8_t *data_in,
				   size_t data_in_length, uint8_t *data_out,
				   size_t data_out_length)
{
	int ret = 0;
	char cmd1_data[4] = { 0 };
	char *task_string = task_string_from_command(task);

	if (!task_string) {
		ELOG("Invalid task requested: 0x%x", task);
		return -1;
	}

	platform_mutex_lock(dev->task_lock);

	/* First make sure we are in valid state to start a task.  CMD1 should
	 * be in a valid state before we start.
	 */
	ret = tps6699x_smbus_read_register(dev, port, TPSREG_CMD1,
					   (uint8_t *)cmd1_data, CMD_SIZE);
	if (ret != CMD_SIZE || !(are_same_cmd(cmd1_data, kEmptyCmd) ||
				 are_same_cmd(cmd1_data, kInvalidCmd))) {
		ELOG("Task register is not ready! Read = %d, Value = 0x%x", ret,
		     *((uint32_t *)cmd1_data));
		ret = -1;
		goto unlock;
	}

	/* Write to INPUT DATAx before writing to command. */
	if (data_in && data_in_length > 0) {
		ret = tps6699x_smbus_write_register(dev, port, TPSREG_DATA1,
						    data_in, data_in_length);
		if (ret != data_in_length) {
			ELOG("Failed to write input data1 for task with result %d",
			     ret);
			ret = -1;
			goto unlock;
		}
	}

	/* Next is writing the task to CMDx. */
	ret = tps6699x_smbus_write_register(dev, port, TPSREG_CMD1,
					    (uint8_t *)task_string, CMD_SIZE);
	if (ret != CMD_SIZE) {
		ELOG("Failed to write cmd1 for task with result %d", ret);
		ret = -1;
		goto unlock;
	}

	/* Now we block for command completion via interrupt and make sure the
	 * command was correctly handled.
	 */
	platform_condvar_wait(dev->task_condvar, dev->task_lock);

	/* Double check that the command was correctly handled. It should be
	 * empty if successful.
	 */
	ret = tps6699x_smbus_read_register(dev, port, TPSREG_CMD1,
					   (uint8_t *)cmd1_data, CMD_SIZE);
	if (ret != CMD_SIZE || !are_same_cmd(cmd1_data, kEmptyCmd)) {
		ELOG("Command didn't complete successfully: read %d, value 0x%x",
		     ret, *((uint32_t *)cmd1_data));
		ret = -1;
		goto unlock;
	}

	if (data_out && data_out_length > 0) {
		/* Resulting data will be in DATAx. This is the final result
		 * that gets outputted as well (bytes read from task).
		 */
		ret = tps6699x_smbus_read_register(dev, port, TPSREG_DATA1,
						   data_out, data_out_length);
	} else {
		/* Nothing read. */
		ret = 0;
	}

unlock:
	platform_mutex_unlock(dev->task_lock);
	return ret;
}

/*
 * TPS6699x doesn't use ARA. Instead, you get a single interrupt and you need to
 * check via INT_EVENTx to see who sent you the alert (by reading the registers
 * using different chip addresses).
 */
static void tps6699x_lpm_irq_task(struct ucsi_pd_device *device)
{
}

static int tps6699x_ucsi_configure_lpm_irq(struct ucsi_pd_device *device)
{
	struct tps6699x_device *dev = CAST_FROM(device);

	if (dev->lpm_interrupt_task != NULL) {
		return 0;
	}

	if (platform_task_init(tps6699x_lpm_irq_task, dev,
			       &dev->lpm_interrupt_task)) {
		return -1;
	}

	return 0;
}

static int tps6699x_ucsi_init_ppm(struct ucsi_pd_device *device)
{
	return -1;
}

static struct ucsi_ppm_driver *
tps6699x_ucsi_get_ppm(struct ucsi_pd_device *device)
{
	struct tps6699x_device *dev = CAST_FROM(device);
	return dev->ppm;
}

#define UCSI_7BIT_PORTMASK(p) ((p) & 0x7F)

static int tps6699x_ucsi_execute_command(struct ucsi_pd_device *device,
					 struct ucsi_control *control,
					 uint8_t *lpm_data_out)
{
	struct tps6699x_device *dev = CAST_FROM(device);
	int ret = 0;
	uint8_t port_num = TI_DEFAULT_PORT;
	uint8_t task = TPSCMD_UCSI;
	uint8_t ucsi_command = control->command;

	/* Unimplemented commands (needs further work). */
	switch (ucsi_command) {
	case UCSI_CMD_SET_NOTIFICATION_ENABLE:
	case UCSI_CMD_ACK_CC_CI:
	case UCSI_CMD_GET_PD_MESSAGE:
		ELOG("%s: Not implemented yet!",
		     ucsi_command_to_string(ucsi_command));
		return -1;
	}

	/* Get the port number for commands that target specific ports. */
	switch (ucsi_command) {
	/* The following UCSI commands change the port being addressed.  These
	 * commands have the connector number at offset 16.
	 */
	case UCSI_CMD_CONNECTOR_RESET:
	case UCSI_CMD_GET_CONNECTOR_CAPABILITY:
	case UCSI_CMD_GET_CAM_SUPPORTED:
	case UCSI_CMD_GET_CURRENT_CAM:
	case UCSI_CMD_SET_NEW_CAM:
	case UCSI_CMD_GET_PDOS:
	case UCSI_CMD_GET_CABLE_PROPERTY:
	case UCSI_CMD_GET_CONNECTOR_STATUS:
	case UCSI_CMD_GET_ERROR_STATUS:
	case UCSI_CMD_GET_PD_MESSAGE:
	case UCSI_CMD_GET_ATTENTION_VDO:
	case UCSI_CMD_GET_CAM_CS:
		port_num = UCSI_7BIT_PORTMASK(control->command_specific[0]);
		break;

	/* The following UCSI commands change the port being addressed.  These
	 * commands have the connector number at offset 24.
	 */
	case UCSI_CMD_GET_ALTERNATE_MODES:
		port_num = UCSI_7BIT_PORTMASK(control->command_specific[1]);
		break;
	}

	ret = tps6699x_smbus_run_task(dev, port_num, task, (uint8_t *)control,
				      sizeof(struct ucsi_control),
				      dev->task_output_buffer,
				      SMBUS_MAX_BLOCK_SIZE);

	/* We got an actual result back and we need to process it and copy back
	 * the actual number of bytes read (subtracting BYTE1 which is a general
	 * result).
	 */
	if (ret > 0) {
		uint8_t task_result =
			TASK_RESULT_MASK(dev->task_output_buffer[0]);

		if (task_result == TASK_SUCCESS) {
			/* Skip result byte and copy remaining data out. */
			ret = ret - 1;
			platform_memcpy(lpm_data_out,
					&dev->task_output_buffer[1], ret - 1);
		} else {
			ELOG("UCSI task failed on command %s with 0x%x",
			     ucsi_command_to_string(ucsi_command), task_result);
			ret = -1;
		}
	} else if (ret == 0) {
		ELOG("Got back 0 bytes from running task. Not valid for UCSI commands.");
		ret = -1;
	}

	return ret;
}

static void tps6699x_ucsi_cleanup(struct ucsi_pd_driver *driver)
{
	if (driver->dev) {
		struct tps6699x_device *dev = CAST_FROM(driver->dev);

		if (dev->ppm) {
			dev->ppm->cleanup(dev->ppm);
			platform_free(dev->ppm);
		}

		if (dev->smbus) {
			dev->smbus->cleanup(dev->smbus);

			if (dev->lpm_interrupt_task) {
				platform_task_complete(dev->lpm_interrupt_task);
			}

			platform_free(dev->smbus);
		}

		platform_free(driver->dev);
		driver->dev = NULL;
	}
}

int tps6699x_get_boot_flags(struct tps6699x_device *dev,
			    struct tps6699x_boot_flags *flags)
{
	if (!flags) {
		return -1;
	}

	return tps6699x_smbus_read_register(dev, TI_DEFAULT_PORT,
					    TPSREG_BOOT_FLAGS, (uint8_t *)flags,
					    sizeof(struct tps6699x_boot_flags));
}

int tps6699x_get_version(struct tps6699x_device *dev, uint32_t *version_out)
{
	if (!version_out) {
		return -1;
	}

	return tps6699x_smbus_read_register(dev, TI_DEFAULT_PORT,
					    TPSREG_VERSION,
					    (uint8_t *)version_out,
					    sizeof(uint32_t));
}

int tps6699x_get_device_info(struct tps6699x_device *dev,
			     struct tps6699x_device_info *device_info)
{
	int ret = -1;

	if (!device_info) {
		return ret;
	}

	ret = tps6699x_smbus_read_register(dev, TI_DEFAULT_PORT,
					   TPSREG_DEVICE_INFO,
					   (uint8_t *)device_info->data,
					   sizeof(struct tps6699x_device_info));

	/* Force last byte to be null in case data doesn't set it. */
	if (ret > 0) {
		device_info->data[sizeof(struct tps6699x_device_info) - 1] = 0;
	}

	return ret;
}

struct ucsi_pd_driver *tps6699x_open(struct smbus_driver *smbus,
				     struct pd_driver_config *config)
{
	struct tps6699x_device *dev = NULL;
	struct ucsi_pd_driver *drv = NULL;

	dev = platform_calloc(1, sizeof(struct tps6699x_device));
	if (!dev) {
		goto handle_error;
	}

	dev->smbus = smbus;
	dev->driver_config = config;

	drv = platform_calloc(1, sizeof(struct ucsi_pd_driver));
	if (!drv) {
		goto handle_error;
	}

	drv->dev = (struct ucsi_pd_device *)dev;

	drv->configure_lpm_irq = tps6699x_ucsi_configure_lpm_irq;
	drv->init_ppm = tps6699x_ucsi_init_ppm;
	drv->get_ppm = tps6699x_ucsi_get_ppm;
	drv->execute_cmd = tps6699x_ucsi_execute_command;
	drv->cleanup = tps6699x_ucsi_cleanup;

	/* Initialize the PPM. */
	dev->ppm = ppm_open(drv, NULL);
	if (!dev->ppm) {
		ELOG("Failed to open PPM");
		goto handle_error;
	}

	return drv;

handle_error:
	if (dev && dev->ppm) {
		dev->ppm->cleanup(dev->ppm);
		dev->ppm = NULL;
	}

	platform_free(dev);
	platform_free(drv);

	return NULL;
}

struct pd_driver_config tps6699x_get_driver_config()
{
	struct pd_driver_config config = {
      .max_num_ports = 2,
      .port_address_map =
          {
              0x20,
              0x24,
          },
      .transport = I2C,
  };

	return config;
}
