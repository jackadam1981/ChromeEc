/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * TCPMv3
 */

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>


#include "i2c.h"
#include "i2c/i2c.h"
#include "usbc/utils.h"

#include <zephyr/device.h>
#include <stdlib.h>

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <zephyr/smf.h>

#include <drivers/pdc_ucsi.h>
#include <usbc/pdc_power_mgnt.h>

#include "usb_pd.h"
#include "charge_manager.h"

#define NODE_ID	DT_NODELABEL(pdc_power_p0p1)

#if DT_NODE_HAS_STATUS(NODE_ID, okay)
#define PDC	NODE_ID
#else
#error "Can't find PDC Node"
#endif

enum cci_flag_t {
	CCI_END_OF_MESSAGE,
	CCI_PORT_CHANGE,
	CCI_PORT0_CHANGE,
	CCI_PORT1_CHANGE,
	CCI_SECURITY_REQUEST,
	CCI_FW_UPDATE_REQUEST,
	CCI_NOT_SUPPORTED,
	CCI_CANCEL_COMPLETED,
	CCI_RESET_COMPLETED,
	CCI_BUSY,
	CCI_ACK_COMMAND,
	CCI_ERROR,
	CCI_CMD_COMPLETED,
};

static void create_thread(const struct device *dev);
static int tcpm_subsys_init(const struct device *dev);

enum local_init_state_t {
	LI_PPM_RESET,
	LI_PPM_RESET_WAIT,
	LI_SET_NOTIFICATION_ENABLE,
	LI_SET_NOTIFICATION_ENABLE_WAIT,
	LI_GET_CAPABILITY,
	LI_GET_CAPABILITY_WAIT,
	LI_GET_PORT0_STATUS,
	LI_GET_PORT0_STATUS_WAIT,
	LI_GET_PORT1_STATUS,
	LI_GET_PORT1_STATUS_WAIT,
	LI_RUN,
};

enum tcpm_state_t {
	TCPM_PORT_CHANGE_EVENT,
        TCPM_UNATTACHED,
	TCPM_SNK_ATTACHED,
	TCPM_SRC_ATTACHED,
};

static const char *const tcpm_state_names[] = {
	[TCPM_PORT_CHANGE_EVENT] = "PortChange.EVENT",
	[TCPM_UNATTACHED] = "Unattached",
	[TCPM_SNK_ATTACHED] = "Attached.SNK",
	[TCPM_SRC_ATTACHED] = "Attached.SRC",
};

static const struct smf_state tcpm_states[];

struct tcpm_data_t;

struct tcpm_port_t {
	struct smf_ctx ctx;
	enum tcpm_state_t last_state;
	atomic_t flags;

	struct tcpm_data_t *device;
	const struct device *pdc;

	uint32_t state;

	enum port_t port_num;
	struct connector_status_t port_status;
	union hpi_pd_status_t pd_status;
	union hpi_tc_status_t tc_status;

	union pdo_source_t pdo;

	uint8_t local_state;
};

struct tcpm_data_t {
	/** This port's thread */
	k_tid_t thread;
	/** This port thread's data */
	struct k_thread thread_data;

	const struct device *pdc;
	uint8_t local_state;
	atomic_t flags;
	union notification_enable_t ne;
	struct device_capability_t dc;
	struct tcpm_port_t port[2];
};

struct tcpm_config_t {
	/**
	 * The usbc stack initializes this pointer that creates the
	 * main thread for this port
	 */
	void (*create_thread)(const struct device *dev);
};

K_THREAD_STACK_DEFINE(my_stack_area, 2000);

static struct tcpm_data_t tcpm_data = {
	.pdc = DEVICE_DT_GET(PDC),
	.local_state = LI_PPM_RESET,
	.port[PORT0].pdc = DEVICE_DT_GET(PDC),
	.port[PORT0].port_num = PORT0,
	.port[PORT1].pdc = DEVICE_DT_GET(PDC),
	.port[PORT1].port_num = PORT1,
};

