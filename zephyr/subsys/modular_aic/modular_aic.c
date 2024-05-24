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
	/* Designated port id, this is applicable for Fixed ports type */
	const int8_t id;
	/* Count of devices bound to this port */
	const int devs_count;
	/* Pointer to list of devices bound this port */
	const struct device **devs;
	/* Port properties */
	union modular_aic_properties props;
	/* Pointer to next port */
	struct modular_aic_port *next;
};

struct modular_aic_slot_data {
	/* Indicates if this slot has been sampled */
	bool is_sampled;
	/* Slot propwerties */
	union modular_aic_properties props;
	/* Bitmap of ports connected to this slot */
	int used_ports;
	/* List of ports connected to this slot */
	struct modular_aic_port *ports;
	/* Power sequence callback */
	struct ap_pwrseq_state_callback cb;
};

struct modular_aic_slot_config {
	uint8_t id;
	/* Detection GPIO spec */
	struct gpio_dt_spec detect_gpio;
	/* AP Power state for sampling */
	enum ap_pwrseq_state power_state;
	/* Number if available ports in this slot */
	int avail_ports;
};

static int modular_aic_slots_init(const struct device *dev);

#define MODULAR_AIC_GET_PROPS(node_id)                                      \
	{                                                                   \
		.type = DT_STRING_TOKEN(node_id, type),                     \
		.conn_type = DT_STRING_TOKEN(node_id, conn_type),           \
		.retimer_status = DT_STRING_TOKEN(node_id, retimer_status), \
		.pd_cntrl_type = DT_STRING_TOKEN(node_id, pd_cntrl_type),   \
		.tbt_status = DT_STRING_TOKEN(node_id, tbt_status),         \
		.data_speed = DT_STRING_TOKEN(node_id, data_speed),         \
	}

#define MODULAR_AIC_DEFINE(inst)                                              \
	struct modular_aic_slot_data modular_aic_data_##inst = {              \
		.props = MODULAR_AIC_GET_PROPS(DT_DRV_INST(inst)),            \
	};                                                                    \
	const struct modular_aic_slot_config modular_aic_config_##inst = {    \
		.id = DT_INST_PROP(inst, id),    \
		.detect_gpio = GPIO_DT_SPEC_INST_GET(inst, detect_gpios),     \
		.power_state = COND_CODE_1(                                   \
			DT_INST_NODE_HAS_PROP(inst, sampling_power_state),    \
			(DT_INST_STRING_TOKEN(inst, sampling_power_state)),   \
			(AP_POWER_STATE_UNDEF)),                              \
		.avail_ports = DT_INST_PROP(inst, available_ports),           \
	};                                                                    \
	DEVICE_DT_INST_DEFINE(inst, modular_aic_slots_init, NULL,             \
			      &modular_aic_data_##inst,                       \
			      &modular_aic_config_##inst, POST_KERNEL,        \
			      CONFIG_APPLICATION_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(MODULAR_AIC_DEFINE)

#define MODULAR_AIC_PORS_IN_SLOTS_DEFINE_ARRAY(node_id)      \
	[DT_PROP(node_id, id)] = DT_PROP(node_id, available_ports),

static const uint8_t ports_in_slots[] = { DT_FOREACH_STATUS_OKAY(
	intel_modular_aic_slot, MODULAR_AIC_PORS_IN_SLOTS_DEFINE_ARRAY) };

#define MODULAR_AIC_DEFINE_ARRAY(node_id) \
	[DT_PROP(node_id, id)] = DEVICE_DT_GET(node_id),

static const struct device *slots[] = { DT_FOREACH_STATUS_OKAY(
	intel_modular_aic_slot, MODULAR_AIC_DEFINE_ARRAY) };

#define MODULAR_AIC_SLOT_DEVS_ARRAY(node_id, prop, idx) \
	DEVICE_DT_GET(DT_CAT5(node_id, _P_, prop, _IDX_, idx)),

#define MODULAR_AIC_PORTS_DEVICES_DEFINE(node_id)                           \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, devices),                     \
		    (const struct device                                    \
			     *node_id##_devices[] = { DT_FOREACH_PROP_ELEM( \
				     node_id, devices,                      \
				     MODULAR_AIC_SLOT_DEVS_ARRAY) };),      \
		    ())

