/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * TI TPS6699X Power Delivery Controller Driver
 */

#include <assert.h>
#include <string.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/smbus.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>
LOG_MODULE_REGISTER(tps6699x, CONFIG_USBC_LOG_LEVEL);
#include "tps6699x_cmd.h"
#include "tps6699x_reg.h"
#include "usbc/utils.h"

#include <drivers/pdc.h>
#include <timer.h>

#define INCBIN_PREFIX g_
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#include "third_party/incbin/incbin.h"
INCBIN(tps6699x_fw, "/mnt/host/source/src/platform/ec/zephyr/"
		    "drivers/usbc/tps6699x_19.8.0.bin");

#define DT_DRV_COMPAT ti_tps6699_pdc

/** @brief PDC IRQ EVENT bit */
#define PDC_IRQ_EVENT BIT(0)
/** @brief PDC COMMAND EVENT bit */
#define PDC_CMD_EVENT BIT(1)
/** @brief Requests the driver to enter the suspended state */
#define PDC_CMD_SUSPEND_REQUEST_EVENT BIT(2)

/**
 * @brief All raw_value data uses byte-0 for contains the register data was
 * written to, or read from, and byte-1 contains the length of said data. The
 * actual data starts at index 2
 */
#define RV_DATA_START 2

/**
 * @brief Number of TPS6699x ports detected
 */
#define NUM_PDC_TPS6699X_PORTS DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT)

/* TODO: b/323371550 */
BUILD_ASSERT(NUM_PDC_TPS6699X_PORTS <= 2,
	     "tps6699x driver supports a maximum of 2 ports");

/**
 * @brief PDC commands
 */
enum cmd_t {
	/** No command */
	CMD_NONE,
	/** CMD_TRIGGER_PDC_RESET */
	CMD_TRIGGER_PDC_RESET,
	/** Set Notification Enable */
	CMD_SET_NOTIFICATION_ENABLE,
	/** PDC Reset */
	CMD_PPM_RESET,
	/** Connector Reset */
	CMD_CONNECTOR_RESET,
	/** Get Capability */
	CMD_GET_CAPABILITY,
	/** Get Connector Capability */
	CMD_GET_CONNECTOR_CAPABILITY,
	/** Set UOR */
	CMD_SET_UOR,
	/** Set PDR */
	CMD_SET_PDR,
	/** Get PDOs */
	CMD_GET_PDOS,
	/** Get Connector Status */
	CMD_GET_CONNECTOR_STATUS,
	/** Get Error Status */
	CMD_GET_ERROR_STATUS,
	/** Get VBUS Voltage */
	CMD_GET_VBUS_VOLTAGE,
	/** Get IC Status */
	CMD_GET_IC_STATUS,
	/** Set CCOM */
	CMD_SET_CCOM,
	/** Read Power Level */
	CMD_READ_POWER_LEVEL,
	/** Get RDO */
	CMD_GET_RDO,
	/** Set Sink Path */
	CMD_SET_SINK_PATH,
	/** Get current Partner SRC PDO */
	CMD_GET_CURRENT_PARTNER_SRC_PDO,
	/** Set the Rp TypeC current */
	CMD_SET_TPC_RP,
	/** set Retimer into FW Update Mode */
	CMD_SET_RETIMER_FW_UPDATE_MODE,
	/** Get the cable properties */
	CMD_GET_CABLE_PROPERTY,
	/** Get VDO(s) of PDC, Cable, or Port partner */
	CMD_GET_VDO,
	/** CMD_GET_IDENTITY_DISCOVERY */
	CMD_GET_IDENTITY_DISCOVERY,
};

/**
 * @brief States of the main state machine
 */
enum state_t {
	/** Irq State */
	ST_IRQ,
	/** Init State */
	ST_INIT,
	/** Idle State */
	ST_IDLE,
	/** Error Recovery State */
	ST_ERROR_RECOVERY,
	/** TASK_WAIT */
	ST_TASK_WAIT,
	/** ST_SUSPENDED */
	ST_SUSPENDED,
};

/**
 * @brief PDC Config object
 */
struct pdc_config_t {
	/** I2C config */
	struct i2c_dt_spec i2c;
	/** pdc power path interrupt */
	struct gpio_dt_spec irq_gpios;
	/** connector number of this port */
	uint8_t connector_number;
	/** Notification enable bits */
	union notification_enable_t bits;
	/** Create thread function */
	void (*create_thread)(const struct device *dev);
};

/**
 * @brief PDC Data object
 */
struct pdc_data_t {
	/** State machine context */
	struct smf_ctx ctx;
	/** PDC device structure */
	const struct device *dev;
	/** Driver thread */
	k_tid_t thread;
	/** Driver thread's data */
	struct k_thread thread_data;
	/** GPIO interrupt callback */
	struct gpio_callback gpio_cb;
	/** Information about the PDC */
	struct pdc_info_t info;
	/** Init done flag */
	bool init_done;
	/** Callback data */
	void *cb_data;
	/** CCI Event */
	union cci_event_t cci_event;
	/** CC Event callback */
	struct pdc_callback *cc_cb;
	/** CC Event one-time callback. If it's NULL, cci_cb will be called. */
	struct pdc_callback *cc_cb_tmp;
	/** Asynchronous (CI) Event callbacks */
	sys_slist_t ci_cb_list;
	/** PDC status */
	union reg_status pdc_status;
	/** PDC interrupt */
	union reg_interrupt pdc_interrupt;
	/** PDC port control */
	union reg_port_control pdc_port_control;
	/** TypeC current */
	enum usb_typec_current_t tcc;
	/** Sink FET enable */
	bool snk_fet_en;
	/** Connector reset type */
	union connector_reset_t connector_reset;
	/** PDO Type */
	enum pdo_type_t pdo_type;
	/** PDO Offset */
	enum pdo_offset_t pdo_offset;
	/** Number of PDOS */
	uint8_t num_pdos;
	/** Port Partner PDO */
	bool port_partner_pdo;
	/** CCOM */
	enum ccom_t ccom;
	/** PDR */
	union pdr_t pdr;
	/** UOR */
	union uor_t uor;
	/** Pointer to user data */
	uint8_t *user_buf;
	/** Command mutex */
	struct k_mutex mtx;
	/** Vendor command to send */
	enum cmd_t cmd;
	/* VDO request list */
	enum vdo_type_t vdo_req_list[8];
	/* Request VDO */
	union get_vdo_t vdo_req;
	/* PDC event: Interrupt or Command */
	struct k_event pdc_event;
};

/**
 * @brief List of human readable state names for console debugging
 */
static const char *const state_names[] = {
	[ST_IRQ] = "IRQ",
	[ST_INIT] = "INIT",
	[ST_IDLE] = "IDLE",
	[ST_ERROR_RECOVERY] = "ERROR RECOVERY",
	[ST_TASK_WAIT] = "TASK_WAIT",
	[ST_SUSPENDED] = "SUSPENDED",
};

static const struct smf_state states[];

static void cmd_set_tpc_rp(struct pdc_data_t *data);
static void cmd_get_rdo(struct pdc_data_t *data);
static void cmd_get_ic_status(struct pdc_data_t *data);
static void cmd_get_vbus_voltage(struct pdc_data_t *data);
static void cmd_get_vdo(struct pdc_data_t *data);
static void cmd_get_identity_discovery(struct pdc_data_t *data);
static void task_gaid(struct pdc_data_t *data);
static void task_srdy(struct pdc_data_t *data);
static void task_ucsi(struct pdc_data_t *data,
		      enum ucsi_command_t ucsi_command);

/**
 * @brief PDC port data used in interrupt handler
 */
static struct pdc_data_t *pdc_data[NUM_PDC_TPS6699X_PORTS];

static enum state_t get_state(struct pdc_data_t *data)
{
	return data->ctx.current - &states[0];
}

static void set_state(struct pdc_data_t *data, const enum state_t next_state)
{
	smf_set_state(SMF_CTX(data), &states[next_state]);
}

/**
 * Atomic flag to suspend sending new commands to chip
 *
 * This flag is shared across driver instances.
 *
 * TODO(b/323371550) When more than one PDC is supported, this flag will need
 * to be tracked per-chip.
 */
static atomic_t suspend_comms_flag = ATOMIC_INIT(0);

static void suspend_comms(void)
{
	atomic_set(&suspend_comms_flag, 1);
}

