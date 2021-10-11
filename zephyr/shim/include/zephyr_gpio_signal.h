/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#if !defined(__CROS_EC_GPIO_SIGNAL_H) || defined(__CROS_EC_ZEPHYR_GPIO_SIGNAL_H)
#error "This file must only be included from gpio_signal.h. Include gpio_signal.h directly."
#endif
#define __CROS_EC_ZEPHYR_GPIO_SIGNAL_H

#include <devicetree.h>
#include <toolchain.h>

#define GPIO_SIGNAL(id) DT_STRING_UPPER_TOKEN(id, enum_name)
#define GPIO_SIGNAL_WITH_COMMA(id) \
	COND_CODE_1(DT_NODE_HAS_PROP(id, enum_name), (GPIO_SIGNAL(id), ), ())
enum gpio_signal {
	GPIO_UNIMPLEMENTED = -1,
#if DT_NODE_EXISTS(DT_PATH(named_gpios))
	DT_FOREACH_CHILD(DT_PATH(named_gpios), GPIO_SIGNAL_WITH_COMMA)
#endif
	GPIO_COUNT,
	GPIO_LIMIT = 0x0FFF,
};
#undef GPIO_SIGNAL_WITH_COMMA
BUILD_ASSERT(GPIO_COUNT < GPIO_LIMIT);

/** @brief Converts a node identifier under named gpios to enum
 *
 * Converts the specified node identifier name, which should be nested under
 * the named_gpios node, into the correct enum gpio_signal that can be used
 * with platform/ec gpio API
 */
#define NAMED_GPIO(name) GPIO_SIGNAL(DT_PATH(named_gpios, name))

/** @brief Obtain a named gpio enum from a label and property
 *
 * Obtains a valid enum gpio_signal that can be used with platform/ec gpio API
 * from the property of a labeled node. The property has to point to a
 * named_gpios node.
 */
#define NAMED_GPIO_NODELABEL(label, prop) \
	GPIO_SIGNAL(DT_PHANDLE(DT_NODELABEL(label), prop))

enum ioex_port {
	IOEX_C0_NCT38XX = 0,
	IOEX_C2_NCT38XX,
	IOEX_ID_1_C0_NCT38XX,
	IOEX_ID_1_C2_NCT38XX,
	IOEX_PORT_COUNT
};
/*
 * While we don't support IO expanders at the moment, multiple
 * platform/ec headers (e.g., espi.h) require some of these constants
 * to be defined.  Define them as a compatibility measure.
 */
enum ioex_signal {
	IOEX_SIGNAL_START = GPIO_LIMIT + 1,
	IOEX_ID_1_USB_C0_RT_RST_ODL,
	IOEX_ID_1_USB_C0_FRS_EN,
	IOEX_ID_1_USB_C0_OC_ODL,
	IOEX_ID_1_USB_C2_RT_RST_ODL,
	IOEX_ID_1_USB_C2_FRS_EN,
	IOEX_ID_1_USB_C1_OC_ODL,
	IOEX_ID_1_USB_C2_OC_ODL,
	IOEX_USB_C0_OC_ODL,
	IOEX_USB_C0_FRS_EN,
	IOEX_USB_C0_RT_RST_ODL,
	IOEX_USB_C2_RT_RST_ODL,
	IOEX_USB_C1_OC_ODL,
	IOEX_USB_C2_OC_ODL,
	IOEX_USB_C2_FRS_EN,
	IOEX_SIGNAL_END,
	IOEX_LIMIT = 0x1FFF,
};
BUILD_ASSERT(IOEX_SIGNAL_END < IOEX_LIMIT);

#define IOEX_COUNT (IOEX_SIGNAL_END - IOEX_SIGNAL_START)

#include "ioexpander.h"
#define IOEX_EXPIN(ioex, port, index) (ioex), (port), BIT(index)

/*
 *  Define the IO expander IO in gpio.inc by the format:
 *    IOEX(name, EXPIN(ioex_port, port, offset), flags)
 *      - name: the name of this IO pin
 *      - EXPIN(ioex, port, offset)
 *         - ioex: the IO expander port (defined in board.c) this IO
 *                 pin belongs to.
 *         - port: the port number in the IO expander chip.
 *         - offset: the bit offset in the port above.
 *      - flags: the same as the flags of GPIO.
 *
 */
