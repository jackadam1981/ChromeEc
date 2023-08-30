/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _COMMON_h_
#define _COMMON_h_

/* UART0 for HOST to EC interface */
#define HOST_IF_UART (UART0_INST)

#define GPIO_INI_SIZE 27
#define NO_ERROR 1
#define SECTOR_SIZE 4096
/* (256 * 1024 = ) */
#define MAX_CHUNK_SIZE (262144U)
/* 256KB buffer from 0xCE000 - 0x10E000 */
#define DATA_IO_BUFFER (*((volatile uint32_t *)0xCE000))
#define BOARD_INIT_ERR (1 << 7)
#define USE_GPIO_INI_PARAMS_FLAG (1 << 7)
#define CMD_ERASE (1 << 9)
#define CMD_PARTIAL_ERASE (1 << 12)

void timer_delay_1ms(uint32_t msec);
#endif
