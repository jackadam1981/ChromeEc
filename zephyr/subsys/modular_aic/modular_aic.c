/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_power/ap_pwrseq.h"
#include "console.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/modular_aic.h>

LOG_MODULE_REGISTER(aic, LOG_LEVEL_INF);

#define DT_DRV_COMPAT intel_modular_aic_slot

#define MODULAR_AIC_TAG "MOD_AIC:"
#define MODULAR_AIC_TAG_LEN 8

#if defined(MODULAR_AIC_DETECTION_CHECK_TAG)
#define MODULAR_AIC_INFO_ADDRESS MODULAR_AIC_TAG_LEN
#else
#define MODULAR_AIC_INFO_ADDRESS 0x00
#endif

union modular_aic_properties {
	struct {
		uint16_t present : 1;
		uint16_t type : 1;
		uint16_t conn_type : 4;
		uint16_t retimer_status : 2;
		uint16_t pd_cntrl_type : 3;
		uint16_t tbt_status : 1;
		uint16_t data_speed : 3;
		uint16_t reserved : 1;
	};
	uint16_t raw_value;
};

struct modular_aic_port {
	/* Count of devices bound to this port */
	const int devs_count;
	/* Pointer to list of devices bound this port */
	const struct device **devs;
	const struct device *slot;
	/* Slot properties */
	union modular_aic_properties props;
	/* Pointer to next port */
	struct modular_aic_port *next;
};

struct modular_aic_slot_data {
	/* Indicates if this slot detection has been done */
	bool detection_done;
	/* Bitmap of ports connected to this slot */
	int used_ports;
	/* List of ports connected to this slot */
	struct modular_aic_port *connected_ports;
	/* Power sequence callback */
	struct ap_pwrseq_state_callback cb;
};

struct modular_aic_slot_config {
	uint8_t id;
	/* Detection GPIO spec */
	struct gpio_dt_spec detect_gpio;
	/* AP Power state for detection */
	enum ap_pwrseq_state power_state;
	/* Number of open ports available for connection in this slot */
	int available_ports;
	/* Ports that can be connected to this slot */
	struct modular_aic_port *ports;
	int ports_count;
	enum modular_aic_port_conn_type conn_type;
	enum modular_aic_port_data_speed data_speed;
};

static int modular_aic_slot_init(const struct device *dev);

#define MODULAR_AIC_USBC_PORTS_DEVS_ARRAY(node_id, prop, idx) \
	DEVICE_DT_GET(DT_CAT5(node_id, _P_, prop, _IDX_, idx)),

#define MODULAR_AIC_USBC_PORTS_DEVICES_DEFINE(node_id)                       \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, devices),                      \
		    (const struct device                                     \
			     *node_id##_devices[] = { DT_FOREACH_PROP_ELEM(  \
				     node_id, devices,                       \
				     MODULAR_AIC_USBC_PORTS_DEVS_ARRAY) };), \
		    ())

DT_FOREACH_STATUS_OKAY(intel_modular_aic_usbc_port,
		       MODULAR_AIC_USBC_PORTS_DEVICES_DEFINE)

#define MODULAR_AIC_HDMI_PORTS_DEVS_ARRAY(node_id, prop, idx) \
	DEVICE_DT_GET(DT_CAT5(node_id, _P_, prop, _IDX_, idx)),

#define MODULAR_AIC_HDMI_PORTS_DEVICES_DEFINE(node_id)                       \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, devices),                      \
		    (const struct device                                     \
			     *node_id##_devices[] = { DT_FOREACH_PROP_ELEM(  \
				     node_id, devices,                       \
				     MODULAR_AIC_HDMI_PORTS_DEVS_ARRAY) };), \
		    ())

DT_FOREACH_STATUS_OKAY(intel_modular_aic_hdmi_port,
		       MODULAR_AIC_HDMI_PORTS_DEVICES_DEFINE)

#define MODULAR_AIC_HDMI_PORTS_PROPS_DEFINE(node_id)                \
	{                                                           \
		.conn_type = MODULAR_AIC_PORT_CONN_TYPE_HDMI,       \
		.data_speed = DT_STRING_TOKEN(node_id, data_speed), \
	}

