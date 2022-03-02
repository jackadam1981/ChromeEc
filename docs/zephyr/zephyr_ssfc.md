# Zephyr SSFC configuration and use.

[TOC]

## Overview

Zephyr CBI SSFC configuration

## Kconfig Options

The CrOS Board Information [CBI] feature is enabled with the
[CONFIG_PLATFORM_EC_CBI_EEPROM] Kconfig.

One of the CBI elements is the Second Source Factory Cache [SSFC] field.
This config is used at run time to select different hardware options or behaviours, and
is defined generally on a board by board basis.  The SSFC describes later decisions
for a board to indicate alternate second sourced hardware stuffing
which can be used by the EC to know which drivers to load.

The SSFC block is limited to 32 bits. The 32 bits are divided into individual
fields of varying sizes. Each field has defined values that may be set to control
the behaviour according to the definition for that board.

Device tree is used to define and specify the field sizes and values.

## Devicetree Nodes

The `SSFC` device tree nodes are defined via the [named-cbi-ssfc] and
[named-cbi-ssfc-value] YAML bindings.

The `named-cbi-fw-config` bindings define the name and size of each field.
The `named-cbi-fw-config-value` bindings allow names/values to be defined for each
value that may be stored in the field.
One of the values may be designated as the default, which is used if
the CBI data cannot be accessed.

Spare CBI SSFC fields are always initialised as zeros, so that
future fields will have a guaranteed value, so typically a zero
value is used as a default, indicating the default field.

An example definition is:
```
cbi-ssfc {
    compatible = "named-cbi-ssfc";
    base_sensor {
        enum-name = "BASE_SENSOR";
        size = <3>;
        bmi160 {
            compatible = "named-cbi-ssfc-value";
            status = "okay";
            value = <1>;
        };
    };
    lid_sensor {
        enum-name = "LID_SENSOR";
        size = <3>;
        bma255 {
            compatible = "named-cbi-ssfc-value";
            status = "okay";
            value = <1>;
        };
    };
    lightbar {
        enum-name = "LIGHTBAR";
        size = <2>;
        10_led {
            compatible = "named-cbi-ssfc-value";
            status = "okay";
            value = <1>;
        };
    };
};
```

This will be converted to be

```
union cbi_ssfc {
    struct {
        uint32_t cbi_ssfc_DT_N_S_cbi_ssfc_S_base_sensor:3
        uint32_t cbi_ssfc_DT_N_S_cbi_ssfc_S_lid_sensor:3
        uint32_t cbi_ssfc_DT_N_S_cbi_ssfc_S_lightbar:2
        uint32_t reserved : 24;
    };
    uint32_t raw_value;
};
```

The device tree will generate a series of
enum values and field names that can used
to read the values (via the CBI driver).

## Board Specific Code

To access the generated enums and names, the CBI driver
should be used to access the CBI API to check for an SSFC match e.g:

```
#include "cros_cbi.h"

    bool is_lid_sensor_bma255()
    {
        return cros_cbi_ssfc_check_match(cbi_dev, bma255);
    }
```

## Threads

No threads used in this feature.

## Testing and Debugging

There are unit tests.


[CBI]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/design_docs/cros_board_info.md
[CONFIG_PLATFORM_EC_CBI_EEPROM]: ./zephyr_cbi.md
[named-cbi-ssfc]: ../zephyr/dts/bindings/cbi/named-cbi-fw-config.yaml
[named-cbi-ssfc-value]: ../zephyr/dts/bindings/cbi/named-cbi-fw-config-value.yaml
[SSFC]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/design_docs/firmware_config.md
