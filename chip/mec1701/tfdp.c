/*****************************************************************************
* Copyright (c) 2016 Microchip Technology Inc. and its subsidiaries.
* You may use this software and any derivatives exclusively with
* Microchip products.
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".
* NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE,
* INCLUDING ANY IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY,
* AND FITNESS FOR A PARTICULAR PURPOSE, OR ITS INTERACTION WITH MICROCHIP
* PRODUCTS, COMBINATION WITH ANY OTHER PRODUCTS, OR USE IN ANY APPLICATION.
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.
* TO THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL
* CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF
* FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
* MICROCHIP PROVIDES THIS SOFTWARE CONDITIONALLY UPON YOUR ACCEPTANCE
* OF THESE TERMS.
*****************************************************************************/

/** @file mec2016_tfdp.c
 *MEC2016 Trace FIFO Data Port hardware access
 */
/** @defgroup MEC2016 Peripherals TFDP
 *  @{
 */

#include "registers.h"
#include "tfdp.h"

#ifdef MCHP_TRACE

#undef TFDP_PIN_1
#undef TFDP_PIN_2

/* MEC17XX */
#define TFDP_PIN_1 (0170)   // Octal, MSCLK is Func1 PullUp enabled, 
#define TFDP_PIN_2 (0171)   // Octal, MSDATA is Func1 PullUp enabled


#define DISABLE_IRQ() __asm__ __volatile__ ("cpsid i" : : : "memory")
#define ENABLE_IRQ() __asm__ __volatile__ ("cpsie i" : : : "memory")

/**
 * tfdp_power - Gate clocks On/Off to TFDP block when idle
 *
 * @param pwr_on (0=Gate clocks when idle), (1=Do not gate
 *                 clocks when idle)
 */
void tfdp_power(uint8_t pwr_on)
{
    if ( pwr_on ) {
	MEC17XX_PCR_SLP_EN1 &= ~(1ul << 7);
    } else {
	MEC17XX_PCR_SLP_EN1 |= (1ul << 7);
    }
}


/**
 * tfdp_enable - Init Trace FIFO Data Port
 * @param uint8_t non-zero=enable TFDP, false=disable TFDP
 * @param uint8_t non-zero=change TFDP pin configuration.
 * If TFDP is enabled then GPIO170/171 set to Alt. Func. 1
 * Else GPIO170/171 set to GPIO input, internal pull-up enabled.
 * @note -
 */
#define MEC17XX_TFDP_DATA	REG8(MEC17XX_TFDP_BASE + 0x00)
#define MEC17XX_TFDP_CTRL	REG8(MEC17XX_TFDP_BASE + 0x04)

void tfdp_enable(uint8_t en, uint8_t pin_cfg)
{
    if (en) {
        MEC17XX_TFDP_CTRL = 0x01u;
        if (pin_cfg) {
		*(uint32_t *)(MEC17XX_GPIO_BASE + ((TFDP_PIN_1) << 2)) = 0x1041ul;
		*(uint32_t *)(MEC17XX_GPIO_BASE + ((TFDP_PIN_2) << 2)) = 0x1041ul;
        }
    } else {
        MEC17XX_TFDP_CTRL = 0x00u;
        if (pin_cfg) {
		*(uint32_t *)(MEC17XX_GPIO_BASE + ((TFDP_PIN_1) << 2)) = 0x0041ul;
		*(uint32_t *)(MEC17XX_GPIO_BASE + ((TFDP_PIN_2) << 2)) = 0x0041ul;
        }
    }
} // end tfdp_enable()


/**
 * TFDPTrace0 - TRACE0: transmit 16-bit trace number lsb first
 * over TFDP.
  *
 * @param nbr 16-bit trace number
 * @param b unused
 *
 * @return uint8_t always TRUE
 * @note Function implements critical section.
 * Uses tool kit __disable_irq()/__enable_irq() pair which may use
 * priviledged Cortex-Mx instructions.
 */
void TFDPTrace0(uint16_t nbr)
{
#ifdef MCHP_TRACE_MASK_IRQ
    uint32_t prim;

    __asm__ __volatile__ ("mrs %0, primask" : "=r" (prim) );
    DISABLE_IRQ();
#endif

    MEC17XX_TFDP_DATA = (TFDP_FRAME_START);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)nbr;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(nbr >> 8);
    TFDP_DELAY();

#ifdef MCHP_TRACE_MASK_IRQ
    if (!prim) {
	ENABLE_IRQ();
    }
#endif
}


