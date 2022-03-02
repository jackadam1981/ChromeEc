# Zephyr CrOS Board Information (CBI) Configuration

[TOC]

## Overview

If your board includes an EEPROM to store [CBI], then this feature must be
enabled and configured.

CBI includes two different pieces of firmware relevant configuration
information.
1) The FW_CONFIG describes board options for a given project which can be
used by the Embedded Controller (EC) and the Application Processor (AP).
2) The Second Source Factory Cache (SSFC) describes later decisions
for a board to describe alternate second sourced hardware stuffing
which can be used by the EC to know which drivers to load.

The values in these CBI fields are programmed during manufacturing and the
following information is present to help firmware interpret the meaning
behind what is programmed in this EEPROM.

Note that the [I2C buses] must be configured and working before enabling CBI.

## Kconfig Options

Kconfig Option      | Default state | Documentation
:------------------ | :-----------: | :------------
`CONFIG_CBI_EEPROM` | y             | [EC CBI]

## CBI Firmware Configuration (FW_CONFIG) Device Tree node

CBI FW_CONFIG fields definition. Each field is defined via a start bit
(from LSB) and a size. The total size of all FW_CONFIG bit fields must
not exceed 32 bits. One default value for each field may be indicated.
The parent node should include that it is compatible with
"cros-ec,cbi-fw-config"

FW_CONFIG properties:

Property    | Description                | Settings
:---------- | :------------------------- | :-----------------
`enum-name` | Name of the field.         | Descriptive name
`start`     | Field starting bit offset. | 0 - 31
`size`      | Field size in bits.        | 1 - 32

FW_CONFIG field value properties describe the possible values a CBI FW_CONFIG
field may have.  This value field must be defined as being compatible with
"cros-ec,cbi-fw-config-value"

FW_CONFIG field value properties:

Property    | Description                     | Settings
:---------- | :------------------------------ | :-----------------------------
`enum-name` | Name of the field value.        | Descriptive enumeration name
`value`     | Unique value for this field.    | Integer
`default`   | Optional indication if default. | Boolean

## CBI FW_CONFIG Device Tree Specification

Every project can have different configuration options that need to be known
before the firmware is started. This information is stored in the FW_CONFIG
portion of CBI. This could, but is not required to, include the following
types of information: sub board types, stylus present, keyboard backlight
present, keyboard number pad present, keyboard layout type and HDMI present.
It could be extended to include other information that is not currently used.

The number of bits, such as the need to account for more sub-boards, would
also be decided upon before the product is finalized. The following structure
represents a conceptual FW_CONFIG that can have up to four different sub
board types, a stylus present and a keyboard backlight present.

```
	struct project_fw_config {
		uint32_t fw_sub_board		: 2;
		uint32_t fw_stylus		: 1;
		uint32_t fw_kb_backlight	: 1;
	};
```

This would map to a Device Tree Specification for <boardname> project.

```
/ {
	boardname-fw-config {
		compatible = "cros-ec,cbi-fw-config";

		/*
		 * FW_CONFIG field to indicate which sub-board
		 * is attached.
		 */
		sub-board {
			enum-name = "FW_SUB_BOARD";
			start = <0>;
			size = <2>;

			sub-board-1 {
				compatible = "cros-ec,cbi-fw-config-value";
				enum-name = "FW_SUB_BOARD_1";
				value = <1>;
			};
			sub-board-2 {
				compatible = "cros-ec,cbi-fw-config-value";
				enum-name = "FW_SUB_BOARD_2";
				value = <2>;
			};
			sub-board-3 {
				compatible = "cros-ec,cbi-fw-config-value";
				enum-name = "FW_SUB_BOARD_3";
				value = <3>;
			};
		};

		/*
		 * FW_CONFIG field to indicate if a stylus is present.
		 */
		stylus {
			enum-name = "FW_STYLUS";
			start = <2>;
			size = <1>;

			stylus-not-present {
				compatible = "cros-ec,cbi-fw-config-value";
				enum-name = "FW_STYLUS_NOT_PRESENT";
				value = <0>;
			};
			stylus-present {
				compatible = "cros-ec,cbi-fw-config-value";
				enum-name = "FW_STYLUS_PRESENT";
				value = <1>;
			};
		};

		/*
		 * FW_CONFIG field to indicate if a keyboard backlight is
		 * present.
		 */
		kb-backlight {
			enum-name = "FW_KB_BACKLIGHT";
			start = <3>;
			size = <1>;

			kb_backlight-not-present {
				compatible = "cros-ec,cbi-fw-config-value";
				enum-name = "FW_KB_BACKLIGHT_NOT_PRESENT";
				value = <0>;
			};
			kb_backlight-present {
				compatible = "cros-ec,cbi-fw-config-value";
				enum-name = "FW_KB_BACKLIGHT_PRESENT";
				value = <1>;
			};
		};
	};
};
```

