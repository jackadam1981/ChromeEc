/*********************
 *      INCLUDES
 *********************/

#include "flash_map_backend.h"
#include "reg.h"
#include <math.h>
#include <stdint.h>
/*********************
 *      DEFINES
 *********************/

/* Status Register bits. */
#define SR_WIP			(0x01)          // Write in progress
#define SR_WEL			(0x02)          // Write enable latch

/* External Flash GPIO pins */
enum {
    EXTR_SPI_CS_PIN     = 107,
    EXTR_SPI_MOSI_PIN   = 108,
    EXTR_SPI_MISO_PIN   = 109,
    EXTR_SPI_CLK_PIN    = 111,
    EXTR_SPI_IO3_PIN    = 122,
    EXTR_SPI_IO2_PIN    = 124,
};

/**********************
 *      TYPEDEFS
 **********************/

union SFDP_Database {
    uint8_t bytes[256];
    struct __attribute__((packed)){
        uint32_t SFDPSignature;
        uint8_t  SFDPMinorRevision;
        uint8_t  SFDPMajorRevision;
        uint8_t  NumOfParamHeader;
        uint8_t  Unused1;
        uint8_t  JEDECID;
        uint8_t  ParamMinorRevision;
        uint8_t  ParamMajorRevision;
        uint8_t  ParamLength;          // unit: dword (32-bits)
        uint8_t  PTP0;                 // Parameter Table Pointer (lsb)
        uint8_t  PTP1;
        uint8_t  PTP2;                 // Parameter Table Pointer (msb)
        uint8_t  Unused2;
    } header;
};

union JEDEC_Flash_Param {
    uint32_t dwords[9];
    struct {
        struct __attribute__((packed)) {
            uint32_t block_sector_erase_size: 2;
            uint32_t write_granularity: 1;
            uint32_t write_enable_instruction_required_volatile_write_status: 1;
            uint32_t write_enable_opcode_volatile_write_status: 1;
            uint32_t unused1: 3;
            uint32_t _4k_erase_opcode: 8;
            uint32_t fastread_1_1_2_supported: 1;
            uint32_t address_byte: 2;
            uint32_t dtr_clocking_supported: 1; // Double Transfer Rate Clocking
            uint32_t fastread_1_2_2_supported: 1;
            uint32_t fastread_1_4_4_supported: 1;
            uint32_t fastread_1_1_4_supported: 1;
            uint32_t unused2: 9;
        } dword_0;
        struct {
            uint32_t flash_memory_density;
        } dword_1;
        struct __attribute__((packed)) {
            uint32_t fastread_1_4_4_num_of_dummy_clock : 5;
            uint32_t fastread_1_4_4_num_of_mode_bits : 3;
            uint32_t fastread_1_4_4_opcode : 8;
            uint32_t fastread_1_1_4_num_of_dummy_clock : 5;
            uint32_t fastread_1_1_4_num_of_mode_bits : 3;
            uint32_t fastread_1_1_4_opcode : 8;
        } dword_2;
        struct __attribute__((packed)) {
            uint32_t fastread_1_1_2_num_of_dummy_clock : 5;
            uint32_t fastread_1_1_2_num_of_mode_bits : 3;
            uint32_t fastread_1_1_2_opcode : 8;
            uint32_t fastread_1_2_2_num_of_dummy_clock : 5;
            uint32_t fastread_1_2_2_num_of_mode_bits : 3;
            uint32_t fastread_1_2_2_opcode : 8;
        } dword_3;
        struct __attribute__((packed)) {
            uint32_t fastread_2_2_2_supported : 1;
            uint32_t reserved1 : 3;
            uint32_t fastread_4_4_4_supported : 1;
            uint32_t reserved2 : 27;
        } dword_4;
        struct __attribute__((packed)) {
            uint32_t reserved1 : 16;
            uint32_t fastread_2_2_2_num_of_dummy_clock : 5;
            uint32_t fastread_2_2_2_num_of_mode_bits : 3;
            uint32_t fastread_2_2_2_opcode : 8;
        } dword_5;
        struct __attribute__((packed)) {
            uint32_t reserved1 : 16;
            uint32_t fastread_4_4_4_num_of_dummy_clock : 5;
            uint32_t fastread_4_4_4_num_of_mode_bits : 3;
            uint32_t fastread_4_4_4_opcode : 8;
        } dword_6;
        struct __attribute__((packed)) {
            uint32_t sector_type_1_size : 8;
            uint32_t sector_type_1_opcode : 8;
            uint32_t sector_type_2_size : 8;
            uint32_t sector_type_2_opcode : 8;
        } dword_7;
        struct __attribute__((packed)) {
            uint32_t sector_type_3_size : 8;
            uint32_t sector_type_3_opcode : 8;
            uint32_t sector_type_4_size : 8;
            uint32_t sector_type_4_opcode : 8;
        } dword_8;
    } __attribute__((packed));
} __attribute__((packed));