static void enable_comms(void)
{
	atomic_set(&suspend_comms_flag, 0);
}

static bool check_comms_suspended(void)
{
	return atomic_get(&suspend_comms_flag) != 0;
}

static void print_current_state(struct pdc_data_t *data)
{
	struct pdc_config_t const *cfg = data->dev->config;

	LOG_INF("DR%d: %s", cfg->connector_number,
		state_names[get_state(data)]);
}

static void call_cci_event_cb(struct pdc_data_t *data)
{
	const struct pdc_config_t *cfg = data->dev->config;
	const union cci_event_t cci = data->cci_event;

	LOG_INF("C%d: CCI=0x%x", cfg->connector_number, cci.raw_value);

	/*
	 * CC and CI events are separately reported. So, we need to call only
	 * one callback or the other.
	 */
	if (cci.connector_change) {
		pdc_fire_callbacks(&data->ci_cb_list, data->dev, cci);
	} else if (data->cc_cb_tmp) {
		data->cc_cb_tmp->handler(data->dev, data->cc_cb_tmp, cci);
		data->cc_cb_tmp = NULL;
	} else if (data->cc_cb) {
		data->cc_cb->handler(data->dev, data->cc_cb, cci);
	}

	data->cci_event.raw_value = 0;
}

static void st_irq_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_irq_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_interrupt pdc_interrupt;
	int rv;
	int i;
	bool interrupt_pending = false;

	/* Read the pending interrupt events */
	rv = tps_rd_interrupt_event(&cfg->i2c, &pdc_interrupt);
	if (rv) {
		LOG_ERR("Read interrupt events failed");
		goto error_recovery;
	}

	/* All raw_value data uses byte-0 for contains the register data was
	 * written too, or read from, and byte-1 contains the length of said
	 * data. The actual data starts at index 2. */
	LOG_DBG("IRQ PORT %d", cfg->connector_number);
	for (i = RV_DATA_START; i < sizeof(union reg_interrupt); i++) {
		LOG_DBG("Byte%d: %02x", i - RV_DATA_START,
			pdc_interrupt.raw_value[i]);
		if (pdc_interrupt.raw_value[i]) {
			interrupt_pending = true;
		}
	}
	LOG_DBG("\n");

	if (interrupt_pending) {
		/* Set CCI EVENT for connector change */
		data->cci_event.connector_change =
			pdc_interrupt.plug_insert_or_removal;
		/* Set CCI EVENT for not supported */
		data->cci_event.not_supported =
			pdc_interrupt.not_supported_received;
		/* Set CCI EVENT for vendor defined indicator (informs subsystem
		 * that an interrupt occurred */
		data->cci_event.vendor_defined_indicator = 1;

		/* TODO(b/345783692): Handle other interrupt bits. */

		/* Clear the pending interrupt events */
		rv = tps_rw_interrupt_clear(&cfg->i2c, &pdc_interrupt,
					    I2C_MSG_WRITE);
		if (rv) {
			LOG_ERR("Clear interrupt events failed");
			goto error_recovery;
		}

		/* Inform the subsystem of the event */
		call_cci_event_cb(data);
	}

	/* All done, transition back to idle state */
	set_state(data, ST_IDLE);
	return;

error_recovery:
	set_state(data, ST_ERROR_RECOVERY);
}

static void st_init_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_init_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	/* Do not start executing commands if suspended */
	if (check_comms_suspended()) {
		set_state(data, ST_SUSPENDED);
		return;
	}

	/* Set PDC notifications */
	data->cmd = CMD_SET_NOTIFICATION_ENABLE;

	/* Transition to the idle state */
	set_state(data, ST_IDLE);
	return;
}

static void st_init_exit(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	/* Inform the driver that the init process is complete */
	/* TODO: Make sure this makes sense if the next state is suspend. It may
	 * be possible to remove ST_INIT entirely by doing this in the init
	 * function.
	 */
	data->init_done = true;
}

static void st_idle_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);

	/* Reset the command */
	data->cmd = CMD_NONE;
}

static void st_idle_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	uint32_t events;

	/* Wait for interrupt or a command to send */
	events = k_event_wait(&data->pdc_event,
			      (PDC_IRQ_EVENT | PDC_CMD_EVENT |
			       PDC_CMD_SUSPEND_REQUEST_EVENT),
			      false, K_FOREVER);

	if (check_comms_suspended()) {
		/* Do not start executing commands or processing IRQs if
		 * suspended. We don't need to check the event flag, it is
		 * only needed to wake this thread.
		 */
		set_state(data, ST_SUSPENDED);
		return;
	}

	if (events & PDC_IRQ_EVENT) {
		k_event_clear(&data->pdc_event, PDC_IRQ_EVENT);
		/* Handle interrupt */
		set_state(data, ST_IRQ);
		return;
	} else if (events & PDC_CMD_EVENT) {
		k_event_clear(&data->pdc_event, PDC_CMD_EVENT);
		/* Handle command */
		/* TODO(b/345783692): enum ucsi_command_t should be extended to
		 * contain vendor-defined commands. That way, switch statements
		 * like this can operate on that enum, and we won't need a bunch
		 * of driver code just to convert from generic commands to
		 * driver commands.
		 */
		switch (data->cmd) {
		case CMD_NONE:
			break;
		case CMD_TRIGGER_PDC_RESET:
			task_gaid(data);
			break;
		case CMD_SET_NOTIFICATION_ENABLE:
			task_ucsi(data, UCSI_SET_NOTIFICATION_ENABLE);
			break;
		case CMD_PPM_RESET:
			task_ucsi(data, UCSI_PPM_RESET);
			break;
		case CMD_CONNECTOR_RESET:
			task_ucsi(data, UCSI_CONNECTOR_RESET);
			break;
		case CMD_GET_CAPABILITY:
			task_ucsi(data, UCSI_GET_CAPABILITY);
			break;
		case CMD_GET_CONNECTOR_CAPABILITY:
			task_ucsi(data, UCSI_GET_CONNECTOR_CAPABILITY);
			break;
		case CMD_SET_UOR:
			task_ucsi(data, UCSI_SET_UOR);
			break;
		case CMD_SET_PDR:
			task_ucsi(data, UCSI_SET_PDR);
			break;
		case CMD_GET_PDOS:
			task_ucsi(data, UCSI_GET_PDOS);
			break;
		case CMD_GET_CONNECTOR_STATUS:
			task_ucsi(data, UCSI_GET_CONNECTOR_STATUS);
			break;
		case CMD_GET_ERROR_STATUS:
			task_ucsi(data, UCSI_GET_ERROR_STATUS);
			break;
		case CMD_GET_VBUS_VOLTAGE:
			cmd_get_vbus_voltage(data);
			break;
		case CMD_GET_IC_STATUS:
			cmd_get_ic_status(data);
			break;
		case CMD_SET_CCOM:
			task_ucsi(data, UCSI_SET_CCOM);
			break;
		case CMD_READ_POWER_LEVEL:
			task_ucsi(data, UCSI_READ_POWER_LEVEL);
			break;
		case CMD_GET_RDO:
			cmd_get_rdo(data);
			break;
		case CMD_SET_SINK_PATH:
			task_srdy(data);
			break;
		case CMD_GET_CURRENT_PARTNER_SRC_PDO:
			task_ucsi(data, UCSI_GET_PDOS);
			break;
		case CMD_SET_TPC_RP:
			cmd_set_tpc_rp(data);
			break;
		case CMD_SET_RETIMER_FW_UPDATE_MODE:
			task_ucsi(data, UCSI_SET_RETIMER_MODE);
			break;
		case CMD_GET_CABLE_PROPERTY:
			task_ucsi(data, UCSI_GET_CABLE_PROPERTY);
			break;
		case CMD_GET_VDO:
			cmd_get_vdo(data);
			break;
		case CMD_GET_IDENTITY_DISCOVERY:
			cmd_get_identity_discovery(data);
			break;
		}
	}
}

static void st_idle_exit(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	/* Clear the CCI EVENT */
	data->cci_event.raw_value = 0;
}

static void st_error_recovery_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_error_recovery_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	/* Don't continue trying if we are suspending communication */
	if (check_comms_suspended()) {
		set_state(data, ST_SUSPENDED);
		return;
	}

	/* TODO: Add proper error recovery */
	/* Currently this state is entered when an I2C command fails */

	/* Command has completed with an error */
	data->cci_event.command_completed = 1;
	data->cci_event.error = 1;

	/* Inform the system of the event */
	call_cci_event_cb(data);

	/* Transition to idle */
	set_state(data, ST_IDLE);
	return;
}

