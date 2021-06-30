/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* HyperDebug board configuration */

#include "common.h"
#include "console.h"
#include "ec_version.h"
#include "gpio.h"
/*
 * #include "hooks.h"
 * #include "i2c.h"
 * #include "i2c_ite_flash_support.h"
 * #include "queue_policies.h"
 */
#include "registers.h"
/*
 * #include "spi.h"
 * #include "system.h"
 * #include "task.h"
 * #include "timer.h"
 * #include "update_fw.h"
 * #include "usart-stm32f0.h"
 * #include "usart_tx_dma.h"
 * #include "usart_rx_dma.h"
 */
#include "usb_hw.h"
/*
 * #include "usb_i2c.h"
 * #include "usb_spi.h"
 */
#include "usb-stream.h"
/*
 * #include "util.h"
 * #include "chip/stm32/usb-stm32f3.h"
 */
#include "gpio_list.h"

void board_config_pre_init(void)
{
}

/******************************************************************************
 * Define the strings used in our USB descriptors.
 */

const void *const usb_strings[] = {
	[USB_STR_DESC]         = usb_string_desc,
	[USB_STR_VENDOR]       = USB_STRING_DESC("Google Inc."),
	[USB_STR_PRODUCT]      = USB_STRING_DESC("HyperDebug"),
	[USB_STR_SERIALNO]     = 0,
	[USB_STR_VERSION]      = USB_STRING_DESC(CROS_EC_VERSION32),
	[USB_STR_I2C_NAME]     = USB_STRING_DESC("I2C"),
	[USB_STR_USART4_STREAM_NAME]  = USB_STRING_DESC("UART3"),
	[USB_STR_CONSOLE_NAME] = USB_STRING_DESC("Servo Shell"),
	[USB_STR_USART3_STREAM_NAME]  = USB_STRING_DESC("CPU"),
	[USB_STR_USART2_STREAM_NAME]  = USB_STRING_DESC("EC"),
	[USB_STR_UPDATE_NAME]  = USB_STRING_DESC("Firmware update"),
};

BUILD_ASSERT(ARRAY_SIZE(usb_strings) == USB_STR_COUNT);

/******************************************************************************
 * Initialize board.
 */
static void board_init(void)
{
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

int usb_i2c_board_is_enabled(void) { return 1; }