static const struct tcpm_config_t tcpm_config = {
	.create_thread = create_thread,
};

static void run_tcpm(void *dev, void *unused1, void *unused2)
{
	struct tcpm_data_t *tcpm = ((const struct device *)dev)->data;

	while (1) {
		switch (tcpm->local_state) {
		case LI_PPM_RESET:
			if (!pdc_reset(tcpm->pdc)) {
				tcpm->local_state = LI_PPM_RESET_WAIT;
			}
			break;
		case LI_PPM_RESET_WAIT:
			if (atomic_test_and_clear_bit(&tcpm->flags, CCI_RESET_COMPLETED)) {
				tcpm->local_state = LI_SET_NOTIFICATION_ENABLE;
			}
			break;
		case LI_SET_NOTIFICATION_ENABLE:
			/* FIXME: Get from devicetree or kconfig */
			tcpm->ne.raw_value = 0xffff;
			if (!pdc_set_notification_enable(tcpm->pdc, tcpm->ne)) {
				tcpm->local_state = LI_SET_NOTIFICATION_ENABLE_WAIT;
			}
			break;
		case LI_SET_NOTIFICATION_ENABLE_WAIT:
			if (atomic_test_and_clear_bit(&tcpm->flags, CCI_CMD_COMPLETED)) {
				tcpm->local_state = LI_GET_CAPABILITY;
			}
			break;
		case LI_GET_CAPABILITY:
			if (!pdc_get_capability(tcpm->pdc, &tcpm->dc)) {
				tcpm->local_state = LI_GET_CAPABILITY_WAIT;
			}
			break;
		case LI_GET_CAPABILITY_WAIT:
			if (atomic_test_and_clear_bit(&tcpm->flags, CCI_CMD_COMPLETED)) {
				tcpm->local_state = LI_RUN;
			}
			break;
		case LI_RUN:
			/* Run port 0 state machine */
			smf_run_state(&tcpm->port[0].ctx);
			/* Run port 1 state machine */
			smf_run_state(&tcpm->port[1].ctx);
			break;
		}
		k_sleep(K_MSEC(25));
	}
}

void tcpm_reset(void)
{
	
}

bool tcpm_is_connected(int port)
{
	if (port > 1) {
		return 0;
	}

	return tcpm_data.port[port].tc_status.is_connected;
}

uint8_t tcpm_get_port_num(void)
{
	return tcpm_data.dc.bNumPorts;
}

void tcpm_reset_port(int port)
{

}

void tcpm_request_data_swap(int port)
{
}

enum attached_state_t tcpm_get_task_state(int port)
{
	if (port > 1) {
		return UNATTACHED;
	}

	return tcpm_data.port[port].tc_status.attached_state;
}

const char *tcpm_get_task_state_name(int port)
{
	return tcpm_state_names[tcpm_get_task_state(port)];
}

int tcpm_comm_is_enabled(int port)
{
	return 0;
}

bool tcpm_get_vconn_state(int port)
{
	return 0;
}

enum pd_data_role tcpm_get_data_role(int port)
{
	return tcpm_data.port[port].pd_status.data_role;
}

enum pd_power_role tcpm_get_power_role(int port)
{
	return tcpm_data.port[port].pd_status.power_role; 
}

bool tcpm_get_partner_dual_role_power(int port)
{
	return 0;
}

bool tcpm_get_partner_data_swap_capable(int port)
{
	return 1;
}

bool tcpm_get_partner_usb_comm_capable(int port)
{
	return 0;
}

bool tcpm_get_partner_unconstr_power(int port)
{
	return 0;
}

bool tcpm_pd_capable(int port)
{
	return 1;
}

