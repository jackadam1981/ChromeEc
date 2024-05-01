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
LOG_MODULE_REGISTER(pdc_tps, LOG_LEVEL_INF);
#include "tps6699x_cmd.h"
#include "tps6699x_reg.h"
#include "usbc/utils.h"

#include <drivers/pdc.h>

#define DT_DRV_COMPAT ti_tps6699_pdc

/** @brief PDC IRQ EVENT bit */
#define PDC_IRQ_EVENT BIT(0)
/** @brief PDC COMMAND EVENT bit */
#define PDC_CMD_EVENT BIT(1)

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
	/** No command 0*/
	CMD_NONE,
	/** CMD_TRIGGER_PDC_RESET 1*/
	CMD_TRIGGER_PDC_RESET,
	/** PDC Enable 2*/
	CMD_VENDOR_ENABLE,
	/** Set Notification Enable 3*/
	CMD_SET_NOTIFICATION_ENABLE,
	/** PDC Reset 4*/
	CMD_PPM_RESET,
	/** Connector Reset 5*/
	CMD_CONNECTOR_RESET,
	/** Get Capability 6*/
	CMD_GET_CAPABILITY,
	/** Get Connector Capability 7*/
	CMD_GET_CONNECTOR_CAPABILITY,
	/** Set UOR 8*/
	CMD_SET_UOR,
	/** Set PDR 9*/
	CMD_SET_PDR,
	/** Get PDOs 10*/
	CMD_GET_PDOS,
	/** Get Connector Status 11*/
	CMD_GET_CONNECTOR_STATUS,
	/** Get Error Status 12*/
	CMD_GET_ERROR_STATUS,
	/** Get VBUS Voltage 13*/
	CMD_GET_VBUS_VOLTAGE,
	/** Get IC Status 14*/
	CMD_GET_IC_STATUS,
	/** Set CCOM 15*/
	CMD_SET_CCOM,
	/** Read Power Level 16*/
	CMD_READ_POWER_LEVEL,
	/** Get RDO 17*/
	CMD_GET_RDO,
	/** Set RDO 18*/
	CMD_SET_RDO,
	/** Set Sink Path 19*/
	CMD_SET_SINK_PATH,
	/** Get current Partner SRC PDO 20*/
	CMD_GET_CURRENT_PARTNER_SRC_PDO,
	/** Set the Rp TypeC current 21*/
	CMD_SET_TPC_RP,
	/** TypeC reconnect 22*/
	CMD_SET_TPC_RECONNECT,
	/** set Retimer into FW Update Mode 23*/
	CMD_SET_RETIMER_FW_UPDATE_MODE,
	/** Get the cable properties 24*/
	CMD_GET_CABLE_PROPERTY,
	/** Get VDO(s) of PDC, Cable, or Port partner 25*/
	CMD_GET_VDO,
	/** CMD_GET_IDENTITY_DISCOVERY 26*/
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
	/** TASK_UCSI */
	ST_TASK_UCSI,
	/** TASK_WAIT */
	ST_TASK_WAIT,
	/** TASK GAID */
	ST_TASK_GAID,
	/** TASK_SRDY */
	ST_TASK_SRDY,
	/** PDC Enable */
	ST_CMD_VENDOR_ENABLE,
	/** Set Notification Enable */
	ST_CMD_SET_NOTIFICATION_ENABLE,
	/** Get Error Status */
	ST_CMD_GET_ERROR_STATUS,
	/** Get VBUS Voltage */
	ST_CMD_GET_VBUS_VOLTAGE,
	/** Get IC Status */
	ST_CMD_GET_IC_STATUS,
	/** Read Power Level */
	ST_CMD_READ_POWER_LEVEL,
	/** Get RDO */
	ST_CMD_GET_RDO,
	/** Set RDO */
	ST_CMD_SET_RDO,
	/** Get current Partner SRC PDO */
	ST_CMD_GET_CURRENT_PARTNER_SRC_PDO,
	/** Set the Rp TypeC current */
	ST_CMD_SET_TPC_RP,
	/** TypeC reconnect */
	ST_CMD_SET_TPC_RECONNECT,
	/** set Retimer into FW Update Mode */
	ST_CMD_SET_RETIMER_FW_UPDATE_MODE,
	/** ST_CMD_GET_VDO */
	ST_CMD_GET_VDO,
	/** ST_CMD_GET_IDENTITY_DISCOVERY */
	ST_CMD_GET_IDENTITY_DISCOVERY,
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
	/** CCI callback */
	pdc_cci_handler_cb_t cci_cb;
	/** Callback data */
	void *cb_data;
	/** CCI Event */
	union cci_event_t cci_event;
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
	/** UCSI command to send */
	enum ucsi_command_t ucsi_command;
	/** Vendor command to send */
	enum cmd_t cmd;
	/* VDO request list */
	enum vdo_type_t vdo_req_list[8];
	/* Request VDO */
	union get_vdo_t vdo_req;
	/* PDC event: Interrupt or Command */
	struct k_event pdc_event;
};

