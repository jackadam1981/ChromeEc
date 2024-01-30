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
	uint8_t task_output_buffer[SMBUS_MAX_BLOCK_SIZE * 2];

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

	/* Per port GET_CONNECTOR_STATUS data. */
	struct ucsiv3_get_connector_status_data *per_port_connector_data;

	/* Whether to use cached data. */
	bool *per_port_use_cached_constat;

	/* Don't send LPM alerts until PPM is ready. */
	bool ppm_is_ready;
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

#define CMD_SIZE 4

/* Map of tps6699x_4cc_tasks to actual values. */
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

struct tps6699x_interrupt_events {
	/* Break up the first 8 bytes for easier access. */
	uint32_t event0;
	uint32_t event1;

	/* We mostly ignore these except to clear them. */
	uint8_t remaining_events[3];

/* Bits we care about for this driver. */
#define TPS6699X_IRQ_EV0_PD_HARD_RESET_BIT (1 << 1)
#define TPS6699X_IRQ_EV0_CMD1_COMPLETE (1 << 30)
#define TPS6699X_IRQ_EV1_UCSI_STATUS_CHANGE (1 << 13)

#define TPS6699X_IRQ_EV0_USED_MASK \
	(TPS6699X_IRQ_EV0_PD_HARD_RESET_BIT | TPS6699X_IRQ_EV0_CMD1_COMPLETE)

#define TPS6699X_IRQ_EV1_USED_MASK (TPS6699X_IRQ_EV1_UCSI_STATUS_CHANGE)
};

struct tps6699x_ucsi_commands {
	uint8_t command;

	/* Fixed size or -1 for variable size. Read 2nd byte of output. */
	int return_length;
};

#define UCSI_CMD_ENTRY(cmd, ret_length)                      \
	{                                                    \
		.command = cmd, .return_length = ret_length, \
	}

static struct tps6699x_ucsi_commands ucsi_commands[UCSI_CMD_VENDOR_CMD + 1] = {
	UCSI_CMD_ENTRY(UCSI_CMD_RESERVED, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_PPM_RESET, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_CANCEL, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_CONNECTOR_RESET, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_ACK_CC_CI, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_NOTIFICATION_ENABLE, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CAPABILITY, 16),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CONNECTOR_CAPABILITY, 4),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_CCOM, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_UOR, 0),
	UCSI_CMD_ENTRY(obsolete_UCSI_CMD_SET_PDM, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_PDR, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_ALTERNATE_MODES, -1),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CAM_SUPPORTED, 2),
	/* TODO - Do we really support up to 64 alt-modes at once? */
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CURRENT_CAM, 8),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_NEW_CAM, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_PDOS, -1),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CABLE_PROPERTY, 5),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CONNECTOR_STATUS, 19),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_ERROR_STATUS, -1),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_POWER_LEVEL, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_PD_MESSAGE, -1),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_ATTENTION_VDO, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_reserved_0x17, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_GET_CAM_CS, -1),
	UCSI_CMD_ENTRY(UCSI_CMD_LPM_FW_UPDATE_REQUEST, -1),
	UCSI_CMD_ENTRY(UCSI_CMD_SECURITY_REQUEST, -1),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_RETIMER_MODE, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_SINK_PATH, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_SET_PDOS, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_READ_POWER_LEVEL, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_CHUNKING_SUPPORT, 0),
	UCSI_CMD_ENTRY(UCSI_CMD_VENDOR_CMD, -1),
};

#define UCSI_7BIT_PORTMASK(p) ((p) & 0x7F)

#define MIN(a, b)                       \
	({                              \
		__typeof__(a) _a = (a); \
		__typeof__(b) _b = (b); \
		_a > _b ? _a : _b;      \
	})

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

static char *task_string_from_enum(uint8_t command)
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
 * @param no_validation: Return immediately after writing to CMD register.
 *
 * @return -1 on error, bytes read from DATAx after task completes otherwise.
 */
