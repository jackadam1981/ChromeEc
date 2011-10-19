/* lid.h - handle lid open/close
 *
 * (Chromium license) */

#ifndef __HOST_INTERFACE_EC_COMMAND_H
#define __HOST_INTERFACE_EC_COMMAND_H

#include <stdint.h>

/* This file is included by BIOS/OS and EC firmware. */

#pragma pack(push)
#pragma pack(1)
struct EcMailbox {
  uint8_t data[31];  /* Size is the actual mailbox size -1.
                      * Data must align to cmd if less than array size. */
  union {            /* Must be at the final byte of mailbox */
    uint8_t cmd;     /* Set by host */
    uint8_t status;  /* Returned by EC */
  };
};
#pragma pack(pop)


enum EcCommand {
  /*------------------------------------------------------------------------*/
  /* Version and boot information */
  EC_COMMAND_INFO_CMD = 0x00,
  EC_COMMAND_INFO_CMD_MASK = 0xf0,
  EC_COMMAND_INFO_GET_CHIP_ID = 0x01,
  EC_COMMAND_INFO_GET_ACTIVE_FIRMWARE = 0x02,
  EC_COMMAND_INFO_GET_FIRMWARE_VERSION = 0x03,
  EC_COMMAND_INFO_GET_RECOVERY_REASON = 0x04,
  EC_COMMAND_INFO_SET_TRY_B_COUNT = 0x05,
  EC_COMMAND_INFO_GET_TRY_B_COUNT = 0x06,
  EC_COMMAND_INFO_REQUEST_REBOOT = 0x07,
  EC_COMMAND_INFO_GET_VBOOT_INFO = 0x08,
  EC_COMMAND_INFO_RESET_ROLLBACK_INDEX = 0x09,

  /*------------------------------------------------------------------------*/
  /* keyboard (not in 8042 protocol */
  EC_COMMAND_KEYBOARD_CMD = 0x10,
  EC_COMMAND_KEYBOARD_CMD_MASK = 0xf0,
  EC_COMMAND_KEYBOARD_SET_BACKLIGHT = 0x11,
  EC_COMMAND_KEYBOARD_GET_BACKLIGHT = 0x12,
  EC_COMMAND_KEYBOARD_GET_KEY_DOWN_LIST = 0x13,
  EC_COMMAND_KEYBOARD_GET_PWB_HOLD_TIME = 0x14,

  /*------------------------------------------------------------------------*/
  /* Thermal and fan */
  EC_COMMAND_THERMAL_CMD = 0x20,
  EC_COMMAND_THERMAL_CMD_MASK = 0xf0,
  EC_COMMAND_THERMAL_GET_CURRENT_FAN_RPM = 0x21,
  EC_COMMAND_THERMAL_GET_TARGET_FAN_RPM = 0x22,
  EC_COMMAND_THERMAL_SET_TARGET_FAN_RPM = 0x23,
  EC_COMMAND_THERMAL_READ_THERMAL_SENSOR = 0x24,
  EC_COMMAND_THERMAL_SET_ALARM_RANGE = 0x25,
  /* TODO: PECI? */

  /*------------------------------------------------------------------------*/
  /* Power */
  EC_COMMAND_POWER_CMD = 0x30,
  EC_COMMAND_POWER_CMD_MASK = 0xf0,
  EC_COMMAND_POWER_SET_S3_WAKE_REASON = 0x31,
  EC_COMMAND_POWER_GET_S3_WAKE_REASON = 0x32,
  EC_COMMAND_POWER_SET_TARGET_POWER_STATE = 0x33,
  EC_COMMAND_POWER_GET_TARGET_POWER_STATE = 0x34,
  EC_COMMAND_POWER_GET_CURRENT_POWER_STATE = 0x35,

  /*------------------------------------------------------------------------*/
  /* BATTERYTERY */
  EC_COMMAND_BATTERY_CMD = 0x40,
  EC_COMMAND_BATTERY_CMD_MASK = 0xe0,  /* 0x41 ~ 0x5f */
  EC_COMMAND_BATTERY_GET_FLAGS = 0x41,
  EC_COMMAND_BATTERY_GET_REMAIN_CAP_PERCENT = 0x42,
  EC_COMMAND_BATTERY_GET_REMAIN_CAP_MAH = 0x43,
  EC_COMMAND_BATTERY_GET_CURRENT_DRAIN_RATE = 0x44,
  EC_COMMAND_BATTERY_GET_VOLTAGE = 0x45,
  EC_COMMAND_BATTERY_GET_DESIGN_CAP = 0x46,
  EC_COMMAND_BATTERY_GET_DESIGN_MIN_CAP = 0x47,
  EC_COMMAND_BATTERY_GET_CURRENT_CAP = 0x48,
  EC_COMMAND_BATTERY_GET_DESIGN_VOL = 0x49,
  EC_COMMAND_BATTERY_GET_TEMPERATURE = 0x4a,
  EC_COMMAND_BATTERY_GET_TYPE = 0x4b,
  EC_COMMAND_BATTERY_GET_OEM_INFO = 0x4c,
  EC_COMMAND_BATTERY_GET_TIME_REMAIN = 0x4d,
  EC_COMMAND_BATTERY_SET_ENABLE_CHARGE = 0x50,
  EC_COMMAND_BATTERY_SET_ENABLE_AC = 0x51,