static const char *const ucsi_cmd_names[] = {
	[UCSI_PPM_RESET] = "PPM_RESET",
	[UCSI_CANCEL] = "CANCEL",
	[UCSI_CONNECTOR_RESET] = "CONNECTOR_RESET",
	[UCSI_ACK_CC_CI] = "ACK_CC_CI",
	[UCSI_SET_NOTIFICATION_ENABLE] = "SET_NOTIFICATION_ENABLE",
	[UCSI_GET_CAPABILITY] = "GET_CAPABILITY",
	[UCSI_GET_CONNECTOR_CAPABILITY] = "GET_CONNECTOR_CAPABILITY",
	[UCSI_SET_CCOM] = "SET_CCOM",
	[UCSI_SET_UOR] = "SET_UOR",
	[UCSI_SET_PDR] = "SET_PDR",
	[UCSI_GET_ALTERNATE_MODES] = "GET_ALTERNATE_MODES",
	[UCSI_GET_CAM_SUPPORTED] = "GET_CAM_SUPPORTED",
	[UCSI_GET_CURRENT_CAM] = "GET_CURRENT_CAM",
	[UCSI_SET_NEW_CAM] = "SET_NEW_CAM",
	[UCSI_GET_PDOS] = "GET_PDOS",
	[UCSI_GET_CABLE_PROPERTY] = "GET_CABLE_PROPERTY",
	[UCSI_GET_CONNECTOR_STATUS] = "GET_CONNECTOR_STATUS",
	[UCSI_GET_ERROR_STATUS] = "GET_ERROR_STATUS",
	[UCSI_SET_POWER_LEVEL] = "SET_POWER_LEVEL",
	[UCSI_GET_PD_MESSAGE] = "GET_PD_MESSAGE",
	[UCSI_GET_ATTENTION_VDO] = "GET_ATTENTION_VDO",
	[UCSI_GET_CAM_CS] = "GET_CAM_CS",
	[UCSI_LPM_FW_UPDATE_REQUEST] = "LPM_FW_UPDATE_REQUEST",
	[UCSI_SECURITY_REQUEST] = "SECURITY_REQUEST",
	[UCSI_SET_RETIMER_MODE] = "SET_RETIMER_MODE",
	[UCSI_SET_SINK_PATH] = "SET_SINK_PATH",
	[UCSI_SET_PDOS] = "SET_PDOS",
	[UCSI_READ_POWER_LEVEL] = "READ_POWER_LEVEL",
	[UCSI_CHUNKING_SUPPORT] = "CHUNKING_SUPPORTED",
	[UCSI_VENDOR_DEFINED_COMMAND] = "VENDOR_DEFINED",
	[UCSI_SET_USB] = "SET_USB",
	[UCSI_GET_LPM_PPM_INFO] = "GET_LPM_PPM_INFO",
};

/**
 * @brief List of human readable state names for console debugging
 */
static const char *const state_names[] = {
	[ST_IRQ] = "IRQ",
	[ST_INIT] = "INIT",
	[ST_IDLE] = "IDLE",
	[ST_ERROR_RECOVERY] = "ERROR RECOVERY",

	[ST_TASK_UCSI] = "CMD_UCSI",
	[ST_TASK_WAIT] = "CMD_WAIT",
	[ST_TASK_GAID] = "CMD_TRIGGER_PDC_RESET",
	[ST_CMD_VENDOR_ENABLE] = "CMD_VENDOR_ENABLE",
	[ST_CMD_SET_NOTIFICATION_ENABLE] = "CMD_SET_NOTIFICATION_ENABLE",
	[ST_CMD_GET_ERROR_STATUS] = "CMD_GET_ERROR_STATUS",
	[ST_CMD_GET_VBUS_VOLTAGE] = "CMD_GET_VBUS_VOLTAGE",
	[ST_CMD_GET_IC_STATUS] = "CMD_GET_IC_STATUS",
	[ST_CMD_READ_POWER_LEVEL] = "CMD_READ_POWER_LEVEL",
	[ST_CMD_GET_RDO] = "CMD_GET_RDO",
	[ST_CMD_SET_RDO] = "CMD_SET_RDO",
	[ST_TASK_SRDY] = "CMD_SET_SINK_PATH",
	[ST_CMD_GET_CURRENT_PARTNER_SRC_PDO] =
		"CMD_GET_CURRENT_PARTNER_SRC_PDO",
	[ST_CMD_SET_TPC_RP] = "CMD_SET_TPC_RP",
	[ST_CMD_SET_TPC_RECONNECT] = "CMD_SET_TPC_RECONNECT",
	[ST_CMD_SET_RETIMER_FW_UPDATE_MODE] = "CMD_SET_RETIMER_FW_UPDATE_MODE",
	[ST_CMD_GET_VDO] = "CMD_GET_VDO",
	[ST_CMD_GET_IDENTITY_DISCOVERY] = "CMD_GET_IDENTITY_DISCOVERY",
};

static const struct device *irq_shared_port;
static int irq_share_pin;
static bool irq_init_done;
static const struct smf_state states[];

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