uint32_t tcpm_get_vbus_voltage(int port)
{
	uint16_t vbus = 0;

	if (port != 0) {
		return vbus;
	}

	while (pdc_read_vbus(tcpm_data.port[port].pdc, port, &vbus)) {
		k_sleep(K_MSEC(1));
	}

	return vbus * 100;
}

static void tcpm_port_handler_cb(enum port_t port)
{
	/* A change has occurred on port, get the TC and PD status */
	pdc_get_tc_status(tcpm_data.port[port].pdc, tcpm_data.port[port].port_num, &tcpm_data.port[port].tc_status.raw_value);
	pdc_get_pd_status(tcpm_data.port[port].pdc, tcpm_data.port[port].port_num, &tcpm_data.port[port].pd_status.raw_value);
}

static void tcpm_cci_handler_cb(enum port_t port, union cci_event_t cci_event)
{
	/* Handle Device CCI events */
	if (port > PORT1) {
		if (cci_event.reset_completed) {
			atomic_set_bit(&tcpm_data.flags, CCI_RESET_COMPLETED);
		}

		if (cci_event.busy) {
			atomic_set_bit(&tcpm_data.flags, CCI_BUSY);
		}

		if (cci_event.acknowledge_command) {
			atomic_set_bit(&tcpm_data.flags, CCI_ACK_COMMAND);
		}

		if (cci_event.error) {
			atomic_set_bit(&tcpm_data.flags, CCI_ERROR);
		}

		if (cci_event.command_completed) {
			atomic_set_bit(&tcpm_data.flags, CCI_CMD_COMPLETED);
		}
		return;
	}
	
	/* Handle Port CCI events */
	if (cci_event.end_of_message) {
		atomic_set_bit(&tcpm_data.port[port].flags, CCI_END_OF_MESSAGE);
	}

	if (cci_event.connector_change == PORT0 + 1) {
		atomic_set_bit(&tcpm_data.port[PORT0].flags, CCI_PORT_CHANGE);
	} else if (cci_event.connector_change == PORT1 + 1) {
		atomic_set_bit(&tcpm_data.port[PORT1].flags, CCI_PORT_CHANGE);
	}

	if (cci_event.security_request) {
		atomic_set_bit(&tcpm_data.port[port].flags, CCI_SECURITY_REQUEST);
	}

	if (cci_event.fw_update_request) {
		atomic_set_bit(&tcpm_data.port[port].flags, CCI_FW_UPDATE_REQUEST);
	}

	if (cci_event.not_supported) {
		atomic_set_bit(&tcpm_data.port[port].flags, CCI_NOT_SUPPORTED);
	}
	
	if (cci_event.cancel_completed) {
		atomic_set_bit(&tcpm_data.port[port].flags, CCI_CANCEL_COMPLETED);
	}

	if (cci_event.reset_completed) {
		atomic_set_bit(&tcpm_data.port[port].flags, CCI_RESET_COMPLETED);
	}

	if (cci_event.busy) {
		atomic_set_bit(&tcpm_data.port[port].flags, CCI_BUSY);
	}

	if (cci_event.acknowledge_command) {
		atomic_set_bit(&tcpm_data.port[port].flags, CCI_ACK_COMMAND);
	}

	if (cci_event.error) {
		atomic_set_bit(&tcpm_data.port[port].flags, CCI_ERROR);
	}

	if (cci_event.command_completed) {
		atomic_set_bit(&tcpm_data.port[port].flags, CCI_CMD_COMPLETED);
	}
}

static void create_thread(const struct device *dev)
{
	struct tcpm_data_t *data = dev->data;

	data->thread = k_thread_create(
		&data->thread_data, my_stack_area,
		K_THREAD_STACK_SIZEOF(my_stack_area), run_tcpm, (void *)dev,
		0, 0, 8, K_ESSENTIAL, K_NO_WAIT);
}

DEVICE_DEFINE(pdc_power_p0p1,"pdc_power_p0p1", &tcpm_subsys_init, NULL, &tcpm_data,
			&tcpm_config, POST_KERNEL,
			CONFIG_APPLICATION_INIT_PRIORITY, NULL);

