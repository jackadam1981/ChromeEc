/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/ppm.h"
#include "platform.h"
#include "ppm_common.h"

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ppm_common, LOG_LEVEL_INF);

const char *ppm_state_strings[PPM_STATE_MAX] = {
	"PPM_STATE_NOT_READY",	    "PPM_STATE_IDLE",
	"PPM_STATE_IDLE_NOTIFY",    "PPM_STATE_PROCESSING_COMMAND",
	"PPM_STATE_WAITING_CC_ACK", "PPM_STATE_WAITING_ASYNC_EV_ACK",
};

const char *ppm_state_to_string(int state)
{
	if (state < PPM_STATE_NOT_READY || state >= PPM_STATE_MAX) {
		return "PPM_STATE_Outside_valid_range";
	}

	return ppm_state_strings[state];
}

const char *ucsi_cmd_strings[UCSI_CMD_MAX] = {
	"UCSI_CMD_RESERVED",
	"UCSI_CMD_PPM_RESET",
	"UCSI_CMD_CANCEL",
	"UCSI_CMD_CONNECTOR_RESET",
	"UCSI_CMD_ACK_CC_CI",
	"UCSI_CMD_SET_NOTIFICATION_ENABLE",
	"UCSI_CMD_GET_CAPABILITY",
	"UCSI_CMD_GET_CONNECTOR_CAPABILITY",
	"UCSI_CMD_SET_CCOM",
	"UCSI_CMD_SET_UOR",
	"obsolete_UCSI_CMD_SET_PDM",
	"UCSI_CMD_SET_PDR",
	"UCSI_CMD_GET_ALTERNATE_MODES",
	"UCSI_CMD_GET_CAM_SUPPORTED",
	"UCSI_CMD_GET_CURRENT_CAM",
	"UCSI_CMD_SET_NEW_CAM",
	"UCSI_CMD_GET_PDOS",
	"UCSI_CMD_GET_CABLE_PROPERTY",
	"UCSI_CMD_GET_CONNECTOR_STATUS",
	"UCSI_CMD_GET_ERROR_STATUS",
	"UCSI_CMD_SET_POWER_LEVEL",
	"UCSI_CMD_GET_PD_MESSAGE",
	"UCSI_CMD_GET_ATTENTION_VDO",
	"UCSI_CMD_reserved_0x17",
	"UCSI_CMD_GET_CAM_CS",
	"UCSI_CMD_LPM_FW_UPDATE_REQUEST",
	"UCSI_CMD_SECURITY_REQUEST",
	"UCSI_CMD_SET_RETIMER_MODE",
	"UCSI_CMD_SET_SINK_PATH",
	"UCSI_CMD_SET_PDOS",
	"UCSI_CMD_READ_POWER_LEVEL",
	"UCSI_CMD_CHUNKING_SUPPORT",
	"UCSI_CMD_VENDOR_CMD",
};

const char *ucsi_command_to_string(uint8_t command)
{
	if (command >= UCSI_CMD_MAX) {
		return "UCSI_CMD_Outside_valid_range";
	}

	return ucsi_cmd_strings[command];
}

/* TODO(b/339702957) - Will be filled in on next commit. */
static void ppm_common_task(void *context)
{
	return;
}

static int ppm_common_init_and_wait(struct ucsi_ppm_device *device,
				    uint8_t num_ports)
{
#define MAX_TIMEOUT_MS 1000
#define POLL_EVERY_MS 10
	struct ppm_common_device *dev = (struct ppm_common_device *)device;
	struct ucsi_memory_region *ucsi_data = &dev->ucsi_data;
	bool ready_to_exit = false;

	/* First clear the PPM shared memory region. */
	platform_memset(ucsi_data, 0, sizeof(*ucsi_data));

	/* Initialize to UCSI version 3.0 */
	ucsi_data->version.version = 0x0300;
	/* TODO - Set real lpm address based on smbus driver. */
	ucsi_data->version.lpm_address = 0x0;

	/* Reset state. */
	dev->cleaning_up = false;
	dev->ppm_state = PPM_STATE_NOT_READY;
	platform_memset(&dev->pending, 0, sizeof(dev->pending));

	/* Init lock to sync PPM task and main task context. */
	if (platform_mutex_init(&dev->ppm_lock)) {
		ELOG("Failed to init ppm_lock");
		return -1;
	}

	/* Init condvar to notify PPM task. */
	if (platform_condvar_init(&dev->ppm_condvar)) {
		ELOG("Failed to init ppm_condvar");
		return -1;
	}

	/* Allocate per port status (used for PPM async event notifications). */
	if (num_ports != dev->num_ports) {
		dev->num_ports = num_ports;
		dev->per_port_status = platform_calloc(
			dev->num_ports,
			sizeof(struct ucsiv3_get_connector_status_data));
	}
	dev->last_connector_changed = 0;
	dev->last_connector_alerted = 0;

	DLOG("Ready to initialize PPM task!");

	/* Initialize the PPM task. */
	if (platform_task_init((void *)ppm_common_task, (void *)dev,
			       &dev->ppm_task_handle)) {
		ELOG("No ppm task created.");
		return -1;
	}

	DLOG("PPM is waiting for task to run.");

	for (int count = 0; count * POLL_EVERY_MS < MAX_TIMEOUT_MS; count++) {
		platform_mutex_lock(dev->ppm_lock);
		ready_to_exit = dev->ppm_state != PPM_STATE_NOT_READY;
		platform_mutex_unlock(dev->ppm_lock);

		if (ready_to_exit) {
			break;
		}

		platform_usleep(POLL_EVERY_MS * 1000);
	}

	DLOG("PPM initialized result: Success=%b", ready_to_exit);

	return (ready_to_exit ? 0 : -1);
}

