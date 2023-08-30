/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SPI_FLASH_H
#define SPI_FLASH_H

extern uint8_t spi_initialization;
extern uint8_t cmd_disp_en_qspi;
extern uint32_t spi_util_cmd;
extern uint32_t flash_start_addr;
extern uint32_t flash_data_len;
extern uint32_t spi_err_flag;
extern uint32_t gpio_addr[GPIO_INI_SIZE];
extern uint32_t gpio_setting[GPIO_INI_SIZE];

extern uint32_t SPI_Operations(uint32_t mem_offset_ptr);

#endif /* #ifndef SPI_FLASH_H */