/**********************
 *  EXTERN PROTOTYPES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int8_t config_command(SPIC_COMMAND_t *command, uint8_t cmd, uint32_t addr,
                        SPIC_ADDRESS_SIZE_t addr_size, uint8_t dummy_count);
static int32_t flash_write_enable(void);
static int32_t flash_write_disable(void);
static int32_t flash_read_sr(void);
static int32_t flash_erase(uint8_t cmd, uint32_t address);
static int32_t flash_wait_till_ready(void);

/**********************
 *  STATIC VARIABLES
 **********************/
static SPIC_COMMAND_t command_default = {
    .instruction = {
        .bus_width = SPIC_CFG_BUS_SINGLE,
        .disabled = 0,
    },
    .address = {
        .bus_width = SPIC_CFG_BUS_SINGLE,
        .size = SPIC_CFG_ADDR_SIZE_24,
        .disabled = 0,
    },
    .alt = {
        .size = 0,
        .disabled = 1,
    },
    .dummy_count = 0,
    .data = {
        .bus_width = SPIC_CFG_BUS_SINGLE,
    },
};

static uint32_t extr_flash_size = 0ul;

/**********************
 *      MACROS
 **********************/
/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int32_t extr_flash_clear_flash_size(void)
{
    extr_flash_size = 0ul;

    return 0;
}

int32_t extr_flash_get_flash_size(uint32_t* size)
{
    if (extr_flash_size == 0) {
        return -1;
    }

    *size = extr_flash_size;
    return 0;
}

int32_t extr_flash_init(FLASH_ADDRESSING_MODE_t* mode)
{

    if (extr_flash_size == 0ul) {
        flash_exit_4byte_mode();

        if (flash_get_capacity_from_sfdp(&extr_flash_size) < 0) {
            if (flash_cal_capacity_from_rdid(&extr_flash_size) < 0) {
                extr_flash_size = FLASH_SIZE_16MB;
            }
        }

    }

    if (extr_flash_size >= FLASH_SIZE_32MB) {
        flash_enter_4byte_mode();
        *mode = FLASH_ADDRESSING_4BYTE;
    } else {
        *mode = FLASH_ADDRESSING_3BYTE;
    }

    return 0;
}

int32_t intr_flash_pin_init(void)
{
    IOPAD->FLASHWP = IOPAD_FLASHWP_INDETEN_Msk;
    IOPAD->FLASHHOLD = IOPAD_FLASHHOLD_INDETEN_Msk;
    IOPAD->FLASHSI = IOPAD_FLASHSI_INDETEN_Msk;
    IOPAD->FLASHSO = IOPAD_FLASHSO_INDETEN_Msk;
    IOPAD->FLASHCS = IOPAD_FLASHCS_INDETEN_Msk;
    IOPAD->FLASHCLK = IOPAD_FLASHCLK_INDETEN_Msk;

    GPIO->GCR[EXTR_SPI_CS_PIN] &= ~GPIO_GCR_MFCTRL_Msk;
    GPIO->GCR[EXTR_SPI_MOSI_PIN] &= ~GPIO_GCR_MFCTRL_Msk;
    GPIO->GCR[EXTR_SPI_MISO_PIN] &= ~GPIO_GCR_MFCTRL_Msk;
    GPIO->GCR[EXTR_SPI_CLK_PIN] &= ~GPIO_GCR_MFCTRL_Msk;
    GPIO->GCR[EXTR_SPI_IO3_PIN] &= ~GPIO_GCR_MFCTRL_Msk;
    GPIO->GCR[EXTR_SPI_IO2_PIN] &= ~GPIO_GCR_MFCTRL_Msk;

    return 0;
}

int32_t flash_init(uint8_t freq, uint8_t mode)
{
    return spic_init(freq, mode);
}

int32_t flash_free(void)
{
    spic_free();

    return 0;
}

int32_t flash_erase_chip(void)
{
    return flash_erase(FLASH_CMD_CE, 0);
}