static void print_current_state(struct pdc_data_t *data)
{
	struct pdc_config_t const *cfg = data->dev->config;

	if (get_state(data) == ST_TASK_UCSI) {
		LOG_INF("DR%d: %s(%s)", cfg->connector_number,
			state_names[get_state(data)],
			ucsi_cmd_names[data->cmd]);
	} else {
		LOG_INF("DR%d: %s", cfg->connector_number,
			state_names[get_state(data)]);
	}
}

static void call_cci_event_cb(struct pdc_data_t *data)
{
	struct pdc_config_t const *cfg = data->dev->config;

	if (data->cci_cb) {
		LOG_INF("C%d: cci_event_cb event=0x%x", cfg->connector_number,
			data->cci_event.raw_value);
		data->cci_cb(data->cci_event, data->cb_data);
		data->cci_event.raw_value = 0;
	}
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

	/* Read the pending interrupt events */
	rv = tps_rd_interrupt_event(&cfg->i2c, cfg->connector_number,
				    &pdc_interrupt);
	if (rv) {
		set_state(data, ST_ERROR_RECOVERY);
		return;
	}

	for (i = 2; i < sizeof(union reg_interrupt); i++) {
		if (pdc_interrupt.raw_value[i]) {
			/* This port has a pending interrupt */
			break;
		}
	}

	if (i < sizeof(union reg_interrupt)) {
		/* Set CCI EVENT for connector change */
		data->cci_event.connector_change =
			pdc_interrupt.plug_insert_or_removal;
		/* Set CCI EVENT for not supported */
		data->cci_event.not_supported =
			pdc_interrupt.not_supported_received;
		/* Set CCI EVENT for vendor defined indicator (informs subsystem
		 * that an interrupt occurred */
		data->cci_event.vendor_defined_indicator = 1;

		/* Clear the pending interrupt events */
		rv = tps_rw_interrupt_clear(&cfg->i2c, cfg->connector_number,
					    &pdc_interrupt, I2C_MSG_WRITE);
		if (rv) {
			set_state(data, ST_ERROR_RECOVERY);
			return;
		}

		/* Inform the subsystem of the event */
		call_cci_event_cb(data);
	}

	/* All done, transition back to idle state */
	set_state(data, ST_IDLE);
	return;
}

static void st_irq_exit(void *o)
{
}

static void st_init_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_init_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	/* TODO: Initialize the device */

	/* Transition to the idle state */
	set_state(data, ST_IDLE);
	return;
}

