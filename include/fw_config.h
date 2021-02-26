/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* FW_CONFIG utils */

#ifndef __CROS_EC_FW_CONFIG_H
#define __CROS_EC_FW_CONFIG_H

#ifdef CONFIG_FW_CONFIG_UTILS

/* Private struct definitions */
struct _fw_config_field {
	const uint8_t offset;
	const uint8_t width;
};
struct _fw_config_value {
	const struct _fw_config_field field;
	const uint8_t value;
};

/* Public access functions */
bool is_fw_config_valid(void);
int get_fw_config_field(const struct _fw_config_field field);
bool is_fw_config_value(const struct _fw_config_value value);

/* Private Macros */
#define _FW_CONFIG_PREFIX FW_CONFIG_
#define _FW_CONFIG_FIELD_NAME(field) CONCAT2(_FW_CONFIG_PREFIX, field)
#define _FW_CONFIG_VALUE_NAME(field, label) \
	CONCAT3(_FW_CONFIG_FIELD_NAME(field), _, label)

#define FW_CONFIG_FIELD(field, offset, width, ...)                         \
	BUILD_ASSERT((width) >= 1 && (width) <= 8,                         \
		     "FW_CONFIG field bit width must be >= 1 and <= 8");   \
	BUILD_ASSERT((offset) >= 0 && (offset) <= 31,                      \
		     "FW_CONFIG field bit offset must be >= 0 and <= 31"); \
	BUILD_ASSERT((offset) + (width) <= 32,                             \
		     "FW_CONFIG field bit offset + width must be <= 32");  \
	_FW_CONFIG_ASSERT_UNIQUE_BITS(offset, width);                      \
	_FW_CONFIG_FIELD(field, offset, width);                            \
	CONCAT2(_FW_CONFIG_VALUE_, VA_ARGS_COUNT(__VA_ARGS__))             \
				(field, offset, width, __VA_ARGS__)

#define _FW_CONFIG_FIELD(_field, _offset, _width)			       \
	static const struct _fw_config_field _FW_CONFIG_FIELD_NAME(_field) = { \
		.offset = (_offset),					       \
		.width = (_width)					       \
	}

#define _FW_CONFIG_VALUE(_field, _label, _value)	\
	static const struct _fw_config_value		\
	_FW_CONFIG_VALUE_NAME(_field, _label) = {	\
		.field = _FW_CONFIG_FIELD_NAME(_field),	\
		.value = (_value)			\
	}

#define _FW_CONFIG_VALUE_BLOCK(field, label, value) \
	_FW_CONFIG_VALUE(field, label, value);

#define _FW_CONFIG_VALUE_0(field, offset, width) \
	_FW_CONFIG_VALUE_BLOCK(field, UNKNOWN, -1)

#define _FW_CONFIG_VALUE_1(field, offset, width, label0) \
	_FW_CONFIG_VALUE_0(field, offset, width);        \
	_FW_CONFIG_VALUE_BLOCK(field, label0, 0)

#define _FW_CONFIG_VALUE_2(field, offset, width, label0, label1) \
	_FW_CONFIG_VALUE_1(field, offset, width, label0);        \
	_FW_CONFIG_VALUE_BLOCK(field, label1, 1)

#define _FW_CONFIG_VALUE_3(field, offset, width, label0, label1, label2) \
	_FW_CONFIG_VALUE_2(field, offset, width, label0, label1);        \
	_FW_CONFIG_VALUE_BLOCK(field, label2, 2)

#define _FW_CONFIG_VALUE_4(field, offset, width, label0, label1, label2,  \
			   label3)                                        \
	_FW_CONFIG_VALUE_3(field, offset, width, label0, label1, label2); \
	_FW_CONFIG_VALUE_BLOCK(field, label3, 3)

#define _FW_CONFIG_VALUE_5(field, offset, width, label0, label1, label2, \
			   label3, label4)                               \
	_FW_CONFIG_VALUE_4(field, offset, width, label0, label1, label2, \
			   label3);                                      \
	_FW_CONFIG_VALUE_BLOCK(field, label4, 4)