int32_t flash_erase_sector(uint32_t address, FLASH_ADDRESSING_MODE_t mode)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_ADDRESS_SIZE_t addr_size = SPIC_CFG_ADDR_SIZE_24;
    SPIC_STATUS_t status;

    uint8_t erase_cmd = FLASH_CMD_SE;

    int32_t rc = 0;
    uint32_t len = 0;

    if (mode == FLASH_ADDRESSING_4BYTE) {
        addr_size = SPIC_CFG_ADDR_SIZE_32;
        erase_cmd = FLASH_CMD_SE_4B;
    }

    flash_write_enable();

    config_command(command, erase_cmd, address, addr_size, 0);
    status = spic_write(command, NULL, &len);
    if (status != SPIC_STATUS_OK) {
        rc = -1;
        goto err_exit;
    }

    flash_wait_till_ready();

err_exit:
    flash_write_disable();
    return rc;
}

int32_t flash_read(uint8_t rdcmd, uint32_t address, uint8_t *data, uint32_t size, FLASH_ADDRESSING_MODE_t mode)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_ADDRESS_SIZE_t addr_size = SPIC_CFG_ADDR_SIZE_24;
    SPIC_STATUS_t status;

    uint32_t src_addr = address;
    uint8_t* dst_idx = data;

    uint32_t remind_size = size;
    uint32_t block_size = 0x8000ul;

    if (mode == FLASH_ADDRESSING_4BYTE) {
        addr_size = SPIC_CFG_ADDR_SIZE_32;
    }

    while(remind_size > 0) {
        switch(rdcmd) {
        default:
        case 0x03:
            config_command(command, FLASH_CMD_READ, src_addr, addr_size, 0);
            break;
        case 0x0B:
            config_command(command, FLASH_CMD_FREAD, src_addr, addr_size, 8);
            break;
        case 0x3B:
            config_command(command, FLASH_CMD_DREAD, src_addr, addr_size, 8);
            break;
        case 0x6B:
            config_command(command, FLASH_CMD_QREAD, src_addr, addr_size, 8);
            break;
        }

        if (remind_size >= block_size) {
            status = spic_read(command, dst_idx, (uint32_t *)&block_size);
            src_addr += block_size;
            remind_size -= block_size;
        } else {
            status = spic_read(command, dst_idx, (uint32_t *)&remind_size);
            remind_size = 0;
        }

        if (status != SPIC_STATUS_OK) {
            return -1;
        }

        dst_idx += block_size;
    }

    return 0;
}

int32_t flash_program_page(uint32_t address, const uint8_t *data, uint32_t size, FLASH_ADDRESSING_MODE_t mode)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_ADDRESS_SIZE_t addr_size = SPIC_CFG_ADDR_SIZE_24;
    SPIC_STATUS_t status;

    uint8_t wr_cmd = FLASH_CMD_PP;

    uint32_t offset = 0, chunk = 0, page_size = FLASH_PAGE_PROGRAM_SIZE;
    int32_t rc = 0;

    if (mode == FLASH_ADDRESSING_4BYTE) {
        addr_size = SPIC_CFG_ADDR_SIZE_32;
        wr_cmd = FLASH_CMD_PP_4B;
    }

    while (size > 0) {
        offset = address % page_size;
        chunk = (offset + size < page_size) ? size : (page_size - offset);

        flash_write_enable();

        config_command(command, wr_cmd, address, addr_size, 0);
        status = spic_write(command, data, (uint32_t *)&chunk);
        if (status != SPIC_STATUS_OK) {
            rc = -1;
            goto err_exit;
        }

        data += chunk;
        address += chunk;
        size -= chunk;

        flash_wait_till_ready();
    }

err_exit:
    flash_write_disable();
    return rc;
}

int32_t flash_enter_4byte_mode(void)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_STATUS_t status;

    uint32_t size = 0;

    config_command(command, FLASH_CMD_EN4B, 0, 0, 0);
    status = spic_write(command, NULL, (uint32_t *)&size);

    if (status != SPIC_STATUS_OK) {
        return -1;
    }

    return 0;
}

int32_t flash_exit_4byte_mode(void)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_STATUS_t status;

    uint32_t size = 0;

    config_command(command, FLASH_CMD_EX4B, 0, 0, 0);
    status = spic_write(command, NULL, (uint32_t *)&size);

    if (status != SPIC_STATUS_OK) {
        return -1;
    }

    return 0;
}