enum tcpm_state_t get_tcpm_state(struct tcpm_port_t *port)
{
	return port->ctx.current - &tcpm_states[0];
}

static void set_tcpm_state(struct tcpm_port_t *port, const enum tcpm_state_t next_state)
{
	port->last_state = get_tcpm_state(port);
	smf_set_state(SMF_CTX(port), &tcpm_states[next_state]);
}

void print_current_tcpm_state(struct tcpm_port_t *port)
{
	printk("PD%d: %s\n",port->port_num, tcpm_state_names[get_tcpm_state(port)]);
}

static void tcpm_port_change_event_entry(void *o)
{
	struct tcpm_port_t *port = (struct tcpm_port_t *)o;

	print_current_tcpm_state(port);

	port->state = 0;
}

static void tcpm_port_change_event_run(void *o)
{
	struct tcpm_port_t *port = (struct tcpm_port_t *)o;
	struct tcpm_data_t *device = port->device;

	switch (port->state) {
	case 0:
		if (pdc_get_connector_status(port->pdc, port->port_num, &port->port_status)) {
			/* Failed to get connector status, so try again */
			return;
		}
		/* Got connector status */
		port->state++;
		break;
	case 1:
		/* Wait until Get_Connector_Status UCSI command completes */
		if (!atomic_test_and_clear_bit(&port->flags, CCI_CMD_COMPLETED) &&
			!atomic_test_bit(&device->flags, CCI_CMD_COMPLETED)) {
			return;
		}

		/* If this was a device command complete, just return to the previous state */
		if (atomic_test_and_clear_bit(&device->flags, CCI_CMD_COMPLETED)) {
			set_tcpm_state(port, port->last_state);
			return;
		}

		/* Test if we're connected */
		if (port->port_status.general_status.connect_status) {
			if (port->port_status.general_status.power_direction) {
				set_tcpm_state(port, TCPM_SRC_ATTACHED);
			} else {
				set_tcpm_state(port, TCPM_SNK_ATTACHED);
			}
		} else {
			set_tcpm_state(port, TCPM_UNATTACHED);
		}
		break;
	}
}

static void tcpm_unattached_entry(void *o)
{
	struct tcpm_port_t *port = (struct tcpm_port_t *)o;

        print_current_tcpm_state(port);
	port->flags = 0;
}

static void tcpm_unattached_run(void *o)
{
	struct tcpm_port_t *port = (struct tcpm_port_t *)o;

	if (atomic_test_and_clear_bit(&port->flags, CCI_PORT_CHANGE)) {
		set_tcpm_state(port, TCPM_PORT_CHANGE_EVENT);
	}
}

static void tcpm_src_attached_entry(void *o)
{
	struct tcpm_port_t *port = (struct tcpm_port_t *)o;

        print_current_tcpm_state(port);
	port->flags = 0;

	/* Disable Sink Power Path */
	pdc_set_sink_path(port->pdc, port->port_num, false);

	pd_set_input_current_limit(port->port_num, 0, 0);
	typec_set_input_current_limit(port->port_num, 0, 0);
	charge_manager_set_ceil(port->port_num, CEIL_REQUESTOR_PD,
				CHARGE_CEIL_NONE);
	charge_manager_update_dualrole(port->port_num, CAP_UNKNOWN);
	port->state = 0;

}

static void tcpm_src_attached_run(void *o)
{
	struct tcpm_port_t *port = (struct tcpm_port_t *)o;

	if (atomic_test_and_clear_bit(&port->flags, CCI_PORT_CHANGE)) {
		set_tcpm_state(port, TCPM_PORT_CHANGE_EVENT);
	}
}

