/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Touch scanning module */

#ifndef __BOARD_KEYBORG_TOUCH_SCAN_H
#define __BOARD_KEYBORG_TOUCH_SCAN_H

enum pin_type {
	PIN_ROW,
	PIN_COL,
	PIN_PD,
	PIN_Z,
};

#define ROW_MODE_IS_PD /* Remember to undef this if ROW_MODE is not PD */
#define ROW_MODE PIN_PD

#define COL_MODE PIN_PD

/* 8-bit window */
#define ADC_WINDOW_POS 2
#define ADC_DATA_WINDOW(x) ((x) >> ADC_WINDOW_POS)

/* Threshold for each cell */
#define THRESHOLD 18

/* ADC speed */
#define SMPR_VAL 0x3 /* 28.5 cycles */
#define LONG_CYCLE 57 /* 28.5 * (PCLK/ADCCLK) / 2 */
#define SHORT_CYCLE 32 /* (28.5 - 12.5) * (PCLK/ADCCLK) / 2 */

struct ts_pin {
	uint16_t port_id; /* GPIO_A = 0, GPIO_B = 1, ... */
	uint16_t mask;
};

#define TS_GPIO_A 0
#define TS_GPIO_B 1
#define TS_GPIO_C 2
#define TS_GPIO_D 3
#define TS_GPIO_E 4
#define TS_GPIO_F 5
#define TS_GPIO_G 6
#define TS_GPIO_H 7
#define TS_GPIO_I 8

extern const struct ts_pin row_pins[];
extern const struct ts_pin col_pins[];

#define ROW_COUNT 41
#define COL_COUNT 60

void touch_scan_init(void);

void touch_scan_slave_start(void);

int touch_scan_full_matrix(void);

#endif /* __BOARD_KEYBORG_TOUCH_SCAN_H */