static void st_suspended_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_suspended_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	/* Stay here while suspended */
	if (check_comms_suspended()) {
		return;
	}

	set_state(data, ST_INIT);
}

static void cmd_set_tpc_rp(struct pdc_data_t *data)
{
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_port_control pdc_port_control;
	int rv;

	/* Read PDC port control */
	rv = tps_rw_port_control(&cfg->i2c, &pdc_port_control, I2C_MSG_READ);
	if (rv) {
		LOG_ERR("Read port control failed");
		goto error_recovery;
	}

	/* Modify */
	switch (data->tcc) {
	case TC_CURRENT_PPM_DEFINED:
		LOG_ERR("Unsupported type: TC_CURRENT_PPM_DEFINED");
		set_state(data, ST_IDLE);
		return;
	case TC_CURRENT_3_0A:
		pdc_port_control.typec_current = 2;
		break;
	case TC_CURRENT_1_5A:
		pdc_port_control.typec_current = 1;
		break;
	case TC_CURRENT_USB_DEFAULT:
		pdc_port_control.typec_current = 0;
		break;
	}

	/* Write PDC port control */
	rv = tps_rw_port_control(&cfg->i2c, &pdc_port_control, I2C_MSG_WRITE);
	if (rv) {
		LOG_ERR("Write port control failed");
		goto error_recovery;
	}

	/* Command has completed */
	data->cci_event.command_completed = 1;
	/* Inform the system of the event */
	call_cci_event_cb(data);

	/* Transition to idle state */
	set_state(data, ST_IDLE);
	return;

error_recovery:
	set_state(data, ST_ERROR_RECOVERY);
}

static void cmd_get_rdo(struct pdc_data_t *data)
{
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_active_rdo_contract active_rdo_contract;
	uint32_t *rdo = (uint32_t *)data->user_buf;
	int rv;

	if (data->user_buf == NULL) {
		LOG_ERR("Null buffer; can't read RDO");
		goto error_recovery;
	}

	rv = tps_rd_active_rdo_contract(&cfg->i2c, &active_rdo_contract);
	if (rv) {
		LOG_ERR("Failed to read active RDO");
		goto error_recovery;
	}

	*rdo = active_rdo_contract.rdo;

	/* TODO(b/345783692): Put command-completed logic in common code. */
	/* Command has completed */
	data->cci_event.command_completed = 1;
	/* Inform the system of the event */
	call_cci_event_cb(data);

	/* Transition to idle */
	set_state(data, ST_IDLE);
	return;

error_recovery:
	set_state(data, ST_ERROR_RECOVERY);
}

static void cmd_get_vdo(struct pdc_data_t *data)
{
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_received_identity_data_object received_identity_data_object;
	uint32_t *vdo = (uint32_t *)data->user_buf;
	int rv;

	if (data->vdo_req.vdo_origin == VDO_ORIGIN_SOP) {
		rv = tps_rd_received_sop_identity_data_object(
			&cfg->i2c, &received_identity_data_object);
		if (rv) {
			LOG_ERR("Failed to read partner identity ACK");
			goto error_recovery;
		}
	} else if (data->vdo_req.vdo_origin == VDO_ORIGIN_SOP_PRIME) {
		rv = tps_rd_received_sop_prime_identity_data_object(
			&cfg->i2c, &received_identity_data_object);
		if (rv) {
			LOG_ERR("Failed to read cable identity ACK");
			goto error_recovery;
		}
	} else {
		/* Unsupported */
		LOG_ERR("Unsupported VDO origin");
		goto error_recovery;
	}

	for (int i = 0; i < data->vdo_req.num_vdos; i++) {
		switch (data->vdo_req_list[i]) {
		case VDO_ID_HEADER:
			vdo[i] = received_identity_data_object.vdo[0];
			break;
		case VDO_CERT_STATE:
			vdo[i] = received_identity_data_object.vdo[1];
			break;
		case VDO_PRODUCT:
			vdo[i] = received_identity_data_object.vdo[2];
			break;
		default:
			/* Unsupported */
			vdo[i] = 0;
		}
	}

	data->cci_event.command_completed = 1;
	/* Inform the system of the event */
	call_cci_event_cb(data);

	/* Transition to idle state */
	set_state(data, ST_IDLE);
	return;

error_recovery:
	set_state(data, ST_ERROR_RECOVERY);
}

static void cmd_get_identity_discovery(struct pdc_data_t *data)
{
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_received_identity_data_object received_identity_data_object;
	bool *disc_state = (bool *)data->user_buf;
	int rv;

	if (data->vdo_req.vdo_origin == VDO_ORIGIN_SOP) {
		rv = tps_rd_received_sop_identity_data_object(
			&cfg->i2c, &received_identity_data_object);
		if (rv) {
			LOG_ERR("Failed to read partner VDO");
			goto error_recovery;
		}
	} else if (data->vdo_req.vdo_origin == VDO_ORIGIN_SOP_PRIME) {
		rv = tps_rd_received_sop_prime_identity_data_object(
			&cfg->i2c, &received_identity_data_object);
		if (rv) {
			LOG_ERR("Failed to read cable VDO");
			goto error_recovery;
		}
	} else {
		/* Unsupported */
		LOG_ERR("Unsupported VDO origin");
		goto error_recovery;
	}

	*disc_state = (received_identity_data_object.response_type == 1) ?
			      true :
			      false;

	data->cci_event.command_completed = 1;
	/* Inform the system of the event */
	call_cci_event_cb(data);

	/* Transition to idle state */
	set_state(data, ST_IDLE);
	return;

error_recovery:
	set_state(data, ST_ERROR_RECOVERY);
}

static void cmd_get_ic_status(struct pdc_data_t *data)
{
	struct pdc_info_t *info = (struct pdc_info_t *)data->user_buf;
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_version version;
	union reg_tx_identity tx_identity;
	int rv;

	if (data->user_buf == NULL) {
		LOG_ERR("Null user buffer; can't read IC status");
		goto error_recovery;
	}

	rv = tps_rd_version(&cfg->i2c, &version);
	if (rv) {
		LOG_ERR("Failed to read version");
		goto error_recovery;
	}

	rv = tps_rw_tx_identity(&cfg->i2c, &tx_identity, I2C_MSG_READ);
	if (rv) {
		LOG_ERR("Failed to read Tx identity");
		goto error_recovery;
	}

	/* TI Is running flash code */
	info->is_running_flash_code = 1;

	/* TI FW main version */
	info->fw_version = version.version;

	/* TI VID PID
	 * (little-endian) */
	info->vid_pid = (*(uint16_t *)tx_identity.vendor_id) << 2 |
			*(uint16_t *)tx_identity.product_id;

	/* TI Running flash bank offset */
	info->running_in_flash_bank = 0;

	/* TI PD Revision (big-endian) */
	info->pd_revision = 0x0000;

	/* TI PD Version (big-endian) */
	info->pd_version = 0x0000;

	/* Command has completed */
	data->cci_event.command_completed = 1;
	/* Inform the system of the event */
	call_cci_event_cb(data);

	/* Transition to idle state */
	set_state(data, ST_IDLE);
	return;

error_recovery:
	set_state(data, ST_ERROR_RECOVERY);
}

static void cmd_get_vbus_voltage(struct pdc_data_t *data)
{
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_adc_results adc_results;

	uint16_t *vbus = (uint16_t *)data->user_buf;
	int rv;

	if (data->user_buf == NULL) {
		LOG_ERR("Null user buffer; can't read VBUS voltage");
		goto error_recovery;
	}

	rv = tps_rd_adc_results(&cfg->i2c, &adc_results);
	if (rv) {
		LOG_ERR("Failed to read ADC results");
		goto error_recovery;
	}

	*vbus = cfg->connector_number ? adc_results.pa_vbus :
					adc_results.pb_vbus;

	/* Command has completed */
	data->cci_event.command_completed = 1;
	/* Inform the system of the event */
	call_cci_event_cb(data);

	set_state(data, ST_IDLE);
	return;

error_recovery:
	set_state(data, ST_ERROR_RECOVERY);
}

static int write_task_cmd(struct pdc_config_t const *cfg,
			  enum command_task task, union reg_data *cmd_data)
{
	union reg_command cmd;
	int rv;

	cmd.command = task;