bool ppm_common_get_next_connector_status(
	struct ucsi_ppm_device *device, uint8_t *out_port_num,
	struct ucsiv3_get_connector_status_data **out_connector_status)
{
	struct ppm_common_device *dev = (struct ppm_common_device *)device;

	if (dev->last_connector_changed) {
		*out_port_num = (uint8_t)dev->last_connector_changed;
		*out_connector_status =
			&dev->per_port_status[dev->last_connector_changed - 1];
		return true;
	}

	return false;
}

static int ppm_common_read(struct ucsi_ppm_device *device, unsigned int offset,
			   void *buf, size_t length)
{
	struct ppm_common_device *dev = (struct ppm_common_device *)device;

	if (!dev) {
		return -1;
	}

	/* Validate memory to read and allow any offset for reading. */
	if (offset + length >= sizeof(struct ucsi_memory_region)) {
		ELOG("UCSI read exceeds bounds of memory: offset(0x%x), length(0x%x)",
		     offset, length);
		return -1;
	}

	platform_memcpy(buf,
			(const void *)((uint8_t *)(&dev->ucsi_data) + offset),
			length);
	return length;
}

static int ppm_common_handle_control_message(struct ppm_common_device *dev,
					     const void *buf, size_t length)
{
	const uint8_t *cmd = (const uint8_t *)buf;
	uint8_t prev_cmd;
	uint8_t busy = 0;

	if (length > sizeof(struct ucsi_control)) {
		ELOG("Tried to send control message that is an invalid size (%d)",
		     (int)length);
		return -1;
	}

	/* If we're currently sending a command, we should immediately discard
	 * this call.
	 */
	{
		platform_mutex_lock(dev->ppm_lock);
		busy = dev->pending.command || dev->ucsi_data.cci.busy;
		prev_cmd = dev->ucsi_data.control.command;
		platform_mutex_unlock(dev->ppm_lock);
	}
	if (busy) {
		ELOG("Tried to send control message (cmd=0x%x) when one is already pending "
		     "(cmd=0x%x).",
		     cmd[0], prev_cmd);
		return -1;
	}

	/* If we didn't get a full CONTROL message, zero the region before
	 * copying.
	 */
	if (length != sizeof(struct ucsi_control)) {
		platform_memset(&dev->ucsi_data.control, 0,
				sizeof(struct ucsi_control));
	}
	platform_memcpy(&dev->ucsi_data.control, cmd, length);

	DLOG("Got valid control message: 0x%x (%s)", cmd[0],
	     ucsi_command_to_string(cmd[0]));

	/* Schedule command send. */
	{
		platform_mutex_lock(dev->ppm_lock);

		/* Mark command pending. */
		dev->pending.command = 1;
		platform_condvar_signal(dev->ppm_condvar);

		DLOG("Signaled pending command");

		platform_mutex_unlock(dev->ppm_lock);
	}

	return 0;
}

/*
 * Only allow writes into two regions:
 * - Control (to send commands)
 * - Message Out (to prepare data to send commands)
 *
 * A control message will result in an actual UCSI command being called if the
 * data is valid.
 *
 * A write into message in doesn't modify the PPM state but is often
 * a precursor to actually sending a control message. This will be used for fw
 * updates.
 *
 * Any writes into non-aligned offsets (except Message IN) will be discarded.
 */