int tps6699x_4cc_run_task(struct tps6699x_device *dev, uint8_t port,
			  uint8_t task, uint8_t *data_in, size_t data_in_length,
			  uint8_t *data_out, size_t data_out_length,
			  bool no_validation)
{
	int ret = 0;
	char cmd1_data[5] = { 0 };
	char *task_string = task_string_from_enum(task);

	if (!task_string) {
		ELOG("Invalid task requested: 0x%x", task);
		return -1;
	}

	platform_mutex_lock(dev->task_lock);

	DLOG("Running task %s on port %d", task_string, port);
	if (task == TPSCMD_UCSI) {
		DLOG_START("Task UCSI with command %s, length 0x%02x: [ ",
			   ucsi_command_to_string(data_in[0]), data_in[1]);
		for (int i = 2; i < data_in_length; ++i) {
			DLOG_LOOP("0x%02x, ", data_in[i]);
		}
		DLOG_END(" ]");
	}

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
		if (ret < 0) {
			ELOG("Failed to write %d bytes to input data1 for task with result %d",
			     data_in_length, ret);
			ret = -1;
			goto unlock;
		}
	}

	/* Next is writing the task to CMDx. */
	ret = tps6699x_smbus_write_register(dev, port, TPSREG_CMD1,
					    (uint8_t *)task_string, CMD_SIZE);
	if (ret < 0) {
		ELOG("Failed to write %s to cmd1 for task with result %d",
		     task_string, ret);
		ret = -1;
		goto unlock;
	}

	if (no_validation) {
		DLOG("Exit early from task %s (no validation).", task_string);
		ret = 0;
		goto unlock;
	}

	DLOG("Waiting on IRQ to handle task completion...");

	do {
		/* Now we block for command completion via interrupt and make
		 * sure the command was correctly handled. We wait 50ms before
		 * reading the command register to see if it was updated.
		 */
		platform_condvar_wait_timeout(dev->task_condvar, dev->task_lock,
					      50000ull);

		/* Double check that the command was correctly handled. It
		 * should be empty if successful. If the command matches the
		 * input, then it is still pending.
		 */
		ret = tps6699x_smbus_read_register(
			dev, port, TPSREG_CMD1, (uint8_t *)cmd1_data, CMD_SIZE);

		if (are_same_cmd(cmd1_data, task_string)) {
			continue;
		}

		if (are_same_cmd(cmd1_data, kInvalidCmd)) {
			ELOG("Task %s was considered invalid (%s)", task_string,
			     kInvalidCmd);
			ret = -1;
			goto unlock;
		}

		if (!are_same_cmd(cmd1_data, kEmptyCmd)) {
			ELOG("Unexpected command value of %s read",
			     ((char *)cmd1_data));
			ret = -1;
			goto unlock;
		}

	} while (false);

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
	if (!dev || !dev->task_lock) {
		ELOG("abps: dev %p or dev->task_lock %p", dev,
		     (dev ? (void *)dev->task_lock : (void *)dev));
	}

	platform_mutex_unlock(dev->task_lock);
	return ret;
}

int tps6699x_broadcast_stream(struct tps6699x_device *dev,
			      uint8_t broadcast_address, void *buf,
			      size_t length)
{
	return dev->smbus->stream_write(dev->smbus->dev, broadcast_address, buf,
					length);
}

/*
 * TPS6699x doesn't use ARA. Instead, you get a single interrupt and you need to
 * check via INT_EVENTx to see who sent you the alert (by reading the registers
 * using different chip addresses).
 */
static void tps6699x_ucsi_handle_interrupt(struct tps6699x_device *dev)
{
	struct tps6699x_interrupt_events irq_event;
	int ret;
	int array_index;

	platform_mutex_lock(dev->task_lock);
	for (int port = 1; port <= dev->active_port_count; ++port) {
		array_index = port - 1;
		ret = tps6699x_smbus_read_register(dev, port, TPSREG_IRQ_EVENT1,
						   (uint8_t *)&irq_event,
						   sizeof(irq_event));

		if (ret < 0) {
			ELOG("Failed to read IRQ events for port %d: %d", port,
			     ret);
			continue;
		}

		/* Check for event changes that we care about in first 64 bits.
		 */
		if (irq_event.event0 == 0 && irq_event.event1 == 0) {
			continue;
		}

		/* Clear the events before we continue. */
		ret = tps6699x_smbus_write_register(dev, port,
						    TPSREG_IRQ_CLEAR1,
						    (uint8_t *)&irq_event,
						    sizeof(irq_event));
		if (ret < 0) {
			ELOG("Failed to clear interrupts on port %d: %d", port,
			     ret);

			/* TODO - How to handle failure to clear interrupt
			 * register here? Exit/crash doesn't seem right.
			 */
		}

		/* Active command completed. Send the command task a signal to
		 * continue.
		 */
		if (irq_event.event0 & TPS6699X_IRQ_EV0_CMD1_COMPLETE) {
			DLOG("IRQ [0x%02x]: CMD1 completed",
			     port_to_chip_address(dev, port));
			platform_condvar_signal(dev->task_condvar);
		}

		/* Any other event than command completion should trigger
		 * a flush of the GET_CONNECTOR_STATUS cached value and allow
		 * a fresh read on the next call. This should also trigger an
		 * LPM alert.
		 *
		 * Events here are either PD reset or UCSI status change.
		 */
		if (irq_event.event1 & TPS6699X_IRQ_EV1_UCSI_STATUS_CHANGE) {
			DLOG("IRQ [0x%02x]: LPM alerted",
			     port_to_chip_address(dev, port));
			dev->per_port_use_cached_constat[array_index] = false;
			/* Only send alerts after PPM can handle it. */
			if (dev->ppm_is_ready) {
				dev->ppm->lpm_alert(dev->ppm->dev, port);
			}
		}
	}
	platform_mutex_unlock(dev->task_lock);
}

