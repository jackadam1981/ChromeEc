/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * The easiest method to enter DFU is to have the system in a known
 * configuration prior which is done with a reboot event and the persistent
 * registers are used to communicate boot state early in the startup sequence.
 * Further differences jumping between the DFU bootloader to the main
 * application create problems, notably flash access so a sequence involving
 * 2 system reboots is used.
 *
 * 1) While running, a DFU host application identifies the device and issues
 *      the DFU_DETACH command over USB
 * 2) The device updates the persistent registers to enter DFU next boot and
 *      issues a sys_reboot().
 * 3) Early in the startup sequence, the device reads the persistent registers
 *      and identifies it as a DFU boot. The persistent registers are updated
 *      again to force another reboot after resuming.
 * 4) Clears any startup configuration and sets up the DFU mode configuration.
 *      This sequence is going to be chip dependent before jumping to the
 *      DFU region's address and entering the bootloader.
 * 5) The host system programs the image and issues the Leave DFU command.
 * 6) Early in the startup sequence, the device reads the persistent registers
 *      and identifies it as requiring a reboot to clear configuration. It
 *      clears the persistent registers and issues the reboot.
 * 7) Application boots normally.
 */

#include "dfu.h"

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>

/* Base address of the STM32 bootloader. */
#define STM32_DFU_BASE 0x1fff0000

/* Magic value to enter DFU mode */
#define MAGIC_ENTER_DFU 0xAAAA
/* Magic value to reboot after boot */
#define MAGIC_NEEDS_REBOOT 0x5555

/* Lacking Zephyr header */
void arm_core_mpu_disable(void);

/*
 * Performs the sequence of operations to enter the bootloader.
 *
 * STM32's document AN3156 describes the sequence of operations required to
 * enter DFU mode. In addition to operations here we need to undo several
 * initialization which happen during Zephyr's boot.
 *
 * There are chip specific behaviors which can be found in other reference
 * manuals. Booting from the bootloader will also change system state so
 * testing should validate it works even when the built-in bootloader entrance
 * paths are used.
 */
static void enter_bootloader(void)
{
	void (*addr)(void);

	addr = (void (*)(void))(*((uint32_t *)(STM32_DFU_BASE + 4)));

	/* Zephyr boot changes */
	__set_BASEPRI(0);

	arm_core_mpu_disable();

	/* Call the Hal DeInit to reset the peripherals */
	HAL_RCC_DeInit();

	/* Clear the systick */
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL = 0;

	/* Remap memory */
	__HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();

	__set_MSP(*(uint32_t *)STM32_DFU_BASE);

	/* Jump to the bootloader */
	addr();
}

/*
 * Writes the backup register storing boot flags
 */
static void write_boot_backup(uint32_t value)
{
	SET_BIT(PWR->CR1, PWR_CR1_DBP);
	volatile uint32_t *reg = &(TAMP->BKP0R);
	*reg = value;
}

/*
 * Reads the backup register storing boot flags
 */
static uint32_t read_boot_backup(void)
{
	SET_BIT(PWR->CR1, PWR_CR1_DBP);
	uint32_t reg = TAMP->BKP0R;
	return reg;
}

FUNC_NORETURN void dfu_enter(void)
{
	write_boot_backup(MAGIC_ENTER_DFU);
	sys_reboot(0);
}

/*
 * Check if we need to reboot into DFU.
 *
 * We need the check to happen early in the boot sequence before any mode
 * changes which can interfere with the jump to DFU.
 */
static int dfu_boot_check(const struct device *device)
{
	UNUSED(device);
	uint32_t value = read_boot_backup();

	if (value == MAGIC_ENTER_DFU) {
		write_boot_backup(MAGIC_NEEDS_REBOOT);
		enter_bootloader();
	} else if (value == MAGIC_NEEDS_REBOOT) {
		write_boot_backup(0);
		sys_reboot(0);
	}
	return 0;
}

SYS_INIT(dfu_boot_check, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);
