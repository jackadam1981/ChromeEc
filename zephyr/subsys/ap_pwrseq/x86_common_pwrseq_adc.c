#include <sys/atomic.h>
#include <x86_non_dsx_adlp_pwrseq_sm.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>
#include "drivers/sensor.h"

LOG_MODULE_DECLARE(ap_pwrseq, LOG_LEVEL_INF);

struct power_signal_adc {
	struct device const *dev_trig_high;
	struct device const *dev_trig_low;
	atomic_t value;
	enum power_signal signal;
	const char *name;
};

static void x86_common_adc_handler(struct power_signal_adc *adc,
				       const struct device *dev)
{
	struct sensor_value val;

	if (dev == adc->dev_trig_high) {
		atomic_set_bit(&adc->value, 0);

		/* Disable this interrupt while it's asserted. */
		val.val1 = false;
		sensor_attr_set(adc->dev_trig_high, SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT, &val);

		/* Enable the voltage low interrupt. */
		val.val1 = true;
		sensor_attr_set(adc->dev_trig_low, SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT, &val);
		LOG_DBG("%s is HIGH", adc->name);
	} else if (dev == adc->dev_trig_low) {
		atomic_clear_bit(&adc->value, 0);

		/* Disable this interrupt while it's asserted. */
		val.val1 = false;
		sensor_attr_set(adc->dev_trig_low, SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT, &val);

		/* Enable the voltage high interrupt. */
		val.val1 = true;
		sensor_attr_set(adc->dev_trig_high, SENSOR_CHAN_VOLTAGE,
			SENSOR_ATTR_ALERT, &val);
		LOG_DBG("%s is LOW", adc->name);
	} else {
		LOG_DBG("%s is Spurious", adc->name);
	}
	power_update_signals();
}

#define ADC_CMP_POWER_SIGNAL_LIST_NODE                                         \
	DT_COMPAT_GET_ANY_STATUS_OKAY(intel_ap_pwrseq_adc_signal)

#define X86_COMMON_ADC_DEFINE(node_id)                                         \
	static struct power_signal_adc x86_adc_##node_id = {                   \
		.dev_trig_high = DEVICE_DT_GET(DT_PROP(node_id, trigger_high)),\
		.dev_trig_low = DEVICE_DT_GET(DT_PROP(node_id, trigger_low)),  \
		.signal = GEN_POWER_SIGNAL_ENUM(node_id),                      \
	};                                                                     \
	static void x86_adc_cb##node_id(const struct device *dev,              \
					       const struct sensor_trigger     \
					       *trigger)                       \
	{                                                                      \
		x86_common_adc_handler(&x86_adc_##node_id, dev);               \
	}

DT_FOREACH_CHILD(ADC_CMP_POWER_SIGNAL_LIST_NODE, X86_COMMON_ADC_DEFINE)

#define X86_COMMON_ADC_INSTS_TO_INIT_WITH_COMMA(node_id)                       \
	{                                                                      \
		.adc = &x86_adc_##node_id,                                     \
		.handler = x86_adc_cb##node_id,                                \
	},

void pwrseq_adc_init(void)
{
	struct sensor_trigger trig = {
		.type = SENSOR_TRIG_THRESHOLD,
		.chan = SENSOR_CHAN_VOLTAGE
	};
	struct sensor_value val;
	struct {
		struct power_signal_adc *adc;
		sensor_trigger_handler_t handler;
	} adc_inst[] = {
		DT_FOREACH_CHILD(ADC_CMP_POWER_SIGNAL_LIST_NODE,
			X86_COMMON_ADC_INSTS_TO_INIT_WITH_COMMA)
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(adc_inst); i++) {
		/* Set high and low trigger callbacks */
		sensor_trigger_set(adc_inst[i].adc->dev_trig_high,
			&trig, adc_inst[i].handler);
		sensor_trigger_set(adc_inst[i].adc->dev_trig_low,
			&trig, adc_inst[i].handler);

		/* Enable high trigger callback only */
		val.val1 = true;
		sensor_attr_set(adc_inst[i].adc->dev_trig_high,
			SENSOR_CHAN_VOLTAGE, SENSOR_ATTR_ALERT, &val);
	}
}

#define X86_COMMON_ADC_INSTS_WITH_COMMA(node_id)    &x86_adc_##node_id,        \

struct power_signal_adc *adc_inst[] = {
		DT_FOREACH_CHILD(ADC_CMP_POWER_SIGNAL_LIST_NODE,
			X86_COMMON_ADC_INSTS_WITH_COMMA)
};

uint8_t pwrseq_adc_get_level(enum power_signal signal)
{
	for (int i = 0; i < ARRAY_SIZE(adc_inst); i++) {
		if (adc_inst[i]->signal == signal) {
			return !!adc_inst[i]->value;
		}
	}
	return 0;
}