static void tps6699x_lpm_irq_task(struct ucsi_pd_device *device)
{
	struct tps6699x_device *dev = CAST_FROM(device);
	struct smbus_driver *smbus = dev->smbus;

	DLOG("LPM IRQ task started");
	while (smbus->block_for_interrupt(smbus->dev) != -1) {
		tps6699x_ucsi_handle_interrupt(dev);
	}

	ELOG("LPM IRQ task ended. This is fatal.");
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
	struct tps6699x_device *dev = CAST_FROM(device);
	uint8_t num_ports = 0;
	uint8_t max_num_ports = dev->driver_config->max_num_ports;
	struct tps6699x_boot_flags flags;
	struct tps6699x_interrupt_events interrupt_mask;
	struct ucsi_control control;
	int ret = 0;

	/* Read the boot flags register to get the number of ports. */
	if (tps6699x_get_boot_flags(dev, &flags) == -1) {
		return -1;
	}

	num_ports = NUM_PORTS_MASK(flags.port_info);
	if (num_ports > max_num_ports) {
		ELOG("Truncated number of ports from %d to %d", num_ports,
		     max_num_ports);
		num_ports = max_num_ports;
	}

	/* Assign active ports and allocate per port caches. */
	dev->active_port_count = num_ports;
	dev->per_port_connector_data = platform_calloc(
		num_ports, sizeof(struct ucsiv3_get_connector_status_data));
	dev->per_port_use_cached_constat =
		platform_calloc(num_ports, sizeof(bool));

	platform_memset(&interrupt_mask, 0,
			sizeof(struct tps6699x_interrupt_events));
	interrupt_mask.event0 = TPS6699X_IRQ_EV0_USED_MASK;
	interrupt_mask.event1 = TPS6699X_IRQ_EV1_USED_MASK;

	platform_memset(&control, 0, sizeof(struct ucsi_control));
	control.command = 0x1;
	control.data_length = 0;

	for (uint8_t port = 1; port <= num_ports; ++port) {
		/* First, set the interrupt mask to only bits we're going to
		 * use.
		 */
		ret = tps6699x_smbus_write_register(
			dev, port, TPSREG_IRQ_MASK1, (uint8_t *)&interrupt_mask,
			sizeof(struct tps6699x_interrupt_events));
		if (ret < 0) {
			ELOG("Failed to set IRQ Mask for port %d: %d", port,
			     ret);
			return -1;
		}

		/* Then clear all those bits so we can start fresh. */
		ret = tps6699x_smbus_write_register(
			dev, port, TPSREG_IRQ_CLEAR1,
			(uint8_t *)&interrupt_mask,
			sizeof(struct tps6699x_interrupt_events));
		if (ret < 0) {
			ELOG("Failed to clear IRQs for port %d: %d", port, ret);
			return -1;
		}

		/* Next reset the PPM (setting the specific port). */
		control.command_specific[0] = UCSI_7BIT_PORTMASK(port);
		ret = tps6699x_4cc_run_task(dev, port, TPSCMD_UCSI,
					    (uint8_t *)&control,
					    sizeof(struct ucsi_control),
					    /*data_out=*/NULL,
					    /*data_out_length=*/0,
					    /*no_validation=*/false);
		if (ret < 0) {
			ELOG("Failed to PPM reset port %d: %d", port, ret);
			return -1;
		}
	}

	DLOG("TPS6699x IRQ registers initialized. Now doing PPM init!");

	/* Init the PPM and return. */
	ret = dev->ppm->init_and_wait(dev->ppm->dev, num_ports);

	/* Mark ppm ready for alerts. */
	if (ret == 0) {
		platform_mutex_lock(dev->task_lock);
		dev->ppm_is_ready = true;
		platform_mutex_unlock(dev->task_lock);
	}

	return ret;
}

static struct ucsi_ppm_driver *
tps6699x_ucsi_get_ppm(struct ucsi_pd_device *device)
{
	struct tps6699x_device *dev = CAST_FROM(device);
	return dev->ppm;
}