/**
 * TRDPTrace1 - TRACE1: transmit 16-bit trace number lsb first
 * and 16-bit data lsb first over TFDP.
 *
 * @param nbr 16-bit trace number
 * @param b unused
 * @param uint32_t p1 16-bit data1 in b[15:0]
 *
 * @return uint8_t always TRUE
 * @note Function implements critical section.
 * Uses tool kit __disable_irq()/__enable_irq() pair which may use
 * priviledged Cortex-Mx instructions.
 */
void TFDPTrace1(uint16_t nbr, uint32_t p1 )
{
#ifdef MCHP_TRACE_MASK_IRQ
    uint32_t prim;

    __asm__ __volatile__ ("mrs %0, primask" : "=r" (prim) );
    DISABLE_IRQ();
#endif

    MEC17XX_TFDP_DATA = (TFDP_FRAME_START);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)nbr;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(nbr >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p1;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 8);
    TFDP_DELAY();

#ifdef MCHP_TRACE_MASK_IRQ
    if (!prim) {
	ENABLE_IRQ();
    }
#endif
}


/**
 * TFDPTrace2 - TRACE2: transmit 16-bit trace number lsb first
 * and two 16-bit data parameters lsb first over TFDP.
 *
 * @param nbr trace number
 * @param b unused
 * @param uint32_t p1 16-bit data1 in b[15:0]
 * @param uint32_t p2 16-bit data2 in b[15:0]
 *
 * @return uint8_t always TRUE
 * @note Uses tool kit functions to save/disable/restore
 *       interrupts for critical section. These may use
 *       priviledged instructions.
 */
void TFDPTrace2(uint16_t nbr, uint32_t p1, uint32_t p2 )
{
#ifdef MCHP_TRACE_MASK_IRQ
    uint32_t prim;

    __asm__ __volatile__ ("mrs %0, primask" : "=r" (prim) );
    DISABLE_IRQ();
#endif

    MEC17XX_TFDP_DATA = (TFDP_FRAME_START);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)nbr;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(nbr >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p1;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p2;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p2 >> 8);
    TFDP_DELAY();

#ifdef MCHP_TRACE_MASK_IRQ
    if (!prim) {
	ENABLE_IRQ();
    }
#endif
}


/**
 * TFDPTrace3 - TRACE3: transmit 16-bit trace number lsb first
 * and three 16-bit data parameters lsb first over TFDP.
 *
 * @param nbr trace number
 * @param b unused
 * @param uint32_t p1 16-bit data1 in b[15:0]
 * @param uint32_t p2 16-bit data2 in b[15:0]
 * @param uint32_t p3 16-bit data3 in b[15:0]
 *
 * @return uint8_t always TRUE
 * @note Uses tool kit functions to save/disable/restore
 *       interrupts for critical section. These may use
 *       priviledged instructions.
 */
void TFDPTrace3(uint16_t nbr, uint32_t p1,
		uint32_t p2, uint32_t p3)
{
#ifdef MCHP_TRACE_MASK_IRQ
    uint32_t prim;

    __asm__ __volatile__ ("mrs %0, primask" : "=r" (prim) );
    DISABLE_IRQ();
#endif

    MEC17XX_TFDP_DATA = (TFDP_FRAME_START);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)nbr;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(nbr >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p1;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p2;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p2 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p3;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p3 >> 8);
    TFDP_DELAY();

#ifdef MCHP_TRACE_MASK_IRQ
    if (!prim) {
	ENABLE_IRQ();
    }
#endif
}


/**
 * TFDPTrace4 - TRACE3: transmit 16-bit trace number lsb first
 * and four 16-bit data parameters lsb first over TFDP.
 *
 * @param nbr trace number
 * @param b unused
 * @param uint32_t p1 16-bit data1 in b[15:0]
 * @param uint32_t p2 16-bit data2 in b[15:0]
 * @param uint32_t p3 16-bit data3 in b[15:0]
 * @param uint32_t p4 16-bit data4 in b[15:0]
 *
 * @return uint8_t always TRUE
 * @note Uses tool kit functions to save/disable/restore
 *       interrupts for critical section. These may use
 *       priviledged instructions.
 */
void TFDPTrace4 (uint16_t nbr, uint32_t p1, uint32_t p2,
		 uint32_t p3, uint32_t p4)
{
#ifdef MCHP_TRACE_MASK_IRQ
    uint32_t prim;

    __asm__ __volatile__ ("mrs %0, primask" : "=r" (prim) );
    DISABLE_IRQ();
#endif

    MEC17XX_TFDP_DATA = (TFDP_FRAME_START);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)nbr;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(nbr >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p1;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p2;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p2 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p3;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p3 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p4;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p4 >> 8);
    TFDP_DELAY();

