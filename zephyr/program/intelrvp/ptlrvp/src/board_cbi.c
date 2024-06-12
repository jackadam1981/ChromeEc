#include "cros_board_info.h"
#include "ec_commands.h"
#include "util.h"

#include <drivers/modular_aic.h>

#define MODULAR_AIC_DEFINE_ARRAY(node_id) \
	[DT_PROP(node_id, id)] = DEVICE_DT_GET(node_id),

static const struct device *mod_aic_slots[] = { DT_FOREACH_STATUS_OKAY(
	intel_modular_aic_slot, MODULAR_AIC_DEFINE_ARRAY) };

int cbi_board_override(enum cbi_data_tag tag, uint8_t *buf, uint8_t *size)
{
	uint8_t slots_count = *buf;
	uint8_t *ret_total_ports = buf;
	uint8_t total_ports = 0;
	uint16_t props;

	switch (tag) {
	case CBI_TAG_MODULAR_IO_ID:
		*size = 1;
		if (slots_count > ARRAY_SIZE(mod_aic_slots)) {
			slots_count = ARRAY_SIZE(mod_aic_slots);
		}
		buf++;
		for (int i = 0; i < slots_count; i++) {
			const struct device *slot = mod_aic_slots[i];
			int ports_count =
				modular_aic_slot_get_avail_ports(slot);

			for (int j = 0; j < ports_count; j++) {
				modular_aic_port_t port =
					modular_aic_slot_get_port(slot, j);
				if (port) {
					modular_aic_port_get_raw_properties(port,
									    &props);
				} else {
					props = 0;
				}
				memcpy(buf, &props, sizeof(props));
				*size += sizeof(props);
				buf += sizeof(props);
				total_ports++;
			}
		}
		*ret_total_ports = total_ports;
		break;
	default:
		break;
	}

	return 0;
}
