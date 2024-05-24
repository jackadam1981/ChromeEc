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

#include <drivers/intel_modular_aic.h>

LOG_MODULE_REGISTER(aic, LOG_LEVEL_INF);

#define DT_DRV_COMPAT intel_modular_aic_slot

#define INTEL_MODULAR_AIC_TAG "MOD_AIC:"
#define INTEL_MODULAR_AIC_TAG_LEN 8

#if defined(INTEL_MODULAR_AIC_DETECTION_CHECK_TAG)
#define INTEL_MODULAR_AIC_INFO_ADDRESS INTEL_MODULAR_AIC_TAG_LEN
#else
#define INTEL_MODULAR_AIC_INFO_ADDRESS 0x00
#endif

struct intel_modular_aic_port {
	/* Count of devices bound to this port */
	const int devs_count;
	/* Pointer to list of devices bound this port */
	const struct device **devs;
	const struct device *slot;
	/* Pointer to next port */
	struct intel_modular_aic_port *next;
};

struct intel_modular_aic_slot_data {
	/* Indicates if this slot detection has been done */
	bool detection_done;
	/* Bitmap of ports connected to this slot */
	int used_ports;
	/* List of ports connected to this slot */
	struct intel_modular_aic_port *connected_ports;
	/* Power sequence callback */
	struct ap_pwrseq_state_callback cb;
};

struct intel_modular_aic_slot_config {
	uint8_t id;
	/* Detection GPIO spec */
	struct gpio_dt_spec detect_gpio;
	/* AP Power state for detection */
	enum ap_pwrseq_state power_state;
	/* Number of open ports available for connection in this slot */
	int available_ports;
	/* Ports that can be connected to this slot */
	struct intel_modular_aic_port *ports;
	int ports_count;
};

static int intel_modular_aic_slot_init(const struct device *dev);

#define INTEL_MODULAR_AIC_USBC_PORTS_DEVS_ARRAY(node_id, prop, idx) \
	DEVICE_DT_GET(DT_CAT5(node_id, _P_, prop, _IDX_, idx)),

#define INTEL_MODULAR_AIC_USBC_PORTS_DEVICES_DEFINE(node_id)                   \
	COND_CODE_1(                                                           \
		DT_NODE_HAS_PROP(node_id, devices),                            \
		(const struct device                                           \
			 *node_id##_devices[] = { DT_FOREACH_PROP_ELEM(        \
				 node_id, devices,                             \
				 INTEL_MODULAR_AIC_USBC_PORTS_DEVS_ARRAY) };), \
		())

DT_FOREACH_STATUS_OKAY(intel_modular_aic_usbc_port,
		       INTEL_MODULAR_AIC_USBC_PORTS_DEVICES_DEFINE)

#define INTEL_MODULAR_AIC_HDMI_PORTS_DEVS_ARRAY(node_id, prop, idx) \
	DEVICE_DT_GET(DT_CAT5(node_id, _P_, prop, _IDX_, idx)),

#define INTEL_MODULAR_AIC_HDMI_PORTS_DEVICES_DEFINE(node_id)                   \
	COND_CODE_1(                                                           \
		DT_NODE_HAS_PROP(node_id, devices),                            \
		(const struct device                                           \
			 *node_id##_devices[] = { DT_FOREACH_PROP_ELEM(        \
				 node_id, devices,                             \
				 INTEL_MODULAR_AIC_HDMI_PORTS_DEVS_ARRAY) };), \
		())

DT_FOREACH_STATUS_OKAY(intel_modular_aic_hdmi_port,
		       INTEL_MODULAR_AIC_HDMI_PORTS_DEVICES_DEFINE)

#define INTEL_MODULAR_AIC_PORT_DEFINE(node_id)                                 \
	{                                                                      \
		.devs = COND_CODE_1(DT_NODE_HAS_PROP(node_id, devices),        \
				    (node_id##_devices), (NULL)),              \
		.devs_count = COND_CODE_1(DT_NODE_HAS_PROP(node_id, devices),  \
					  (DT_PROP_LEN(node_id, devices)),     \
					  (0)),                                \
	}

#define INTEL_MODULAR_AIC_PORT_DEFINE_(node_id) \
	INTEL_MODULAR_AIC_PORT_DEFINE(node_id)

#define INTEL_MODULAR_AIC_PORT_NAME(node_id, prop, idx) \
	INTEL_MODULAR_AIC_PORT_DEFINE_(DT_CAT5(node_id, _P_, prop, _IDX_, idx)),

#define INTEL_MODULAR_AIC_SLOT_PORTS_ARRAY_DEFINE(inst)                         \
	COND_CODE_1(                                                            \
		DT_INST_NODE_HAS_PROP(inst, ports),                             \
		(struct intel_modular_aic_port                                  \
			 slot##inst##_ports[] = { DT_INST_FOREACH_PROP_ELEM(    \
				 inst, ports, INTEL_MODULAR_AIC_PORT_NAME) };), \
		())

