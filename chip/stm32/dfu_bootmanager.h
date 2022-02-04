/* DFU Bootmanager for STM32s
 *
 * STM32's include a built in DFU bootloader. This module allows an
 * application to jump to the bootloader from the firmware updates.
 */


#ifndef __DFU_BOOTMANAGER_H
#define __DFU_BOOTMANAGER_H

/*
 * Reads the backup registers and performs validation. The value stored
 * within the VALUE_MASK is returned and the status code indicates
 * if the valid check passed.
 *
 * @param value[out] Value stored within the BACKUP_VALUE_MASK
 * @return           EC_SUCCESS, or non-zero if error.
 */
int dfu_bootmanager_enter_dfu(void);

/*
 * An event has occured indicating the device is operating correctly.
 * If the CONFIG_DFU_BOOTMANAGER_MAX_REBOOT configuration is set, reboots
 * act as watchdog events which require servicing.
 */
void dfu_bootmanager_clear_reboot_counter(void);

#endif /* __DFU_BOOTMANAGER_H */