static void st_init_exit(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	/* Inform the driver that the init process is complete */
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
	events = k_event_wait(&data->pdc_event, (PDC_IRQ_EVENT | PDC_CMD_EVENT),
			      false, K_FOREVER);

	if (events & PDC_IRQ_EVENT) {
		k_event_clear(&data->pdc_event, PDC_IRQ_EVENT);
		/* Handle interrupt */
		set_state(data, ST_IRQ);
		return;
	} else if (events & PDC_CMD_EVENT) {
		k_event_clear(&data->pdc_event, PDC_CMD_EVENT);
		/* Handle command */
		switch (data->cmd) {
		case CMD_NONE:
			break;
		case CMD_TRIGGER_PDC_RESET:
			set_state(data, ST_TASK_GAID);
			return;
		case CMD_VENDOR_ENABLE:
			set_state(data, ST_CMD_VENDOR_ENABLE);
			return;
		case CMD_SET_NOTIFICATION_ENABLE:
			set_state(data, ST_CMD_SET_NOTIFICATION_ENABLE);
			return;
		case CMD_PPM_RESET:
			data->ucsi_command = UCSI_PPM_RESET;
			set_state(data, ST_TASK_UCSI);
			return;
		case CMD_CONNECTOR_RESET:
			data->ucsi_command = UCSI_CONNECTOR_RESET;
			set_state(data, ST_TASK_UCSI);
			return;
		case CMD_GET_CAPABILITY:
			data->ucsi_command = UCSI_GET_CAPABILITY;
			set_state(data, ST_TASK_UCSI);
			return;
		case CMD_GET_CONNECTOR_CAPABILITY:
			data->ucsi_command = UCSI_GET_CONNECTOR_CAPABILITY;
			set_state(data, ST_TASK_UCSI);
			return;
		case CMD_SET_UOR:
			data->ucsi_command = UCSI_SET_UOR;
			set_state(data, ST_TASK_UCSI);
			return;
		case CMD_SET_PDR:
			data->ucsi_command = UCSI_SET_PDR;
			set_state(data, ST_TASK_UCSI);
			return;
		case CMD_GET_PDOS:
			data->ucsi_command = UCSI_GET_PDOS;
			set_state(data, ST_TASK_UCSI);
			return;
		case CMD_GET_CONNECTOR_STATUS:
			data->ucsi_command = UCSI_GET_CONNECTOR_STATUS;
			set_state(data, ST_TASK_UCSI);
			return;
		case CMD_GET_ERROR_STATUS:
			set_state(data, ST_CMD_GET_ERROR_STATUS);
			return;
		case CMD_GET_VBUS_VOLTAGE:
			set_state(data, ST_CMD_GET_VBUS_VOLTAGE);
			return;
		case CMD_GET_IC_STATUS:
			set_state(data, ST_CMD_GET_IC_STATUS);
			return;
		case CMD_SET_CCOM:
			data->ucsi_command = UCSI_SET_CCOM;
			set_state(data, ST_TASK_UCSI);
			return;
		case CMD_READ_POWER_LEVEL:
			set_state(data, ST_CMD_READ_POWER_LEVEL);
			return;
		case CMD_GET_RDO:
			set_state(data, ST_CMD_GET_RDO);
			return;
		case CMD_SET_RDO:
			set_state(data, ST_CMD_SET_RDO);
			return;
		case CMD_SET_SINK_PATH:
			set_state(data, ST_TASK_SRDY);
			return;
		case CMD_GET_CURRENT_PARTNER_SRC_PDO:
			set_state(data, ST_CMD_GET_CURRENT_PARTNER_SRC_PDO);
			return;
		case CMD_SET_TPC_RP:
			set_state(data, ST_CMD_SET_TPC_RP);
			return;
		case CMD_SET_TPC_RECONNECT:
			set_state(data, ST_CMD_SET_TPC_RECONNECT);
			return;
		case CMD_SET_RETIMER_FW_UPDATE_MODE:
			set_state(data, ST_CMD_SET_RETIMER_FW_UPDATE_MODE);
			return;
		case CMD_GET_CABLE_PROPERTY:
			data->ucsi_command = UCSI_GET_CABLE_PROPERTY;
			set_state(data, ST_TASK_UCSI);
			return;
		case CMD_GET_VDO:
			set_state(data, ST_CMD_GET_VDO);
			return;
		case CMD_GET_IDENTITY_DISCOVERY:
			set_state(data, ST_CMD_GET_IDENTITY_DISCOVERY);
			return;
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

static void st_cmd_set_tpc_rp_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_cmd_set_tpc_rp_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_port_control pdc_port_control;
	int rv;

	/* Read PDC port control */
	rv = tps_rw_port_control(&cfg->i2c, &pdc_port_control, I2C_MSG_READ);
	if (rv) {
		set_state(data, ST_ERROR_RECOVERY);
		return;
	}

	/* Modify */
	switch (data->tcc) {
	case TC_CURRENT_PPM_DEFINED:
		LOG_INF("Unsupported type: TC_CURRENT_PPM_DEFINED");
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
		set_state(data, ST_ERROR_RECOVERY);
		return;
	}

	/* Command has completed */
	data->cci_event.command_completed = 1;
	/* Inform the system of the event */
	call_cci_event_cb(data);

	/* Transition to idle state */
	set_state(data, ST_IDLE);
	return;
}

static void st_cmd_get_rdo_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_cmd_get_rdo_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_active_rdo_contract active_rdo_contract;
	uint32_t *rdo = (uint32_t *)data->user_buf;
	int rv;

	if (data->user_buf == NULL) {
		set_state(data, ST_ERROR_RECOVERY);
		return;
	}

	rv = tps_rd_active_rdo_contract(&cfg->i2c, &active_rdo_contract);
	if (rv) {
		set_state(data, ST_ERROR_RECOVERY);
		return;
	}

	*rdo = active_rdo_contract.rdo;
	/* Command has completed */
	data->cci_event.command_completed = 1;
	/* Inform the system of the event */
	call_cci_event_cb(data);

	/* Transition to idle */
	set_state(data, ST_IDLE);
	return;
}

static void st_cmd_get_vdo_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_cmd_get_vdo_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_received_identity_data_object received_identity_data_object;
	uint32_t *vdo = (uint32_t *)data->user_buf;
	int rv;

	if (data->vdo_req.vdo_origin == VDO_ORIGIN_SOP) {
		rv = tps_rd_received_sop_identity_data_object(
			&cfg->i2c, &received_identity_data_object);
		if (rv) {
			set_state(data, ST_ERROR_RECOVERY);
			return;
		}
	} else if (data->vdo_req.vdo_origin == VDO_ORIGIN_SOP_PRIME) {
		rv = tps_rd_received_sop_prime_identity_data_object(
			&cfg->i2c, &received_identity_data_object);
		if (rv) {
			set_state(data, ST_ERROR_RECOVERY);
			return;
		}
	} else {
		/* Unsupported */
		set_state(data, ST_ERROR_RECOVERY);
		return;
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
}

static void st_cmd_get_identity_discovery_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_cmd_get_identity_discovery_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_received_identity_data_object received_identity_data_object;
	bool *disc_state = (bool *)data->user_buf;
	int rv;

	if (data->vdo_req.vdo_origin == VDO_ORIGIN_SOP) {
		rv = tps_rd_received_sop_identity_data_object(
			&cfg->i2c, &received_identity_data_object);
		if (rv) {
			set_state(data, ST_ERROR_RECOVERY);
			return;
		}
	} else if (data->vdo_req.vdo_origin == VDO_ORIGIN_SOP_PRIME) {
		rv = tps_rd_received_sop_prime_identity_data_object(
			&cfg->i2c, &received_identity_data_object);
		if (rv) {
			set_state(data, ST_ERROR_RECOVERY);
			return;
		}
	} else {
		/* Unsupported */
		set_state(data, ST_ERROR_RECOVERY);
		return;
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
}

static void st_cmd_get_ic_status_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_cmd_get_ic_status_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	struct pdc_info_t *info = (struct pdc_info_t *)data->user_buf;

	if (data->user_buf == NULL) {
		set_state(data, ST_ERROR_RECOVERY);
		return;
	}

	/* TODO: Set to proper values */

	/* TI Is running flash code: Byte 1 */
	info->is_running_flash_code = 1;

	/* TI FW main version: Byte4, Byte5, Byte6 */
	info->fw_version = 0x000000;

	/* TI VID PID: Byte10, Byte11, Byte12, Byte13
	 * (little-endian) */
	info->vid_pid = 0x0000;

	/* TI Running flash bank offset: Byte15 */
	info->running_in_flash_bank = 0;

	/* TI PD Revision: Byte23, Byte24 (big-endian) */
	info->pd_revision = 0x0000;

	/* TI PD Version: Byte25, Byte26 (big-endian) */
	info->pd_version = 0x0000;

	/* Command has completed */
	data->cci_event.command_completed = 1;
	/* Inform the system of the event */
	call_cci_event_cb(data);

	/* Transition to idle state */
	set_state(data, ST_IDLE);
	return;
}

