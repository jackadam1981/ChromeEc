# Zephyr ADC Comparator Implementation Guidelines

[TOC]

## Background

Analog comparator uses ADC to continuously monitor analog signals with minimal CPU
involvement. Signal magnitude is compared with established threshold value, event
will be triggered so long as comparison criterion is met, this is: signal magnitude
is greater-than or equal-or-less-than threshold value. Many MCU vendors have
comparator features included with ADC peripherals.

## Objective

This document's goal is to depict the interface used to expose analog comparator
driver functionality within zephyr OS, additionally, establishes minimal
implementation guidelines in order to promote portability.

## Proposal

Analog comparator driver implementation is abstracted by leveraging from Zephyr
sensor interface, this set of API’s is proven to meet driver requirements and allow
using device tree to set static configuration during system initialization.

Structure previously mentioned is defined in `$(zephyr_root)/include/zephyr/drivers/sensor.h`
with following prototype:
```
__subsystem struct sensor_driver_api {
    sensor_attr_set_t attr_set;
    sensor_attr_get_t attr_get;
    sensor_trigger_set_t trigger_set;
    sensor_sample_fetch_t sample_fetch;
    sensor_channel_get_t channel_get;
};
```

Each driver must fill the `sensor_driver_api` structure with its own functions
implementation and export them by having `api` member of `struct device` holding
the reference to `sensor_deriver_api` instance.

## Functional description

Driver main purpose is to invoke callback function while signal monitored by ADC and
defined threshold value meet the comparison criterion. User is responsible for the well
use and configuration of the driver to achieve project goals. User must be mindful of
deactivating comparator driver if single notification is required.

Driver configuration and activation is achieved by the sensor API function `sensor_attr_set`
along with corresponding values for `sensor_attribute` and `sensor_value`, see Minimal
implementation guidelines. Similarly, user must register callback routine with the API
function `sensor_trigger_set`.

Please note that comparator activation (using `SENSOR_ATTR_ALERT` inside
` enum sensor_attribute`) must not be successful until all required parameters and function
callback are set, otherwise error must be returned.

## Driver Configuration structure

Individual driver `struct config` member must be used to hold static parameters, these
parameters must be set at compilation time and obtained from the devicetree.

## Devicetree Nodes Parameters

Each driver implementation must declare its own device tree node yaml file, these static
parameters must include the capability to set ADC port handler and ADC channel.

## Minimal implementation guidelines

In order to achieve a portable interface, it is encouraged to adhere to some guidelines
when implementing functions listed in sensor_driver_api structure. This section establishes
minimum requirements that each individual function should support, any additional feature
should be documented separately.

```
typedef int (*sensor_attr_set_t)(const struct device *dev,
                                 enum sensor_channel chan,
                                 enum sensor_attribute attr,
                                 const struct sensor_value *val);
```
Mandatory: YES

Parameter | Description
:---      | :----------
`dev`     | Pointer to ADC comparator driver.|
`chan`    | Sensor channel - For the purpose of analog comparator, this enum must support `SENSOR_CHAN_VOLTAGE`.|
`attr`    | Sensor attribute - This parameter offers wide range of functionality and it can be extended accordingly,
minimal parameters supported must be:
  `SENSOR_ATTR_LOWER_THRESH`: Set voltage corresponding to lower threshold in ADC raw value.
  `SENSOR_ATTR_UPPER_THRESH`: Set voltage corresponding to upper threshold in ADC raw value.
  `SENSOR_ATTR_ALERT`: Enable or disable triggering event when analog magnitude meets threshold selection.|
`val`     | Attribute value to be set.|
return    | Return 0 upon success.
Return -ENOTSUP when the attribute is not being supported.
Return -EINVAL when value is out of valid range.|

```
typedef int (*sensor_attr_get_t)(const struct device *dev,
                                 enum sensor_channel chan,
                                 enum sensor_attribute attr,
                                 struct sensor_value *val);
```
Mandatory: NO

