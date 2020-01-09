/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Nucleo-F767ZI development board configuration */

/* ToDo: Enable USB-UARTS
 * ToDo: Enable USB-SPI
 * ToDo: Enable USB-I2C
 * ToDo: Enable DMA (For USB and UARTS)
 * ToDo: Enable serial number
 * ToDo: Request PID at go/usb?
 * ToDo: Enable USB updating?  Maybe see sweetberry, looks like USB-FS supports
 *   DFU, but not HS core?  Or use serial DFU as a recovery option (like
 * ToDo: Look at servod control of this?
 */

#ifndef __BOARD_H
#define __BOARD_H

/* 180 MHz CPU/AHB clock frequency, 45 Mhz APB2, 45 Mhz APB1 */
#define CPU_CLOCK 180000000
#define CONFIG_FLASH_WRITE_SIZE STM32_FLASH_WRITE_SIZE_3300


/* random experiments... */
/* Enable USART1,3,4 and USB streams */



/* the UART console is on USART2 (PD5/PD6) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 3 /* 2 elee: Use USART3, which connects to stlink */
#undef CONFIG_UART_TX_DMA  /* elee: Probably save DMA for other interfaces... */
#undef CONFIG_UART_RX_DMA

/* USB Configuration */
#define CONFIG_USB
#define CONFIG_USB_PID 0x501a
#define CONFIG_USB_CONSOLE
#define CONFIG_STREAM_USB  /* elee: keep it simple to start*/
/* #define CONFIG_USB_UPDATE */
/* #define CONFIG_USB_POWER */
/* nucleo is powered from stlink USB port.  I assume this simplifies things. */
#undef CONFIG_USB_MAXPOWER_MA
/* 100 elee: 500 is  the default!!! With 100 it would hang on usb insert */
#define CONFIG_USB_MAXPOWER_MA 500


/* elee: OK, the CONFIG_USB_DWC_FS does seems necessary.
 * Without it the board hangs during bootup, in the middle of usb_softrest,
 * while printing a string...
 * [Image: RW, nucleo-f767_v2.0.3008+705ae1c3e 2020-01-10 13:03:23
 * eleenest@eleenest.mtv.corp.google.com]
 * [0.131558 Inits done]
 * [0.197325 usb_init]
 * [0.230391 usb_softreset]
 *  US
 */
#define CONFIG_USB_DWC_FS

#define CONFIG_USB_SERIALNO
#define DEFAULT_SERIALNO "Uninitialized"


/* USB interface indexes (use define rather than enum to expand them) */

/*
 * #define USB_IFACE_USART4_STREAM	0
 * #define USB_IFACE_UPDATE	1
 * #define USB_IFACE_SPI		2
 */
#define USB_IFACE_CONSOLE	0 /*3 */
/* #define USB_IFACE_I2C		4
 * #define USB_IFACE_USART3_STREAM	5
 * #define USB_IFACE_USART2_STREAM	6
 */
#define USB_IFACE_COUNT		1 /*7 */

/* USB endpoint indexes (use define rather than enum to expand them) */
#define USB_EP_CONTROL		0
/* #define USB_EP_USART4_STREAM	1
 * #define USB_EP_UPDATE		2
 * #define USB_EP_SPI		3
 */
#define USB_EP_CONSOLE		1 /*4 */
/* #define USB_EP_I2C		5
 * #define USB_EP_USART3_STREAM	6
 * #define USB_EP_USART2_STREAM	7
 */
#define USB_EP_COUNT		2 /*8 */


/* Optional features */
#undef CONFIG_LID_SWITCH
#undef CONFIG_HIBERNATE
#define CONFIG_FPU

#define CONFIG_PRINTF_LEGACY_LI_FORMAT
#define CONFIG_SHA256
#define CONFIG_SHA256_UNROLLED
#define CONFIG_STM_HWTIMER32
#define CONFIG_TASK_PROFILING
#define CONFIG_WATCHDOG_HELP

#undef CONFIG_ADC
#undef CONFIG_I2C

/* SPI configuration for the fingerprint sensor */
#define CONFIG_SPI_MASTER
/* #define CONFIG_SPI_FP_PORT    1 elee: remove to avoid cryptoc/util.h
 *   compile err */ /* SPI3: second master config */
#ifdef SECTION_IS_RW
/* #undef  CONFIG_FP_SENSOR_EFSA614 elee: remove, this is not in
 * include/config.h and the precommit script complains
 */
#undef CONFIG_FP_SENSOR_FPC1025
/* #define CONFIG_FP_SENSOR_FPC1145 elee: remove, gives compile error for
 *   cryptoc/util.h (not found)
 */
/* #define CONFIG_CMD_FPSENSOR_DEBUG */
/*
 * Use the malloc code only in the RW section (for the private library),
 * we cannot enable it in RO since it is not compatible with the RW verification
 * (shared_mem_init done too late).
 */
#define CONFIG_MALLOC

/* we are doing slow compute */
#undef CONFIG_WATCHDOG_PERIOD_MS
#define CONFIG_WATCHDOG_PERIOD_MS 10000

#else /* SECTION_IS_RO */
/* RO verifies the RW partition signature */
/* #define CONFIG_RSA
 * #define CONFIG_RSA_KEY_SIZE 3072
 * #define CONFIG_RSA_EXPONENT_3
 * #define CONFIG_RWSIG
 */
#endif
#define CONFIG_RWSIG_TYPE_RWSIG

/* #define CONFIG_CMD_SPI_XFER elee: remove spi/fpsensor for now, compile err */


/*
 * #define CONFIG_UART_TX_DMA_CH STM32_DMA1_STREAM6 // STM32_DMAS_USART2_TX
 * #define CONFIG_UART_RX_DMA_CH STM32_DMA1_STREAM5 //STM32_DMAS_USART2_RX
 * #define CONFIG_UART_TX_REQ_CH STM32_REQ_USART2_TX
 * #define CONFIG_UART_RX_REQ_CH STM32_REQ_USART2_RX
 */

/* Polyberry uses uart3 and 4 here.  Might not be needed, if is dma related? */
/* #define CONFIG_UART_TX_REQ_CH 4 */
/* #define CONFIG_UART_RX_REQ_CH 4 */
/* STM32_DMAS_USART3_TX is not defined.  Just leave DMA off for UART for now. */



#define CONFIG_CMD_FLASH

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_WATCHDOG 11

#define CONFIG_WP_ALWAYS

#include "gpio_signal.h"

void fps_event(enum gpio_signal signal);

/* USB string indexes */
enum usb_strings {
	USB_STR_DESC = 0,
	USB_STR_VENDOR,
	USB_STR_PRODUCT,
	USB_STR_SERIALNO,
	USB_STR_VERSION,
	USB_STR_I2C_NAME,
	USB_STR_USART4_STREAM_NAME,
	USB_STR_CONSOLE_NAME,
	USB_STR_USART3_STREAM_NAME,
	USB_STR_USART2_STREAM_NAME,
	USB_STR_UPDATE_NAME,


	USB_STR_COUNT
};

#endif /* !__ASSEMBLER__ */
#endif /* __BOARD_H */