#define MODULAR_AIC_USBC_PORTS_PROPS_DEFINE(node_id)                        \
	{                                                                   \
		.conn_type = MODULAR_AIC_PORT_CONN_TYPE_USBC,               \
		.pd_cntrl_type = DT_STRING_TOKEN(node_id, pd_cntrl_type),   \
		.retimer_status = DT_STRING_TOKEN(node_id, retimer_status), \
		.tbt_status = DT_STRING_TOKEN(node_id, tbt_status),         \
		.data_speed = DT_STRING_TOKEN(node_id, data_speed),         \
	}

#define MODULAR_AIC_PORT_DEFINE(node_id)                                        \
	{                                                                       \
		.devs = COND_CODE_1(DT_NODE_HAS_PROP(node_id, devices),         \
				    (node_id##_devices), (NULL)),               \
		.devs_count = COND_CODE_1(DT_NODE_HAS_PROP(node_id, devices),   \
					  (DT_PROP_LEN(node_id, devices)),      \
					  (0)),                                 \
		.props = COND_CODE_1(                                           \
			DT_NODE_HAS_COMPAT(node_id,                             \
					   intel_modular_aic_usbc_port),        \
			(MODULAR_AIC_USBC_PORTS_PROPS_DEFINE(node_id)),         \
			(COND_CODE_1(                                           \
				DT_NODE_HAS_COMPAT(                             \
					node_id, intel_modular_aic_hdmi_port),  \
				(MODULAR_AIC_HDMI_PORTS_PROPS_DEFINE(node_id)), \
				({ 0 })))),                                     \
	}

#define MODULAR_AIC_PORT_DEFINE_(node_id) MODULAR_AIC_PORT_DEFINE(node_id)

#define MODULAR_AIC_PORT_NAME(node_id, prop, idx) \
	MODULAR_AIC_PORT_DEFINE_(DT_CAT5(node_id, _P_, prop, _IDX_, idx)),

#define MODULAR_AIC_SLOT_PORTS_ARRAY_DEFINE(inst)                            \
	COND_CODE_1(                                                         \
		DT_INST_NODE_HAS_PROP(inst, ports),                          \
		(struct modular_aic_port                                     \
			 slot##inst##_ports[] = { DT_INST_FOREACH_PROP_ELEM( \
				 inst, ports, MODULAR_AIC_PORT_NAME) };),    \
		())

DT_INST_FOREACH_STATUS_OKAY(MODULAR_AIC_SLOT_PORTS_ARRAY_DEFINE)