	if (cmd_data) {
		rv = tps_rw_data_for_cmd1(&cfg->i2c, cmd_data, I2C_MSG_WRITE);
		if (rv) {
			return rv;
		}
	}

	rv = tps_rw_command_for_i2c1(&cfg->i2c, &cmd, I2C_MSG_WRITE);

	return rv;
}

static void task_gaid(struct pdc_data_t *data)
{
	struct pdc_config_t const *cfg = data->dev->config;
	int rv;

	rv = write_task_cmd(cfg, COMMAND_TASK_GAID, NULL);
	if (rv) {
		set_state(data, ST_ERROR_RECOVERY);
		return;
	}

	set_state(data, ST_TASK_WAIT);
	return;
}

static void task_srdy(struct pdc_data_t *data)
{
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_data cmd_data;
	union reg_autonegotiate_sink an_snk;
	int rv;

	rv = tps_rw_autonegotiate_sink(&cfg->i2c, &an_snk, I2C_MSG_READ);
	if (rv) {
		LOG_ERR("Failed to read auto-negotiate sink");
		goto error_recovery;
	}

	an_snk.auto_neg_rdo_priority = 1;
	an_snk.no_capability_mismatch = 0;
	an_snk.auto_enable_standby_srdy = 1;

	rv = tps_rw_autonegotiate_sink(&cfg->i2c, &an_snk, I2C_MSG_WRITE);
	if (rv) {
		LOG_ERR("Failed to read auto-negotiate sink");
		goto error_recovery;
	}

	if (data->snk_fet_en) {
		/* Enable Sink FET */
		cmd_data.data[0] = cfg->connector_number ? 0x02 : 0x03;
		rv = write_task_cmd(cfg, COMMAND_TASK_SRDY, &cmd_data);
	} else {
		/* Disable Sink FET */
		rv = write_task_cmd(cfg, COMMAND_TASK_SRYR, NULL);
	}

	if (rv) {
		LOG_ERR("Failed to write command");
		goto error_recovery;
	}

	/* Transition to wait state */
	set_state(data, ST_TASK_WAIT);
	return;

error_recovery:
	set_state(data, ST_ERROR_RECOVERY);
}

static void task_ucsi(struct pdc_data_t *data, enum ucsi_command_t ucsi_command)
{
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_data cmd_data;
	int rv;

	memset(cmd_data.data, 0, sizeof(cmd_data.data));
	/* Byte 0: UCSI Command Code */
	cmd_data.data[0] = ucsi_command;
	/* Byte 1: Data length per UCSI spec */
	cmd_data.data[1] = 0;
	/* Connector Number: Byte 2, bits 6:0. Bit 7 is reserved */
	cmd_data.data[2] = cfg->connector_number + 1;

	/* TODO(b/345783692): The bit shifts in this function come from the
	 * awkward mapping between the structures in ucsi_v3.h and the TI
	 * command format, but this can probably be cleaned up a bit.
	 */
	switch (data->cmd) {
	case CMD_CONNECTOR_RESET:
		cmd_data.data[2] |= (data->connector_reset.reset_type << 7);
		break;
	case CMD_GET_PDOS:
		/* Partner PDO: Byte 2, bits 7 */
		cmd_data.data[2] |= (data->port_partner_pdo << 7);
		/* PDO Offset: Byte 3, bits 7:0 */
		cmd_data.data[3] = data->pdo_offset;
		/* Number of PDOs: Byte 4, bits 1:0 */
		cmd_data.data[4] = data->num_pdos;
		/* Source or Sink PDOSs: Byte 4, bits 2 */
		cmd_data.data[4] |= (data->pdo_type << 2);
		/* Source Capabilities Type: Byte 4, bits 4:3 */
		/* cmd_data.data[4] |= (0x00 << 3); */
		break;
	case CMD_SET_CCOM:
		switch (data->ccom) {
		case CCOM_RP:
			cmd_data.data[2] |= (1 << 7);
			break;
		case CCOM_RD:
			cmd_data.data[3] = 1;
			break;
		case CCOM_DRP:
			cmd_data.data[3] = 2;
			break;
		}
		break;
	case CMD_SET_UOR:
		if (data->uor.swap_to_dfp) {
			cmd_data.data[2] |= (1 << 7);
		} else if (data->uor.swap_to_ufp) {
			cmd_data.data[3] = 1;
		} else if (data->uor.accept_dr_swap) {
			cmd_data.data[3] = 2;
		}
		break;
	case CMD_SET_PDR:
		if (data->pdr.swap_to_src) {
			cmd_data.data[2] |= (1 << 7);
		} else if (data->pdr.swap_to_snk) {
			cmd_data.data[3] = 1;
		} else if (data->pdr.accept_pr_swap) {
			cmd_data.data[3] = 2;
		}
		break;
	case CMD_SET_NOTIFICATION_ENABLE:
		*(uint32_t *)&cmd_data.data[2] = cfg->bits.raw_value;
		break;
	default:
		/* Data doesn't need processed */
		break;
	}

	rv = write_task_cmd(cfg, COMMAND_TASK_UCSI, &cmd_data);

	/* Transition to wait state */
	set_state(data, ST_TASK_WAIT);
	return;
}

static void st_task_wait_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_task_wait_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_command cmd;
	union reg_data cmd_data;
	uint8_t offset;
	uint32_t len;
	int rv;

	/* Read command register for the particular port */
	rv = tps_rw_command_for_i2c1(&cfg->i2c, &cmd, I2C_MSG_READ);
	if (rv) {
		/* I2C transaction failed */
		LOG_ERR("Failed to read command");
		goto error_recovery;
	}

	/*
	 * Wait for command to complete:
	 *  1) command is set to 0 when command is sent
	 *  2) command is set to "!CMD" for unknown command
	 */
	if (cmd.command && cmd.command != COMMAND_TASK_NO_COMMAND) {
		return;
	}

	/*
	 * Read status of command for particular port:
	 *  1) cmd_data is set to zero on success
	 *  2) cmd_data is set to an error code on failure
	 */
	rv = tps_rw_data_for_cmd1(&cfg->i2c, &cmd_data, I2C_MSG_READ);
	if (rv) {
		/* I2C transaction failed */
		LOG_ERR("Failed to read command");
		goto error_recovery;
	}

	/* Data byte offset 0 is the return error code */
	if (cmd.command || cmd_data.data[0] != 0) {
		/* Command has completed with error */
		data->cci_event.error = 1;
	}

	switch (data->cmd) {
	case CMD_GET_CONNECTOR_CAPABILITY:
		offset = 1;
		len = sizeof(union connector_capability_t);
		break;
	case CMD_GET_CONNECTOR_STATUS:
		offset = 1;
		len = sizeof(union connector_status_t);
		/* TODO(b/345783692): Cache result */
		break;
	case CMD_GET_CABLE_PROPERTY:
		offset = 1;
		len = sizeof(union cable_property_t);
		break;
	case CMD_GET_ERROR_STATUS:
		offset = 2;
		len = cmd_data.data[1];
		break;
	case CMD_GET_PDOS: {
		len = cmd_data.data[1];
		offset = 2;
		break;
	}
	default:
		/* No data for this command */
		len = 0;
	}

	if (data->user_buf && len) {
		if (data->cci_event.error) {
			memset(data->user_buf, 0, len);
		} else {
			/* No preprocessing needed for the user data */
			memcpy(data->user_buf, &cmd_data.data[offset], len);
		}
	}

	/* Command has completed */
	data->cci_event.command_completed = 1;
	/* Inform the system of the event */
	call_cci_event_cb(data);

	/* Transition to idle state */
	set_state(data, ST_IDLE);
	return;

error_recovery:
	set_state(data, ST_ERROR_RECOVERY);
}

/* Populate state table */
static const struct smf_state states[] = {
	[ST_IRQ] = SMF_CREATE_STATE(st_irq_entry, st_irq_run, NULL, NULL, NULL),
	[ST_INIT] = SMF_CREATE_STATE(st_init_entry, st_init_run, st_init_exit,
				     NULL, NULL),
	[ST_IDLE] = SMF_CREATE_STATE(st_idle_entry, st_idle_run, st_idle_exit,
				     NULL, NULL),
	[ST_ERROR_RECOVERY] = SMF_CREATE_STATE(st_error_recovery_entry,
					       st_error_recovery_run, NULL,
					       NULL, NULL),
	[ST_TASK_WAIT] = SMF_CREATE_STATE(st_task_wait_entry, st_task_wait_run,
					  NULL, NULL, NULL),
	[ST_SUSPENDED] = SMF_CREATE_STATE(st_suspended_entry, st_suspended_run,
					  NULL, NULL, NULL),
};