static void tcpm_snk_attached_entry(void *o)
{
	struct tcpm_port_t *port = (struct tcpm_port_t *)o;
	uint32_t max_ma, max_mv, max_mw;
	union pdo_source_t pdo;

        print_current_tcpm_state(port);

	/* Clear the port flags */
	port->flags = 0;

	/* Get the current PDO */
	if (pdc_get_current_pdo(port->pdc, port->port_num, &pdo.raw_value)) {
		printk("ERROR2\n");
	}

	/*  Don't charge if the PDO is invalid or hasn't changed */
	if (pdo.raw_value == 0 || pdo.raw_value == port->pdo.raw_value) {
		return;
	}

	/* Save PDO */
	port->pdo.raw_value = pdo.raw_value;

	/* Extract Current, Voltage, and calculate Power */
	max_ma = port->pdo.max_current * 10;
	max_mv = port->pdo.voltage * 50;
	max_mw = max_ma * max_mv / 1000;

	printk("START CHARGING ON PORT%d\n", port->port_num);
	printk("PDO: %08x\n", port->pdo.raw_value);
	printk("V: %d\n", max_mv);
	printk("C: %d\n", max_ma);
	printk("P: %d\n", max_mw);

	pd_set_input_current_limit(port->port_num, max_ma, max_mv);
	charge_manager_set_ceil(port->port_num, CEIL_REQUESTOR_PD, max_ma);

	if ((port->pdo.type == FIXED && (!port->pdo.drp || port->pdo.unconstrained_pwr))
			|| (max_mw >= PD_DRP_CHARGE_POWER_MIN)) {
		charge_manager_update_dualrole(port->port_num, CAP_DEDICATED);
	} else {
		charge_manager_update_dualrole(port->port_num, CAP_DUALROLE);
	}

	/* Enable the Sink Power Path */
	pdc_set_sink_path(port->pdc, port->port_num, true);
}

static void tcpm_snk_attached_run(void *o)
{
	struct tcpm_port_t *port = (struct tcpm_port_t *)o;

	if (atomic_test_and_clear_bit(&port->flags, CCI_PORT_CHANGE)) {
		set_tcpm_state(port, TCPM_PORT_CHANGE_EVENT);
	}
}

/* Populate state table */
static const struct smf_state tcpm_states[] = {
        /* Normal States */
	[TCPM_PORT_CHANGE_EVENT] = SMF_CREATE_STATE(
		tcpm_port_change_event_entry,
		tcpm_port_change_event_run,
		NULL,
		NULL),
	[TCPM_UNATTACHED] = SMF_CREATE_STATE(
		tcpm_unattached_entry,
		tcpm_unattached_run,
		NULL,
		NULL),
	[TCPM_SNK_ATTACHED] = SMF_CREATE_STATE(
		tcpm_snk_attached_entry,
		tcpm_snk_attached_run,
		NULL,
		NULL),
	[TCPM_SRC_ATTACHED] = SMF_CREATE_STATE(
		tcpm_src_attached_entry,
		tcpm_src_attached_run,
		NULL,
		NULL),
};

/**
 * @brief Initialize the USB-C Subsystem
 */
static int tcpm_subsys_init(const struct device *dev)
{
	struct tcpm_data_t *data = dev->data;
	const struct tcpm_config_t *const config = dev->config;
	const struct device *pdc = data->port[0].pdc;

	printk("TCPMv3 start\n");

	/* Make sure TCPC is ready */
	if (!device_is_ready(pdc)) {
		printk("PDC NOT READY\n");
		return -ENODEV;
	}

	pdc_set_handler_cb(pdc, tcpm_cci_handler_cb, tcpm_port_handler_cb);

	/* Set initial state */
	smf_set_initial(&data->port[0].ctx, &tcpm_states[TCPM_UNATTACHED]);
	smf_set_initial(&data->port[1].ctx, &tcpm_states[TCPM_UNATTACHED]);

	data->port[0].device = dev->data;
	data->port[1].device = dev->data;

	/* Create the thread for this port */
	config->create_thread(dev);

	return 0;
}
