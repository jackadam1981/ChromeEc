#ifndef __FLASH_MAP_BACKEND_H__
#define __FLASH_MAP_BACKEND_H__

/*********************
 *      INCLUDES
 *********************/

#include "flash_spic.h"
#include <stdint.h>
/*********************
 *      DEFINES
 *********************/

#define FLASH_SIZE_32MB             0x02000000ul
#define FLASH_SIZE_16MB             0x01000000ul

#define FLASH_SECTOR_EARSE_SIZE     4096
#define FLASH_PAGE_PROGRAM_SIZE     256

/* General flash opcode. */
#define FLASH_CMD_WREN		0x06            // write enable
#define FLASH_CMD_WRDI		0x04            // write disable
#define FLASH_CMD_WRSR		0x01            // write status register
#define FLASH_CMD_RDID		0x9F            // read idenfication
#define FLASH_CMD_RDSR		0x05            // read status register
#define FLASH_CMD_RDSR2		0x35            // read status register-2
#define FLASH_CMD_RDSR3		0x15            // read status register-3
#define FLASH_CMD_READ		0x03            // read data
#define FLASH_CMD_FREAD		0x0B            // fast read data
#define FLASH_CMD_RDSFDP	0x5A            // Read SFDP
#define FLASH_CMD_RES		0xAB            // Read Electronic ID
#define FLASH_CMD_REMS		0x90            // Read Electronic Manufacturer & Device ID
#define FLASH_CMD_DREAD		0x3B            // Double Output Mode command
#define FLASH_CMD_SE		0x20            // Sector Erase for 3-byte addressing
#define FLASH_CMD_SE_4B		0x21            // Sector Erase for 4-byte addressing
#define FLASH_CMD_BE		0xD8            // 0x52 //64K Block Erase
#define FLASH_CMD_CE		0xC7            // Chip Erase(or 0x60)
#define FLASH_CMD_PP		0x02            // Page Program for 3-byte addressing
#define FLASH_CMD_PP_4B		0x12            // Page Program for 4-byte addressing
#define FLASH_CMD_DP		0xB9            // Deep Power Down
#define FLASH_CMD_RDP		0xAB            // Release from Deep Power-Down
#define FLASH_CMD_2READ		0xBB            // 2 x I/O read  command
#define FLASH_CMD_4READ		0xEB            // 4 x I/O read  command
#define FLASH_CMD_QREAD		0x6B            // 1I / 4O read command
#define FLASH_CMD_4PP		0x38            // quad page program
#define FLASH_CMD_FF		0xFF            // Release Read Enhanced
#define FLASH_CMD_REMS2		0x92            // read ID for 2x I/O mode, diff with MXIC
#define FLASH_CMD_REMS4		0x94            // read ID for 4x I/O mode, diff with MXIC
#define FLASH_CMD_RDSCUR	0x48            // read security register,  diff with MXIC
#define FLASH_CMD_WRSCUR	0x42            // write security register, diff with MXIC
#define FLASH_CMD_EN_RST    0x66            // reset enable
#define FLASH_CMD_RST_DEV   0x99            // reset device

/* Support address 4 byte opcode for large size flash */
#define FLASH_CMD_EN4B		0xB7            // Enter 4-byte mode
#define FLASH_CMD_EX4B		0xE9            // Exit 4-byte mode

/* Bank addr access commands */
#define FLASH_CMD_EXTNADDR_WREAR	0xC5    // Write extended address register
#define FLASH_CMD_EXTNADDR_RDEAR	0xC8    // Read extended address register

/**********************
 *      TYPEDEFS
 **********************/

typedef enum {
    FLASH_ADDRESSING_3BYTE = 0,
    FLASH_ADDRESSING_4BYTE
} FLASH_ADDRESSING_MODE_t;

/**
 * @brief Structure describing an area on a flash device.
 *
 * Multiple flash devices may be available in the system, each of
 * which may have its own areas. For this reason, flash areas track
 * which flash device they are part of.
 */
struct flash_area {
    /**
     * This flash area's ID; unique in the system.
     */
    uint8_t fa_id;

    /**
     * ID of the flash device this area is a part of.
     */
    uint8_t fa_device_id;

    uint16_t pad16;

    /**
     * This area's offset, relative to the beginning of its flash
     * device's storage.
     */
    uint32_t fa_off;

    /**
     * This area's size, in bytes.
     */
    uint32_t fa_size;
};

static inline uint8_t flash_area_get_id(const struct flash_area *fa)
{
    return fa->fa_id;
}

static inline uint8_t flash_area_get_device_id(const struct flash_area *fa)
{
    return fa->fa_device_id;
}

static inline uint32_t flash_area_get_off(const struct flash_area *fa)
{
    return fa->fa_off;
}

static inline uint32_t flash_area_get_size(const struct flash_area *fa)
{
    return fa->fa_size;
}

/**
 * @brief Structure describing a sector within a flash area.
 *
 * Each sector has an offset relative to the start of its flash area
 * (NOT relative to the start of its flash device), and a size. A
 * flash area may contain sectors with different sizes.
 */
struct flash_sector {
    /**
     * Offset of this sector, from the start of its flash area (not device).
     */
    uint32_t fs_off;

    /**
     * Size of this sector, in bytes.
     */
    uint32_t fs_size;
};

static inline uint32_t flash_sector_get_off(const struct flash_sector *fs)
{
    return fs->fs_off;
}

static inline uint32_t flash_sector_get_size(const struct flash_sector *fs)
{
    return fs->fs_size;
}

/**********************
 *  GLOBAL PROTOTYPES
 **********************/

int32_t extr_flash_clear_flash_size(void);
int32_t extr_flash_get_flash_size(uint32_t* size);

int32_t extr_flash_init(FLASH_ADDRESSING_MODE_t* mode);
int32_t extr_flash_deinit(FLASH_ADDRESSING_MODE_t mode);

int32_t extr_flash_pin_deinit(void);
int32_t intr_flash_pin_init(void);

int32_t flash_init(uint8_t freq, uint8_t mode);
int32_t flash_free(void);
int32_t flash_erase_chip(void);
int32_t flash_erase_sector(uint32_t address, FLASH_ADDRESSING_MODE_t mode);
int32_t flash_read(uint8_t rdcmd, uint32_t address, uint8_t *data, uint32_t size, FLASH_ADDRESSING_MODE_t mode);
int32_t flash_program_page(uint32_t address, const uint8_t *data, uint32_t size, FLASH_ADDRESSING_MODE_t mode);
int32_t flash_enter_4byte_mode(void);
int32_t flash_exit_4byte_mode(void);
int32_t flash_write_status_reg(uint8_t *val, uint8_t cnt);
int32_t flash_read_status_reg1(uint8_t* val);
int32_t flash_read_status_reg2(uint8_t* val);
int32_t flash_read_status_reg3(uint8_t* val);
int32_t flash_read_rdid(uint32_t* id);
int32_t flash_cal_capacity_from_rdid(uint32_t *cap);
int32_t flash_read_sfdp(uint8_t *sfdp);
int32_t flash_get_capacity_from_sfdp(uint32_t *cap);

#endif  /* __FLASH_MAP_BACKEND_H__ */