DT_INST_FOREACH_STATUS_OKAY(INTEL_MODULAR_AIC_SLOT_PORTS_ARRAY_DEFINE)

#define INTEL_MODULAR_AIC_DEFINE(inst)                                       \
	struct intel_modular_aic_slot_data intel_modular_aic_data_##inst;    \
                                                                             \
	const struct intel_modular_aic_slot_config                           \
		intel_modular_aic_config_##inst = {                          \
			.id = DT_INST_REG_ADDR(inst),                        \
			.detect_gpio =                                       \
				GPIO_DT_SPEC_INST_GET(inst, detect_gpios),   \
			.power_state = DT_INST_STRING_TOKEN(                 \
				inst, detection_power_state),                \
			.available_ports =                                   \
				DT_INST_PROP(inst, available_ports),         \
			.ports = COND_CODE_1(DT_INST_NODE_HAS_PROP(inst,     \
								   ports),   \
					     (slot##inst##_ports), (NULL)),  \
			.ports_count = COND_CODE_1(                          \
				DT_INST_NODE_HAS_PROP(inst, ports),          \
				(DT_INST_PROP_LEN(inst, ports)), (0)),       \
		};                                                           \
	DEVICE_DT_INST_DEFINE(inst, intel_modular_aic_slot_init, NULL,       \
			      &intel_modular_aic_data_##inst,                \
			      &intel_modular_aic_config_##inst, POST_KERNEL, \
			      CONFIG_APPLICATION_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(INTEL_MODULAR_AIC_DEFINE)

#define INTEL_MODULAR_AIC_DEFINE_ARRAY(node_id) \
	[DT_REG_ADDR(node_id)] = DEVICE_DT_GET(node_id),

static const struct device *slots[] = { DT_FOREACH_STATUS_OKAY(
	intel_modular_aic_slot, INTEL_MODULAR_AIC_DEFINE_ARRAY) };

static void
intel_modular_aic_connect_ports(const struct device *dev)
{
	const struct intel_modular_aic_slot_config *config = dev->config;
	struct intel_modular_aic_slot_data *data = dev->data;
	struct intel_modular_aic_port **ports_list = &data->connected_ports;

	if (data->used_ports >= config->available_ports) {
		return;
	}

	for (int i = 0; (data->used_ports < config->available_ports) &&
			i < config->ports_count;
	     i++) {
		struct intel_modular_aic_port *port = &config->ports[i];

		for (int j = 0; j < port->devs_count; j++) {
			/* TODO: Implement device probing mechanism */
			if (device_is_ready(port->devs[j])) {
				continue;
			}
			device_init(port->devs[j]);
		}
		*ports_list = port;
		ports_list = &port->next;
		data->used_ports++;
	}
}

static int intel_modular_aic_slot_do_detection(const struct device *dev)
{
	const struct intel_modular_aic_slot_config *cfg = dev->config;
	struct intel_modular_aic_slot_data *data = dev->data;

	if (data->detection_done) {
		return 0;
	}

	if (!gpio_pin_get_dt(&cfg->detect_gpio)) {
		LOG_INF("AIC Device %s NOT Detected", dev->name);
		goto detection_end;
	}

	LOG_INF("AIC Device %s Detected", dev->name);
	intel_modular_aic_connect_ports(dev);
detection_end:
	data->detection_done = true;
	return 0;
}

static void power_state_callback(const struct device *dev,
				 enum ap_pwrseq_state entry,
				 enum ap_pwrseq_state exit)
{
	/* TODO: Improve this to avoid using array of slots */
	for (int i = 0; i < ARRAY_SIZE(slots); i++) {
		const struct device *slot = slots[i];
		const struct intel_modular_aic_slot_config *cfg = slot->config;

		if (entry == cfg->power_state) {
			intel_modular_aic_slot_do_detection(slot);
		}
	}
}

static int intel_modular_aic_slot_init(const struct device *dev)
{
	const struct intel_modular_aic_slot_config *cfg = dev->config;
	struct intel_modular_aic_slot_data *data = dev->data;

	gpio_pin_configure_dt(&cfg->detect_gpio, GPIO_INPUT);

	if (cfg->power_state == AP_POWER_STATE_UNDEF) {
		intel_modular_aic_slot_do_detection(dev);
	} else {
		const struct device *ap_pwrseq_dev = ap_pwrseq_get_instance();

		data->cb.states_bit_mask = BIT(cfg->power_state);
		data->cb.cb = power_state_callback;

		ap_pwrseq_register_state_entry_callback(ap_pwrseq_dev,
							&data->cb);
	}

	return 0;
}

int intel_modular_aic_slot_get_avail_ports(const struct device *slot)
{
	const struct intel_modular_aic_slot_config *cfg = slot->config;

	return cfg->available_ports;
}

intel_modular_aic_port_t
intel_modular_aic_slot_get_port(const struct device *slot, uint8_t port_id)
{
	struct intel_modular_aic_slot_data *data = slot->data;
	const struct intel_modular_aic_slot_config *cfg = slot->config;
	struct intel_modular_aic_port *port;
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