#define _FW_CONFIG_VALUE_6(field, offset, width, label0, label1, label2, \
			   label3, label4, label5)                       \
	_FW_CONFIG_VALUE_5(field, offset, width, label0, label1, label2, \
			   label3, label4);                              \
	_FW_CONFIG_VALUE_BLOCK(field, label5, 5)

#define _FW_CONFIG_VALUE_7(field, offset, width, label0, label1, label2, \
			   label3, label4, label5, label6)               \
	_FW_CONFIG_VALUE_6(field, offset, width, label0, label1, label2, \
			   label3, label4, label5);                      \
	_FW_CONFIG_VALUE_BLOCK(field, label6, 6)

#define _FW_CONFIG_VALUE_8(field, offset, width, label0, label1, label2, \
			   label3, label4, label5, label6, label7)       \
	_FW_CONFIG_VALUE_7(field, offset, width, label0, label1, label2, \
			   label3, label4, label5, label6);              \
	_FW_CONFIG_VALUE_BLOCK(field, label7, 7)

/*
 * These macros deconstruct the bit offset and range into unique variable
 * declarations. This serves to detect overlaps in bit ranges at compile time.
 * One level of indirection is required for incrementing the offset. Another
 * level of indirection is required for decrementing the width. Currently only
 * supports widths up to 8 bits and offsets up to 32 bits. Copy and paste to
 * support more bits.
 */
#define _FW_CONFIG_ASSERT_UNIQUE_BITS(offset, width) \
	CONCAT2(_FW_CONFIG_NEXT_UNIQUE_BIT_, width)(offset)

#define _FW_CONFIG_UNIQUE_BIT(bit) static int const _FW_CONFIG_UNIQUE_BIT_##bit;

#define _FW_CONFIG_NEXT_UNIQUE_BIT_0(offset)
#define _FW_CONFIG_NEXT_UNIQUE_BIT_1(offset) _FW_CONFIG_UNIQUE_BIT_##offset(0)
#define _FW_CONFIG_NEXT_UNIQUE_BIT_2(offset) _FW_CONFIG_UNIQUE_BIT_##offset(1)
#define _FW_CONFIG_NEXT_UNIQUE_BIT_3(offset) _FW_CONFIG_UNIQUE_BIT_##offset(2)
#define _FW_CONFIG_NEXT_UNIQUE_BIT_4(offset) _FW_CONFIG_UNIQUE_BIT_##offset(3)
#define _FW_CONFIG_NEXT_UNIQUE_BIT_5(offset) _FW_CONFIG_UNIQUE_BIT_##offset(4)
#define _FW_CONFIG_NEXT_UNIQUE_BIT_6(offset) _FW_CONFIG_UNIQUE_BIT_##offset(5)
#define _FW_CONFIG_NEXT_UNIQUE_BIT_7(offset) _FW_CONFIG_UNIQUE_BIT_##offset(6)
#define _FW_CONFIG_NEXT_UNIQUE_BIT_8(offset) _FW_CONFIG_UNIQUE_BIT_##offset(7)

#define _FW_CONFIG_UNIQUE_BIT_0(width) \
	_FW_CONFIG_UNIQUE_BIT(0) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(1)
#define _FW_CONFIG_UNIQUE_BIT_1(width) \
	_FW_CONFIG_UNIQUE_BIT(1) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(2)
#define _FW_CONFIG_UNIQUE_BIT_2(width) \
	_FW_CONFIG_UNIQUE_BIT(2) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(3)
#define _FW_CONFIG_UNIQUE_BIT_3(width) \
	_FW_CONFIG_UNIQUE_BIT(3) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(4)
#define _FW_CONFIG_UNIQUE_BIT_4(width) \
	_FW_CONFIG_UNIQUE_BIT(4) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(5)
#define _FW_CONFIG_UNIQUE_BIT_5(width) \
	_FW_CONFIG_UNIQUE_BIT(5) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(6)
#define _FW_CONFIG_UNIQUE_BIT_6(width) \
	_FW_CONFIG_UNIQUE_BIT(6) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(7)
