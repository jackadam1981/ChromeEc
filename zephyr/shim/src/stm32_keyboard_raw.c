/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Functions needed by keyboard scanner module for Chrome EC */

#include "keyboard_raw.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <soc.h>

LOG_MODULE_REGISTER(shim_cros_kb_raw, LOG_LEVEL_ERR);

/**
 * Initialize the raw keyboard interface.
 */
void keyboard_raw_init(void)
{
	return;
}

/**
 * Finish initialization after task scheduling has started.
 */
void keyboard_raw_task_start(void)
{
	/* Enable interrupts for keyboard matrix inputs */
}

/**
 * Drive the specified column low.
 */
test_mockable void keyboard_raw_drive_column(int col)
{
	/* TODO: */
}

/**
 * Read raw row state.
 * Bits are 1 if signal is present, 0 if not present.
 */
test_mockable int keyboard_raw_read_rows(void)
{
	/* TODO: */
	return 0;
}
/**
 * Enable or disable keyboard interrupts.
 */
void keyboard_raw_enable_interrupt(int enable)
{
	/* TODO: */
}

// /**
//  * Enable or disable keyboard alternative function.
//  */
// void keybaord_raw_config_alt(bool enable)
// {
// 	/* TODO: */
// }
