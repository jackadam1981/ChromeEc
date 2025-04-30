/*********************
 *      INCLUDES
 *********************/

#include <stdio.h>

#include "reg.h"
#include "flash_map_backend.h"
#include <stdint.h>
/*********************
 *      DEFINES
 *********************/

#define RTS_MONITOR_UUT_TAG			0x4352544Bul
#define RTS_MONITOR_HEADER_ADDR     0x20010000ul
#define RTS_TEMP_DATA_ADDR          0x20020000ul
#define RTS_CMD_SEL_ADDR          0x2005F000ul
#define RTS_SPI_PROGRAMMING_FLAG    0x20018000ul

/**********************
 *      TYPEDEFS
 **********************/

struct monitor_header_tag {
	/* offset 0x00: TAG NPCX_MONITOR_TAG */
	uint32_t tag;
	/* offset 0x04: Size of the binary being programmed (in bytes) */
	uint32_t size;
	/* offset 0x08: The RAM address of the binary to program into the SPI */
	uint32_t src_addr;
	/* offset 0x0C: The Flash address to be programmed (Absolute address) */
	uint32_t dest_addr;
	/* offset 0x10: Maximum allowable flash clock frequency */
	uint8_t max_clock;
	/* offset 0x11: SPI Flash read mode */
	uint8_t read_mode;
	/* offset 0x12: Reserved */
	uint16_t reserved;
} __packed;


/* =========================================================================================================================== */
/* ================                                           UART                                            ================ */
/* =========================================================================================================================== */