static int tps6699x_ucsi_execute_cmd(struct ucsi_pd_device *device,
				     struct ucsi_control *control,
				     uint8_t *lpm_data_out)
{
	struct tps6699x_device *dev = CAST_FROM(device);
	int ret = 0;
	uint8_t port_num = TI_DEFAULT_PORT;
	uint8_t task = TPSCMD_UCSI;
	uint8_t ucsi_command = control->command;
	int array_index;
	int return_length = ucsi_commands[ucsi_command].return_length;

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

	/* If non-zero, that means we have a 1-indexed port num. */
	array_index = port_num ? port_num - 1 : port_num;

	if (ucsi_command == UCSI_CMD_GET_CONNECTOR_STATUS &&
	    array_index < dev->active_port_count &&
	    dev->per_port_use_cached_constat[array_index]) {
		platform_memcpy(
			lpm_data_out,
			&dev->per_port_connector_data[array_index],
			sizeof(struct ucsiv3_get_connector_status_data));

		return sizeof(struct ucsiv3_get_connector_status_data);
	}

	/* TODO(abps) - Remove hacks as TI fixes inconsistencies. */
	switch (ucsi_command) {
	/* TI expects connector number in some commands that don't exist in UCSI
	 * spec.
	 */
	case UCSI_CMD_PPM_RESET:
	case UCSI_CMD_GET_CAPABILITY:
		control->command_specific[0] = port_num;
		break;

	/* When acking commands, just skip. When acking CI, we clear
	 * GET_CONNECTOR_STATUS.
	 */
	case UCSI_CMD_ACK_CC_CI:
		struct ucsiv3_ack_cc_ci_cmd *ack_cmd =
			(struct ucsiv3_ack_cc_ci_cmd *)control->command_specific;

		/* If we're acking connector change, clear the connector status.
		 */
		if (ack_cmd->connector_change_ack) {
			dev->per_port_use_cached_constat[array_index] = false;
		}
		/* For command complete, just return. */
		return 0;
	}

	ret = tps6699x_4cc_run_task(dev, port_num, task, (uint8_t *)control,
				    sizeof(struct ucsi_control),
				    dev->task_output_buffer,
				    SMBUS_MAX_BLOCK_SIZE,
				    /*no_validation=*/false);

	/* We got an actual result back and we need to process it and copy back
	 * the actual number of bytes read (subtracting BYTE1 which is a general
	 * result).
	 */
	if (ret > 0) {
		uint8_t task_result =
			TASK_RESULT_MASK(dev->task_output_buffer[0]);

		if (task_result == TASK_SUCCESS) {
			uint8_t *output = &dev->task_output_buffer[1];

			/* Skip result byte and copy remaining data out. */
			ret = ret - 1;
			if (return_length == -1) {
				output = &dev->task_output_buffer[2];
				return_length = dev->task_output_buffer[1];
			}

			/* Truncate returns so we don't read garbage. */
			if (ret < return_length) {
				return_length = ret;
			} else {
				ret = return_length;
			}

			platform_memcpy(lpm_data_out, output, return_length);
		} else {
			ELOG("UCSI task failed on command %s with 0x%x",
			     ucsi_command_to_string(ucsi_command), task_result);
			ret = -1;
		}
	} else if (ret == 0) {
		ELOG("Got back 0 bytes from running task. Not valid for UCSI commands.");
		ret = -1;
	}

	/* Cache the connector status. */
	if (ucsi_command == UCSI_CMD_GET_CONNECTOR_STATUS && ret > 0 &&
	    array_index < dev->active_port_count) {
		dev->per_port_use_cached_constat[array_index] = true;
		platform_memcpy(
			&dev->per_port_connector_data[array_index],
			lpm_data_out,
			MIN(sizeof(struct ucsiv3_get_connector_status_data),
			    ret));
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

int tps6699x_ucsi_get_active_port_count(struct ucsi_pd_device *device)
{
	struct tps6699x_device *dev = CAST_FROM(device);

	return dev->active_port_count;
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
	dev->active_port_count = config->max_num_ports;

	drv = platform_calloc(1, sizeof(struct ucsi_pd_driver));
	if (!drv) {
		goto handle_error;
	}

	drv->dev = (struct ucsi_pd_device *)dev;

	drv->configure_lpm_irq = tps6699x_ucsi_configure_lpm_irq;
	drv->init_ppm = tps6699x_ucsi_init_ppm;
	drv->get_ppm = tps6699x_ucsi_get_ppm;
	drv->execute_cmd = tps6699x_ucsi_execute_cmd;
	drv->get_active_port_count = tps6699x_ucsi_get_active_port_count;
	drv->cleanup = tps6699x_ucsi_cleanup;

	/* Initialize mutex and condvar necessary for task execution. */
	if (platform_mutex_init(&dev->task_lock) < 0) {
		goto handle_error;
	}

	if (platform_condvar_init(&dev->task_condvar) < 0) {
		goto handle_error;
	}

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