static void st_cmd_get_vbus_voltage_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_cmd_get_vbus_voltage_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_adc_results adc_results;

	uint16_t *vbus = (uint16_t *)data->user_buf;
	int rv;

	if (data->user_buf == NULL) {
		set_state(data, ST_ERROR_RECOVERY);
		return;
	}

	rv = tps_rd_adc_results(&cfg->i2c, &adc_results);
	if (rv) {
		set_state(data, ST_ERROR_RECOVERY);
		return;
	}

	*vbus = cfg->connector_number ? adc_results.pa_vbus :
					adc_results.pb_vbus;

	/* Command has completed */
	data->cci_event.command_completed = 1;
	/* Inform the system of the event */
	call_cci_event_cb(data);

	set_state(data, ST_IDLE);
	return;
}

static void st_cmd_ni_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_cmd_ni_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	/* Command has completed */
	data->cci_event.command_completed = 1;
	/* Inform the system of the event */
	call_cci_event_cb(data);

	set_state(data, ST_IDLE);
	return;
}

static void inline set_task_cmd(union reg_command *cmd, char *str)
{
	memcpy(&cmd->raw_value[2], str, 4);
}

static void st_task_gaid_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_task_gaid_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_command cmd;
	int rv;

	set_task_cmd(&cmd, "GAID");

	if (cfg->connector_number) {
		rv = tps_rw_command_for_i2c1(&cfg->i2c, &cmd, I2C_MSG_WRITE);
	} else {
		rv = tps_rw_command_for_i2c2(&cfg->i2c, &cmd, I2C_MSG_WRITE);
	}

	if (rv) {
		set_state(data, ST_ERROR_RECOVERY);
		return;
	}

	set_state(data, ST_TASK_WAIT);
	return;
}

static void st_task_srdy_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_task_srdy_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_command cmd;
	union reg_data cmd_data;
	union reg_autonegotiate_sink an_snk;
	int rv;

	rv = tps_rw_autonegotiate_sink(&cfg->i2c, &an_snk, I2C_MSG_READ);
	an_snk.auto_neg_rdo_priority = 1;
	an_snk.no_capability_mismatch = 0;
	an_snk.auto_enable_standby_srdy = 1;
	rv = tps_rw_autonegotiate_sink(&cfg->i2c, &an_snk, I2C_MSG_WRITE);

	if (cfg->connector_number) {
		/* Port 1 */
		if (data->snk_fet_en) {
			/* Enable Sink FET */
			set_task_cmd(&cmd, "SRDY");
			cmd_data.data[0] = 0x03;
			rv = tps_rw_data_for_cmd1(&cfg->i2c, &cmd_data,
						  I2C_MSG_WRITE);
			if (rv) {
				set_state(data, ST_ERROR_RECOVERY);
				return;
			}
		} else {
			/* Disable Sink FET */
			set_task_cmd(&cmd, "SRYR");
		}

		rv = tps_rw_command_for_i2c1(&cfg->i2c, &cmd, I2C_MSG_WRITE);
		if (rv) {
			set_state(data, ST_ERROR_RECOVERY);
			return;
		}
	} else {
		/* Port 0 */
		if (data->snk_fet_en) {
			/* Enable Sink FET */
			set_task_cmd(&cmd, "SRDY");
			cmd_data.data[0] = 0x02;
			rv = tps_rw_data_for_cmd2(&cfg->i2c, &cmd_data,
						  I2C_MSG_WRITE);
			if (rv) {
				set_state(data, ST_ERROR_RECOVERY);
				return;
			}
		} else {
			/* Disable Sink FET */
			set_task_cmd(&cmd, "SRYR");
		}

		rv = tps_rw_command_for_i2c2(&cfg->i2c, &cmd, I2C_MSG_WRITE);
		if (rv) {
			set_state(data, ST_ERROR_RECOVERY);
			return;
		}
	}

	/* Transition to wait state */
	set_state(data, ST_TASK_WAIT);
	return;
}