DT_FOREACH_STATUS_OKAY(intel_modular_aic_port, MODULAR_AIC_PORTS_DEVICES_DEFINE)

#define MODULAR_AIC_PORTS_DEFINE(node_id)                                     \
	{                                          \
		.id = COND_CODE_1(DT_NODE_HAS_PROP(node_id, id), \
					  (DT_PROP(node_id, id)),    \
					  (-1)),\
		.devs_count = COND_CODE_1(DT_NODE_HAS_PROP(node_id, devices), \
					  (DT_PROP_LEN(node_id, devices)),    \
					  (0)),                               \
		.props = MODULAR_AIC_GET_PROPS(node_id),                      \
		.devs = COND_CODE_1(DT_NODE_HAS_PROP(node_id, devices),       \
				    (node_id##_devices), (NULL)),             \
	},

static struct modular_aic_port ports[] = { DT_FOREACH_STATUS_OKAY(
	intel_modular_aic_port, MODULAR_AIC_PORTS_DEFINE) };

static inline uint8_t modular_aic_slot_get_port_id_start(uint8_t slot_id)
{
	uint8_t port_id_start = 0;

	for (uint8_t i = 0; i < slot_id; i++) {
		port_id_start += ports_in_slots[i];
	}

	return port_id_start;
}

static bool modular_aic_slot_port_is_free(const struct device *dev,
					  struct modular_aic_port *port)
{
	const struct modular_aic_slot_config *cfg = dev->config;
	struct modular_aic_slot_data *data = dev->data;
	uint8_t used_ports_map = data->used_ports;

	if (data->props.type == MODULAR_AIC_PORT_TYPE_FIXED) {
		uint8_t valid_ports_map = BIT(cfg->avail_ports) - 1;
		uint8_t port_id_start = modular_aic_slot_get_port_id_start(cfg->id);

		valid_ports_map <<= port_id_start;
		if (!(BIT(port->id) & valid_ports_map)) {
			return false;
		}
		used_ports_map <<= port_id_start;
		if (BIT(port->id) & used_ports_map) {
			return false;
		}
	} else {
		for (uint8_t i = 0; i < cfg->avail_ports; i++) {
			if (!(used_ports_map & BIT(i))) {
				return true;
			}
		}
		return false;
	}

	return true;
}

static void modular_aic_slot_set_used_ports(const struct device *dev,
					    struct modular_aic_port *port)
{
	const struct modular_aic_slot_config *cfg = dev->config;
	struct modular_aic_slot_data *data = dev->data;
	uint8_t port_map;
	uint8_t port_id_start;

	if (data->props.type == MODULAR_AIC_PORT_TYPE_FIXED) {
		port_id_start = modular_aic_slot_get_port_id_start(cfg->id);

		port_map = BIT(port->id - port_id_start);
	} else {
		for (uint8_t i = 0; i < cfg->avail_ports; i++) {
			port_map = BIT(i);
			if (!(data->used_ports & port_map)) {
				break;
			}
		}
	}

	data->used_ports |= port_map;
}

static bool modular_aic_slot_port_is_compatible(const struct device *dev,
					   struct modular_aic_port *port)
{
	struct modular_aic_slot_data *data = dev->data;

	if (port->props.type != data->props.type ||
	    port->props.conn_type != data->props.conn_type) {
		return false;
	}

	if (port->props.conn_type == MODULAR_AIC_PORT_CONN_TYPE_TYPEC) {
		if (port->props.retimer_status != data->props.retimer_status ||
		    port->props.pd_cntrl_type != data->props.pd_cntrl_type ||
		    port->props.tbt_status != data->props.tbt_status ||
		    port->props.data_speed != data->props.data_speed) {
			return false;
		}
	} else if (port->props.conn_type == MODULAR_AIC_PORT_CONN_TYPE_HDMI) {
		if (port->props.data_speed != data->props.data_speed) {
			return false;
		}
	}

	return true;
}

static void modular_aic_init_devices(const struct device *dev)
{
	struct modular_aic_slot_data *data = dev->data;
	struct modular_aic_port **ports_list = &data->ports;

	for (int i = 0; i < ARRAY_SIZE(ports); i++) {
		struct modular_aic_port *port = &ports[i];

		if (port->props.present) {
			/* Current port is already used */
			continue;
		}
		if (!modular_aic_slot_port_is_compatible(dev, port)) {
			/* This slot and port are not compatible */
			continue;
		}
		if (!modular_aic_slot_port_is_free(dev, port)) {
			/* Slot does not have ports available */
			continue;
		}
		for (int j = 0; j < port->devs_count; j++) {
			if (device_is_ready(port->devs[j])) {
				continue;
			}
			device_init(port->devs[j]);
		}
		port->props.present = 1;
		modular_aic_slot_set_used_ports(dev, port);
		*ports_list = port;
		ports_list = &port->next;
	}
	*ports_list = NULL;
}

static int modular_aic_slot_sample(const struct device *dev)
{
	const struct modular_aic_slot_config *cfg = dev->config;
	struct modular_aic_slot_data *data = dev->data;

	if (data->is_sampled) {
		return 0;
	}

	if (!gpio_pin_get_dt(&cfg->detect_gpio)) {
		LOG_INF("AIC Device %s NOT Detected", dev->name);
		goto sample_end;
	}

	LOG_INF("AIC Device %s Detected", dev->name);
	LOG_INF("%s Device Found %s", __func__, dev->name);
	data->props.present = 1;
	modular_aic_init_devices(dev);
sample_end:
	data->is_sampled = true;
	return 0;
}

static void power_state_callback(const struct device *dev,
				 enum ap_pwrseq_state entry,
				 enum ap_pwrseq_state exit)
{
	for (int i = 0; i < ARRAY_SIZE(slots); i++) {
		const struct device *slot = slots[i];
		const struct modular_aic_slot_config *cfg = slot->config;

		if (entry == cfg->power_state) {
			modular_aic_slot_sample(slot);
		}
	}
}

static int modular_aic_slots_init(const struct device *dev)
{
	const struct modular_aic_slot_config *cfg = dev->config;
	struct modular_aic_slot_data *data = dev->data;

	gpio_pin_configure_dt(&cfg->detect_gpio, GPIO_INPUT);

	if (cfg->power_state == AP_POWER_STATE_UNDEF) {
		modular_aic_slot_sample(dev);
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

	return cfg->avail_ports;
}

modular_aic_port_t modular_aic_slot_get_port(const struct device *slot,
					     uint8_t port_id)
{
	struct modular_aic_slot_data *data = slot->data;
	const struct modular_aic_slot_config *cfg = slot->config;
	struct modular_aic_port *port;
	uint8_t i = 0;

	if (port_id >= cfg->avail_ports) {
		return NULL;
	}

	port = data->ports;
	while (port != NULL && i < port_id) {
		port = port->next;
		i++;
	}

	return port;
}

int modular_aic_port_get_raw_properties(modular_aic_port_t port,
					uint16_t *props)
{
	struct modular_aic_port *data = port;

	*props = data->props.raw_value;

	return 0;
}

bool modular_aic_port_is_present(modular_aic_port_t port)
{
	struct modular_aic_port *data = port;

	return data->props.present;
}

enum modular_aic_port_type modular_aic_port_get_type(modular_aic_port_t port)
{
	struct modular_aic_port *data = port;

	return data->props.type;
}

enum modular_aic_port_conn_type
modular_aic_port_get_conn_type(modular_aic_port_t port)
{
	struct modular_aic_port *data = port;

	return data->props.conn_type;
}

enum modular_aic_port_retimer_status
modular_aic_port_get_retimer_status(modular_aic_port_t port)
{
	struct modular_aic_port *data = port;

	return data->props.retimer_status;
}

enum modular_aic_port_pd_cntrl_type
modular_aic_port_get_pd_cntrl_type(modular_aic_port_t port)
{
	struct modular_aic_port *data = port;

	return data->props.pd_cntrl_type;
}

enum modular_aic_port_tbt_status
modular_aic_port_get_tbt_status(modular_aic_port_t port)
{
	struct modular_aic_port *data = port;

	return data->props.tbt_status;
}

enum modular_aic_port_data_speed
modular_aic_port_get_data_speed(modular_aic_port_t port)
{
	struct modular_aic_port *data = port;

	return data->props.data_speed;
}

static int modular_aic_command(int argc, const char **argv)
{
	for (int i = 0; i < ARRAY_SIZE(slots); i++) {
		const struct device *slot = slots[i];
		const struct modular_aic_slot_config *cfg = slot->config;
		struct modular_aic_slot_data *data = slot->data;

		ccprintf("\nSlot %d:\n", i);
		if (!data->is_sampled) {
			ccprintf("  Not Sampled.\n");
			continue;
		}
		if (!data->props.present) {
			ccprintf("  Not present.\n");
			continue;
		}
		switch (data->props.conn_type) {
		case MODULAR_AIC_PORT_CONN_TYPE_TYPEA:
			ccprintf("  Conn Type:  TYPEA\n");
			break;
		case MODULAR_AIC_PORT_CONN_TYPE_TYPEC:
			ccprintf("  Conn Type:  TYPEC\n");
			ccprintf("  EEPROM:     %s\n",
				 cfg->eeprom ? "Present" : "Not Present");
			ccprintf(
				"  Retimer:    %s\n",
				data->props.retimer_status ==
						MODULAR_AIC_PORT_RETIMER_NOT_PRESENT ?
					"Not Present" :
				data->props.retimer_status ==
						MODULAR_AIC_PORT_RETIMER_SINGLE ?
					"Single" :
				data->props.retimer_status ==
						MODULAR_AIC_PORT_RETIMER_DUAL ?
					"Dual" :
					"Unsupported");
			ccprintf(
				"  PD Vendor:  %s\n",
				data->props.pd_cntrl_type ==
						MODULAR_AIC_PORT_PD_CNTRL_TI ?
					"TI" :
				data->props.pd_cntrl_type ==
						MODULAR_AIC_PORT_PD_CNTRL_CYPRESS ?
					"Cypress" :
				data->props.pd_cntrl_type ==
						MODULAR_AIC_PORT_PD_CNTRL_REALTEK ?
					"Realtek" :
				data->props.pd_cntrl_type ==
						MODULAR_AIC_PORT_PD_CNTRL_GOTHIC_BRIDGE ?
					"Gothic Bridge" :
					"Unknown");
			ccprintf("  TBT status: %s\n", data->props.tbt_status ?
							       "Capable" :
							       "Not Capable");
			ccprintf(
				"  Data Speed: %s\n",
				data->props.data_speed ==
						MODULAR_AIC_PORT_DATA_SPEED_6GBPS ?
					"6 Gbps" :
				data->props.data_speed ==
						MODULAR_AIC_PORT_DATA_SPEED_12GBPS ?
					"12 Gbps" :
				data->props.data_speed ==
						MODULAR_AIC_PORT_DATA_SPEED_20GBPS ?
					"20 Gbps" :
				data->props.data_speed ==
						MODULAR_AIC_PORT_DATA_SPEED_40GBPS ?
					"40 Gbps" :
				data->props.data_speed ==
						MODULAR_AIC_PORT_DATA_SPEED_80GBPS ?
					"80 Gbps" :
					"Unsupported");
			break;
		case MODULAR_AIC_PORT_CONN_TYPE_HDMI:
			ccprintf("  Conn Type:  HDMI\n");
			ccprintf(
				"  Data Speed: %s\n",
				data->props.data_speed ==
						MODULAR_AIC_PORT_DATA_SPEED_6GBPS ?
					"6 Gbps" :
				data->props.data_speed ==
						MODULAR_AIC_PORT_DATA_SPEED_12GBPS ?
					"12 Gbps" :
					"Unsupported");
			break;
		case MODULAR_AIC_PORT_CONN_TYPE_DP:
			ccprintf("  Conn Type: DP\n");
			break;
		default:
			ccprintf("  Undefined Port!!\n");
			continue;
		}
	}

	return 0;
}
DECLARE_CONSOLE_COMMAND(modular_aic, modular_aic_command, NULL,
			"Print modular AIC information");
