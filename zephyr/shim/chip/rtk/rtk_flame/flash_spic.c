/*********************
 *      INCLUDES
 *********************/

#include "reg.h"
#include "flash_spic.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <zephyr/toolchain.h>
/*********************
 *      DEFINES
 *********************/
#define MODE(x)                    ((uint32_t)((x) & 0x00000003) << 6)
#define TMOD(x)                    ((uint32_t)(((x) & 0x00000003) << 8))
#define CMD_CH(x)                  ((uint32_t)(((x) & 0x00000003) << 20))
#define ADDR_CH(x)                 ((uint32_t)(((x) & 0x00000003) << 16))
#define DATA_CH(x)                 ((uint32_t)(((x) & 0x00000003) << 18))


#define USER_CMD_LENGHT(x)          ((uint32_t)(((x) & 0x00000003ul) << SPIC_USERLENGTH_CMDLEN_Pos))
#define USER_ADDR_LENGTH(x)         ((uint32_t)(((x) & 0x0000000Ful) << SPIC_USERLENGTH_ADDRLEN_Pos))
#define USER_RD_DUMMY_LENGTH(x)     ((uint32_t)(((x) & 0x00000FFFul) << SPIC_USERLENGTH_RDDUMMYLEN_Pos))

#define TX_NDF(x)                  ((uint32_t)(((x) & 0x00FFFFFFul) << SPIC_TXNDF_NUM_Pos))
#define RX_NDF(x)                  ((uint32_t)(((x) & 0x00FFFFFFul) << SPIC_RXNDF_NUM_Pos))

#define CK_MTIMES(x)               ((uint32_t)(((x) & 0x0000001Ful) << 23))
#define GET_CK_MTIMES(x)           ((uint32_t)(((x >> 23) & 0x0000001Ful)))

#define SIPOL(x)                   ((uint32_t)(((x) & 0x0000001Ful) << 0))
#define GET_SIPOL(x)               ((uint32_t)(((x >> 0) & 0x0000001Ful)))

/**********************
 *      TYPEDEFS
 **********************/
enum {
    COMMAND_READ = 0,
    COMMAND_WRITE = 1,
};

/**********************
 *  EXTERN PROTOTYPES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void spic_wait_finish(void);
static void spic_flush_fifo(void);
static void spic_cs_active(void);
static void spic_cs_deactive(void);
static void spic_usermode(void);
static void spic_automode(void);
static SPIC_STATUS_t spic_prepare_command(const SPIC_COMMAND_t *command, \
    uint32_t tx_size, uint32_t rx_size, uint8_t write);
static SPIC_STATUS_t spic_transmit_data(const void *data, uint32_t *length);
static SPIC_STATUS_t spic_receive_data(void *data, uint32_t *length);

/**********************
 *  STATIC VARIABLES
 **********************/
