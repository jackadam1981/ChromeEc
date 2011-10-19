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
  INFO_CMD = 0x00,
  INFO_CMD_MASK = 0xf0,
  INFO_GET_CHIP_ID = 0x01,
  INFO_GET_ACTIVE_FIRMWARE = 0x02,
  INFO_GET_FIRMWARE_VERSION = 0x03,
  INFO_GET_RECOVERY_REASON = 0x04,
  INFO_SET_TRY_B_COUNT = 0x05,
  INFO_GET_TRY_B_COUNT = 0x06,
  INFO_REQUEST_REBOOT = 0x07,
  INFO_GET_VBOOT_INFO = 0x08,
  INFO_RESET_ROLLBACK_INDEX = 0x09,

  /*------------------------------------------------------------------------*/
  /* keyboard (not in 8042 protocol */
  KB_CMD = 0x10,
  KB_CMD_MASK = 0xf0,
  KB_SET_BACKLIGHT = 0x11,
  KB_GET_BACKLIGHT = 0x12,
  KB_GET_KEY_DOWN_LIST = 0x13,
  KB_GET_PWB_HOLD_TIME = 0x14,

  /*------------------------------------------------------------------------*/
  /* Thermal and fan */
  THM_CMD = 0x20,
  THM_CMD_MASK = 0xf0,
  THM_GET_CURRENT_FAN_RPM = 0x21,
  THM_GET_TARGET_FAN_RPM = 0x22,
  THM_SET_TARGET_FAN_RPM = 0x23,
  THM_READ_THM_SENSOR = 0x24,
  THM_SET_ALARM_RANGE = 0x25,
  /* TODO: PECI? */

  /*------------------------------------------------------------------------*/
  /* Power */
  PWR_CMD = 0x30,
  PWR_CMD_MASK = 0xf0,
  PWR_SET_S3_WAKE_REASON = 0x31,
  PWR_GET_S3_WAKE_REASON = 0x32,
  PWR_SET_TARGET_POWER_STATE = 0x33,
  PWR_GET_TARGET_POWER_STATE = 0x34,
  PWR_GET_CURRENT_POWER_STATE = 0x35,

  /*------------------------------------------------------------------------*/
  /* BATTERY */
  BAT_CMD = 0x40,
  BAT_CMD_MASK = 0xe0,  /* 0x41 ~ 0x5f */
  BAT_GET_FLAGS = 0x41,
  BAT_GET_REMAIN_CAP_PERCENT = 0x42,
  BAT_GET_REMAIN_CAP_MAH = 0x43,
  BAT_GET_CURRENT_DRAIN_RATE = 0x44,
  BAT_GET_VOLTAGE = 0x45,
  BAT_GET_DESIGN_CAP = 0x46,
  BAT_GET_DESIGN_MIN_CAP = 0x47,
  BAT_GET_CURRENT_CAP = 0x48,
  BAT_GET_DESIGN_VOL = 0x49,
  BAT_GET_TEMPERATURE = 0x4a,
  BAT_GET_TYPE = 0x4b,
  BAT_GET_OEM_INFO = 0x4c,
  BAT_GET_TIME_REMAIN = 0x4d,
  BAT_SET_ENABLE_CHARGE = 0x50,
  BAT_SET_ENABLE_AC = 0x51,

  /*------------------------------------------------------------------------*/
  /* Lid */
  LID_CMD = 0x60,
  LID_CMD_MASK = 0xf0,
  LID_GET_FLAGS = 0x41,

  /*------------------------------------------------------------------------*/
  /* Flash */
  FLASH_CMD = 0x70,
  FLASH_CMD_MASK = 0xf0,
  FLASH_GET_INFO = 0x71,
  FLAHS_READ = 0x72,
  FLASH_WRITE = 0x73,
  FLASH_ERASE = 0x74,
  FLASH_SET_ENABLE_WRITE_PROTECT = 0x75,
  FLASH_GET_ENABLE_WRITE_PROTECT = 0x76,
  FLASH_SET_WRITE_PROTECT_RANGE = 0x77,
  FLASH_GET_WRITE_PROTECT_RANGE = 0x78,
  FLASH_GET_WRITE_PROTECT_GPIO = 0x79,
  FLASH_GET_FMAP_OFFSET = 0x7a,

  /*------------------------------------------------------------------------*/
  /* Debug */
  DEBUG_CMD = 0x80,
  DEBUG_CMD_MASK = 0xf0,
  DEBUG_GET_EC_BOOT_REASON = 0x81,
  DEBUG_GET_LAST_CRASH_INFO = 0x82,
  DEBUG_GET_GPIO_VALUE = 0x83,

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