## CBI FW_CONFIG Access

In order to use the above structure to access the sub-board field of the
CBI FW_CONFIG, one would do something like this
```
enum boardname_sub_board_type {
	BOARDNAME_SB_UNKNOWN,
	BOARDNAME_SB_NONE,
	BOARDNAME_SB_C_A,
	BOARDNAME_SB_C_LTE,
	BOARDNAME_SB_HDMI_A
};
enum boardname_sub_board_type boardname_get_sb_type(void)
{
	static enum boardname_sub_board_type sb = BOARDNAME_SB_UNKNOWN;
	int ret;
	uint32_t val;

	/*
	 * Return cached value.
	 */
	if (sb != BOARDNAME_SB_UNKNOWN)
		return sb;

	sb = BOARDNAME_SB_NONE;	/* Defaults to none */
	ret = cros_cbi_get_fw_config(FW_SUB_BOARD, &val);
	if (ret != 0) {
		LOG_WRN("Error retrieving CBI FW_CONFIG field %d",
			FW_SUB_BOARD);
		return sb;
	}
	switch (val) {
	case FW_SUB_BOARD_1:
		sb = BOARDNAME_SB_C_A;
		LOG_INF("SB: USB type C, USB type A");
		break;

	case FW_SUB_BOARD_2:
		sb = BOARDNAME_SB_C_LTE;
		LOG_INF("SB: USB type C, WWAN LTE");
		break;

	case FW_SUB_BOARD_3:
		sb = BOARDNAME_SB_HDMI_A;
		LOG_INF("SB: HDMI, USB type A");
		break;

	default:
		LOG_WRN("No sub-board defined");
		break;
	}
	return sb;
}
```

In order to use the above structure to access the stylus field of the
CBI FW_CONFIG, one would do something like this
```
enum boardname_stylus_present {
	BOARDNAME_STYLUS_PRESENT_UNKNOWN,
	BOARDNAME_STYLUS_NOT_PRESENT,
	BOARDNAME_STYLUS_PRESENT
};
enum boardname_stylus_present boardname_get_stylus_present(void)
{
	static enum boardname_stylus_present stylus_present =
		BOARDNAME_STYLUS_PRESENT_UNKNOWN;
	int ret;
	uint32_t val;

	/*
	 * Return cached value.
	 */
	if (sb != BOARDNAME_STYLUS_PRESENT_UNKNOWN)
		return sb;

	ret = cros_cbi_get_fw_config(FW_STYLUS, &val);
	if (ret != 0) {
		LOG_WRN("Error retrieving CBI FW_CONFIG field %d",
			FW_SUB_BOARD);
		return BOARDNAME_STYLUS_NOT_PRESENT;
	}
	switch (val) {
	case FW_STYLUS_PRESENT:
		stylus_present = BOARDNAME_STYLUS_PRESENT;
		LOG_INF("STYLUS: present");
		break;

	case FW_STYLUS_NOT_PRESENT:
		stylus_present = BOARDNAME_STYLUS_NOT_PRESENT;
		LOG_INF("STYLUS: not present");
		break;

	default:
		LOG_WRN("No stylus defined");
		break;
	}
	return stylus_present;
}
```

## CBI Secondary Source Factory Cache (SSFC) Device Tree node

CBI SSFC fields definition. The order of the
children in this node define the order of the SSFC bit fields from least
significant bit to most significant bit. The total size of all SSFC bit
fields must not exceed 32 bits. The parent node should include that it
is compatible with "named-cbi-ssfc"

SSFC properties:

Property    | Description         | Settings
:---------- | :------------------ | :---------------
`enum-name` | Name of the field.  | `"BASE_SENSOR"` <br> `"LID_SENSOR"` <br> `"LIGHTBAR`" <br> `"USB_SS_MUX"`
`size`      | Field size in bits. | 1 - 32

SSFC field value properties describe the possible values a CBI SSFC
field may have.  This value field must be defined as being compatible with
"named-cbi-ssfc-value"

SSFC field value properties:

Property    | Description                     | Settings
:---------- | :------------------------------ | :--------
`value`     | Unique value for this field.    | Integer
`default`   | Optional indication if default. | Boolean

## CBI SSFC Device Tree Specification

Every project can have different SSFC configuration options that need to be
known before the firmware is started. This information is stored in the
SSFC portion of CBI.

The number of bits needed to account for more second source options would
need to be decided upon as these fields are created. The following structure
represents a conceptual SSFC to make room for eight base sensors, eight
lid sensors and four lightbar options.

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

## CBI SSFC Access

## Threads

No threads used in this feature.

## Testing and Debugging

There are unit tests.

[CBI]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/design_docs/cros_board_info.md
[I2C buses]: ./i2c.md