#define _FW_CONFIG_UNIQUE_BIT_7(width) \
	_FW_CONFIG_UNIQUE_BIT(7) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(8)
#define _FW_CONFIG_UNIQUE_BIT_8(width) \
	_FW_CONFIG_UNIQUE_BIT(8) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(9)
#define _FW_CONFIG_UNIQUE_BIT_9(width) \
	_FW_CONFIG_UNIQUE_BIT(9) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(10)
#define _FW_CONFIG_UNIQUE_BIT_10(width) \
	_FW_CONFIG_UNIQUE_BIT(10) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(11)
#define _FW_CONFIG_UNIQUE_BIT_11(width) \
	_FW_CONFIG_UNIQUE_BIT(11) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(12)
#define _FW_CONFIG_UNIQUE_BIT_12(width) \
	_FW_CONFIG_UNIQUE_BIT(12) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(13)
#define _FW_CONFIG_UNIQUE_BIT_13(width) \
	_FW_CONFIG_UNIQUE_BIT(13) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(14)
#define _FW_CONFIG_UNIQUE_BIT_14(width) \
	_FW_CONFIG_UNIQUE_BIT(14) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(15)
#define _FW_CONFIG_UNIQUE_BIT_15(width) \
	_FW_CONFIG_UNIQUE_BIT(15) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(16)
#define _FW_CONFIG_UNIQUE_BIT_16(width) \
	_FW_CONFIG_UNIQUE_BIT(16) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(17)
#define _FW_CONFIG_UNIQUE_BIT_17(width) \
	_FW_CONFIG_UNIQUE_BIT(17) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(18)
#define _FW_CONFIG_UNIQUE_BIT_18(width) \
	_FW_CONFIG_UNIQUE_BIT(18) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(19)
#define _FW_CONFIG_UNIQUE_BIT_19(width) \
	_FW_CONFIG_UNIQUE_BIT(19) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(20)
#define _FW_CONFIG_UNIQUE_BIT_20(width) \
	_FW_CONFIG_UNIQUE_BIT(20) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(21)
#define _FW_CONFIG_UNIQUE_BIT_21(width) \
	_FW_CONFIG_UNIQUE_BIT(21) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(22)
#define _FW_CONFIG_UNIQUE_BIT_22(width) \
	_FW_CONFIG_UNIQUE_BIT(22) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(23)
#define _FW_CONFIG_UNIQUE_BIT_23(width) \
	_FW_CONFIG_UNIQUE_BIT(23) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(24)
#define _FW_CONFIG_UNIQUE_BIT_24(width) \
	_FW_CONFIG_UNIQUE_BIT(24) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(25)
#define _FW_CONFIG_UNIQUE_BIT_25(width) \
	_FW_CONFIG_UNIQUE_BIT(25) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(26)
#define _FW_CONFIG_UNIQUE_BIT_26(width) \
	_FW_CONFIG_UNIQUE_BIT(26) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(27)
#define _FW_CONFIG_UNIQUE_BIT_27(width) \
	_FW_CONFIG_UNIQUE_BIT(27) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(28)
#define _FW_CONFIG_UNIQUE_BIT_28(width) \
	_FW_CONFIG_UNIQUE_BIT(28) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(29)
#define _FW_CONFIG_UNIQUE_BIT_29(width) \
	_FW_CONFIG_UNIQUE_BIT(29) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(30)
#define _FW_CONFIG_UNIQUE_BIT_30(width) \
	_FW_CONFIG_UNIQUE_BIT(30) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(31)
#define _FW_CONFIG_UNIQUE_BIT_31(width) \
	_FW_CONFIG_UNIQUE_BIT(31) _FW_CONFIG_NEXT_UNIQUE_BIT_##width(32)
#define _FW_CONFIG_UNIQUE_BIT_33(width) \
	BUILD_ASSERT(false, "fw_config bit range overflow!")

/* Apply macros to board defined fw_config.inc */
#include "fw_config.inc"

#endif /* CONFIG_FW_CONFIG_UTILS */

#endif /* __CROS_EC_FW_CONFIG_H */