/**
  * @brief UART Controller (UART)
  */

  typedef struct {                                /*!< (@ 0x40010100) UART Structure                                             */
  
	union {
	  union {
		volatile const  uint32_t RBR;                       /*!< (@ 0x00000000) RECEIVE BUFFER REGISTER                                    */
		
		struct {
		  volatile const  uint32_t DATA     : 8;            /*!< [7..0] Receive Buffer                                                     */
				uint32_t          : 24;
		} RBR_b;
	  } ;
	  
	  union {
		volatile  uint32_t THR;                       /*!< (@ 0x00000000) TRANSMIT HOLDING REGISTER                                  */
		
		struct {
		  volatile  uint32_t DATA     : 8;            /*!< [7..0] Transmit Holding                                                   */
				uint32_t          : 24;
		} THR_b;
	  } ;
	  
	  union {
		volatile  uint32_t DLL;                       /*!< (@ 0x00000000) DIVISOR LATCH LOW REGISTER                                 */
		
		struct {
		  volatile  uint32_t DIVL     : 8;            /*!< [7..0] Divisor Latch Low Byte                                             */
				uint32_t          : 24;
		} DLL_b;
	  } ;
	};
	
	union {
	  union {
		volatile  uint32_t DLH;                       /*!< (@ 0x00000004) DIVISOR LATCH HIGH REGISTER                                */
		
		struct {
		  volatile  uint32_t DIVH     : 8;            /*!< [7..0] Divisor Latch High Byte                                            */
				uint32_t          : 24;
		} DLH_b;
	  } ;
	  
	  union {
		volatile  uint32_t IER;                       /*!< (@ 0x00000004) INTRRRUPT ENABLE REGISTER                                  */
		
		struct {
		  volatile  uint32_t ERBFI    : 1;            /*!< [0..0] Enable Received Data Available Interrupt                           */
		  volatile  uint32_t ETBEI    : 1;            /*!< [1..1] Enable Transmit Holding Register Empty Interrupt                   */
		  volatile  uint32_t ELSI     : 1;            /*!< [2..2] Enable Receiver Line Status Interrupt                              */
				uint32_t          : 4;
		  volatile  uint32_t PTIME    : 1;            /*!< [7..7] Programmable THRE Interrupt Mode Enable                            */
				uint32_t          : 24;
		} IER_b;
	  } ;
	};
	
	union {
	  union {
		volatile   uint32_t IIR;                       /*!< (@ 0x00000008) INTERRUPT IDENTIFICATION                                   */
		
		struct {
		  volatile   uint32_t IID      : 4;            /*!< [3..0] Interrupt ID                                                       */
				uint32_t          : 2;
		  volatile   uint32_t FIFOSE   : 2;            /*!< [7..6] FIFOs Enabled                                                      */
				uint32_t          : 24;
		} IIR_b;
	  } ;
	  
	  union {
		volatile  uint32_t FCR;                       /*!< (@ 0x00000008) FIFO CONTROL REGISTER                                      */
		
		struct {
		  volatile  uint32_t FIFOE    : 1;            /*!< [0..0] FIFO Enabled                                                       */
		  volatile  uint32_t RFIFOR   : 1;            /*!< [1..1] Rx FIFO Reset                                                      */
		  volatile  uint32_t XFIFOR   : 1;            /*!< [2..2] Tx FIFO Reset                                                      */
				uint32_t          : 1;
		  volatile  uint32_t TXTRILEV : 2;            /*!< [5..4] TX Empty Trigger Level                                             */
		  volatile  uint32_t RXTRILEV : 2;            /*!< [7..6] Rx Trigger Level                                                   */
				uint32_t          : 24;
		} FCR_b;
	  } ;
	};
	
	union {
	  volatile  uint32_t LCR;                         /*!< (@ 0x0000000C) LINE CONTROL REGISTER                                      */
	  
	  struct {
		volatile  uint32_t DLS        : 2;            /*!< [1..0] Data Length Select                                                 */
		volatile  uint32_t STOP       : 1;            /*!< [2..2] Number of Stop Bits                                                */
		volatile  uint32_t PEN        : 1;            /*!< [3..3] Parity Enable                                                      */
		volatile  uint32_t EPS        : 1;            /*!< [4..4] Even Parity Select                                                 */
		volatile  uint32_t STP        : 1;            /*!< [5..5] Stick Parity                                                       */
		volatile  uint32_t BC         : 1;            /*!< [6..6] Break Control Bit                                                  */
		volatile  uint32_t DLAB       : 1;            /*!< [7..7] Divisor Latch Access Bit                                           */
			  uint32_t            : 24;
	  } LCR_b;
	} ;
	volatile const  uint32_t  RESERVED;
	
	union {
	  volatile const  uint32_t LSR;                         /*!< (@ 0x00000014) LINE STATUS REGISTER                                       */
	  
	  struct {
		volatile const  uint32_t DR         : 1;            /*!< [0..0] Data Ready Bit                                                     */
		volatile const  uint32_t OE         : 1;            /*!< [1..1] Overrun Error bit                                                  */
		volatile const  uint32_t PE         : 1;            /*!< [2..2] Parity Error bit                                                   */
		volatile const  uint32_t FE         : 1;            /*!< [3..3] Framing Error bit                                                  */
		volatile const  uint32_t BI         : 1;            /*!< [4..4] Break Interrupt bit                                                */
		volatile const  uint32_t THRE       : 1;            /*!< [5..5] Transmit Holding Register Empty bit                                */
		volatile const  uint32_t TEMT       : 1;            /*!< [6..6] Transmitter Empty Bit                                              */
		volatile const  uint32_t RFE        : 1;            /*!< [7..7] Receiver FIFO Error                                                */
			  uint32_t            : 24;
	  } LSR_b;
	} ;
	volatile const  uint32_t  RESERVED1[25];
	
	union {
	  volatile const  uint32_t USR;                         /*!< (@ 0x0000007C) UART Status Register                                       */
	  
	  struct {
		volatile const  uint32_t BUSY       : 1;            /*!< [0..0] UART Busy                                                          */
		volatile const  uint32_t TFNF       : 1;            /*!< [1..1] Transmit FIFO Not Full                                             */
		volatile const  uint32_t TFE        : 1;            /*!< [2..2] Transmit FIFO Empty                                                */
		volatile const  uint32_t RFNE       : 1;            /*!< [3..3] Receive FIFO Not Empty                                             */
		volatile const  uint32_t RFF        : 1;            /*!< [4..4] Receive FIFO Full                                                  */
			  uint32_t            : 27;
	  } USR_b;
	} ;
	volatile const  uint32_t  TFL;                          /*!< (@ 0x00000080) UART TRANSMIT FIFO LEVEL                                   */
	volatile const  uint32_t  RFL;                          /*!< (@ 0x00000084) UART RECEIVE FIFO LEVEL                                    */
	
	union {
	  volatile  uint32_t SRR;                         /*!< (@ 0x00000088) UART SOFTWARE RESET REGISTER                               */
	  
	  struct {
		volatile  uint32_t UR         : 1;            /*!< [0..0] UART Reset                                                         */
		volatile  uint32_t RFR        : 1;            /*!< [1..1] RCVR FIFO Reset                                                    */
		volatile  uint32_t XFR        : 1;            /*!< [2..2] XMIT FIFO Reset                                                    */
			  uint32_t            : 29;
	  } SRR_b;
	} ;
  } UART_Type;                                    /*!< Size = 140 (0x8c)                                                         */
  
  