Parameter | Description
:---      | :----------
`dev`     | Pointer to ADC comparator driver.|
`chan`    | Sensor channel - For the purpose of analog comparator, this enum must support `SENSOR_CHAN_VOLTAGE`.|
`attr`    | Sensor attribute - This parameter offers wide range of functionality and it can be extended accordingly,
minimal parameters supported must be:
  `SENSOR_ATTR_LOWER_THRESH`: Obtain voltage corresponding to lower threshold in ADC raw value.
  `SENSOR_ATTR_UPPER_THRESH`: Obtain voltage corresponding to upper threshold in ADC raw value.
  `SENSOR_ATTR_ALERT`: Get comparator current alert status: enabled or disabled.|
`val`     | Pointer of attribute value to be obtained.|
return    | Return 0 upon success.
Return -ENOTSUP when the attribute is not being supported.
Return -EINVAL when value is out of valid range.|

```
typedef int (*sensor_trigger_set_t)(const struct device *dev,
                                    const struct sensor_trigger *trig,
                                    sensor_trigger_handler_t handler);
```
Mandatory: YES

Parameter | Description
:---      | :----------
`dev`     | Pointer to ADC comparator driver.|
`trig`    | Pointer to sensor trigger structure, as minimal, members should support the following values:
    type - `SENSOR_TRIG_THRESHOLD`
    chan - `SENSOR_CHAN_VOLTAGE` |
`handler` | Reference to function to be called upon event triggered.|
return    | Return 0 upon success.
Return -ENOTSUP when the attribute is not being supported.
Return -EINVAL when value is out of valid range.|

```
typedef int (*sensor_sample_fetch_t)(const struct device *dev,
                                     enum sensor_channel chan);
```
Mandatory: NO

Parameter | Description
:---      | :----------
`dev`     | Pointer to ADC comparator driver.|
`chan`    | Sensor channel - For the purpose of analog comparator, this enum must support `SENSOR_CHAN_VOLTAGE`.|
return    | Return 0 when the ADC sample has been successfully fetched, nevative value otherwise.|

```
typedef int (*sensor_channel_get_t)(const struct device *dev,
                                    enum sensor_channel chan,
                                    struct sensor_value *val);
```
Mandatory: NO

Parameter | Description
:---      | :----------
`dev`     | Pointer to ADC comparator driver.
`chan`    | Sensor channel - For the purpose of analog comparator, this enum must support `SENSOR_CHAN_VOLTAGE`.|
`val`     | Pointer of value structure that will hold the pre-fetched result.|
return    | Return 0 when successfully and val will have sensor sampled value.|

## User Example

Please note that this only applies to minimal use cases, utilizing only functions presented
as mandatory in the previous section.

Node configuration for drivers parameters:
```
my_comparator {
        compatible = "vendor,adc-cmp";
        /*
         * Minimal parameters:
         *     ADC port handler & ADC channel.
         */
        io-channels = <&adc0 8>;
        /* Vendor specific static parameters */
        ...
};
```

Inside user .c file:
```
/* Obtaining ADC comparator driver reference */
struct device *dev = DEVICE_DT_GET(...)

void user_threshold_callback(const struct device *dev,
                             const struct sensor_trigger *trigger);

void user_enable_threshold_trigger(int voltage)
{
        struct sensor_trigger trig = {
        .type = SENSOR_TRIG_THRESHOLD,
        .chan = SENSOR_CHAN_VOLTAGE
        };
        struct sensor_value val;

        /* Set upper threshold */
        val.val1 = voltage;
        sensor_trigger_set(dev, SENSOR_CHAN_VOLTAGE,SENSOR_ATTR_UPPER_THRESH, &val);
        /*
         * Enables event trigger, when signal measured by ADC
         * exceeds voltage previously set, user_threshold_callback will
         * be invoked.
         */
        sensor_trigger_set(dev, &trig, &user_threshold_callback);
        val.val1 = true;
        sensor_trigger_set(dev, SENSOR_CHAN_VOLTAGE,SENSOR_ATTR_ALERT, &val);
}

void user_threshold_callback(const struct device *dev,
                             const struct sensor_trigger *trigger)
{
        /*
         * Handler code, this function will be contiguously called while
         * ADC measured voltage exceeds the onse set by
         * user_arm_threshold_trigger.
         */
        struct sensor_value val;
        /* It is recommended to disable ALERT */
        val.val1 = false;
        sensor_trigger_set(dev, SENSOR_CHAN_VOLTAGE,SENSOR_ATTR_ALERT, &val);
}
```