#ifdef MCHP_TRACE_MASK_IRQ
    if (!prim) {
	ENABLE_IRQ();
    }
#endif
}


/**
 *  TFDPTrace11 - Transmit one 32-bit data item over TFDP
 *
 *  @param nbr trace number
 *  @param b unused
 *  @param uint32_t p1 32-bit data to be transmitted
 *
 */
void TFDPTrace11(uint16_t nbr, uint32_t p1)
{
#ifdef MCHP_TRACE_MASK_IRQ
    uint32_t prim;

    __asm__ __volatile__ ("mrs %0, primask" : "=r" (prim) );
    DISABLE_IRQ();
#endif

    MEC17XX_TFDP_DATA = (TFDP_FRAME_START);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)nbr;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(nbr >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p1;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 16);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 24);
    TFDP_DELAY();

#ifdef MCHP_TRACE_MASK_IRQ
    if (!prim) {
	ENABLE_IRQ();
    }
#endif
}


/**
 *  TFDPTrace12 - Transmit two 32-bit data items over TFDP
 *
 *  @param nbr trace number
 *  @param b unused
 *  @param uint32_t p1 32-bit data1 to be transmitted
 *  @param uint32_t p2 32-bit data2 to be transmitted
 *
 */
void TFDPTrace12(uint16_t nbr, uint32_t p1, uint32_t p2)
{
#ifdef MCHP_TRACE_MASK_IRQ
    uint32_t prim;

    __asm__ __volatile__ ("mrs %0, primask" : "=r" (prim) );
    DISABLE_IRQ();
#endif

    MEC17XX_TFDP_DATA = (TFDP_FRAME_START);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)nbr;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(nbr >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p1;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 16);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 24);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p2;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p2 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p2 >> 16);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p2 >> 24);
    TFDP_DELAY();

#ifdef MCHP_TRACE_MASK_IRQ
    if (!prim) {
	ENABLE_IRQ();
    }
#endif
}

/**
 *  TFDPTrace13 - Transmit three 32-bit data items over TFDP
 *
 *  @param nbr trace number
 *  @param b unused
 *  @param uint32_t p1 32-bit data1 to be transmitted
 *  @param uint32_t p2 32-bit data2 to be transmitted
 *  @param uint32_t p3 32-bit data3 to be transmitted
 *
 */
void TFDPTrace13(uint16_t nbr, uint32_t p1,
		 uint32_t p2, uint32_t p3 )
{
#ifdef MCHP_TRACE_MASK_IRQ
    uint32_t prim;

    __asm__ __volatile__ ("mrs %0, primask" : "=r" (prim) );
    DISABLE_IRQ();
#endif

    MEC17XX_TFDP_DATA = (TFDP_FRAME_START);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)nbr;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(nbr >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p1;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 16);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 24);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p2;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p2 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p2 >> 16);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p2 >> 24);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p3;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p3 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p3 >> 16);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p3 >> 24);
    TFDP_DELAY();

#ifdef MCHP_TRACE_MASK_IRQ
    if (!prim) {
	ENABLE_IRQ();
    }
#endif
}

/**
 *  TFDPTrace14 - Transmit four 32-bit data items over TFDP
 *
 *  @param nbr trace number
 *  @param b unused
 *  @param uint32_t p1 32-bit data1 to be transmitted
 *  @param uint32_t p2 32-bit data2 to be transmitted
 *  @param uint32_t p3 32-bit data3 to be transmitted
 *  @param uint32_t p4 32-bit data4 to be transmitted
 */
void TFDPTrace14(uint16_t nbr, uint32_t p1, uint32_t p2,
		 uint32_t p3, uint32_t p4 )
{
#ifdef MCHP_TRACE_MASK_IRQ
    uint32_t prim;

    __asm__ __volatile__ ("mrs %0, primask" : "=r" (prim) );
    DISABLE_IRQ();
#endif

    MEC17XX_TFDP_DATA = (TFDP_FRAME_START);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)nbr;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(nbr >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p1;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 16);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p1 >> 24);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p2;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p2 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p2 >> 16);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p2 >> 24);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p3;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p3 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p3 >> 16);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p3 >> 24);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)p4;
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p4 >> 8);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p4 >> 16);
    TFDP_DELAY();
    MEC17XX_TFDP_DATA = (uint8_t)(p4 >> 24);
    TFDP_DELAY();

#ifdef MCHP_TRACE_MASK_IRQ
    if (!prim) {
	ENABLE_IRQ();
    }
#endif
}

#endif // #ifdef MCHP_TRACE_FAST


/* end tfdp.c */
/**   @}
 */