#define MODULAR_AIC_DEFINE(inst)                                               \
	struct modular_aic_slot_data modular_aic_data_##inst;                  \
                                                                               \
	const struct modular_aic_slot_config modular_aic_config_##inst = {     \
		.id = DT_INST_REG_ADDR(inst),                                  \
		.detect_gpio = GPIO_DT_SPEC_INST_GET(inst, detect_gpios),      \
		.power_state =                                                 \
			DT_INST_STRING_TOKEN(inst, detection_power_state),     \
		.available_ports = DT_INST_PROP(inst, available_ports),        \
		.ports = COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, ports),       \
				     (slot##inst##_ports), (NULL)),            \
		.ports_count = COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, ports), \
					   (DT_INST_PROP_LEN(inst, ports)),    \
					   (0)),                               \
		.conn_type = DT_INST_STRING_TOKEN(inst, conn_type),            \
		.data_speed = DT_INST_STRING_TOKEN(inst, max_data_speed),      \
	};                                                                     \
	DEVICE_DT_INST_DEFINE(inst, modular_aic_slot_init, NULL,               \
			      &modular_aic_data_##inst,                        \
			      &modular_aic_config_##inst, POST_KERNEL,         \
			      CONFIG_APPLICATION_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(MODULAR_AIC_DEFINE)

#define MODULAR_AIC_DEFINE_ARRAY(node_id) \
	[DT_REG_ADDR(node_id)] = DEVICE_DT_GET(node_id),

static const struct device *slots[] = { DT_FOREACH_STATUS_OKAY(
	intel_modular_aic_slot, MODULAR_AIC_DEFINE_ARRAY) };

static void modular_aic_connect_ports(const struct device *dev,
				      union modular_aic_properties props)
{
	const struct modular_aic_slot_config *config = dev->config;
	struct modular_aic_slot_data *data = dev->data;
	struct modular_aic_port **ports_list = &data->connected_ports;

	if (data->used_ports >= config->available_ports) {
		return;
	}
	
	props.type = MODULAR_AIC_PORT_TYPE_FIXED;
	props.conn_type = config->conn_type;
	props.data_speed = config->data_speed;

	for (int i = 0; (data->used_ports < config->available_ports) &&
			i < config->ports_count;
	     i++) {
		struct modular_aic_port *port = &config->ports[i];

		if (port->props.conn_type != props.conn_type ||
		    port->props.data_speed > props.data_speed) {
			/* Incompatible port, moving on */
			continue;
		}

		for (int j = 0; j < port->devs_count; j++) {
			/* TODO: Implement device probing mechanism */
			if (device_is_ready(port->devs[j])) {
				continue;
			}
			device_init(port->devs[j]);
		}
		port->props.present = true;
		port->props.type = props.type;
		*ports_list = port;
		ports_list = &port->next;
		data->used_ports++;
	}
}

static int modular_aic_slot_do_detection(const struct device *dev)
{
	const struct modular_aic_slot_config *cfg = dev->config;
	struct modular_aic_slot_data *data = dev->data;
	union modular_aic_properties props;

	if (data->detection_done) {
		return 0;
	}

	if (!gpio_pin_get_dt(&cfg->detect_gpio)) {
		LOG_INF("AIC Device %s NOT Detected", dev->name);
		goto detection_end;
	}

	LOG_INF("AIC Device %s Detected", dev->name);
	modular_aic_connect_ports(dev, props);
detection_end:
	data->detection_done = true;
	return 0;
}

static void power_state_callback(const struct device *dev,
				 enum ap_pwrseq_state entry,
				 enum ap_pwrseq_state exit)
{
	/* TODO: Improve this to avoid usign array of slots */
	for (int i = 0; i < ARRAY_SIZE(slots); i++) {
		const struct device *slot = slots[i];
		const struct modular_aic_slot_config *cfg = slot->config;

		if (entry == cfg->power_state) {
			modular_aic_slot_do_detection(slot);
		}
	}
}

static int modular_aic_slot_init(const struct device *dev)
{
	const struct modular_aic_slot_config *cfg = dev->config;
	struct modular_aic_slot_data *data = dev->data;

	gpio_pin_configure_dt(&cfg->detect_gpio, GPIO_INPUT);

	if (cfg->power_state == AP_POWER_STATE_UNDEF) {
		modular_aic_slot_do_detection(dev);
	} else {
		const struct device *ap_pwrseq_dev = ap_pwrseq_get_instance();

		data->cb.states_bit_mask = BIT(cfg->power_state);
		data->cb.cb = power_state_callback;

		ap_pwrseq_register_state_entry_callback(ap_pwrseq_dev,
							&data->cb);
	}

	return 0;
}

int modular_aic_slot_get_avail_ports(const struct device *slot)
{
	const struct modular_aic_slot_config *cfg = slot->config;

	return cfg->available_ports;
}

modular_aic_port_t modular_aic_slot_get_port(const struct device *slot,
					     uint8_t port_id)
{
	struct modular_aic_slot_data *data = slot->data;
	const struct modular_aic_slot_config *cfg = slot->config;
	struct modular_aic_port *port;
	uint8_t i = 0;

	if (port_id >= cfg->available_ports) {
		return NULL;
	}

	port = data->connected_ports;
	while (port != NULL && i < port_id) {
		port = port->next;
		i++;
	}

	return port;
}

int modular_aic_port_get_raw_properties(modular_aic_port_t port,
					uint16_t *props)
{
	struct modular_aic_port *port_data = port;

	if (port_data->slot) {
		*props = port_data->props.raw_value;
	}

	return 0;
}

static void modular_aic_print_port_props(struct modular_aic_port *port)
{
	switch (port->props.conn_type) {
	case MODULAR_AIC_PORT_CONN_TYPE_USBA:
		ccprintf("    Conn Type:  USBA\n");
		break;
	case MODULAR_AIC_PORT_CONN_TYPE_USBC:
		ccprintf("    Conn Type:  USBC\n");
		ccprintf("    Retimer:    %s\n",
			 port->props.retimer_status ==
					 MODULAR_AIC_PORT_RETIMER_NOT_PRESENT ?
				 "Not Present" :
			 port->props.retimer_status ==
					 MODULAR_AIC_PORT_RETIMER_SINGLE ?
				 "Single" :
			 port->props.retimer_status ==
					 MODULAR_AIC_PORT_RETIMER_DUAL ?
				 "Dual" :
				 "Unsupported");
		ccprintf(
			"    PD Vendor:  %s\n",
			port->props.pd_cntrl_type ==
					MODULAR_AIC_PORT_PD_CNTRL_TI ?
				"TI" :
			port->props.pd_cntrl_type ==
					MODULAR_AIC_PORT_PD_CNTRL_CYPRESS ?
				"Cypress" :
			port->props.pd_cntrl_type ==
					MODULAR_AIC_PORT_PD_CNTRL_REALTEK ?
				"Realtek" :
			port->props.pd_cntrl_type ==
					MODULAR_AIC_PORT_PD_CNTRL_GOTHIC_BRIDGE ?
				"Gothic Bridge" :
				"Unknown");
		ccprintf("    TBT status: %s\n",
			 port->props.tbt_status ? "Capable" : "Not Capable");
		ccprintf("    Data Speed: %s\n",
			 port->props.data_speed ==
					 MODULAR_AIC_PORT_DATA_SPEED_6GBPS ?
				 "6 Gbps" :
			 port->props.data_speed ==
					 MODULAR_AIC_PORT_DATA_SPEED_12GBPS ?
				 "12 Gbps" :
			 port->props.data_speed ==
					 MODULAR_AIC_PORT_DATA_SPEED_20GBPS ?
				 "20 Gbps" :
			 port->props.data_speed ==
					 MODULAR_AIC_PORT_DATA_SPEED_40GBPS ?
				 "40 Gbps" :
			 port->props.data_speed ==
					 MODULAR_AIC_PORT_DATA_SPEED_80GBPS ?
				 "80 Gbps" :
				 "Unsupported");
		break;
	case MODULAR_AIC_PORT_CONN_TYPE_HDMI:
		ccprintf("    Conn Type:  HDMI\n");
		ccprintf("    Data Speed: %s\n",
			 port->props.data_speed ==
					 MODULAR_AIC_PORT_DATA_SPEED_6GBPS ?
				 "6 Gbps" :
			 port->props.data_speed ==
					 MODULAR_AIC_PORT_DATA_SPEED_12GBPS ?
				 "12 Gbps" :
				 "Unsupported");
		break;
	case MODULAR_AIC_PORT_CONN_TYPE_DP:
		ccprintf("    Conn Type: DP\n");
		break;
	default:
		ccprintf("    Undefined Port!!\n");
	}
}

static int modular_aic_command(int argc, const char **argv)
{
	for (int i = 0; i < ARRAY_SIZE(slots); i++) {
		const struct device *slot = slots[i];
		struct modular_aic_slot_data *data = slot->data;
		int j = 0;

		ccprintf("\nSlot %d:\n", i);
		if (!data->detection_done) {
			ccprintf("  Pending Detection.\n");
			continue;
		}

		if (!data->connected_ports) {
			ccprintf("  No Ports Connected.\n");
			continue;
		}

		for (struct modular_aic_port *port = data->connected_ports;
		     port; port = port->next, j++) {
			ccprintf("  Port: %d\n", j);
			modular_aic_print_port_props(port);
		}
	}

	return 0;
}
DECLARE_CONSOLE_COMMAND(modular_aic, modular_aic_command, NULL,
			"Print modular AIC information");