int32_t flash_write_status_reg(uint8_t *val, uint8_t cnt)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_STATUS_t status;

    uint32_t len = cnt;

    flash_write_enable();

    config_command(command, FLASH_CMD_WRSR, 0, 0, 0);
    status = spic_write(command, val, &len);
    if (status != SPIC_STATUS_OK) {
        return (int32_t)status;
    }

    flash_wait_till_ready();
    flash_write_disable();

    return 0;
}

int32_t flash_read_status_reg1(uint8_t* val)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_STATUS_t status;

    uint32_t len = 1;
    uint8_t sr;

    config_command(command, FLASH_CMD_RDSR, 0, 0, 0);
    status = spic_read(command, &sr, &len);
    if (status != SPIC_STATUS_OK) {
        //printf("error reading SR: %d\n", status);
        return -1;
    }

    *val = sr;

    return 0;
}

int32_t flash_read_status_reg2(uint8_t* val)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_STATUS_t status;

    uint32_t len = 1;
    uint8_t sr;

    config_command(command, FLASH_CMD_RDSR2, 0, 0, 0);
    status = spic_read(command, &sr, &len);
    if (status != SPIC_STATUS_OK) {
        //printf("error reading SR: %d\n", status);
        return -1;
    }

    *val = sr;

    return 0;
}

int32_t flash_read_status_reg3(uint8_t* val)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_STATUS_t status;

    uint32_t len = 1;
    uint8_t sr;

    config_command(command, FLASH_CMD_RDSR3, 0, 0, 0);
    status = spic_read(command, &sr, &len);
    if (status != SPIC_STATUS_OK) {
        //printf("error reading SR: %d\n", status);
        return -1;
    }

    *val = sr;

    return 0;
}

int32_t flash_read_rdid(uint32_t* id)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_STATUS_t status;

    uint8_t tmp[3];
    uint32_t size = 3;

    config_command(command, FLASH_CMD_RDID, 0, 0, 0);
    status = spic_read(command, tmp, (uint32_t *)&size);

    if (status != SPIC_STATUS_OK) {
        return -1;
    }

    *id = (uint32_t)(((tmp[0] << 16) | (tmp[1] << 8) | tmp[2]) & 0x00FFFFFFul);

    return 0;
}

int32_t flash_cal_capacity_from_rdid(uint32_t *cap)
{
    uint32_t rdid[2];
    uint8_t retry = 0;

    /* Read RDID twice and comapre, retry at least 3 times if
       not equeal. */
    do {
        flash_read_rdid(&rdid[0]);
        flash_read_rdid(&rdid[1]);

        if (rdid[0] == rdid[1]) {
            if (rdid[0] != 0ul) {
                if (rdid[0] != 0x00fffffful) {
                    break;
                }
            }
        }
    } while(retry++ < 3);

    if (retry > 3) {
        return -1;
    }

    rdid[0] = (rdid[0] & 0x000000FFul);
    if ((rdid[0] & 0xe0ul) != 0) {
        return -1;
    }

    *cap = (uint32_t)(0x01ul << rdid[0]);
    return 0;
}

int32_t flash_read_sfdp(uint8_t *sfdp)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_STATUS_t status;

    uint8_t* dst_idx = sfdp;
    uint32_t len = 256;

    config_command(command, FLASH_CMD_RDSFDP, 0, SPIC_CFG_ADDR_SIZE_24, 8);
    status = spic_read(command, dst_idx, (uint32_t *)&len);

    if (status != SPIC_STATUS_OK) {
        return -1;
    }

    if ((sfdp[0] != 'S') || (sfdp[1] != 'F') || (sfdp[2] != 'D') || (sfdp[3] != 'P')) {
        return -1;
    }

    return 0;
}