#define IOEX(name, expin, flags) {#name, IOEX_##expin, flags},
/*
 *  Define the IO expander IO which supports interrupt in gpio.inc by
 *  the format:
 *    IOEX_INT(name, EXPIN(ioex_port, port, offset), flags, handler)
 *      - name: the name of this IO pin
 *      - EXPIN(ioex, port, offset)
 *         - ioex: the IO expander port (defined in board.c) this IO
 *                 pin belongs to.
 *         - port: the port number in the IO expander chip.
 *         - offset: the bit offset in the port above.
 *      - flags: the same as the flags of GPIO.
 *      - handler: the IOEX IO's interrupt handler.
 */
#define IOEX_INT(name, expin, flags, handler) {#name, IOEX_##expin, flags},

#define EXPIN(a, b, c...) \
	static const int _expin_ ## a ## _ ## b ## _ ## c \
	__attribute__((unused, section(".unused"))) = __LINE__; \
	BUILD_ASSERT(a < CONFIG_IO_EXPANDER_PORT_COUNT);
#if 0
#define GPIO_OPEN_DRAIN2    (BIT(1) | BIT(2))  /* Output type is open-drain */
//#define GPIO_PULL_UP       BIT(4)  /* Enable on-chip pullup */
//#define GPIO_PULL_DOWN     BIT(5)  /* Enable on-chip pulldown */
//#define GPIO_INPUT         BIT(8)  /* Input */
#define GPIO_OUTPUT2        BIT(9)  /* Output */
#define GPIO_LOW           BIT(6)  /* If GPIO_OUTPUT, set level low */
#define GPIO_HIGH          BIT(7)  /* If GPIO_OUTPUT, set level high */
#define GPIO_ODR_LOW        (GPIO_OUTPUT2 | GPIO_OPEN_DRAIN2 | GPIO_LOW)
#define GPIO_ODR_HIGH       (GPIO_OUTPUT2 | GPIO_OPEN_DRAIN2 | GPIO_HIGH)
/* IO expander signal list. */
const struct ioex_info ioex_list[] = {
/* Board ID 1 IO expander configuration */

IOEX(ID_1_USB_C0_RT_RST_ODL, EXPIN(IOEX_ID_1_C0_NCT38XX, 0, 2), GPIO_ODR_LOW)
/* GPIO03_P1 to PU */
IOEX(ID_1_USB_C0_FRS_EN,     EXPIN(IOEX_ID_1_C0_NCT38XX, 0, 4), GPIO_LOW)
IOEX(ID_1_USB_C0_OC_ODL,     EXPIN(IOEX_ID_1_C0_NCT38XX, 0, 6), GPIO_ODR_HIGH)
/* GPIO07_P1 to PU */

IOEX(ID_1_USB_C2_RT_RST_ODL, EXPIN(IOEX_ID_1_C2_NCT38XX, 0, 2), GPIO_ODR_LOW)
/* GPIO03_P2 to PU */
IOEX(ID_1_USB_C2_FRS_EN,     EXPIN(IOEX_ID_1_C2_NCT38XX, 0, 4), GPIO_LOW)
IOEX(ID_1_USB_C1_OC_ODL,     EXPIN(IOEX_ID_1_C2_NCT38XX, 0, 6), GPIO_ODR_HIGH)
IOEX(ID_1_USB_C2_OC_ODL,     EXPIN(IOEX_ID_1_C2_NCT38XX, 0, 7), GPIO_ODR_HIGH)

/* Board ID 2 IO expander configuration */

/* GPIO02_P2 to PU */
/* GPIO03_P2 to PU */
IOEX(USB_C0_OC_ODL,          EXPIN(IOEX_C0_NCT38XX, 0, 4), GPIO_ODR_HIGH)
IOEX(USB_C0_FRS_EN,          EXPIN(IOEX_C0_NCT38XX, 0, 6), GPIO_LOW)
IOEX(USB_C0_RT_RST_ODL,      EXPIN(IOEX_C0_NCT38XX, 0, 7), GPIO_ODR_LOW)

IOEX(USB_C2_RT_RST_ODL,      EXPIN(IOEX_C2_NCT38XX, 0, 2), GPIO_ODR_LOW)
IOEX(USB_C1_OC_ODL,          EXPIN(IOEX_C2_NCT38XX, 0, 3), GPIO_ODR_HIGH)
IOEX(USB_C2_OC_ODL,          EXPIN(IOEX_C2_NCT38XX, 0, 4), GPIO_ODR_HIGH)
IOEX(USB_C2_FRS_EN,          EXPIN(IOEX_C2_NCT38XX, 0, 6), GPIO_LOW)
/* GPIO07_P2 to PU */
};

#endif