static void st_task_ucsi_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
}

static void st_task_ucsi_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	struct pdc_config_t const *cfg = data->dev->config;
	union reg_command cmd;
	union reg_data cmd_data;
	int rv;

	/* Set Task Command */
	set_task_cmd(&cmd, "UCSI");

	memset(cmd_data.data, 0, sizeof(cmd_data.data));
	cmd_data.data[0] = data->ucsi_command;
	cmd_data.data[1] = 0;
	/* Connector Number: Byte 2, bits 6:0 */
	cmd_data.data[2] = cfg->connector_number + 1;

	switch (data->cmd) {
	case CMD_CONNECTOR_RESET:
		cmd_data.data[2] |= (data->connector_reset.reset_type << 7);
		break;
	case CMD_GET_PDOS:
		/* Partner PDO: Byte 2, bits 7 */
		cmd_data.data[2] |= (data->port_partner_pdo << 7);
		/* PDO Offset: Byte 3, bits 7:0 */
		cmd_data.data[3] = 1; // data->pdo_offset;
		/* Number of PDOs: Byte 4, bits 1:0 */
		cmd_data.data[4] = 3; // data->num_pdos;
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
	default:
	}

	if (cfg->connector_number) {
		/* Port 1 */
		rv = tps_rw_data_for_cmd1(&cfg->i2c, &cmd_data, I2C_MSG_WRITE);
		if (rv) {
			set_state(data, ST_ERROR_RECOVERY);
			return;
		}

		rv = tps_rw_command_for_i2c1(&cfg->i2c, &cmd, I2C_MSG_WRITE);
		if (rv) {
			set_state(data, ST_ERROR_RECOVERY);
			return;
		}
	} else {
		/* Port 0 */
		rv = tps_rw_data_for_cmd2(&cfg->i2c, &cmd_data, I2C_MSG_WRITE);
		if (rv) {
			set_state(data, ST_ERROR_RECOVERY);
			return;
		}

		rv = tps_rw_command_for_i2c2(&cfg->i2c, &cmd, I2C_MSG_WRITE);
		if (rv) {
			set_state(data, ST_ERROR_RECOVERY);
			return;
		}
	}

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
	if (cfg->connector_number) {
		rv = tps_rw_command_for_i2c1(&cfg->i2c, &cmd, I2C_MSG_READ);
	} else {
		rv = tps_rw_command_for_i2c2(&cfg->i2c, &cmd, I2C_MSG_READ);
	}

	if (rv) {
		/* I2C transaction failed */
		set_state(data, ST_ERROR_RECOVERY);
		return;
	}

	/*
	 * Wait for command to complete:
	 *  1) command is set to 0 when command is sent
	 *  2) command is set to "!CMD" for unknown command
	 */
	if (cmd.command && cmd.command != 0x444d4321) {
		return;
	}

	/*
	 * Read status of command for particular port:
	 *  1) cmd_data is set to zero on success
	 *  2) cmd_data is set to an error code on failure
	 */
	if (cfg->connector_number) {
		rv = tps_rw_data_for_cmd1(&cfg->i2c, &cmd_data, I2C_MSG_READ);
	} else {
		rv = tps_rw_data_for_cmd2(&cfg->i2c, &cmd_data, I2C_MSG_READ);
	}

	if (rv) {
		/* I2C transaction failed */
		set_state(data, ST_ERROR_RECOVERY);
		return;
	}

	if (cmd.command || cmd_data.data[0] != 0) {
		/* Command has completed with error */
		data->cci_event.error = 1;

		LOG_ERR("\nC%d: COMMAD %d (%d) FAILED\n", cfg->connector_number,
			data->cmd, cmd_data.data[0]);
	} else {
		switch (data->cmd) {
		case CMD_GET_CONNECTOR_CAPABILITY:
			offset = 1;
			len = sizeof(union connector_capability_t);
			break;
		case CMD_GET_CONNECTOR_STATUS:
			offset = 1;
			len = sizeof(union connector_status_t);
			break;
		case CMD_GET_CABLE_PROPERTY:
			offset = 1;
			len = sizeof(union cable_property_t);
			break;
		case CMD_GET_PDOS: {
			/* TODO(FIX HACK): Create 5V PDO */
			uint32_t pdo0 = 0x0a01912c;

			offset = 2;
			len = cmd_data.data[1];
			memcpy(data->user_buf, (uint8_t *)&pdo0, 4);
			memcpy(data->user_buf + 4, &cmd_data.data[offset], len);
			break;
		}
		default:
			/* No data for this command */
			len = 0;
		}

		if (len && data->cmd != CMD_GET_PDOS) {
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
}

/* Populate state table */
static const struct smf_state states[] = {
	[ST_IRQ] =
		SMF_CREATE_STATE(st_irq_entry, st_irq_run, st_irq_exit, NULL),
	[ST_INIT] = SMF_CREATE_STATE(st_init_entry, st_init_run, st_init_exit,
				     NULL),
	[ST_IDLE] = SMF_CREATE_STATE(st_idle_entry, st_idle_run, st_idle_exit,
				     NULL),
	[ST_ERROR_RECOVERY] = SMF_CREATE_STATE(
		st_error_recovery_entry, st_error_recovery_run, NULL, NULL),

	[ST_CMD_SET_TPC_RP] = SMF_CREATE_STATE(
		st_cmd_set_tpc_rp_entry, st_cmd_set_tpc_rp_run, NULL, NULL),
	[ST_CMD_GET_RDO] = SMF_CREATE_STATE(st_cmd_get_rdo_entry,
					    st_cmd_get_rdo_run, NULL, NULL),
	[ST_CMD_GET_IC_STATUS] = SMF_CREATE_STATE(st_cmd_get_ic_status_entry,
						  st_cmd_get_ic_status_run,
						  NULL, NULL),
	[ST_CMD_GET_VBUS_VOLTAGE] =
		SMF_CREATE_STATE(st_cmd_get_vbus_voltage_entry,
				 st_cmd_get_vbus_voltage_run, NULL, NULL),

	[ST_CMD_GET_VDO] = SMF_CREATE_STATE(st_cmd_get_vdo_entry,
					    st_cmd_get_vdo_run, NULL, NULL),
	[ST_CMD_GET_IDENTITY_DISCOVERY] =
		SMF_CREATE_STATE(st_cmd_get_identity_discovery_entry,
				 st_cmd_get_identity_discovery_run, NULL, NULL),
	[ST_TASK_UCSI] = SMF_CREATE_STATE(st_task_ucsi_entry, st_task_ucsi_run,
					  NULL, NULL),
	[ST_TASK_WAIT] = SMF_CREATE_STATE(st_task_wait_entry, st_task_wait_run,
					  NULL, NULL),
	[ST_TASK_GAID] = SMF_CREATE_STATE(st_task_gaid_entry, st_task_gaid_run,
					  NULL, NULL),
	[ST_CMD_VENDOR_ENABLE] =
		SMF_CREATE_STATE(st_cmd_ni_entry, st_cmd_ni_run, NULL, NULL),
	[ST_CMD_SET_NOTIFICATION_ENABLE] =
		SMF_CREATE_STATE(st_cmd_ni_entry, st_cmd_ni_run, NULL, NULL),
	[ST_CMD_GET_ERROR_STATUS] =
		SMF_CREATE_STATE(st_cmd_ni_entry, st_cmd_ni_run, NULL, NULL),
	[ST_CMD_READ_POWER_LEVEL] =
		SMF_CREATE_STATE(st_cmd_ni_entry, st_cmd_ni_run, NULL, NULL),
	[ST_CMD_SET_RDO] =
		SMF_CREATE_STATE(st_cmd_ni_entry, st_cmd_ni_run, NULL, NULL),
	[ST_TASK_SRDY] = SMF_CREATE_STATE(st_task_srdy_entry, st_task_srdy_run,
					  NULL, NULL),
	[ST_CMD_GET_CURRENT_PARTNER_SRC_PDO] =
		SMF_CREATE_STATE(st_cmd_ni_entry, st_cmd_ni_run, NULL, NULL),
	[ST_CMD_SET_TPC_RECONNECT] =
		SMF_CREATE_STATE(st_cmd_ni_entry, st_cmd_ni_run, NULL, NULL),
	[ST_CMD_SET_RETIMER_FW_UPDATE_MODE] =
		SMF_CREATE_STATE(st_cmd_ni_entry, st_cmd_ni_run, NULL, NULL),
};

static int tps_post_command(const struct device *dev, enum cmd_t cmd,
			    uint8_t *user_buf)
{
	struct pdc_data_t *data = dev->data;

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

static int tps_get_ucsi_version(const struct device *dev, uint16_t *version)
{
	if (version == NULL) {
		return -EINVAL;
	}

	*version = UCSI_VERSION;

	return 0;
}

static int tps_set_handler_cb(const struct device *dev,
			      pdc_cci_handler_cb_t cci_cb, void *cb_data)
{
	struct pdc_data_t *data = dev->data;

	data->cci_cb = cci_cb;
	data->cb_data = cb_data;

	return 0;
}

static int tps_read_power_level(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	return tps_post_command(dev, CMD_READ_POWER_LEVEL, NULL);
}

static int tps_reconnect(const struct device *dev)
{
	/* TODO */
	return 0;
}

static int tps_pdc_reset(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	return tps_post_command(dev, CMD_TRIGGER_PDC_RESET, NULL);
}

static int tps_connector_reset(const struct device *dev,
			       union connector_reset_t type)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	data->connector_reset = type;

	return tps_post_command(dev, CMD_CONNECTOR_RESET, NULL);
}

static int tps_set_power_level(const struct device *dev,
			       enum usb_typec_current_t tcc)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	data->tcc = tcc;

	return tps_post_command(dev, CMD_SET_TPC_RP, NULL);
}