int32_t flash_get_capacity_from_sfdp(uint32_t *cap)
{
    int32_t ret = 0;

    union SFDP_Database sfdp;
    union JEDEC_Flash_Param* parm;

    uint32_t offset;

    ret = flash_read_sfdp((uint8_t*)&sfdp);
    if (ret < 0) {
        return -1;
    }

    offset = sfdp.header.PTP0 | (sfdp.header.PTP1 << 8) | (sfdp.header.PTP2 << 16);
    parm = (union JEDEC_Flash_Param*)((uint32_t) sfdp.bytes + offset);

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static int8_t config_command(SPIC_COMMAND_t *command, uint8_t cmd, uint32_t addr,
                        SPIC_ADDRESS_SIZE_t addr_size, uint8_t dummy_count)
{
    int8_t rc = 0;

    switch (cmd) {
    case FLASH_CMD_WREN:
    case FLASH_CMD_WRDI:
    case FLASH_CMD_WRSR:
    case FLASH_CMD_RDID:
    case FLASH_CMD_RDSR:
    case FLASH_CMD_RDSR2:
    case FLASH_CMD_CE:
    case FLASH_CMD_EN4B:
    case FLASH_CMD_EX4B:
    case FLASH_CMD_EXTNADDR_WREAR:
    case FLASH_CMD_EXTNADDR_RDEAR:
    case FLASH_CMD_EN_RST:
    case FLASH_CMD_RST_DEV:
        command->address.disabled = 1;
        command->data.bus_width = SPIC_CFG_BUS_SINGLE;
        break;
    case FLASH_CMD_READ:
    case FLASH_CMD_FREAD:
    case FLASH_CMD_SE:
    case FLASH_CMD_SE_4B:
    case FLASH_CMD_BE:
    case FLASH_CMD_RDSFDP:
    case FLASH_CMD_PP:
        command->address.disabled = 0;
        command->address.bus_width = SPIC_CFG_BUS_SINGLE;
        command->data.bus_width = SPIC_CFG_BUS_SINGLE;
        break;
    case FLASH_CMD_DREAD:
        command->address.disabled = 0;
        command->address.bus_width = SPIC_CFG_BUS_SINGLE;
        command->data.bus_width = SPIC_CFG_BUS_DUAL;
        break;
    case FLASH_CMD_QREAD:
        command->address.disabled = 0;
        command->address.bus_width = SPIC_CFG_BUS_SINGLE;
        command->data.bus_width = SPIC_CFG_BUS_QUAD;
        break;
    case FLASH_CMD_2READ:
        command->address.disabled = 0;
        command->address.bus_width = SPIC_CFG_BUS_DUAL;
        command->data.bus_width = SPIC_CFG_BUS_DUAL;
        break;
    case FLASH_CMD_4READ:
    case FLASH_CMD_4PP:
        command->address.disabled = 0;
        command->address.bus_width = SPIC_CFG_BUS_QUAD;
        command->data.bus_width = SPIC_CFG_BUS_QUAD;
        break;
    default:
        rc = -1;
        break;
    }

    command->instruction.value = cmd;
    command->address.size = addr_size;
    command->address.value = addr;
    command->dummy_count = dummy_count;

    return rc;
}

static int32_t flash_write_enable(void)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_STATUS_t status;

    uint32_t len = 0;

    config_command(command, FLASH_CMD_WREN, 0, 0, 0);
    status = spic_write(command, NULL, &len);
    if (status != SPIC_STATUS_OK) {
        //printf("error write enable: %d\n", status);
        return (int32_t)status;
    }

    return 0;
}

static int32_t flash_write_disable(void)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_STATUS_t status;

    uint32_t len = 0;

    config_command(command, FLASH_CMD_WRDI, 0, 0, 0);
    status = spic_write(command, NULL, &len);
    if (status != SPIC_STATUS_OK) {
        //printf("error write disable: %d\n", status);
        return (int32_t)status;
    }

    return 0;
}

static int32_t flash_read_sr(void)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_STATUS_t status;

    uint32_t len = 1;
    uint8_t sr;

    config_command(command, FLASH_CMD_RDSR, 0, 0, 0);
    status = spic_read(command, &sr, &len);
    if (status != SPIC_STATUS_OK) {
        //printf("error reading SR: %d\n", status);
        return (int32_t)status;
    }

    return sr;
}

static int32_t flash_erase(uint8_t cmd, uint32_t address)
{
    SPIC_COMMAND_t *command = &command_default;
    SPIC_STATUS_t status;

    int32_t rc = 0;
    uint32_t len = 0;

    flash_write_enable();

    config_command(command, cmd, address, 3, 0);
    status = spic_write(command, NULL, &len);
    if (status != SPIC_STATUS_OK) {
        rc = -1;
        goto err_exit;
    }

    flash_wait_till_ready();

err_exit:
    flash_write_disable();
    return rc;
}

static int32_t flash_wait_till_ready(void)
{
    int8_t sr;

    do {
        sr = (int8_t)flash_read_sr();
        if (sr < 0)
            return sr;
    } while (sr & SR_WIP);

    return 0;
}