static int tps_post_command(const struct device *dev, enum cmd_t cmd,
			    void *user_buf)
{
	struct pdc_data_t *data = dev->data;

	/* TODO(b/345783692): Double check this logic. */
	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (k_mutex_lock(&data->mtx, K_MSEC(100)) == 0) {
		if (data->cmd != CMD_NONE) {
			k_mutex_unlock(&data->mtx);
			return -EBUSY;
		}

		data->user_buf = user_buf;
		data->cmd = cmd;

		k_mutex_unlock(&data->mtx);
		k_event_post(&data->pdc_event, PDC_CMD_EVENT);
	} else {
		return -EBUSY;
	}

	return 0;
}

static int tps_manage_callback(const struct device *dev,
			       struct pdc_callback *callback, bool set)
{
	struct pdc_data_t *const data = dev->data;

	return pdc_manage_callbacks(&data->ci_cb_list, callback, set);
}

static int tps_ack_cc_ci(const struct device *dev,
			 union conn_status_change_bits_t ci, bool cc,
			 uint16_t vendor_defined)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	/* TODO(b/345783692): Implement */

	return 0;
}

static int tps_get_ucsi_version(const struct device *dev, uint16_t *version)
{
	if (version == NULL) {
		return -EINVAL;
	}

	*version = UCSI_VERSION;

	return 0;
}

static int tps_set_handler_cb(const struct device *dev,
			      struct pdc_callback *callback)
{
	struct pdc_data_t *data = dev->data;

	data->cc_cb = callback;

	return 0;
}

static int tps_read_power_level(const struct device *dev)
{
	return tps_post_command(dev, CMD_READ_POWER_LEVEL, NULL);
}

static int tps_reconnect(const struct device *dev)
{
	/* TODO */
	return 0;
}

static int tps_pdc_reset(const struct device *dev)
{
	return tps_post_command(dev, CMD_TRIGGER_PDC_RESET, NULL);
}

static int tps_connector_reset(const struct device *dev,
			       union connector_reset_t type)
{
	struct pdc_data_t *data = dev->data;

	data->connector_reset = type;

	return tps_post_command(dev, CMD_CONNECTOR_RESET, NULL);
}

static int tps_set_power_level(const struct device *dev,
			       enum usb_typec_current_t tcc)
{
	struct pdc_data_t *data = dev->data;

	data->tcc = tcc;

	return tps_post_command(dev, CMD_SET_TPC_RP, NULL);
}

static int tps_set_sink_path(const struct device *dev, bool en)
{
	struct pdc_data_t *data = dev->data;

	data->snk_fet_en = en;

	return tps_post_command(dev, CMD_SET_SINK_PATH, NULL);
}

static int tps_get_capability(const struct device *dev,
			      struct capability_t *caps)
{
	return tps_post_command(dev, CMD_GET_CAPABILITY, (uint8_t *)caps);
}

static int tps_get_connector_capability(const struct device *dev,
					union connector_capability_t *caps)
{
	return tps_post_command(dev, CMD_GET_CONNECTOR_CAPABILITY, caps);
}

static int tps_get_connector_status(const struct device *dev,
				    union connector_status_t *cs)
{
	return tps_post_command(dev, CMD_GET_CONNECTOR_STATUS, cs);
}

static int tps_get_error_status(const struct device *dev,
				union error_status_t *es)
{
	return tps_post_command(dev, CMD_GET_ERROR_STATUS, es);
}

static int tps_set_rdo(const struct device *dev, uint32_t rdo)
{
	/* TODO */
	return 0;
}

static int tps_get_rdo(const struct device *dev, uint32_t *rdo)
{
	return tps_post_command(dev, CMD_GET_RDO, rdo);
}

static int tps_get_pdos(const struct device *dev, enum pdo_type_t pdo_type,
			enum pdo_offset_t pdo_offset, uint8_t num_pdos,
			bool port_partner_pdo, uint32_t *pdos)
{
	struct pdc_data_t *data = dev->data;

	/* TODO(b/345783692): Make sure these accesses don't need to be
	 * synchronized.
	 */

	data->pdo_type = pdo_type;
	data->pdo_offset = pdo_offset;
	data->num_pdos = num_pdos;
	data->port_partner_pdo = port_partner_pdo;

	return tps_post_command(dev, CMD_GET_PDOS, pdos);
}

static int tps_get_info(const struct device *dev, struct pdc_info_t *info,
			bool live)
{
	return tps_post_command(dev, CMD_GET_IC_STATUS, info);
}

static int tps_get_bus_info(const struct device *dev,
			    struct pdc_bus_info_t *info)
{
	/* TODO */
	return 0;
}

static int tps_get_vbus_voltage(const struct device *dev, uint16_t *voltage)
{
	return tps_post_command(dev, CMD_GET_VBUS_VOLTAGE, voltage);
}

static int tps_set_ccom(const struct device *dev, enum ccom_t ccom)
{
	struct pdc_data_t *data = dev->data;

	data->ccom = ccom;

	return tps_post_command(dev, CMD_SET_CCOM, NULL);
}

static int tps_set_uor(const struct device *dev, union uor_t uor)
{
	struct pdc_data_t *data = dev->data;

	data->uor = uor;

	return tps_post_command(dev, CMD_SET_UOR, NULL);
}

static int tps_set_pdr(const struct device *dev, union pdr_t pdr)
{
	struct pdc_data_t *data = dev->data;

	data->pdr = pdr;

	return tps_post_command(dev, CMD_SET_PDR, NULL);
}

static int tps_get_current_pdo(const struct device *dev, uint32_t *pdo)
{
	/* TODO */
	return 0;
}

static int tps_get_cable_property(const struct device *dev,
				  union cable_property_t *cp)
{
	if (cp == NULL) {
		return -EINVAL;
	}

	return tps_post_command(dev, CMD_GET_CABLE_PROPERTY, cp);
}

static int tps_get_vdo(const struct device *dev, union get_vdo_t vdo_req,
		       uint8_t *vdo_req_list, uint32_t *vdo)
{
	struct pdc_data_t *data = dev->data;

	if (vdo == NULL || vdo_req_list == NULL) {
		return -EINVAL;
	}

	for (int i = 0; i < vdo_req.num_vdos; i++) {
		data->vdo_req_list[i] = vdo_req_list[i];
	}
	data->vdo_req = vdo_req;

	return tps_post_command(dev, CMD_GET_VDO, vdo);
}

static int tps_get_identity_discovery(const struct device *dev,
				      bool *disc_state)
{
	if (disc_state == NULL) {
		return -EINVAL;
	}

	return tps_post_command(dev, CMD_GET_IDENTITY_DISCOVERY, disc_state);
}