  /*------------------------------------------------------------------------*/
  /* Lid */
  EC_COMMAND_LID_CMD = 0x60,
  EC_COMMAND_LID_CMD_MASK = 0xf0,
  EC_COMMAND_LID_GET_FLAGS = 0x41,

  /*------------------------------------------------------------------------*/
  /* Flash */
  EC_COMMAND_FLASH_CMD = 0x70,
  EC_COMMAND_FLASH_CMD_MASK = 0xf0,
  EC_COMMAND_FLASH_GET_INFO = 0x71,
  EC_COMMAND_FLAHS_READ = 0x72,
  EC_COMMAND_FLASH_WRITE = 0x73,
  EC_COMMAND_FLASH_ERASE = 0x74,
  EC_COMMAND_FLASH_SET_ENABLE_WRITE_PROTECT = 0x75,
  EC_COMMAND_FLASH_GET_ENABLE_WRITE_PROTECT = 0x76,
  EC_COMMAND_FLASH_SET_WRITE_PROTECT_RANGE = 0x77,
  EC_COMMAND_FLASH_GET_WRITE_PROTECT_RANGE = 0x78,
  EC_COMMAND_FLASH_GET_WRITE_PROTECT_GPIO = 0x79,
  EC_COMMAND_FLASH_GET_FMAP_OFFSET = 0x7a,

  /*------------------------------------------------------------------------*/
  /* Debug */
  EC_COMMAND_DEBUG_CMD = 0x80,
  EC_COMMAND_DEBUG_CMD_MASK = 0xf0,
  EC_COMMAND_DEBUG_GET_EC_BOOT_REASON = 0x81,
  EC_COMMAND_DEBUG_GET_LAST_CRASH_INFO = 0x82,
  EC_COMMAND_DEBUG_GET_GPIO_VALUE = 0x83,

  /*------------------------------------------------------------------------*/
  /* 0xe0~0xff are reserved for return value */
};


enum EcStatus {
  /*
   *  +----+----+----+----+----+----+----+----+
   *  |  1 |  1 |  1 |done|    error code     |
   *  +----+----+----+----+----+----+----+----+
   *
   * Note that 0xE0~0xEF mean in busy (not done).
   */
  RET_MASK = 0xf0,
  RET_BUSY = 0xe0,
  RET_DONE = 0xf0,
  RET_SUCCESS = 0xf0,
  RET_CMD_NOT_SUPPORTED = 0xfc,
  RET_TIMEOUT = 0xfd,  /* EC firmware detects a timeout in its peripheral. */
  RET_GENERIC_ERROR = 0xfe,
  RET_UNDEFINED = 0xff,  /* reserved for future */
};


/* EC access sample code for host side.
 *
 *  int get_ec_firmware_version(uint8_t index, uint8_t *major, uint8_t *minor) {
 *    volatile struct EcMailbox *mailbox;
 *
 *    mailbox->data[30] = index;
 *    mailbox->cmd = INFO_GET_FIRMWARE_VERSION; // write the last byte to
 *                                              // triggers EC handler.
 *
 *    // The 'cmd' byte could remain INFO_GET_FIRMWARE_VERSION if the EC
 *    // is not fast enough to take away.
 *    // Then, EC can modify status byte to 0xE0 to indicate he has received
 *    // the command and is busy on hanlding it.
 *    while ((mailbox->status & RET_MASK) != RET_DONE) {
 *      // host side kicks off timer, in case that EC crashes and not respond.
 *    }
 *
 *    // Lookup result
 *    if (mailbox->status == RET_SUCCESS) {
 *      *major = mailbox->data[29];
 *      *minor = mailbox->data[30];
 *      return OK;
 *    } else {
 *      error_message("EC command failed: 0x%02x\n", mailbox->status);
 *      return FAILED;
 *    }
 *  }
 *
 */


#endif  /* __HOST_INTERFACE_EC_COMMAND_H */