static int tps_set_sink_path(const struct device *dev, bool en)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	data->snk_fet_en = en;

	return tps_post_command(dev, CMD_SET_SINK_PATH, NULL);
}

static int tps_get_capability(const struct device *dev,
			      struct capability_t *caps)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	return tps_post_command(dev, CMD_GET_CAPABILITY, (uint8_t *)caps);
}

static int tps_get_connector_capability(const struct device *dev,
					union connector_capability_t *caps)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	return tps_post_command(dev, CMD_GET_CONNECTOR_CAPABILITY,
				(uint8_t *)caps);
}

static int tps_get_connector_status(const struct device *dev,
				    union connector_status_t *cs)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	return tps_post_command(dev, CMD_GET_CONNECTOR_STATUS, (uint8_t *)cs);
}

static int tps_get_error_status(const struct device *dev,
				union error_status_t *es)
{
	/* TODO */
	return 0;
}

static int tps_set_rdo(const struct device *dev, uint32_t rdo)
{
	/* TODO */
	return 0;
}

static int tps_get_rdo(const struct device *dev, uint32_t *rdo)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	return tps_post_command(dev, CMD_GET_RDO, (uint8_t *)rdo);
}

static int tps_get_pdos(const struct device *dev, enum pdo_type_t pdo_type,
			enum pdo_offset_t pdo_offset, uint8_t num_pdos,
			bool port_partner_pdo, uint32_t *pdos)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	data->pdo_type = pdo_type;
	data->pdo_offset = pdo_offset;
	data->num_pdos = num_pdos;
	data->port_partner_pdo = port_partner_pdo;

	return tps_post_command(dev, CMD_GET_PDOS, (uint8_t *)pdos);
}