static int tps_set_comms_state(const struct device *dev, bool comms_active)
{
	struct pdc_data_t *data = dev->data;

	if (comms_active) {
		/* Re-enable communications. Clearing the suspend flag will
		 * trigger a reset. Note: if the driver is in the disabled
		 * state due to a previous comms failure, it will remain
		 * disabled. (Thus, suspending/resuming comms on a disabled
		 * PDC driver is a no-op)
		 */
		enable_comms();

	} else {
		/** Allow 3 seconds for the driver to suspend itself. */
		const int suspend_timeout_usec = 3 * USEC_PER_SEC;

		/* Request communication to be stopped. This allows in-progress
		 * operations to complete first.
		 */
		suspend_comms();

		/* Signal the driver with the suspend request event in case the
		 * thread is blocking on an event to process.
		 */
		k_event_post(&data->pdc_event, PDC_CMD_SUSPEND_REQUEST_EVENT);

		/* Wait for driver to enter the suspended state */
		if (!WAIT_FOR((get_state(data) == ST_SUSPENDED),
			      suspend_timeout_usec, k_sleep(K_MSEC(50)))) {
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static bool tps_is_init_done(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;

	return data->init_done;
}

static const struct pdc_driver_api_t pdc_driver_api = {
	.is_init_done = tps_is_init_done,
	.get_ucsi_version = tps_get_ucsi_version,
	.reset = tps_pdc_reset,
	.connector_reset = tps_connector_reset,
	.get_capability = tps_get_capability,
	.get_connector_capability = tps_get_connector_capability,
	.set_ccom = tps_set_ccom,
	.set_uor = tps_set_uor,
	.set_pdr = tps_set_pdr,
	.set_sink_path = tps_set_sink_path,
	.get_connector_status = tps_get_connector_status,
	.get_pdos = tps_get_pdos,
	/* TODO(b/345783692): Implement set_pdos */
	.get_rdo = tps_get_rdo,
	.set_rdo = tps_set_rdo,
	.get_error_status = tps_get_error_status,
	.get_vbus_voltage = tps_get_vbus_voltage,
	.get_current_pdo = tps_get_current_pdo,
	.set_handler_cb = tps_set_handler_cb,
	.read_power_level = tps_read_power_level,
	.get_info = tps_get_info,
	.get_bus_info = tps_get_bus_info,
	.set_power_level = tps_set_power_level,
	.reconnect = tps_reconnect,
	.get_cable_property = tps_get_cable_property,
	.get_vdo = tps_get_vdo,
	.get_identity_discovery = tps_get_identity_discovery,
	.manage_callback = tps_manage_callback,
	.ack_cc_ci = tps_ack_cc_ci,
	.set_comms_state = tps_set_comms_state,
};

static void pdc_interrupt_callback(const struct device *dev,
				   struct gpio_callback *cb, uint32_t pins)
{
	/* All ports share a common interrupt, so post a PDC_IRQ_EVENT to all
	 * drivers. The driver IRQ state will determine if it has a pending
	 * interrupt */
	for (int i = 0; i < NUM_PDC_TPS6699X_PORTS; i++) {
		k_event_post(&pdc_data[i]->pdc_event, PDC_IRQ_EVENT);
	}
}

/* LCOV_EXCL_START - non-shipping code */
struct tfu_initiate {
	uint16_t num_blocks;
	uint16_t data_block_size;
	uint16_t timeout_secs;
	uint16_t broadcast_address;
} __attribute__((__packed__));

struct tfu_download {
	uint16_t num_blocks;
	uint16_t data_block_size;
	uint16_t timeout_secs;
	uint16_t broadcast_address;
} __attribute__((__packed__));

/*
 * Complete uses custom values for switch/copy instead of true false.
 * Write these values to the register instead of true/false.
 */
#define DO_SWITCH 0xAC
#define DO_COPY 0xAC
struct tfu_complete {
	uint8_t do_switch;
	uint8_t do_copy;
} __attribute__((__packed__));

struct tfu_query {
	uint8_t bank;
	uint8_t cmd;
} __attribute__((__packed__));

struct tps6699x_tfu_query_output {
	uint8_t result;
	uint8_t tfu_state;
	uint8_t complete_image;
	uint16_t blocks_written;
	uint8_t header_block_status;
	uint8_t per_block_status[12];
	uint8_t num_header_bytes_written;
	uint8_t num_data_bytes_written;
	uint8_t num_appconfig_bytes_written;
} __attribute__((__packed__));

/* Largest chunk we want to read before writing. */
#define MAX_READ_CHUNK_SIZE 0x4000

/* Send metadata with TFUi */
#define METADATA_OFFSET 0x4
#define METADATA_LENGTH 0x8

/* Stream header with i2c_stream AFTER TFUi */
#define HEADER_BLOCK_OFFSET 0xC
#define HEADER_BLOCK_LENGTH 0x800

/* Size of fw not including appconfig and header block is at this offset. */
#define FW_SIZE_OFFSET 0x4F8

/* Stream data blocks after you write metadata with TFUd. */
#define DATA_REGION_OFFSET 0x80C
#define DATA_BLOCK_SIZE 0x4000
#define DATA_METADATA_LENGTH 0x8
#define DATA_METADATA_OFFSET_AT(block)                        \
	(((DATA_BLOCK_SIZE + DATA_METADATA_LENGTH) * block) + \
	 DATA_REGION_OFFSET)
#define DATA_AT(block) (DATA_METADATA_OFFSET_AT(block) + DATA_METADATA_LENGTH)
#define TFUD_CHUNK_SIZE (64)

#define MAX_NUM_BLOCKS 12

#define GAID_MAGIC_VALUE 0xAC
union gaid_params_t {
	struct {
		uint8_t switch_banks;
		uint8_t copy_banks;
	} __packed;
	uint8_t raw[2];
};

static int get_and_print_device_info(const struct device *dev)
{
	struct pdc_config_t const *cfg = dev->config;
	union reg_version version;
	int rv;

	rv = tps_rd_version(&cfg->i2c, &version);
	if (rv != 0) {
		return rv;
	}

	return 0;
}

/**
 * @brief Convert a 4CC command/task enum to a NUL-terminated printable string
 *
 * @param task The 4CC task enum
 * @param str_out Pointer to a char array capable of holding 5 characters where
 *        the output will be written to.
 */
static void command_task_to_string(enum command_task task, char str_out[5])
{
	if (task == 0) {
		strncpy(str_out, "0000", sizeof(*str_out));
		return;
	}

	str_out[0] = (((uint32_t)task) >> 0);
	str_out[1] = (((uint32_t)task) >> 8);
	str_out[2] = (((uint32_t)task) >> 16);
	str_out[3] = (((uint32_t)task) >> 24);
	str_out[4] = '\0';
}

static int run_task_sync(const struct device *dev, enum command_task task,
			 union reg_data *cmd_data, uint8_t *user_buf)
{
	struct pdc_config_t const *cfg = dev->config;
	union reg_command cmd;
	int rv;
	int attempts = 0;
	char task_str[5];

	command_task_to_string(task, task_str);

	/* Set up self-contained synchronous command call */
	if (cmd_data) {
		rv = tps_rw_data_for_cmd1(&cfg->i2c, cmd_data, I2C_MSG_WRITE);
		if (rv) {
			LOG_ERR("Cannot set command data for '%s' (%d)",
				task_str, rv);
			return rv;
		}
	}

	cmd.command = task;

	rv = tps_rw_command_for_i2c1(&cfg->i2c, &cmd, I2C_MSG_WRITE);
	if (rv) {
		LOG_ERR("Cannot set command for '%s' (%d)", task_str, rv);
		return rv;
	}

	/* Poll for successful completion */
	while (1) {
		k_sleep(K_USEC(200));

		rv = tps_rw_command_for_i2c1(&cfg->i2c, &cmd, I2C_MSG_READ);
		if (rv) {
			LOG_ERR("Cannot poll command status for '%s' (%d)",
				task_str, rv);
			return rv;
		}

		if (cmd.command == 0) {
			/* Command complete */
			break;
		} else if (cmd.command == 0x444d4321) {
			/* Unknown command ("!CMD") */
			LOG_ERR("Command '%s' is invalid", task_str);
			return -1;
		}

		if (attempts > 500) {
			/* 100 ms allowed max */
			LOG_ERR("Command '%s' timed out", task_str);
			return -ETIMEDOUT;
		}

		attempts++;
	}

	LOG_INF("Command '%s' finished...", task_str);

	/* Read out success code */
	union reg_data cmd_data_check;

	rv = tps_rw_data_for_cmd1(&cfg->i2c, &cmd_data_check, I2C_MSG_READ);
	if (rv) {
		LOG_ERR("Cannot get command result status for '%s' (%d)",
			task_str, rv);
		return rv;
	}

	/* Data byte offset 0 is the return error code */
	if (cmd_data_check.data[0] != 0) {
		LOG_ERR("Command '%s' failed. Chip says %02x", task_str,
			cmd_data_check.data[0]);
		return rv;
	}

	LOG_ERR("Command '%s' succeeded!!", task_str);

	/* Provide response data to user if a buffer is provided */
	if (user_buf != NULL) {
		memcpy(user_buf, cmd_data_check.data,
		       sizeof(cmd_data_check.data));
	}

	k_sleep(K_USEC(500));

	return 0;
}

static int do_reset_pdc(const struct device *dev)
{
	union reg_data cmd_data;
	union gaid_params_t params;
	int rv;

	/* Default behavior is to switch banks. */
	params.switch_banks = GAID_MAGIC_VALUE;
	params.copy_banks = 0;

	memcpy(cmd_data.data, &params, sizeof(params));

	rv = run_task_sync(dev, COMMAND_TASK_GAID, &cmd_data, NULL);

	if (rv == 0) {
		k_msleep(1000);
	}

	return rv;
}

/* Simply point to the offset in the file */
static int read_file_offset(int offset, const uint8_t **buf, int len)
{
	/* Exceed size of file. */
	if (offset + len > g_tps6699x_fw_size) {
		return -1;
	}

	*buf = &g_tps6699x_fw_data[offset];

	return len;
}

int get_appconfig_offsets(uint16_t num_data_blocks, int *metadata_offset,
			  int *data_block_offset)
{
	int bytes_read;
	uint32_t *fw_size;

	bytes_read = read_file_offset(
		FW_SIZE_OFFSET, (const uint8_t **)&fw_size, sizeof(fw_size));

	if (bytes_read < 0) {
		LOG_ERR("Failed to read firmware size from binary: %d",
			bytes_read);
		return -1;
	}

	// The Application Configuration is stored at the following offset
	// FirmwareImageSize (Which excludes Header and App Config) + 0x800
	// (Header Block Size)
	// + (8 (Meta Data for Each Block including Header block) * Number of
	// Data block + 1)
	// + 4 (File Identifier)
	*metadata_offset = *fw_size + HEADER_BLOCK_LENGTH +
			   (DATA_METADATA_LENGTH * (num_data_blocks + 1)) +
			   METADATA_OFFSET;

	*data_block_offset = *metadata_offset + DATA_METADATA_LENGTH;

	return 0;
}

static int tfud_block(const struct device *dev, uint8_t *fbuf,
		      int metadata_offset, int data_block_offset)
{
	struct pdc_config_t const *cfg = dev->config;
	struct tfu_download *tfud;
	union reg_data cmd_data;
	int bytes_read;
	uint8_t rbuf[64];
	int ret;

	/* First read the block metadata. */
	bytes_read = read_file_offset(metadata_offset, (const uint8_t **)&tfud,
				      DATA_METADATA_LENGTH);

	if (bytes_read < 0 || bytes_read != DATA_METADATA_LENGTH) {
		LOG_ERR("Failed to read block metadata. Wanted %d, got %d",
			DATA_METADATA_LENGTH, bytes_read);
		return -1;
	}

	LOG_INF("TFUd Info: nblks=%u, blksize=%u, timeout=%us, addr=%x",
		tfud->num_blocks, tfud->data_block_size, tfud->timeout_secs,
		tfud->broadcast_address);

	if (tfud->data_block_size > DATA_BLOCK_SIZE) {
		LOG_ERR("TFUd block size too big: 0x%x (max is 0x%x)",
			tfud->data_block_size, DATA_BLOCK_SIZE);
		return -1;
	}

	memcpy(&cmd_data.data, tfud, sizeof(*tfud));
	ret = run_task_sync(dev, COMMAND_TASK_TFUD, &cmd_data, rbuf);

	if (ret < 0 || rbuf[0] != 0) {
		LOG_ERR("Failed to run TFUd. Ret=%d, rbuf[0] = %u", ret,
			rbuf[0]);
		return -1;
	}

	bytes_read = read_file_offset(data_block_offset,
				      (const uint8_t **)&fbuf,
				      tfud->data_block_size);

	if (bytes_read < 0 || bytes_read != tfud->data_block_size) {
		LOG_ERR("Failed to read block. Wanted %d, got %d",
			tfud->data_block_size, bytes_read);
		return -1;
	}

	/* Stream the data block */
	ret = tps_stream_data(&cfg->i2c, tfud->broadcast_address, fbuf,
			      tfud->data_block_size);
	if (ret) {
		LOG_ERR("Downloading data block failed (%d)", ret);
		return -1;
	}

	/* Wait 150ms after each data block. */
	k_msleep(150);

	return 0;
}

int tfuq_run(const struct device *dev, uint8_t *output)
{
	union reg_data cmd_data;
	struct tfu_query *tfuq = (struct tfu_query *)cmd_data.data;
	tfuq->bank = 0;
	tfuq->cmd = 0;

	return run_task_sync(dev, COMMAND_TASK_TFUQ, &cmd_data, output);
};

/**
 * @brief Temporary EC-based FW update routine
 *
 * @param dev Device pointer for the PDC to update (needed only once per chip)
 */
int tps6699x_do_firmware_update(const struct device *dev)
{
	int appconfig_metadata_offset, appconfig_data_offset;
	struct tfu_initiate *tfui;
	union reg_data cmd_data;
	struct pdc_config_t const *cfg = dev->config;
	int bytes_read = 0;
	uint8_t rbuf[64];
	int ret = 0;
	uint8_t *fbuf;

	bool dry_run = false;

	/*
	 * Flow of operations for firmware update:
	 *   - TFUs: Start TFU process (puts device into bootloader mode)
	 *   - TFUi: Initiate firmware update. This also validates header.
	 *   - TFUd - Loop to download firmware.
	 *   - TFUc - Complete firmware update.
	 *
	 * To cancel or query current status, you can also do the following:
	 *   - TFUq: Query the TFU process
	 *   - TFUe: Cancel back to initial download state.
	 */

	/********************
	 * TFUs stage - enter bootloader code
	 */

	union reg_command cmd = {
		.command = COMMAND_TASK_TFUS,
	};

	ret = tps_rw_command_for_i2c1(&cfg->i2c, &cmd, I2C_MSG_WRITE);
	if (ret) {
		LOG_ERR("Cannot write TFUs command (%d)", ret);
		return ret;
	}

	/* Wait 500ms for entry to bootloader mode */
	k_msleep(500);

	/* Check mode register for "F211" value */
	union reg_mode mode;

	ret = tps_rd_mode(&cfg->i2c, &mode);
	if (ret) {
		LOG_ERR("Cannot read mode reg (%d)", ret);
		return ret;
	}

	if (memcmp("F211", mode.data, sizeof(mode.data)) != 0) {
		LOG_ERR("TFUs failed! Mode is %02x %02x %02x %02x",
			mode.data[0], mode.data[1], mode.data[2], mode.data[3]);
		return -1;
	}

	LOG_INF("TFUs complete, got F211");

	/********************
	 * TFUi stage
	 */

	/* Read metadata header. */
	bytes_read = read_file_offset(METADATA_OFFSET, (const uint8_t **)&tfui,
				      METADATA_LENGTH);
	if (bytes_read < 0) {
		LOG_ERR("Failed to read metadata. Wanted %d, got %d",
			METADATA_LENGTH, bytes_read);
		goto cleanup;
	}

	LOG_INF("Sending TFUi.");

	/* Write TFUi with header. */
	memcpy(cmd_data.data, tfui, sizeof(*tfui));
	ret = run_task_sync(dev, COMMAND_TASK_TFUI, &cmd_data, rbuf);

	if (ret < 0 || rbuf[0] != 0) {
		LOG_ERR("Failed to run TFUi. Ret=%d, rbuf[0]=%u", ret, rbuf[0]);
		goto cleanup;
	}

	/* Read metadata buffer and stream at address given. */
	bytes_read = read_file_offset(HEADER_BLOCK_OFFSET,
				      (const uint8_t **)&fbuf,
				      HEADER_BLOCK_LENGTH);
	if (bytes_read < 0 || bytes_read != HEADER_BLOCK_LENGTH) {
		LOG_ERR("Failed to read header stream. Wanted %d but got %d",
			HEADER_BLOCK_LENGTH, bytes_read);
		goto cleanup;
	}

	LOG_INF("Streaming header to broadcast addr $%x",
		tfui->broadcast_address);

	ret = tps_stream_data(&cfg->i2c, tfui->broadcast_address, fbuf,
			      HEADER_BLOCK_LENGTH);
	if (ret) {
		LOG_ERR("Streaming header failed (%d)", ret);
		goto cleanup;
	}

	LOG_INF("TFUi complete and header streamed. Number of blocks: %u",
		tfui->num_blocks);

	/* Wait 200ms after streaming header to do data block. */
	k_msleep(200);

	/* Iterate through all image blocks. */
	for (int block = 0; block < tfui->num_blocks; ++block) {
		LOG_INF("Flashing block %d (%d/%u)", block, block + 1,
			tfui->num_blocks);
		ret = tfud_block(dev, fbuf, DATA_METADATA_OFFSET_AT(block),
				 DATA_AT(block));
		if (ret) {
			LOG_ERR("Error while flashing block (%d)", ret);
			goto cleanup;
		}
	}

	LOG_INF("Flashing appconfig to block %d", tfui->num_blocks);
	if (get_appconfig_offsets(tfui->num_blocks, &appconfig_metadata_offset,
				  &appconfig_data_offset) < 0) {
		LOG_ERR("Failed to get appconfig offsets!");
		goto cleanup;
	}

	ret = tfud_block(dev, fbuf, appconfig_metadata_offset,
			 appconfig_data_offset);
	if (ret) {
		LOG_ERR("Failed to write appconfig block (%d)", ret);
		goto cleanup;
	}

	/* Check the status with TFUq */
	struct tps6699x_tfu_query_output *tfuq;

	ret = tfuq_run(dev, rbuf);
	if (ret) {
		LOG_ERR("Could not query FW update status (%d)", ret);
		goto cleanup;
	}

	tfuq = (struct tps6699x_tfu_query_output *)rbuf;
	LOG_INF("Block bitmask: 0x%04x", tfuq->blocks_written);
	LOG_INF("TFU State: 0x%02x, Complete Image: 0x%02x", tfuq->tfu_state,
		tfuq->complete_image);
	LOG_INF("Header Bytes: %u, Data Bytes: %u, Appconfig Bytes: %u",
		tfuq->num_header_bytes_written, tfuq->num_data_bytes_written,
		tfuq->num_appconfig_bytes_written);

	for (int i = 0; i < MAX_NUM_BLOCKS; i++) {
		LOG_INF("  Block %d status: 0x%08x", i,
			tfuq->per_block_status[i]);
	}

	/* Only commit changes if not dry run */
	if (!dry_run) {
		/* Finish update with a TFU copy. */
		struct tfu_complete tfuc;
		tfuc.do_switch = 0;
		tfuc.do_copy = DO_COPY;

		LOG_INF("Running TFUc [Switch: 0x%02x, Copy: 0x%02x]",
			tfuc.do_switch, tfuc.do_copy);
		memcpy(cmd_data.data, &tfuc, sizeof(tfuc));
		ret = run_task_sync(dev, COMMAND_TASK_TFUC, &cmd_data, rbuf);

		if (ret < 0 || rbuf[0] != 0) {
			LOG_ERR("Failed 4cc task with result %d, rbuf[0] = %d",
				ret, rbuf[0]);
			goto cleanup;
		}

		LOG_INF("TFUq bytes [Success: 0x%02x, State: 0x%02x, Complete: 0x%02x]",
			rbuf[1], rbuf[2], rbuf[3]);

		/* Wait 1600ms for reset to complete. */
		k_msleep(1600);

		/* Confirm we're on the new firmware now. */
		get_and_print_device_info(dev);
	} else {
		LOG_INF("Exiting dry run with TFUe");
		ret = run_task_sync(dev, COMMAND_TASK_TFUE, NULL, rbuf);
		if (ret < 0 || rbuf[0] != 0) {
			LOG_ERR("Cleaning up resulted in ret=%d and result byte=0x%02x",
				ret, rbuf[0]);
		}

		do_reset_pdc(dev);
		get_and_print_device_info(dev);
	}

	return 0;

cleanup:
	ret = run_task_sync(dev, COMMAND_TASK_TFUE, NULL, rbuf);

	LOG_ERR("Cleaning up resulted in ret=%d and result byte=0x%02x", ret,
		rbuf[0]);

	/* Reset and confirm we restored original firmware. */
	do_reset_pdc(dev);
	get_and_print_device_info(dev);

	return -1;
}
/* LCOV_EXCL_STOP - non-shipping code */

static int pdc_init(const struct device *dev)
{
	const struct pdc_config_t *cfg = dev->config;
	struct pdc_data_t *data = dev->data;
	int rv;

	rv = i2c_is_ready_dt(&cfg->i2c);
	if (rv < 0) {
		LOG_ERR("device %s not ready", cfg->i2c.bus->name);
		return -ENODEV;
	}

	rv = gpio_is_ready_dt(&cfg->irq_gpios);
	if (rv < 0) {
		LOG_ERR("device %s not ready", cfg->irq_gpios.port->name);
		return -ENODEV;
	}

	rv = gpio_pin_configure_dt(&cfg->irq_gpios, GPIO_INPUT);
	if (rv < 0) {
		LOG_ERR("Unable to configure GPIO");
		return rv;
	}

	gpio_init_callback(&data->gpio_cb, pdc_interrupt_callback,
			   BIT(cfg->irq_gpios.pin));

	rv = gpio_add_callback(cfg->irq_gpios.port, &data->gpio_cb);
	if (rv < 0) {
		LOG_ERR("Unable to add callback");
		return rv;
	}

	rv = gpio_pin_interrupt_configure_dt(&cfg->irq_gpios,
					     GPIO_INT_EDGE_FALLING);
	if (rv < 0) {
		LOG_ERR("Unable to configure interrupt");
		return rv;
	}

	k_event_init(&data->pdc_event);
	k_mutex_init(&data->mtx);

	data->cmd = CMD_NONE;
	data->dev = dev;
	pdc_data[cfg->connector_number] = data;
	data->init_done = false;

	/* Set initial state */
	smf_set_initial(SMF_CTX(data), &states[ST_INIT]);

	/* Create the thread for this port */
	cfg->create_thread(dev);

	/* Trigger an interrupt on startup */
	k_event_post(&data->pdc_event, PDC_IRQ_EVENT);

	LOG_INF("TI TPS6699X PDC DRIVER FOR PORT %d", cfg->connector_number);

	return 0;
}

static void tps_thread(void *dev, void *unused1, void *unused2)
{
	struct pdc_data_t *data = ((const struct device *)dev)->data;

	while (1) {
		smf_run_state(SMF_CTX(data));
		/* TODO(b/345783692): Consider waiting for an event with a
		 * timeout to avoid high interrupt-handling latency.
		 */
		k_sleep(K_MSEC(50));
	}
}

#define PDC_DEFINE(inst)                                                       \
	K_THREAD_STACK_DEFINE(thread_stack_area_##inst,                        \
			      CONFIG_USBC_PDC_TPS6699X_STACK_SIZE);            \
                                                                               \
	static void create_thread_##inst(const struct device *dev)             \
	{                                                                      \
		struct pdc_data_t *data = dev->data;                           \
                                                                               \
		data->thread = k_thread_create(                                \
			&data->thread_data, thread_stack_area_##inst,          \
			K_THREAD_STACK_SIZEOF(thread_stack_area_##inst),       \
			tps_thread, (void *)dev, 0, 0,                         \
			CONFIG_USBC_PDC_TPS6699X_THREAD_PRIORITY, K_ESSENTIAL, \
			K_NO_WAIT);                                            \
		k_thread_name_set(data->thread, "TPS6699X" STRINGIFY(inst));   \
	}                                                                      \
                                                                               \
	static struct pdc_data_t pdc_data_##inst;                              \
                                                                               \
	/* TODO(b/345783692): Make sure interrupt enable bits match the events \
	 * we need to respond to.                                              \
	 */                                                                    \
	static const struct pdc_config_t pdc_config##inst = {                  \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                             \
		.irq_gpios = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),           \
		.connector_number =                                            \
			USBC_PORT_FROM_DRIVER_NODE(DT_DRV_INST(inst), pdc),    \
		.bits.command_completed = 0, /* Reserved on TI */              \
		.bits.external_supply_change = 1,                              \
		.bits.power_operation_mode_change = 1,                         \
		.bits.attention = 0,                                           \
		.bits.fw_update_request = 0,                                   \
		.bits.provider_capability_change_supported = 1,                \
		.bits.negotiated_power_level_change = 1,                       \
		.bits.pd_reset_complete = 1,                                   \
		.bits.support_cam_change = 1,                                  \
		.bits.battery_charging_status_change = 1,                      \
		.bits.security_request_from_port_partner = 0,                  \
		.bits.connector_partner_change = 1,                            \
		.bits.power_direction_change = 1,                              \
		.bits.set_retimer_mode = 0,                                    \
		.bits.connect_change = 1,                                      \
		.bits.error = 1,                                               \
		.bits.sink_path_status_change = 1,                             \
		.create_thread = create_thread_##inst,                         \
	};                                                                     \
                                                                               \
	DEVICE_DT_INST_DEFINE(inst, pdc_init, NULL, &pdc_data_##inst,          \
			      &pdc_config##inst, POST_KERNEL,                  \
			      CONFIG_APPLICATION_INIT_PRIORITY,                \
			      &pdc_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PDC_DEFINE)
