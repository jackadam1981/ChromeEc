# Zephyr CrOS Board Information (CBI) Configuration

[TOC]

## Overview

If your board includes an EEPROM to store [CBI], then this feature must be
enabled and configured. Note that the [I2C buses] must be configured and
working before enabling CBI.

CBI includes two different pieces of firmware configuration information.
1) The FW_CONFIG that describes board options for a given project and
2) SSFC, second source firmware cache, which describes later decisions
for a board on any hardware that can be used as an alternate for the
original hardware.

## Kconfig Options

Kconfig Option                     | Default state | Documentation
:--------------------------------- | :-----------: | :------------
`CONFIG_CBI_EEPROM`                | y             | [EC CBI]

## FW_CONFIG Device Tree Specification

Every project can have different configuration options that needs to be known
before the firmware is started. This information is stored in the FW_CONFIG
portion of CBI. This could, but is not required to, include the following
types of information: sub board types, stylus present, keyboard backlight
present, keyboard number pad present, keyboard layout type and HDMI present.
It could include other information that is not currently used.

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

This could map to a Device Tree Specification for <boardname> project.

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
				enum-name = "FW_STYLUS_NOT_PRESET";
				value = <0>;
			};
			stylus-present {
				compatible = "cros-ec,cbi-fw-config-value";
				enum-name = "FW_STYLUS_PRESET";
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
				enum-name = "FW_KB_BACKLIGHT_NOT_PRESET";
				value = <0>;
			};
			kb_backlight-present {
				compatible = "cros-ec,cbi-fw-config-value";
				enum-name = "FW_KB_BACKLIGHT_PRESET";
				value = <1>;
			};
		};
	};
};
```

## Accessing CBI FW_CONFIG

In order to use the above structure to access the sub-board field of the
CBI FW_CONFIG, one would do something like this
```
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

## SSFC Device Tree Specification

## Testing and Debugging

Refer to the [CBI debugging information] to verify communication with the CBI
EEPROM.

[CBI]: https://chromium.googlesource.com/chromiumos/docs/+/HEAD/design_docs/cros_board_info.md
[I2C buses]: ./i2c.md
[CBI debugging information]: ./i2c.md#