static int tps_get_info(const struct device *dev, struct pdc_info_t *info)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	return tps_post_command(dev, CMD_GET_IC_STATUS, (uint8_t *)info);
}

static int tps_get_bus_info(const struct device *dev,
			    struct pdc_bus_info_t *info)
{
	/* TODO */
	return 0;
}

static int tps_get_vbus_voltage(const struct device *dev, uint16_t *voltage)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	return tps_post_command(dev, CMD_GET_VBUS_VOLTAGE, (uint8_t *)voltage);
}

static int tps_set_ccom(const struct device *dev, enum ccom_t ccom)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	data->ccom = ccom;

	return tps_post_command(dev, CMD_SET_CCOM, NULL);
}

static int tps_set_uor(const struct device *dev, union uor_t uor)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	data->uor = uor;

	return tps_post_command(dev, CMD_SET_UOR, NULL);
}

static int tps_set_pdr(const struct device *dev, union pdr_t pdr)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

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
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (cp == NULL) {
		return -EINVAL;
	}

	return tps_post_command(dev, CMD_GET_CABLE_PROPERTY, (uint8_t *)cp);
}

static int tps_get_vdo(const struct device *dev, union get_vdo_t vdo_req,
		       uint8_t *vdo_req_list, uint32_t *vdo)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (vdo == NULL || vdo_req_list == NULL) {
		return -EINVAL;
	}

	for (int i = 0; i < vdo_req.num_vdos; i++) {
		data->vdo_req_list[i] = vdo_req_list[i];
	}
	data->vdo_req = vdo_req;

	return tps_post_command(dev, CMD_GET_VDO, (uint8_t *)vdo);
}

static int tps_get_identity_discovery(const struct device *dev,
				      bool *disc_state)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (disc_state == NULL) {
		return -EINVAL;
	}

	return tps_post_command(dev, CMD_GET_IDENTITY_DISCOVERY,
				(uint8_t *)disc_state);
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
};

static void pdc_interrupt_callback(const struct device *dev,
				   struct gpio_callback *cb, uint32_t pins)
{
	for (int i = 0; i < NUM_PDC_TPS6699X_PORTS; i++) {
		k_event_post(&pdc_data[i]->pdc_event, PDC_IRQ_EVENT);
	}
}

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

	if (!irq_init_done) {
		irq_shared_port = cfg->irq_gpios.port;
		irq_share_pin = cfg->irq_gpios.pin;

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

		irq_init_done = true;
	} else {
		if (irq_shared_port != cfg->irq_gpios.port ||
		    irq_share_pin != cfg->irq_gpios.pin) {
			LOG_ERR("All TPS ports must use the same interrupt");
			return -EINVAL;
		}
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
	static const struct pdc_config_t pdc_config##inst = {                  \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                             \
		.irq_gpios = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),           \
		.connector_number =                                            \
			USBC_PORT_FROM_DRIVER_NODE(DT_DRV_INST(inst), pdc),    \
		.create_thread = create_thread_##inst,                         \
	};                                                                     \
                                                                               \
	DEVICE_DT_INST_DEFINE(inst, pdc_init, NULL, &pdc_data_##inst,          \
			      &pdc_config##inst, POST_KERNEL,                  \
			      CONFIG_APPLICATION_INIT_PRIORITY,                \
			      &pdc_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PDC_DEFINE)