static int ppm_common_write(struct ucsi_ppm_device *device, unsigned int offset,
			    const void *buf, size_t length)
{
	struct ppm_common_device *dev = (struct ppm_common_device *)device;
	bool valid_fixed_offset;

	if (!buf || length == 0) {
		ELOG("Invalid buffer (%p) or length (%x)", buf, length);
		return -1;
	}

	valid_fixed_offset = (offset == UCSI_VERSION_OFFSET) ||
			     (offset == UCSI_CCI_OFFSET) ||
			     (offset == UCSI_CONTROL_OFFSET);

	if (!valid_fixed_offset &&
	    !(offset >= UCSI_MESSAGE_OUT_OFFSET &&
	      offset < UCSI_MESSAGE_OUT_OFFSET + MESSAGE_OUT_SIZE)) {
		ELOG("UCSI can't write to invalid offset: 0x%x", offset);
		return -1;
	}

	/* Handle control messages */
	if (offset == UCSI_CONTROL_OFFSET) {
		return ppm_common_handle_control_message(dev, buf, length);
	}

	if (offset >= UCSI_MESSAGE_OUT_OFFSET &&
	    offset + length > UCSI_MESSAGE_OUT_OFFSET + MESSAGE_OUT_SIZE) {
		ELOG("UCSI write [0x%x ~ 0x%x] exceeds the "
		     "MESSAGE_OUT range [0x%x ~ 0x%x]",
		     offset, offset + length - 1, UCSI_MESSAGE_OUT_OFFSET,
		     UCSI_MESSAGE_OUT_OFFSET + MESSAGE_OUT_SIZE - 1);

		return -1;
	}

	/* Copy from input buffer to offset within MESSAGE_OUT. */
	platform_memcpy(dev->ucsi_data.message_out +
				(offset - UCSI_MESSAGE_OUT_OFFSET),
			buf, length);
	return 0;
}

static int ppm_common_register_notify(struct ucsi_ppm_device *device,
				      ucsi_ppm_notify *callback, void *context)
{
	struct ppm_common_device *dev = (struct ppm_common_device *)device;
	int ret = 0;

	/* Are we replacing the notify? */
	if (dev->opm_notify) {
		DLOG("Replacing existing notify function!");
		ret = 1;
	}

	dev->opm_notify = callback;
	dev->opm_context = context;

	return ret;
}

static int
ppm_common_register_platform_policy(struct ucsi_ppm_device *device,
				    ucsi_ppm_apply_platform_policy *callback,
				    void *context)
{
	struct ppm_common_device *dev = (struct ppm_common_device *)device;
	int ret = 0;

	if (dev->apply_platform_policy) {
		DLOG("Replacing platform policy callback!");
		ret = 1;
	}

	dev->apply_platform_policy = callback;
	dev->apply_platform_policy_context = context;

	return ret;
}

static void ppm_common_lpm_alert(struct ucsi_ppm_device *device, uint8_t lpm_id)
{
	struct ppm_common_device *dev = (struct ppm_common_device *)device;

	DLOG("LPM alert seen on connector %d!", lpm_id);

	platform_mutex_lock(dev->ppm_lock);

	if (lpm_id <= dev->num_ports) {
		/* Set async event and mark port status as not read. */
		dev->pending.async_event = 1;
		dev->last_connector_alerted = lpm_id;

		platform_condvar_signal(dev->ppm_condvar);
	} else {
		ELOG("Alert id out of range: %d (num_ports = %d)", lpm_id,
		     dev->num_ports);
	}

	platform_mutex_unlock(dev->ppm_lock);
}

static void ppm_common_cleanup(struct ucsi_ppm_driver *driver)
{
	if (driver->dev) {
		struct ppm_common_device *dev =
			(struct ppm_common_device *)driver->dev;

		DLOG("Cleaning up.");
		/* Signal clean up to waiting thread. */
		platform_mutex_lock(dev->ppm_lock);
		dev->cleaning_up = true;
		platform_condvar_signal(dev->ppm_condvar);
		platform_mutex_unlock(dev->ppm_lock);

		/* Wait for task to complete. */
		if (platform_task_complete(dev->ppm_task_handle) != 0) {
			ELOG("Failed to wait for ppm task to complete.");
		}

		platform_free(dev->ppm_condvar);
		platform_free(dev->ppm_lock);

		platform_free(driver->dev);
		driver->dev = NULL;
	}
}

struct ucsi_ppm_driver *ppm_open(const struct ucsi_pd_driver *pd_driver,
				 struct ucsiv3_get_connector_status_data *data,
				 const struct device *device)
{
	struct ppm_common_device *dev = NULL;
	struct ucsi_ppm_driver *drv = NULL;

	drv = platform_allocate_ppm();
	if (!drv)
		return NULL;

	dev = (struct ppm_common_device *)drv->dev;
	dev->pd = pd_driver;
	dev->num_ports = pd_driver->get_active_port_count(NULL);
	dev->per_port_status = data;
	dev->device = device;

	drv->init_and_wait = ppm_common_init_and_wait;
	drv->get_next_connector_status = ppm_common_get_next_connector_status;
	drv->read = ppm_common_read;
	drv->write = ppm_common_write;
	drv->register_notify = ppm_common_register_notify;
	drv->register_platform_policy = ppm_common_register_platform_policy;
	drv->lpm_alert = ppm_common_lpm_alert;
	drv->cleanup = ppm_common_cleanup;

	return drv;
}