/**********************
 *  EXTERN PROTOTYPES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void eflash_erase(int offset, int size);
static void eflash_write(int offset, int size, const char *data);
static int eflash_verify(int offset, int size, const char *data);
static void eflash_read(int offset, int size, const char *data);
/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *  GLOBAL VARIABLES
 **********************/
#define UART_BASE                   0x40010100UL
#define UART                        ((UART_Type*)    UART_BASE)
#define UART_USR_TFNF_Msk           (0x2UL)
#define UART_USR_TFE_Msk            (0x4UL)
#define UART_LSR_THRE_Msk            (0x20UL)
/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int spic_flash_upload(void)
{
	/*
	 * Flash image has been uploaded to Code RAM
	 */
	uint32_t sz_image;
	uint32_t uut_tag;
	const char *image_base;
	uint32_t *flag_upload;
	struct monitor_header_tag *monitor_header =
		(struct monitor_header_tag *)(RTS_MONITOR_HEADER_ADDR);

    int spi_offset;

    flag_upload = (uint32_t *)(RTS_SPI_PROGRAMMING_FLAG);
	*flag_upload = 0;

	uut_tag = monitor_header->tag;
	/* If it is UUT tag, read required parameters from header */
	if (uut_tag == RTS_MONITOR_UUT_TAG) {
		sz_image = monitor_header->size;
		spi_offset = monitor_header->dest_addr;

		image_base = (const char *)(monitor_header->src_addr);
	} else {
		*flag_upload = 0x08;
        return -1;
	}

    intr_flash_pin_init();
    spic_init(3, 0);
	UART->FCR |= 0x47;
    /* Clear status reg of spi flash for protection */
    // if (eflash_unlock_write_protect_area() == 0) {
		/* Start to erase */
		// eflash_erase(spi_offset, sz_image);

		// /* Start to write */
		// if (image_base != NULL) {
        //     eflash_write(spi_offset, sz_image, image_base);
        // }

		// /* Verify data */
		// if (eflash_verify(spi_offset, sz_image, image_base) == 0) {
        //     *flag_upload |= 0x02;
        // }
    // }
//20250502 JASN SAR




// while((UART->LSR & UART_LSR_THRE_Msk) == 0){;}
// 			UART->THR = 0xA1;


if(1){
	uint32_t *temp_to_load;
	temp_to_load = (uint32_t*)RTS_CMD_SEL_ADDR;
    if( *temp_to_load == 0xA5A5A5A5){

		*flag_upload |= 0x04;
		eflash_read(spi_offset, sz_image, image_base);

	}else{
		/* Start to erase */
		eflash_erase(spi_offset, sz_image);
		// while((UART->LSR & UART_LSR_THRE_Msk) == 0){;}
		// UART->THR = 0xC1;
		/* Start to write */
		if (image_base != NULL) {
			eflash_write(spi_offset, sz_image, image_base);
		}
	
		/* Verify data */
		if (eflash_verify(spi_offset, sz_image, image_base) == 0) {
			*flag_upload |= 0x02;
		}
	}
}

//while(1){
// while((UART->LSR & UART_LSR_THRE_Msk) == 0){;}
// 			UART->THR = 0xB1;
// while((UART->LSR & UART_LSR_THRE_Msk) == 0){;}
// 			UART->THR = 0xB1;
// while((UART->LSR & UART_LSR_THRE_Msk) == 0){;}
// 			UART->THR = 0xB1;
// while(!(UART->USR & UART_USR_TFNF_Msk)){;}
// 			UART->THR =  0xC2;
// 			while(!(UART->USR & UART_USR_TFNF_Msk)){;}
// 			UART->THR =  0xCC;
//}
//20250502 JASN END
	/* Mark we have finished upload work */
	*flag_upload |= 0x01;

	/* Return the status back to ROM code is required for UUT */
	if (uut_tag == RTS_MONITOR_UUT_TAG)
		return *flag_upload;

	/* Infinite loop */
	for (;;)
		;

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void eflash_erase(int offset, int size)
{
	/* Alignment has been checked in upper layer */
	for (; size > 0; size -= FLASH_SECTOR_EARSE_SIZE,
			 offset += FLASH_SECTOR_EARSE_SIZE) {
        flash_erase_sector(offset, FLASH_ADDRESSING_3BYTE);
	}
}

static void eflash_write(int offset, int size, const char *data)
{
	int dest_addr = offset;
	const int sz_page = FLASH_PAGE_PROGRAM_SIZE;

	/* Write the data per CONFIG_FLASH_WRITE_IDEAL_SIZE bytes */
	for (; size >= sz_page; size -= sz_page) {
        flash_program_page(dest_addr, (uint8_t*)data, sz_page, FLASH_ADDRESSING_3BYTE);

		data += sz_page;
		dest_addr += sz_page;
	}

	/* Handle final partial page, if any */
	if (size != 0) {
        flash_program_page(dest_addr, (uint8_t*)data, size, FLASH_ADDRESSING_3BYTE);
	}
}

static void serial_polling_send(const char* buf, uint32_t len)
{
    uint32_t i = 0;
	for (i = 0; i < len; i++) {
        while((UART->LSR & UART_LSR_THRE_Msk) == 0){;}
        UART->THR = buf[i];
    }
}

static void eflash_read(int offset, int size, const char *data)
{
	int dest_addr = offset;
	const int sz_page = FLASH_PAGE_PROGRAM_SIZE;

	/* Write the data per CONFIG_FLASH_WRITE_IDEAL_SIZE bytes */
	for (; size >= sz_page; size -= sz_page) {
        flash_read(0x03, dest_addr, (char *)data, sz_page, FLASH_ADDRESSING_3BYTE);

		serial_polling_send(data, sz_page);

		data += sz_page;
		dest_addr += sz_page;
	}

	/* Handle final partial page, if any */
	if (size != 0) {
        flash_read(0x03, dest_addr, (char *)data, size, FLASH_ADDRESSING_3BYTE);
		serial_polling_send(data, size);
	}
}

static int eflash_verify(int offset, int size, const char *data)
{
	int dest_addr = offset;
	const int sz_page = FLASH_PAGE_PROGRAM_SIZE;

    uint32_t i;
    uint8_t tmp;
    uint8_t rd_buf[FLASH_PAGE_PROGRAM_SIZE];

	/* Write the data per CONFIG_FLASH_WRITE_IDEAL_SIZE bytes */
	for (; size >= sz_page; size -= sz_page) {
        flash_read(0x03, dest_addr, rd_buf, sz_page, FLASH_ADDRESSING_3BYTE);
        for (i = 0; i < sz_page; i++) {
            tmp = *(data + i);
            if (rd_buf[i] != tmp) {
                return -1;
            }
        }

		data += sz_page;
		dest_addr += sz_page;
	}

	/* Handle final partial page, if any */
	if (size != 0) {
        flash_read(0x03, dest_addr, rd_buf, size, FLASH_ADDRESSING_3BYTE);
        for (i = 0; i < size; i++) {
            tmp = *(data + i);
            if (rd_buf[i] != tmp) {
                return -1;
            }
        }
	}

    return 0;
}