static const uint8_t user_addr_len[] = {
    [SPIC_CFG_ADDR_SIZE_8] = 1,
    [SPIC_CFG_ADDR_SIZE_16] = 2,
    [SPIC_CFG_ADDR_SIZE_24] = 3,
    [SPIC_CFG_ADDR_SIZE_32] = 4,
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

SPIC_STATUS_t spic_init(uint8_t hz, uint8_t mode)
{
    return spic_init_direct(hz, mode);
}

SPIC_STATUS_t spic_init_direct(uint8_t hz, uint8_t mode)
{
    /* mode | POL PHA
     * -----+--------
     *   0  |  0   0
     *   1  |  0   1
     *   2  |  1   0
     *   3  |  1   1
     */
    if (mode > 3) {
        return SPIC_STATUS_INVALID_PARAMETER;
    }

    /* set SSIENR: deactive to program this transfer */
    SPIC->SSIENR = 0ul;

    /* default auto mode, single channel */
    SPIC->CTRL0 = CK_MTIMES(GET_CK_MTIMES(SPIC->CTRL0)) | \
            CMD_CH(0) | DATA_CH(0) | ADDR_CH(0) | MODE(mode) | \
            SIPOL(GET_SIPOL(SPIC->CTRL0));

    /* disable all interrupt */
    SPIC->IMR = 0ul;

    return spic_frequency(hz);
}

SPIC_STATUS_t spic_free(void)
{
    SPIC->SSIENR = 0ul;

    return SPIC_STATUS_OK;
}

SPIC_STATUS_t spic_frequency(uint8_t hz)
{
    if (hz == 0) {
        return SPIC_STATUS_INVALID_PARAMETER;
    }

    SPIC->BAUDR = hz;
    SPIC->FBAUD = hz;

    return SPIC_STATUS_OK;
}

SPIC_STATUS_t spic_write(const SPIC_COMMAND_t *command,
                         const void *data, uint32_t *length)
{
    spic_usermode();
    spic_prepare_command(command, *length, 0, COMMAND_WRITE);
    spic_cs_active();

    spic_transmit_data(data, length);
    spic_wait_finish();

    spic_cs_deactive();
    spic_automode();

    return SPIC_STATUS_OK;
}

SPIC_STATUS_t spic_command_transfer(const SPIC_COMMAND_t *command,
                                    const void *tx_data, uint32_t tx_size,
                                    void *rx_data, uint32_t rx_size)
{
    spic_usermode();
    spic_prepare_command(command, tx_size, rx_size, COMMAND_WRITE);
    spic_cs_active();

    spic_transmit_data(tx_data, &tx_size);
    spic_receive_data(rx_data, &rx_size);
    spic_wait_finish();

    spic_cs_deactive();
    spic_automode();

    return SPIC_STATUS_OK;
}

SPIC_STATUS_t spic_read(const SPIC_COMMAND_t *command, void *data,
                        uint32_t *length)
{
    spic_usermode();
    spic_prepare_command(command, 0, *length, COMMAND_READ);
    spic_cs_active();

    spic_receive_data(data, length);
    spic_wait_finish();

    spic_cs_deactive();
    spic_automode();

    return SPIC_STATUS_OK;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void spic_wait_finish(void)
{
    while(SPIC->SSIENR & SPIC_SSIENR_SPICEN_Msk);
}

static void spic_flush_fifo(void)
{
    SPIC->FLUSH = SPIC_FLUSH_ALL_Msk;
}

static void spic_cs_active(void)
{
    SPIC->SER = 1;
}

static void spic_cs_deactive(void)
{
    SPIC->SER = 0ul;
}

static void spic_usermode(void)
{
    SPIC->CTRL0 |= SPIC_CTRL0_USERMD_Msk;
}

static void spic_automode(void)
{
    SPIC->CTRL0 &= ~SPIC_CTRL0_USERMD_Msk;
}

static SPIC_STATUS_t spic_prepare_command(const SPIC_COMMAND_t *command,
                                          uint32_t tx_size, uint32_t rx_size,
                                          uint8_t write)
{
    uint32_t i;
    uint8_t addr_len = user_addr_len[command->address.size];

    spic_flush_fifo();

    /* set SSIENR: deactive to program this transfer */
    SPIC->SSIENR = 0ul;

    /* set CTRLR0: TX mode and channel */
    SPIC->CTRL0 &= ~(TMOD(3) | CMD_CH(3) | ADDR_CH(3) | DATA_CH(3));
    SPIC->CTRL0 |= TMOD(write == 0x01 ? 0x00ul : 0x03ul) | \
                ADDR_CH(command->address.bus_width) | \
                DATA_CH(command->data.bus_width);

    /* set USER_LENGTH */
    SPIC->USERLENGTH = USER_CMD_LENGHT(1) | \
                    USER_ADDR_LENGTH(command->address.disabled ? 0 : addr_len) | \
                    USER_RD_DUMMY_LENGTH(command->dummy_count * SPIC->BAUDR * 2);

    /* Write command */
    if (!command->instruction.disabled) {
        SPIC->DR.BYTE = command->instruction.value;
    }

    /* Write address */
    if (!command->address.disabled) {
        for (i = 0; i < addr_len; i++) {
            SPIC->DR.BYTE = (uint8_t)(command->address.value >> (8 * (addr_len - i - 1)));
        }
    }

    /* Set TX_NDF: frame number of Tx data */
    SPIC->TXNDF = TX_NDF(tx_size);

    /* Set RX_NDF: frame number of receiving data. */
    SPIC->RXNDF = RX_NDF(rx_size);

    return SPIC_STATUS_OK;
}

static SPIC_STATUS_t spic_transmit_data(const void *data, uint32_t *length)
{
    uint32_t i, len = *length;

    /* set SSIENR to start the transfer */
    SPIC->SSIENR = SPIC_SSIENR_SPICEN_Msk;

    /* write the remaining data into fifo */
    for (i = 0; i < len;) {
        if (SPIC->SR & SPIC_SR_TFNF_Msk) {
            SPIC->DR.BYTE = ((uint8_t *)data)[i];
            i++;
        }
    }

    return SPIC_STATUS_OK;
}

void *memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    while (n--) {
    *d++ = *s++;
    }
    return dest;
}

static SPIC_STATUS_t spic_receive_data(void *data, uint32_t *length)
{
    uint32_t i, cnt, rx_num, fifo, len;
    uint8_t *rx_data = data;

    len = *length;
    rx_data = data;

    /* set SSIENR to start the transfer */
    SPIC->SSIENR = SPIC_SSIENR_SPICEN_Msk;

    rx_num = 0;
    while (rx_num < len) {
        cnt = SPIC->RXFLR;

        for (i = 0; i < (cnt / 4); i++) {
            fifo = SPIC->DR.WORD;
            memcpy((void *)(rx_data + rx_num), (void *)&fifo, 4);
            rx_num += 4;
        }

        if (((rx_num / 4) == (len / 4)) && (rx_num < len)) {
            for (i = 0; i < (cnt % 4); i++) {
                *(uint8_t *)(rx_data + rx_num) = SPIC->DR.BYTE;
                rx_num += 1;
            }
        }
    }

    return SPIC_STATUS_OK;
}
