/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * For Chipset: RTK EC
 *
 * Function: RTK Flash Utility
 */

#ifndef __REG_H__

#include <stdint.h>
#include <stdio.h>

/**
 * @brief I/O Pad Controller (IOPAD)
 */

typedef struct { /*!< (@ 0x40091000) IOPAD Structure */

	union {
		volatile uint32_t FLASHWP; /*!< (@ 0x00000000) INTERNAL FLASH_WP
					      PAD CONTROL REGISTER */

		struct {
			volatile uint32_t INDETEN : 1; /*!< [0..0] Input
							  Detection Enable */
			volatile uint32_t OUTDRV : 1; /*!< [1..1] Pin Status */
			volatile uint32_t SLEWRATE : 1; /*!< [2..2] Slew-rate
							   selection */
			volatile uint32_t PULLDWEN : 1; /*!< [3..3] Pull-Down
							   Enable */
			volatile uint32_t PULLUPEN : 1; /*!< [4..4] Pull-Up
							   Enable */
			volatile uint32_t SCHEN : 1; /*!< [5..5]
							Schmitter-Trigger Enable
						      */
			uint32_t : 26;
		} FLASHWP_b;
	};

	union {
		volatile uint32_t FLASHHOLD; /*!< (@ 0x00000004) INTERNAL
						FLASH_HOLD PAD CONTROL REGISTER
					      */

		struct {
			volatile uint32_t INDETEN : 1; /*!< [0..0] Input
							  Detection Enable */
			volatile uint32_t OUTDRV : 1; /*!< [1..1] Pin Status */
			volatile uint32_t SLEWRATE : 1; /*!< [2..2] Slew-rate
							   selection */
			volatile uint32_t PULLDWEN : 1; /*!< [3..3] Pull-Down
							   Enable */
			volatile uint32_t PULLUPEN : 1; /*!< [4..4] Pull-Up
							   Enable */
			volatile uint32_t SCHEN : 1; /*!< [5..5]
							Schmitter-Trigger Enable
						      */
			uint32_t : 26;
		} FLASHHOLD_b;
	};

	union {
		volatile uint32_t FLASHSI; /*!< (@ 0x00000008) INTERNAL FLASH_SI
					      PAD CONTROL REGISTER */

		struct {
			volatile uint32_t INDETEN : 1; /*!< [0..0] Input
							  Detection Enable */
			volatile uint32_t OUTDRV : 1; /*!< [1..1] Pin Status */
			volatile uint32_t SLEWRATE : 1; /*!< [2..2] Slew-rate
							   selection */
			volatile uint32_t PULLDWEN : 1; /*!< [3..3] Pull-Down
							   Enable */
			volatile uint32_t PULLUPEN : 1; /*!< [4..4] Pull-Up
							   Enable */
			volatile uint32_t SCHEN : 1; /*!< [5..5]
							Schmitter-Trigger Enable
						      */
			uint32_t : 26;
		} FLASHSI_b;
	};

	union {
		volatile uint32_t FLASHSO; /*!< (@ 0x0000000C) INTERNAL FLASH_SO
					      PAD CONTROL REGISTER */

		struct {
			volatile uint32_t INDETEN : 1; /*!< [0..0] Input
							  Detection Enable */
			volatile uint32_t OUTDRV : 1; /*!< [1..1] Pin Status */
			volatile uint32_t SLEWRATE : 1; /*!< [2..2] Slew-rate
							   selection */
			volatile uint32_t PULLDWEN : 1; /*!< [3..3] Pull-Down
							   Enable */
			volatile uint32_t PULLUPEN : 1; /*!< [4..4] Pull-Up
							   Enable */
			volatile uint32_t SCHEN : 1; /*!< [5..5]
							Schmitter-Trigger Enable
						      */
			uint32_t : 26;
		} FLASHSO_b;
	};

	union {
		volatile uint32_t FLASHCS; /*!< (@ 0x00000010) INTERNAL FLASH_CS
					      PAD CONTROL REGISTER */

		struct {
			volatile uint32_t INDETEN : 1; /*!< [0..0] Input
							  Detection Enable */
			volatile uint32_t OUTDRV : 1; /*!< [1..1] Pin Status */
			volatile uint32_t SLEWRATE : 1; /*!< [2..2] Slew-rate
							   selection */
			volatile uint32_t PULLDWEN : 1; /*!< [3..3] Pull-Down
							   Enable */
			volatile uint32_t PULLUPEN : 1; /*!< [4..4] Pull-Up
							   Enable */
			volatile uint32_t SCHEN : 1; /*!< [5..5]
							Schmitter-Trigger Enable
						      */
			uint32_t : 26;
		} FLASHCS_b;
	};

	union {
		volatile uint32_t FLASHCLK; /*!< (@ 0x00000014) INTERNAL
					       FLASH_CLK PAD CONTROL REGISTER */

		struct {
			volatile uint32_t INDETEN : 1; /*!< [0..0] Input
							  Detection Enable */
			volatile uint32_t OUTDRV : 1; /*!< [1..1] Pin Status */
			volatile uint32_t SLEWRATE : 1; /*!< [2..2] Slew-rate
							   selection */
			volatile uint32_t PULLDWEN : 1; /*!< [3..3] Pull-Down
							   Enable */
			volatile uint32_t PULLUPEN : 1; /*!< [4..4] Pull-Up
							   Enable */
			volatile uint32_t SCHEN : 1; /*!< [5..5]
							Schmitter-Trigger Enable
						      */
			uint32_t : 26;
		} FLASHCLK_b;
	};

	union {
		volatile uint32_t PECI; /*!< (@ 0x00000018) PECI PAD CONTROL
					   REGISTER */

		struct {
			volatile uint32_t INDETEN : 1; /*!< [0..0] Input
							  Detection Enable */
			volatile uint32_t OUTDRV : 1; /*!< [1..1] Pin Status */
			volatile uint32_t SLEWRATE : 1; /*!< [2..2] Slew-rate
							   selection */
			volatile uint32_t PULLDWEN : 1; /*!< [3..3] Pull-Down
							   Enable */
			volatile uint32_t PULLUPEN : 1; /*!< [4..4] Pull-Up
							   Enable */
			volatile uint32_t SCHEN : 1; /*!< [5..5]
							Schmitter-Trigger Enable
						      */
			uint32_t : 26;
		} PECI_b;
	};
} IOPAD_Type; /*!< Size = 28 (0x1c) */

typedef struct { /*!< (@ 0x4000C200) SLWTMR0 Structure */
	volatile uint32_t LDCNT; /*!< (@ 0x00000000) LOAD COUNTER REGISTER */
	volatile uint32_t CNT; /*!< (@ 0x00000004) CURRENT COUNTER REGISTER */

	union {
		volatile uint32_t CTRL; /*!< (@ 0x00000008) CONTROL REGISTER */

		struct {
			volatile uint32_t EN : 1; /*!< [0..0] Enable Timer
						     Contoller */
			volatile uint32_t MDSEL : 1; /*!< [1..1] Timer Operating
							Mode Selection */
			volatile uint32_t INTEN : 1; /*!< [2..2] Enable Timer
							Interrupt */
			volatile uint32_t STOP : 1; /*!< [3..3] Stop Timer */
			uint32_t : 28;
		} CTRL_b;
	};

	union {
		volatile uint32_t INTSTS; /*!< (@ 0x0000000C) INTERRUPT STATUS
					     REGISTER */

		struct {
			volatile uint32_t STS : 1; /*!< [0..0] Timer Interrupt
						      Status */
			uint32_t : 31;
		} INTSTS_b;
	};
} SLWTMR_Type; /*!< Size = 16 (0x10)  */

/*
 * @brief SPIC Controller (SPIC)
 */

typedef struct { /*!< (@ 0x40010200) SPIC Structure */

	union {
		volatile uint32_t CTRL0; /*!< (@ 0x00000000) CONTROL REGISTER #0
					  */

		struct {
			uint32_t : 6;
			volatile uint32_t SCPH : 1; /*!< [6..6] Serial Clock
						       Phase */
			volatile uint32_t SCPOL : 1; /*!< [7..7] Serial Clock
							Polarity */
			volatile uint32_t TMOD : 2; /*!< [9..8] Transfer Mode */
			uint32_t : 6;
			volatile uint32_t ADDRCH : 2; /*!< [17..16] Channel
							 Number of Address Phase
							 After Command Phase */
			volatile uint32_t DATACH : 2; /*!< [19..18] Channel
							Number of Data in
							Transmitting or
							Receiving Data */
			volatile uint32_t CMDCH : 2; /*!< [21..20] Channel
						       Number of Command Phase
						       in Transmitting or
						       Receiving Data */
			uint32_t : 9;
			volatile uint32_t USERMD : 1; /*!< [31..31] User Mode
							 Bit */
		} CTRL0_b;
	};

	union {
		volatile uint32_t RXNDF; /*!< (@ 0x00000004) NUMBER OF RX DATA
					    FRAME REGISTER */

		struct {
			volatile uint32_t NUM : 24; /*!< [23..0] Number of Rx
						       Data Frames */
			uint32_t : 8;
		} RXNDF_b;
	};

	union {
		volatile uint32_t SSIENR; /*!< (@ 0x00000008) ENABLE REGISTER */

		struct {
			volatile uint32_t SPICEN : 1; /*!< [0..0] Set to Enable
							 SPIC and Start User
							 Mode Transaction */
			volatile uint32_t ATCKCMD : 1; /*!< [1..1] Set to enable
							  ATCK_CMD
							  implementation */
			uint32_t : 30;
		} SSIENR_b;
	};
	volatile uint32_t RESERVED;

	union {
		volatile uint32_t SER; /*!< (@ 0x00000010) SELECT TARGET FLASH
					  REGISTER */

		struct {
			volatile uint32_t SEL : 1; /*!< [0..0] Select to SPI
						      Flash */
			uint32_t : 31;
		} SER_b;
	};

	union {
		volatile uint32_t BAUDR; /*!< (@ 0x00000014) BAUD RATE SELECT
					    REGISTER */

		struct {
			volatile uint32_t SCKDV : 12; /*!< [11..0] Define SPI
							 Clock Divider Value */
			uint32_t : 20;
		} BAUDR_b;
	};
	volatile uint32_t TXFTLR; /*!< (@ 0x00000018) TX FIFO THRESHOLD REGISTER
				   */
	volatile uint32_t RXFTLR; /*!< (@ 0x0000001C) RECEIVE FIFO THRESHOLD
				     LEVEL                               */
	volatile uint32_t TXFLR; /*!< (@ 0x00000020) TRANSMIT FIFO LEVEL
				    REGISTER                               */
	volatile uint32_t RXFLR; /*!< (@ 0x00000024) RECEIVE FIFO LEVEL REGISTER
				  */

	union {
		volatile uint32_t SR; /*!< (@ 0x00000028) STATUS REGISTER */

		struct {
			volatile const uint32_t BUSY : 1; /*!< [0..0] SPIC busy
							     flag */
			volatile const uint32_t TFNF : 1; /*!< [1..1] Transmit
							     FIFO is not full */
			volatile const uint32_t TFE : 1; /*!< [2..2] Transmit
							    FIFO is empty */
			volatile const uint32_t RFNE : 1; /*!< [3..3] Receive
							     FIFO is not empty
							   */
			volatile const uint32_t RFF : 1; /*!< [4..4] Receive
							    FIFO full */
			volatile const uint32_t TXE : 1; /*!< [5..5]
							    Transmission error
							  */
			uint32_t : 26;
		} SR_b;
	};

	union {
		volatile uint32_t IMR; /*!< (@ 0x0000002C) Interrupt Mask
					  Register */

		struct {
			volatile uint32_t TXEIM : 1; /*!< [0..0] Transmit FIFO
							empty interrupt masked
						      */
			volatile uint32_t TXOIM : 1; /*!< [1..1] Transmit FIFO
							overflow interrupt mask
						      */
			volatile uint32_t RXUIM : 1; /*!< [2..2] Receive FIFO
							underflow interrupt
							masked */
			volatile uint32_t RXOIM : 1; /*!< [3..3] Receive FIFO
							overflow interrupt
							masked */
			volatile uint32_t RXFIM : 1; /*!< [4..4] Receive FIFO
							full interrupt masked */
			volatile uint32_t FSEIM : 1; /*!< [5..5] FIFO size error
							interrupt mask */
			uint32_t : 3;
			volatile uint32_t USSIM : 1; /*!< [9..9] User-mode error
							interrupt mask */
			volatile uint32_t TFSIM : 1; /*!< [10..10] Transmit
							finish interrupt mask */
			uint32_t : 21;
		} IMR_b;
	};

	union {
		volatile uint32_t ISR; /*!< (@ 0x00000030) INTERRUPT STATUS
					  REGISTER */

		struct {
			volatile const uint32_t TXEIS : 1; /*!< [0..0] Transmit
							      FIFO empty
							      interrupt status
							      after masking */
			volatile const uint32_t TXOIS : 1; /*!< [1..1] Transmit
							      FIFO overflow
							      interrupt status
							      after masking */
			volatile const uint32_t RXUIS : 1; /*!< [2..2] Receive
							      FIFO underflow
							      interrupt status
							      after masking */
			volatile const uint32_t RXOIS : 1; /*!< [3..3] Receive
							      FIFO overflow
							      interrupt status
							      after masking */
			volatile const uint32_t RXFIS : 1; /*!< [4..4] Receive
							      FIFO full
							      interrupt status
							      after masking */
			volatile const uint32_t FSEIS : 1; /*!< [5..5] FIFO size
							      error interrupt
							      status after
							      masking */
			uint32_t : 3;
			volatile const uint32_t USEIS : 1; /*!< [9..9] User mode
							      error status after
							      masking */
			volatile const uint32_t TFSIS : 1; /*!< [10..10]
							      Transmit finish
							      status after
							      masking */
			uint32_t : 21;
		} ISR_b;
	};

	union {
		volatile uint32_t RISR; /*!< (@ 0x00000034) RAW INTERRUPT STATUS
					   REGISTER */

		struct {
			volatile const uint32_t TXEIR : 1; /*!< [0..0] Transmit
							      Fifo Empty
							      Interrupt Status
							      Before Masking */
			volatile const uint32_t TXOIR : 1; /*!< [1..1] Transmit
							      Fifo Overflow
							      Interrupt Status
							      Before Masking */
			volatile const uint32_t RXUIR : 1; /*!< [2..2] Receive
							      Fifo Underflow
							      Interrupt Status
							      Before Masking */
			volatile const uint32_t RXOIR : 1; /*!< [3..3] Receive
							      Fifo Overflow
							      Interrupt Status
							      Before Masking */
			volatile const uint32_t RXFIR : 1; /*!< [4..4] Receive
							      Fifo Full
							      Interrupt Status
							      Before Masking */
			volatile const uint32_t FSEIR : 1; /*!< [5..5] FIFO Size
							      Error Interrupt
							      Status Before
							      Masking */
			uint32_t : 3;
			volatile const uint32_t USEIR : 1; /*!< [9..9] User_mode
							      Error Status
							      Interrupt Status
							      Before Masking */
			volatile const uint32_t TFSIR : 1; /*!< [10..10]
							      Transmit Finish
							      Status Interrupt
							      Status Before
							      Masking */
			uint32_t : 21;
		} RISR_b;
	};
	volatile uint32_t TXOICR; /*!< (@ 0x00000038) TRANSMIT FIFO OVERFLOW
				     INTERRUPT CLEAR REGISTER            */
	volatile uint32_t RXOICR; /*!< (@ 0x0000003C) RECEIVE FIFO OVERFLOW
				     INTERRUPT CLEAR REGISTER             */
	volatile uint32_t RXUICR; /*!< (@ 0x00000040) RECEIVE FIFO UNDERFLOW
				     INTERRUPT CLEAR REGISTER            */
	volatile uint32_t MSTICR; /*!< (@ 0x00000044) MASTER ERROR INTERRUPT
				     CLEAR REGISTER                      */
	volatile uint32_t ICR; /*!< (@ 0x00000048) INTERRUPT CLEAR REGISTER */
	volatile const uint32_t RESERVED1[5];
	union {
		volatile uint8_t BYTE;
		volatile uint16_t HALF;
		volatile uint32_t WORD;
	} DR;
	volatile const uint32_t RESERVED2[44];
	volatile uint32_t FBAUD;
	union {
		volatile uint32_t USERLENGTH; /*!< (@ 0x00000118) DECIDES BYTE
						 NUMBERS OF COMMAND, ADDRESS AND
							       DATA PHASE TO
						 TRANSMIT */

		struct {
			volatile uint32_t RDDUMMLEN : 12; /*!< [11..0] Indicate
							      Delay Cycles For
							      Receiving Data In
							      User Mode */
			volatile uint32_t CMDLEN : 2; /*!< [13..12] Indicate
							 Number Of Bytes In
							 Command Phase In User
							 Mode           */
			uint32_t : 2;
			volatile uint32_t ADDRLEN : 4; /*!< [19..16] Indicate
							  Number Of Bytes In
							  Address Phase In User
							  Mode           */
			uint32_t : 12;
		} USERLENGTH_b;
	};
	volatile const uint32_t RESERVED3[3];

	union {
		volatile uint32_t FLUSH; /*!< (@ 0x00000128) FLUSH FIFO REGISTER
					  */

		struct {
			volatile uint32_t ALL : 1; /*!< [0..0] Clear All Data In
						     All FIFO (Include TX_FIFO,
						     RX_FIFO and ST_FIFO) */
			volatile uint32_t DRFIFO : 1; /*!< [1..1] Clear All Data
							 in the TX FIFO and RX
							 FIFO */
			volatile uint32_t STFIFO : 1; /*!< [2..2] Clear All Data
							 in the ST_FIFO */
			uint32_t : 29;
		} FLUSH_b;
	};
	volatile const uint32_t RESERVED4;

	union {
		volatile uint32_t TXNDF; /*!< (@ 0x00000130) A NUMBER OF DATA
					    FRAMES OF TX DATA IN USER MODE */

		struct {
			volatile uint32_t NUM : 24; /*!< [23..0] Number of Rx
						       Data Frames */
			uint32_t : 8;
		} TXNDF_b;
	};
} SPIC_Type; /*!< Size = 308 (0x134) */

/**
 * @brief GPIO Controller (GPIO)
 */

typedef struct { /*!< (@ 0x40090000) GPIO Structure */

	union {
		volatile uint32_t GCR[132]; /*!< (@ 0x00000000) CONTROL REGISTER
					     */

		struct {
			volatile uint32_t DIR : 1; /*!< [0..0] Direction */
			volatile uint32_t INDETEN : 1; /*!< [1..1] Input
							  Detection Enable */
			volatile uint32_t INVOLMD : 1; /*!< [2..2] Input Voltage
							  Mode */
			const volatile uint32_t PINSTS : 1; /*!< [3..3] Status
							       of GPIO Pin */
			uint32_t : 4;
			volatile uint32_t MFCTRL : 3; /*!< [10..8] Multiple
							 Function Control */
			volatile uint32_t OUTDRV : 1; /*!< [11..11] Driving
							 Current Selection */
			volatile uint32_t SLEWRATE : 1; /*!< [12..12] Slew Rate
							   Selection */
			volatile uint32_t PULLDWEN : 1; /*!< [13..13] Internal
							   Pull-Down Resistor
							   Enable */
			volatile uint32_t PULLUPEN : 1; /*!< [14..14] Internal
							   Pull-Up Resistor
							   Enable */
			volatile uint32_t SCHEN : 1; /*!< [15..15]
							Schmitter-Trigger Enable
						      */
			volatile uint32_t OUTMD : 1; /*!< [16..16] Output Mode
							Selection */
			volatile uint32_t OUTCTRL : 1; /*!< [17..17] Output
							  Control */
			uint32_t : 6;
			volatile uint32_t INTCTRL : 3; /*!< [26..24] Type of
							  GPIO Interrupt */
			uint32_t : 1;
			volatile uint32_t INTEN : 1; /*!< [28..28] Interrupt
							Enable */
			uint32_t : 2;
			volatile uint32_t INTSTS : 1; /*!< [31..31] Interrupt
							 Status */
		} GCR_b[132];
	};
} GPIO_Type; /*!< Size = 528 (0x210) */

/**
 * @brief UART Controller (UART)
 */

typedef struct { /*!< (@ 0x40010100) UART Structure */
	union {
		union {
			volatile const uint32_t RBR; /*!< (@ 0x00000000) RECEIVE
							BUFFER REGISTER */

			struct {
				volatile const uint32_t DATA : 8; /*!< [7..0]
								     Receive
								     Buffer */
				uint32_t : 24;
			} RBR_b;
		};

		union {
			volatile uint32_t THR; /*!< (@ 0x00000000) TRANSMIT
						  HOLDING REGISTER */

			struct {
				volatile uint32_t DATA : 8; /*!< [7..0] Transmit
							       Holding */
				uint32_t : 24;
			} THR_b;
		};

		union {
			volatile uint32_t DLL; /*!< (@ 0x00000000) DIVISOR LATCH
						  LOW REGISTER */

			struct {
				volatile uint32_t DIVL : 8; /*!< [7..0] Divisor
							       Latch Low Byte */
				uint32_t : 24;
			} DLL_b;
		};
	};

	union {
		union {
			volatile uint32_t DLH; /*!< (@ 0x00000004) DIVISOR LATCH
						  HIGH REGISTER */

			struct {
				volatile uint32_t DIVH : 8; /*!< [7..0] Divisor
							       Latch High Byte
							     */
				uint32_t : 24;
			} DLH_b;
		};

		union {
			volatile uint32_t IER; /*!< (@ 0x00000004) INTRRRUPT
						  ENABLE REGISTER */

			struct {
				volatile uint32_t ERBFI : 1; /*!< [0..0] Enable
								Received Data
								Available
								Interrupt */
				volatile uint32_t ETBEI : 1; /*!< [1..1] Enable
								Transmit Holding
								Register Empty
								Interrupt */
				volatile uint32_t ELSI : 1; /*!< [2..2] Enable
							       Receiver Line
							       Status Interrupt
							     */
				uint32_t : 4;
				volatile uint32_t PTIME : 1; /*!< [7..7]
								Programmable
								THRE Interrupt
								Mode Enable */
				uint32_t : 24;
			} IER_b;
		};
	};

	union {
		union {
			volatile uint32_t IIR; /*!< (@ 0x00000008) INTERRUPT
						  IDENTIFICATION */

			struct {
				volatile uint32_t IID : 4; /*!< [3..0] Interrupt
							      ID */
				uint32_t : 2;
				volatile uint32_t FIFOSE : 2; /*!< [7..6] FIFOs
								 Enabled */
				uint32_t : 24;
			} IIR_b;
		};

		union {
			volatile uint32_t FCR; /*!< (@ 0x00000008) FIFO CONTROL
						  REGISTER */

			struct {
				volatile uint32_t FIFOE : 1; /*!< [0..0] FIFO
								Enabled */
				volatile uint32_t RFIFOR : 1; /*!< [1..1] Rx
								 FIFO Reset */
				volatile uint32_t XFIFOR : 1; /*!< [2..2] Tx
								 FIFO Reset */
				uint32_t : 1;
				volatile uint32_t TXTRILEV : 2; /*!< [5..4] TX
								   Empty Trigger
								   Level */
				volatile uint32_t RXTRILEV : 2; /*!< [7..6] Rx
								   Trigger Level
								 */
				uint32_t : 24;
			} FCR_b;
		};
	};

	union {
		volatile uint32_t LCR; /*!< (@ 0x0000000C) LINE CONTROL REGISTER
					*/

		struct {
			volatile uint32_t DLS : 2; /*!< [1..0] Data Length
						      Select */
			volatile uint32_t STOP : 1; /*!< [2..2] Number of Stop
						       Bits */
			volatile uint32_t PEN : 1; /*!< [3..3] Parity Enable */
			volatile uint32_t EPS : 1; /*!< [4..4] Even Parity
						      Select */
			volatile uint32_t STP : 1; /*!< [5..5] Stick Parity */
			volatile uint32_t BC : 1; /*!< [6..6] Break Control Bit
						   */
			volatile uint32_t DLAB : 1; /*!< [7..7] Divisor Latch
						       Access Bit */
			uint32_t : 24;
		} LCR_b;
	};
	volatile const uint32_t RESERVED;

	union {
		volatile const uint32_t LSR; /*!< (@ 0x00000014) LINE STATUS
						REGISTER */

		struct {
			volatile const uint32_t DR : 1; /*!< [0..0] Data Ready
							   Bit */
			volatile const uint32_t OE : 1; /*!< [1..1] Overrun
							   Error bit */
			volatile const uint32_t PE : 1; /*!< [2..2] Parity Error
							   bit */
			volatile const uint32_t FE : 1; /*!< [3..3] Framing
							   Error bit */
			volatile const uint32_t BI : 1; /*!< [4..4] Break
							   Interrupt bit */
			volatile const uint32_t THRE : 1; /*!< [5..5] Transmit
							     Holding Register
							     Empty bit */
			volatile const uint32_t TEMT : 1; /*!< [6..6]
							     Transmitter Empty
							     Bit */
			volatile const uint32_t RFE : 1; /*!< [7..7] Receiver
							    FIFO Error */
			uint32_t : 24;
		} LSR_b;
	};
	volatile const uint32_t RESERVED1[25];

	union {
		volatile const uint32_t USR; /*!< (@ 0x0000007C) UART Status
						Register */

		struct {
			volatile const uint32_t BUSY : 1; /*!< [0..0] UART Busy
							   */
			volatile const uint32_t TFNF : 1; /*!< [1..1] Transmit
							     FIFO Not Full */
			volatile const uint32_t TFE : 1; /*!< [2..2] Transmit
							    FIFO Empty */
			volatile const uint32_t RFNE : 1; /*!< [3..3] Receive
							     FIFO Not Empty */
			volatile const uint32_t RFF : 1; /*!< [4..4] Receive
							    FIFO Full */
			uint32_t : 27;
		} USR_b;
	};
	volatile const uint32_t TFL; /*!< (@ 0x00000080) UART TRANSMIT FIFO
					LEVEL */
	volatile const uint32_t RFL; /*!< (@ 0x00000084) UART RECEIVE FIFO LEVEL
				      */

	union {
		volatile uint32_t SRR; /*!< (@ 0x00000088) UART SOFTWARE RESET
					  REGISTER */

		struct {
			volatile uint32_t UR : 1; /*!< [0..0] UART Reset */
			volatile uint32_t RFR : 1; /*!< [1..1] RCVR FIFO Reset
						    */
			volatile uint32_t XFR : 1; /*!< [2..2] XMIT FIFO Reset
						    */
			uint32_t : 29;
		} SRR_b;
	};
} UART_Type; /*!< Size = 140 (0x8c) */

/**
 * @brief Keyboard Matrix Controller (KBM)
 */

typedef struct { /*!< (@ 0x40010000) KBM Structure */

	union {
		volatile uint32_t SCANOUT; /*!< (@ 0x00000000) SCAN OUT CONTROL
					      REGISTER */

		struct {
			volatile uint32_t KSO0 : 1; /*!< [0..0] KSO Output Level
						     */
			volatile uint32_t KSO1 : 1; /*!< [1..1] KSO Output Level
						     */
			volatile uint32_t KSO2 : 1; /*!< [2..2] KSO Output Level
						     */
			volatile uint32_t KSO3 : 1; /*!< [3..3] KSO Output Level
						     */
			volatile uint32_t KSO4 : 1; /*!< [4..4] KSO Output Level
						     */
			volatile uint32_t KSO5 : 1; /*!< [5..5] KSO Output Level
						     */
			volatile uint32_t KSO6 : 1; /*!< [6..6] KSO Output Level
						     */
			volatile uint32_t KSO7 : 1; /*!< [7..7] KSO Output Level
						     */
			volatile uint32_t KSO8 : 1; /*!< [8..8] KSO Output Level
						     */
			volatile uint32_t KSO9 : 1; /*!< [9..9] KSO Output Level
						     */
			volatile uint32_t KSO10 : 1; /*!< [10..10] KSO Output
							Level */
			volatile uint32_t KSO11 : 1; /*!< [11..11] KSO Output
							Level */
			volatile uint32_t KSO12 : 1; /*!< [12..12] KSO Output
							Level */
			volatile uint32_t KSO13 : 1; /*!< [13..13] KSO Output
							Level */
			volatile uint32_t KSO14 : 1; /*!< [14..14] KSO Output
							Level */
			volatile uint32_t KSO15 : 1; /*!< [15..15] KSO Output
							Level */
			volatile uint32_t KSO16 : 1; /*!< [16..16] KSO Output
							Level */
			volatile uint32_t KSO17 : 1; /*!< [17..17] KSO Output
							Level */
			volatile uint32_t KSO18 : 1; /*!< [18..18] KSO Output
							Level */
			volatile uint32_t KSO19 : 1; /*!< [19..19] KSO Output
							Level */
			uint32_t : 12;
		} SCANOUT_b;
	};

	union {
		volatile const uint32_t SCANIN; /*!< (@ 0x00000004) SACN INPUT
						   STATUS REGISTER */

		struct {
			volatile const uint32_t KSI0 : 1; /*!< [0..0] KSI Input
							     Level */
			volatile const uint32_t KSI1 : 1; /*!< [1..1] KSI Input
							     Level */
			volatile const uint32_t KSI2 : 1; /*!< [2..2] KSI Input
							     Level */
			volatile const uint32_t KSI3 : 1; /*!< [3..3] KSI Input
							     Level */
			volatile const uint32_t KSI4 : 1; /*!< [4..4] KSI Input
							     Level */
			volatile const uint32_t KSI5 : 1; /*!< [5..5] KSI Input
							     Level */
			volatile const uint32_t KSI6 : 1; /*!< [6..6] KSI Input
							     Level */
			volatile const uint32_t KSI7 : 1; /*!< [7..7] KSI Input
							     Level */
			volatile const uint32_t KSI8 : 1; /*!< [8..8] KSI Input
							     Level */
			volatile const uint32_t KSI9 : 1; /*!< [9..9] KSI Input
							     Level */
			uint32_t : 22;
		} SCANIN_b;
	};

	union {
		volatile uint32_t INTEN; /*!< (@ 0x00000008) INTERRUPT ENABLE
					    REGISTER */

		struct {
			volatile uint32_t KSI0 : 1; /*!< [0..0] KSI Interrupt
						       Enable */
			volatile uint32_t KSI1 : 1; /*!< [1..1] KSI Interrupt
						       Enable */
			volatile uint32_t KSI2 : 1; /*!< [2..2] KSI Interrupt
						       Enable */
			volatile uint32_t KSI3 : 1; /*!< [3..3] KSI Interrupt
						       Enable */
			volatile uint32_t KSI4 : 1; /*!< [4..4] KSI Interrupt
						       Enable */
			volatile uint32_t KSI5 : 1; /*!< [5..5] KSI Interrupt
						       Enable */
			volatile uint32_t KSI6 : 1; /*!< [6..6] KSI Interrupt
						       Enable */
			volatile uint32_t KSI7 : 1; /*!< [7..7] KSI Interrupt
						       Enable */
			volatile uint32_t KSI8 : 1; /*!< [8..8] KSI Interrupt
						       Enable */
			volatile uint32_t KSI9 : 1; /*!< [9..9] KSI Interrupt
						       Enable */
			uint32_t : 22;
		} INTEN_b;
	};

	union {
		volatile uint32_t CTRL; /*!< (@ 0x0000000C) CONTROL REGISTER */

		struct {
			volatile uint32_t KSOTYPE : 1; /*!< [0..0] Output Type
							  of KSO pin */
			volatile uint32_t KSI8EN : 1; /*!< [1..1] Enable KSI8 */
			volatile uint32_t KSI9EN : 1; /*!< [2..2] Enable KSI9 */
			uint32_t : 13;
			volatile uint32_t KSO18EN : 1; /*!< [16..16] Enable
							  KSO18 */
			volatile uint32_t KSO19EN : 1; /*!< [17..17] Enable
							  KSO19 */
			volatile uint32_t KSIINTSTS : 1; /*!< [18..18] KSI
							    Interrupt Status */
			uint32_t : 13;
		} CTRL_b;
	};
} KBM_Type; /*!< Size = 16 (0x10) */

#define UART_BASE 0x40010100UL
#define SPIC_BASE 0x40010200UL
#define GPIO_BASE 0x40090000UL
#define IOPAD_BASE 0x40091000UL
#define SYSTEM_BASE 0x40020000UL
#define KBM_BASE 0x40010000UL
#define SLWTMR0_BASE 0x4000C200UL

#define SPIC ((SPIC_Type *)SPIC_BASE)
#define GPIO ((GPIO_Type *)GPIO_BASE)
#define IOPAD ((IOPAD_Type *)IOPAD_BASE)
#define SYSTEM ((SYSTEM_Type *)SYSTEM_BASE)
#define KBM ((KBM_Type *)KBM_BASE)
#define UART ((UART_Type *)UART_BASE)
#define SLWTMR0 ((SLWTMR_Type *)SLWTMR0_BASE)

#define UART_USR_TFNF_Msk (0x2UL)
#define UART_USR_TFE_Msk (0x4UL)
#define UART_LSR_THRE_Msk (0x20UL)
#define SYSTEM_PERICLKPWR1_SLWTMR0CLKPWR_Msk (0x200000UL)

/* KBM pins */
#define KBM_KSO_0_PIN 41
#define KBM_KSO_1_PIN 42
#define KBM_KSO_2_PIN 43
#define KBM_KSO_3_PIN 44
#define KBM_KSO_4_PIN 45
#define KBM_KSO_5_PIN 46
#define KBM_KSO_6_PIN 47
#define KBM_KSO_7_PIN 48
#define KBM_KSO_8_PIN 49
#define KBM_KSO_9_PIN 50

#define KBM_KSI_0_PIN 64
#define KBM_KSI_1_PIN 65
#define KBM_KSI_2_PIN 66
#define KBM_KSI_3_PIN 67
#define KBM_KSI_4_PIN 68
#define KBM_KSI_5_PIN 69
#define KBM_KSI_6_PIN 70
#define KBM_KSI_7_PIN 71

/**
 * @brief System Controller (SYSTEM)
 */

typedef struct { /*!< (@ 0x40020000) SYSTEM Structure */

	union {
		union {
			volatile uint32_t I3CCLK; /*!< (@ 0x00000000) I3C CLOCK
						     REGISTER */

			struct {
				volatile uint32_t I3C0DIV : 2; /*!< [1..0] I3C0
								  Clock Divider
								*/
				volatile uint32_t I3C1DIV : 2; /*!< [3..2] I3C1
								  Clock Divider
								*/
				uint32_t : 28;
			} I3CCLK_b;
		};

		union {
			volatile uint32_t TMRRST; /*!< (@ 0x00000000) TIMER
						     RESET REGISTER */

			struct {
				uint32_t : 4;
				volatile uint32_t TMR0RST : 1; /*!< [4..4] Reset
								  Timer0,
								  low-active */
				volatile uint32_t TMR1RST : 1; /*!< [5..5] Reset
								  Timer1,
								  low-active */
				volatile uint32_t TMR2RST : 1; /*!< [6..6] Reset
								  Timer2,
								  low-active */
				volatile uint32_t TMR3RST : 1; /*!< [7..7] Reset
								  Timer3,
								  low-active */
				volatile uint32_t TMR4RST : 1; /*!< [8..8] Reset
								  Timer4,
								  low-active */
				volatile uint32_t TMR5RST : 1; /*!< [9..9] Reset
								  Timer5,
								  low-active */
				uint32_t : 22;
			} TMRRST_b;
		};
	};

	union {
		volatile uint32_t I2CCLK; /*!< (@ 0x00000004) I2C CLOCK REGISTER
					   */

		struct {
			volatile uint32_t I2C0CLKPWR : 1; /*!< [0..0] I2C0 Clock
							     Power */
			volatile uint32_t I2C0CLKSRC : 1; /*!< [1..1] I2C0 Clock
							     Source */
			volatile uint32_t I2C0CLKDIV : 2; /*!< [3..2] I2C0 Clock
							     Divider */
			volatile uint32_t I2C1CLKPWR : 1; /*!< [4..4] I2C1 Clock
							     Power */
			volatile uint32_t I2C1CLKSRC : 1; /*!< [5..5] I2C1 Clock
							     Source */
			volatile uint32_t I2C1CLKDIV : 2; /*!< [7..6] I2C1 Clock
							     Divider */
			volatile uint32_t I2C2CLKPWR : 1; /*!< [8..8] I2C2 Clock
							     Power */
			volatile uint32_t I2C2CLKSRC : 1; /*!< [9..9] I2C2 Clock
							     Source */
			volatile uint32_t I2C2CLKDIV : 2; /*!< [11..10] I2C2
							     Clock Divider */
			volatile uint32_t I2C3CLKPWR : 1; /*!< [12..12] I2C3
							     Clock Power */
			volatile uint32_t I2C3CLKSRC : 1; /*!< [13..13] I2C3
							     Clock Source */
			volatile uint32_t I2C3CLKDIV : 2; /*!< [15..14] I2C3
							     Clock Divider */
			volatile uint32_t I2C4CLKPWR : 1; /*!< [16..16] I2C4
							     Clock Power */
			volatile uint32_t I2C4CLKSRC : 1; /*!< [17..17] I2C4
							     Clock Source */
			volatile uint32_t I2C4CLKDIV : 2; /*!< [19..18] I2C4
							     Clock Divider */
			volatile uint32_t I2C5CLKPWR : 1; /*!< [20..20] I2C5
							     Clock Power */
			volatile uint32_t I2C5CLKSRC : 1; /*!< [21..21] I2C5
							     Clock Source */
			volatile uint32_t I2C5CLKDIV : 2; /*!< [23..22] I2C5
							     Clock Divider */
			volatile uint32_t I2C6CLKPWR : 1; /*!< [24..24] I2C6
							     Clock Power */
			volatile uint32_t I2C6CLKSRC : 1; /*!< [25..25] I2C6
							     Clock Source */
			volatile uint32_t I2C6CLKDIV : 2; /*!< [27..26] I2C6
							     Clock Divider */
			volatile uint32_t I2C7CLKPWR : 1; /*!< [28..28] I2C7
							     Clock Power */
			volatile uint32_t I2C7CLKSRC : 1; /*!< [29..29] I2C7
							     Clock Source */
			volatile uint32_t I2C7CLKDIV : 2; /*!< [31..30] I2C7
							     Clock Divider */
		} I2CCLK_b;
	};

	union {
		volatile uint32_t TMRCLK; /*!< (@ 0x00000008) TIMER32 CLOCK
					     REGISTER */

		struct {
			volatile uint32_t TMR0DIV : 4; /*!< [3..0] Timer0 Clock
							  Divider */
			volatile uint32_t TMR1DIV : 4; /*!< [7..4] Timer1 Clock
							  Divider */
			volatile uint32_t TMR2DIV : 4; /*!< [11..8] Timer2 Clock
							  Divider */
			volatile uint32_t TMR3DIV : 4; /*!< [15..12] Timer3
							  Clock Divider */
			volatile uint32_t TMR4DIV : 4; /*!< [19..16] Timer4
							  Clock Divider */
			volatile uint32_t TMR5DIV : 4; /*!< [23..20] Timer5
							  Clock Divider */
			volatile uint32_t TMR0PAUSE : 1; /*!< [24..24] Timer0
							    Clock Pause */
			volatile uint32_t TMR1PAUSE : 1; /*!< [25..25] Timer1
							    Clock Pause */
			volatile uint32_t TMR2PAUSE : 1; /*!< [26..26] Timer2
							    Clock Pause */
			volatile uint32_t TMR3PAUSE : 1; /*!< [27..27] Timer3
							    Clock Pause */
			volatile uint32_t TMR4PAUSE : 1; /*!< [28..28] Timer4
							    Clock Pause */
			volatile uint32_t TMR5PAUSE : 1; /*!< [29..29] Timer5
							    Clock Pause */
			uint32_t : 2;
		} TMRCLK_b;
	};

	union {
		volatile uint32_t PERICLKPWR0; /*!< (@ 0x0000000C) PERIPHERAL
						  CLOCK POWER REGISTER #0 */

		struct {
			volatile uint32_t GPIOCLKPWR : 1; /*!< [0..0] GPIO Clock
							     Power */
			volatile uint32_t TACHO0CLKPWR : 1; /*!< [1..1]
							       Tachometer0 Clock
							       Power */
			volatile uint32_t TACHO1CLKPWR : 1; /*!< [2..2]
							       Tachometer1 Clock
							       Power */
			volatile uint32_t TACHO2CLKPWR : 1; /*!< [3..3]
							       Tachometer2 Clock
							       Power */
			volatile uint32_t TACHO3CLKPWR : 1; /*!< [4..4]
							       Tachometer3 Clock
							       Power */
			volatile uint32_t PS2CLKPWR : 1; /*!< [5..5] PS2 Clock
							    Power */
			volatile uint32_t KBMCLKPWR : 1; /*!< [6..6] KBM Clock
							    Power */
			volatile uint32_t PECICLKPWR : 1; /*!< [7..7] PECI Clock
							     Power */
			volatile uint32_t PL0CLKPWR : 1; /*!< [8..8] PWMLED0
							    Clock Power */
			volatile uint32_t PL1CLKPWR : 1; /*!< [9..9] PWMLED1
							    Clock Power */
			volatile uint32_t PWM0CLKPWR : 1; /*!< [10..10] PWM0
							     Clock Power */
			volatile uint32_t PWM1CLKPWR : 1; /*!< [11..11] PWM1
							     Clock Power */
			volatile uint32_t PWM2CLKPWR : 1; /*!< [12..12] PWM2
							     Clock Power */
			volatile uint32_t PWM3CLKPWR : 1; /*!< [13..13] PWM3
							     Clock Power */
			volatile uint32_t PWM4CLKPWR : 1; /*!< [14..14] PWM4
							     Clock Power */
			volatile uint32_t PWM5CLKPWR : 1; /*!< [15..15] PWM5
							     Clock Power */
			volatile uint32_t PWM6CLKPWR : 1; /*!< [16..16] PWM6
							     Clock Power */
			volatile uint32_t PWM7CLKPWR : 1; /*!< [17..17] PWM7
							     Clock Power */
			volatile uint32_t PWM8CLKPWR : 1; /*!< [18..18] PWM8
							     Clock Power */
			volatile uint32_t PWM9CLKPWR : 1; /*!< [19..19] PWM9
							     Clock Power */
			volatile uint32_t PWM10CLKPWR : 1; /*!< [20..20] PWM10
							      Clock Power */
			volatile uint32_t PWM11CLKPWR : 1; /*!< [21..21] PWM11
							      Clock Power */
			volatile uint32_t ESPICLKPWR : 1; /*!< [22..22] eSPI
							     Clock Power */
			volatile uint32_t KBCCLKPWR : 1; /*!< [23..23] KBC Clock
							    Power */
			volatile uint32_t ACPICLKPWR : 1; /*!< [24..24] ACPI
							     Clock Power */
			volatile uint32_t PMPORT0CLKPWR : 1; /*!< [25..25]
								PMPORT0 Clock
								Power */
			volatile uint32_t PMPORT1CLKPWR : 1; /*!< [26..26]
								PMPORT1 Clock
								Power */
			volatile uint32_t PMPORT2CLKPWR : 1; /*!< [27..27]
								PMPORT2 Clock
								Power */
			volatile uint32_t PMPORT3CLKPWR : 1; /*!< [28..28]
								PMPORT3 Clock
								Power */
			volatile uint32_t P80CLKPWR : 1; /*!< [29..29] Port80
							    Clock Power */
			volatile uint32_t EMI0CLKPWR : 1; /*!< [30..30] EMI0
							     Clock Power */
			volatile uint32_t EMI1CLKPWR : 1; /*!< [31..31] EMI1
							     Clock Power */
		} PERICLKPWR0_b;
	};

	union {
		volatile uint32_t UARTCLK; /*!< (@ 0x00000010) UART CLOCK
					      REGISTER */

		struct {
			volatile uint32_t PWR : 1; /*!< [0..0] UART Clock Power
						    */
			volatile uint32_t SRC : 1; /*!< [1..1] UART Clock Source
						    */
			volatile uint32_t DIV : 2; /*!< [3..2] UART Clock
						      Divider */
			uint32_t : 28;
		} UARTCLK_b;
	};

	union {
		volatile uint32_t SYSCLK; /*!< (@ 0x00000014) SYSTEM CLOCK
					     REGISTER */

		struct {
			uint32_t : 1;
			volatile uint32_t SRC : 1; /*!< [1..1] System Clock
						      Source */
			volatile uint32_t DIV : 1; /*!< [2..2] System Clock
						      Divider */
			uint32_t : 29;
		} SYSCLK_b;
	};

	union {
		volatile uint32_t ADCCLK; /*!< (@ 0x00000018) ADC CLOCK REGISTER
					   */

		struct {
			volatile uint32_t PWR : 1; /*!< [0..0] ADC Clock Power
						    */
			volatile uint32_t SRC : 1; /*!< [1..1] ADC Clock Source
						    */
			volatile uint32_t DIV : 3; /*!< [4..2] ADC Clock Divider
						    */
			uint32_t : 27;
		} ADCCLK_b;
	};

	union {
		volatile uint32_t PERICLKPWR1; /*!< (@ 0x0000001C) PERIPHERAL
						  CLOCK POWER REGISTER #1 */

		struct {
			volatile uint32_t EMI2CLKPWR : 1; /*!< [0..0] EMI2 Clock
							     Power */
			volatile uint32_t EMI3CLKPWR : 1; /*!< [1..1] EMI3 Clock
							     Power */
			volatile uint32_t EMI4CLKPWR : 1; /*!< [2..2] EMI4 Clock
							     Power */
			volatile uint32_t EMI5CLKPWR : 1; /*!< [3..3] EMI5 Clock
							     Power */
			volatile uint32_t EMI6CLKPWR : 1; /*!< [4..4] EMI6 Clock
							     Power */
			volatile uint32_t EMI7CLKPWR : 1; /*!< [5..5] EMI7 Clock
							     Power */
			uint32_t : 3;
			volatile uint32_t I3C0CLKPWR : 1; /*!< [9..9] I3C0 Clock
							     Power */
			volatile uint32_t I3C1CLKPWR : 1; /*!< [10..10] I3C1
							     Clock Power */
			volatile uint32_t I2CAUTOCLKPWR : 1; /*!< [11..11] I2C
								Auto-Power
								Circuit Clock
								Power */
			volatile uint32_t MCCLKPWR : 1; /*!< [12..12] Monotonic
							   Counter Clock Power
							 */
			volatile uint32_t TMR0CLKPWR : 1; /*!< [13..13] Timer0
							     Clock Power */
			volatile uint32_t TMR1CLKPWR : 1; /*!< [14..14] Timer1
							     Clock Power */
			volatile uint32_t TMR2CLKPWR : 1; /*!< [15..15] Timer2
							     Clock Power */
			volatile uint32_t TMR3CLKPWR : 1; /*!< [16..16] Timer3
							     Clock Power */
			volatile uint32_t TMR4CLKPWR : 1; /*!< [17..17] Timer4
							     Clock Power */
			volatile uint32_t TMR5CLKPWR : 1; /*!< [18..18] Timer5
							     Clock Power */
			volatile uint32_t RTMRCLKPWR : 1; /*!< [19..19] RTOS
							     Timer Clock Power
							   */
			volatile uint32_t SLWTMR0CLKPWR : 1; /*!< [20..20] Slow
								Timer 0 Clock
								Power */
			volatile uint32_t SLWTMR1CLKPWR : 1; /*!< [21..21] Slow
								Timer 1 Clock
								Power */
			uint32_t : 10;
		} PERICLKPWR1_b;
	};
	volatile const uint32_t RESERVED[24];

	union {
		volatile uint32_t SLPCTRL; /*!< (@ 0x00000080) SYSTEM SLEEP
					      CONTROL REGISTER */

		struct {
			uint32_t : 1;
			volatile uint32_t SLPMDSEL : 1; /*!< [1..1] Sleep Mode
							   Selection */
			volatile uint32_t ESPIWKEN : 1; /*!< [2..2] eSPI Wake-up
							   Enable */
			volatile uint32_t PS2WKEN : 1; /*!< [3..3] PS2 Wake-up
							  Enable */
			volatile uint32_t I2CWKEN : 1; /*!< [4..4] I2C Wake-up
							  Enable */
			volatile uint32_t GPIOWKEN : 1; /*!< [5..5] GPIO Wake-up
							   Enable */
			uint32_t : 26;
		} SLPCTRL_b;
	};
	volatile const uint32_t RESERVED1[7];

	union {
		volatile uint32_t VIVOCTRL; /*!< (@ 0x000000A0) VINVOUT CONTROL
					       REGISTER */

		struct {
			volatile uint32_t VIN0MD : 1; /*!< [0..0] VIN0 Mode
							 Selection */
			volatile uint32_t VIN1MD : 1; /*!< [1..1] VIN1 Mode
							 Selection */
			volatile uint32_t VIN2MD : 1; /*!< [2..2] VIN2 Mode
							 Selection */
			volatile uint32_t VIN3MD : 1; /*!< [3..3] VIN3 Mode
							 Selection */
			volatile uint32_t VIN4MD : 1; /*!< [4..4] VIN4 Mode
							 Selection */
			volatile uint32_t VIN5MD : 1; /*!< [5..5] VI5 Mode
							 Selection */
			volatile const uint32_t VIN0STS : 1; /*!< [6..6] VIN0
								Status */
			volatile const uint32_t VIN1STS : 1; /*!< [7..7] VIN1
								Status */
			volatile const uint32_t VIN2STS : 1; /*!< [8..8] VIN2
								Status */
			volatile const uint32_t VIN3STS : 1; /*!< [9..9] VIN3
								Status */
			volatile const uint32_t VIN4STS : 1; /*!< [10..10] VIN4
								Status */
			volatile const uint32_t VIN5STS : 1; /*!< [11..11] VIN5
								Status */
			volatile uint32_t VIN0POL : 1; /*!< [12..12] VIN0
							  Polarity */
			volatile uint32_t VIN1POL : 1; /*!< [13..13] VIN1
							  Polarity */
			volatile uint32_t VIN2POL : 1; /*!< [14..14] VIN2
							  Polarity */
			volatile uint32_t VIN3POL : 1; /*!< [15..15] VIN3
							  Polarity */
			volatile uint32_t VIN4POL : 1; /*!< [16..16] VIN4
							  Polarity */
			volatile uint32_t VIN5POL : 1; /*!< [17..17] VIN5
							  Polarity */
			uint32_t : 12;
			volatile uint32_t REGWREN : 1; /*!< [30..30] Register
							  Write Enable */
			volatile uint32_t VOUTMD : 1; /*!< [31..31] VOUT Mode
							 Selection */
		} VIVOCTRL_b;
	};

	union {
		volatile uint32_t LDOCTRL; /*!< (@ 0x000000A4) LDO CONTROL
					      REGISTER */

		struct {
			uint32_t : 3;
			volatile uint32_t LDO2EN : 1; /*!< [3..3] LDO2 Power
							 Enable */
			uint32_t : 3;
			volatile uint32_t LDO3EN : 1; /*!< [7..7] LDO3 Power
							 Enable */
			uint32_t : 24;
		} LDOCTRL_b;
	};

	union {
		volatile uint32_t RC25MCTRL; /*!< (@ 0x000000A8) RC25M CONTROL
						REGISTER */

		struct {
			volatile uint32_t EN : 1; /*!< [0..0] RC25M Power Enable
						   */
			volatile uint32_t CALCURR : 7; /*!< [7..1] RC25M
							  Calibration
							  Coefficient */
			uint32_t : 24;
		} RC25MCTRL_b;
	};

	union {
		volatile uint32_t PLLCTRL; /*!< (@ 0x000000AC) PLL CONTROL
					      REGISTER */

		struct {
			volatile uint32_t EN : 1; /*!< [0..0] PLL Power Enable
						   */
			uint32_t : 18;
			volatile uint32_t RDY : 1; /*!< [19..19] PLL Ready and
						      Stable */
			uint32_t : 12;
		} PLLCTRL_b;
	};
	volatile const uint32_t RESERVED2[12];

	union {
		volatile uint32_t RC32KCTRL; /*!< (@ 0x000000E0) RC32K CONTROL
						REGISTER */

		struct {
			volatile uint32_t EN : 1; /*!< [0..0] RC32K Power Enable
						   */
			volatile uint32_t CAL : 6; /*!< [6..1] RC32K Calibration
						      Coefficient */
			uint32_t : 25;
		} RC32KCTRL_b;
	};
	volatile const uint32_t RESERVED3;

	union {
		volatile uint32_t PERICLKPWR2; /*!< (@ 0x000000E8) PERIPHERAL
						  CLOCK POWER REGISTER #2 */

		struct {
			volatile uint32_t RTCCLKPWR : 1; /*!< [0..0] RTC Clock
							    Power */
			volatile uint32_t WDTCLKPWR : 1; /*!< [1..1] WDT Clock
							    Power */
			volatile uint32_t PWRBTNCLKPWR : 1; /*!< [2..2] Power
							       Button over WDT
							       Clock Power */
			uint32_t : 27;
			volatile uint32_t RC32KSRC : 2; /*!< [31..30] RC32K
							   Clock Source
							   Selection */
		} PERICLKPWR2_b;
	};
} SYSTEM_Type; /*!< Size = 236 (0xec) */

/* =========================================================  CTRL0
 * ========================================================= */
#define SPIC_CTRL0_SCPH_Pos \
	(6UL) /*!< SCPH (Bit 6)                                          */
#define SPIC_CTRL0_SCPH_Msk \
	(0x40UL) /*!< SCPH (Bitfield-Mask: 0x01)                            */
#define SPIC_CTRL0_SCPOL_Pos \
	(7UL) /*!< SCPOL (Bit 7)                                         */
#define SPIC_CTRL0_SCPOL_Msk \
	(0x80UL) /*!< SCPOL (Bitfield-Mask: 0x01)                           */
#define SPIC_CTRL0_TMOD_Pos \
	(8UL) /*!< TMOD (Bit 8)                                          */
#define SPIC_CTRL0_TMOD_Msk (0x300UL) /*!< TMOD (Bitfield-Mask: 0x03) */
#define SPIC_CTRL0_ADDRCH_Pos \
	(16UL) /*!< ADDRCH (Bit 16)                                       */
#define SPIC_CTRL0_ADDRCH_Msk (0x30000UL) /*!< ADDRCH (Bitfield-Mask: 0x03) */
#define SPIC_CTRL0_DATACH_Pos \
	(18UL) /*!< DATACH (Bit 18)                                       */
#define SPIC_CTRL0_DATACH_Msk (0xc0000UL) /*!< DATACH (Bitfield-Mask: 0x03) */
#define SPIC_CTRL0_CMDCH_Pos \
	(20UL) /*!< CMDCH (Bit 20)                                        */
#define SPIC_CTRL0_CMDCH_Msk (0x300000UL) /*!< CMDCH (Bitfield-Mask: 0x03) */
#define SPIC_CTRL0_USERMD_Pos \
	(31UL) /*!< USERMD (Bit 31)                                       */
#define SPIC_CTRL0_USERMD_Msk \
	(0x80000000UL) /*!< USERMD (Bitfield-Mask: 0x01) */
/* =========================================================  RXNDF
 * ========================================================= */
#define SPIC_RXNDF_NUM_Pos \
	(0UL) /*!< NUM (Bit 0)                                           */
#define SPIC_RXNDF_NUM_Msk (0xffffffUL) /*!< NUM (Bitfield-Mask: 0xffffff) */
/* ========================================================  SSIENR
 * ========================================================= */
#define SPIC_SSIENR_SPICEN_Pos \
	(0UL) /*!< SPICEN (Bit 0)                                        */
#define SPIC_SSIENR_SPICEN_Msk \
	(0x1UL) /*!< SPICEN (Bitfield-Mask: 0x01)                          */
#define SPIC_SSIENR_ATCKCMD_Pos \
	(1UL) /*!< ATCKCMD (Bit 1)                                       */
#define SPIC_SSIENR_ATCKCMD_Msk \
	(0x2UL) /*!< ATCKCMD (Bitfield-Mask: 0x01)                         */
/* ==========================================================  SER
 * ========================================================== */
#define SPIC_SER_SEL_Pos \
	(0UL) /*!< SEL (Bit 0)                                           */
#define SPIC_SER_SEL_Msk \
	(0x1UL) /*!< SEL (Bitfield-Mask: 0x01)                             */
/* =========================================================  BAUDR
 * ========================================================= */
#define SPIC_BAUDR_SCKDV_Pos \
	(0UL) /*!< SCKDV (Bit 0)                                         */
#define SPIC_BAUDR_SCKDV_Msk (0xfffUL) /*!< SCKDV (Bitfield-Mask: 0xfff) */
/* ========================================================  TXFTLR
 * ========================================================= */
/* ========================================================  RXFTLR
 * ========================================================= */
/* =========================================================  TXFLR
 * ========================================================= */
/* =========================================================  RXFLR
 * ========================================================= */
/* ==========================================================  SR
 * =========================================================== */
#define SPIC_SR_BUSY_Pos \
	(0UL) /*!< BUSY (Bit 0)                                          */
#define SPIC_SR_BUSY_Msk \
	(0x1UL) /*!< BUSY (Bitfield-Mask: 0x01)                            */
#define SPIC_SR_TFNF_Pos \
	(1UL) /*!< TFNF (Bit 1)                                          */
#define SPIC_SR_TFNF_Msk \
	(0x2UL) /*!< TFNF (Bitfield-Mask: 0x01)                            */
#define SPIC_SR_TFE_Pos \
	(2UL) /*!< TFE (Bit 2)                                           */
#define SPIC_SR_TFE_Msk \
	(0x4UL) /*!< TFE (Bitfield-Mask: 0x01)                             */
#define SPIC_SR_RFNE_Pos \
	(3UL) /*!< RFNE (Bit 3)                                          */
#define SPIC_SR_RFNE_Msk \
	(0x8UL) /*!< RFNE (Bitfield-Mask: 0x01)                            */
#define SPIC_SR_RFF_Pos \
	(4UL) /*!< RFF (Bit 4)                                           */
#define SPIC_SR_RFF_Msk \
	(0x10UL) /*!< RFF (Bitfield-Mask: 0x01)                             */
#define SPIC_SR_TXE_Pos \
	(5UL) /*!< TXE (Bit 5)                                           */
#define SPIC_SR_TXE_Msk \
	(0x20UL) /*!< TXE (Bitfield-Mask: 0x01)                             */
/* ==========================================================  IMR
 * ========================================================== */
#define SPIC_IMR_TXEIM_Pos \
	(0UL) /*!< TXEIM (Bit 0)                                         */
#define SPIC_IMR_TXEIM_Msk \
	(0x1UL) /*!< TXEIM (Bitfield-Mask: 0x01)                           */
#define SPIC_IMR_TXOIM_Pos \
	(1UL) /*!< TXOIM (Bit 1)                                         */
#define SPIC_IMR_TXOIM_Msk \
	(0x2UL) /*!< TXOIM (Bitfield-Mask: 0x01)                           */
#define SPIC_IMR_RXUIM_Pos \
	(2UL) /*!< RXUIM (Bit 2)                                         */
#define SPIC_IMR_RXUIM_Msk \
	(0x4UL) /*!< RXUIM (Bitfield-Mask: 0x01)                           */
#define SPIC_IMR_RXOIM_Pos \
	(3UL) /*!< RXOIM (Bit 3)                                         */
#define SPIC_IMR_RXOIM_Msk \
	(0x8UL) /*!< RXOIM (Bitfield-Mask: 0x01)                           */
#define SPIC_IMR_RXFIM_Pos \
	(4UL) /*!< RXFIM (Bit 4)                                         */
#define SPIC_IMR_RXFIM_Msk \
	(0x10UL) /*!< RXFIM (Bitfield-Mask: 0x01)                           */
#define SPIC_IMR_FSEIM_Pos \
	(5UL) /*!< FSEIM (Bit 5)                                         */
#define SPIC_IMR_FSEIM_Msk \
	(0x20UL) /*!< FSEIM (Bitfield-Mask: 0x01)                           */
#define SPIC_IMR_USSIM_Pos \
	(9UL) /*!< USSIM (Bit 9)                                         */
#define SPIC_IMR_USSIM_Msk (0x200UL) /*!< USSIM (Bitfield-Mask: 0x01) */
#define SPIC_IMR_TFSIM_Pos \
	(10UL) /*!< TFSIM (Bit 10)                                        */
#define SPIC_IMR_TFSIM_Msk (0x400UL) /*!< TFSIM (Bitfield-Mask: 0x01) */
/* ==========================================================  ISR
 * ========================================================== */
#define SPIC_ISR_TXEIS_Pos \
	(0UL) /*!< TXEIS (Bit 0)                                         */
#define SPIC_ISR_TXEIS_Msk \
	(0x1UL) /*!< TXEIS (Bitfield-Mask: 0x01)                           */
#define SPIC_ISR_TXOIS_Pos \
	(1UL) /*!< TXOIS (Bit 1)                                         */
#define SPIC_ISR_TXOIS_Msk \
	(0x2UL) /*!< TXOIS (Bitfield-Mask: 0x01)                           */
#define SPIC_ISR_RXUIS_Pos \
	(2UL) /*!< RXUIS (Bit 2)                                         */
#define SPIC_ISR_RXUIS_Msk \
	(0x4UL) /*!< RXUIS (Bitfield-Mask: 0x01)                           */
#define SPIC_ISR_RXOIS_Pos \
	(3UL) /*!< RXOIS (Bit 3)                                         */
#define SPIC_ISR_RXOIS_Msk \
	(0x8UL) /*!< RXOIS (Bitfield-Mask: 0x01)                           */
#define SPIC_ISR_RXFIS_Pos \
	(4UL) /*!< RXFIS (Bit 4)                                         */
#define SPIC_ISR_RXFIS_Msk \
	(0x10UL) /*!< RXFIS (Bitfield-Mask: 0x01)                           */
#define SPIC_ISR_FSEIS_Pos \
	(5UL) /*!< FSEIS (Bit 5)                                         */
#define SPIC_ISR_FSEIS_Msk \
	(0x20UL) /*!< FSEIS (Bitfield-Mask: 0x01)                           */
#define SPIC_ISR_USEIS_Pos \
	(9UL) /*!< USEIS (Bit 9)                                         */
#define SPIC_ISR_USEIS_Msk (0x200UL) /*!< USEIS (Bitfield-Mask: 0x01) */
#define SPIC_ISR_TFSIS_Pos \
	(10UL) /*!< TFSIS (Bit 10)                                        */
#define SPIC_ISR_TFSIS_Msk (0x400UL) /*!< TFSIS (Bitfield-Mask: 0x01) */
/* =========================================================  RISR
 * ========================================================== */
#define SPIC_RISR_TXEIR_Pos \
	(0UL) /*!< TXEIR (Bit 0)                                         */
#define SPIC_RISR_TXEIR_Msk \
	(0x1UL) /*!< TXEIR (Bitfield-Mask: 0x01)                           */
#define SPIC_RISR_TXOIR_Pos \
	(1UL) /*!< TXOIR (Bit 1)                                         */
#define SPIC_RISR_TXOIR_Msk \
	(0x2UL) /*!< TXOIR (Bitfield-Mask: 0x01)                           */
#define SPIC_RISR_RXUIR_Pos \
	(2UL) /*!< RXUIR (Bit 2)                                         */
#define SPIC_RISR_RXUIR_Msk \
	(0x4UL) /*!< RXUIR (Bitfield-Mask: 0x01)                           */
#define SPIC_RISR_RXOIR_Pos \
	(3UL) /*!< RXOIR (Bit 3)                                         */
#define SPIC_RISR_RXOIR_Msk \
	(0x8UL) /*!< RXOIR (Bitfield-Mask: 0x01)                           */
#define SPIC_RISR_RXFIR_Pos \
	(4UL) /*!< RXFIR (Bit 4)                                         */
#define SPIC_RISR_RXFIR_Msk \
	(0x10UL) /*!< RXFIR (Bitfield-Mask: 0x01)                           */
#define SPIC_RISR_FSEIR_Pos \
	(5UL) /*!< FSEIR (Bit 5)                                         */
#define SPIC_RISR_FSEIR_Msk \
	(0x20UL) /*!< FSEIR (Bitfield-Mask: 0x01)                           */
#define SPIC_RISR_USEIR_Pos \
	(9UL) /*!< USEIR (Bit 9)                                         */
#define SPIC_RISR_USEIR_Msk (0x200UL) /*!< USEIR (Bitfield-Mask: 0x01) */
#define SPIC_RISR_TFSIR_Pos \
	(10UL) /*!< TFSIR (Bit 10)                                        */
#define SPIC_RISR_TFSIR_Msk (0x400UL) /*!< TFSIR (Bitfield-Mask: 0x01) */
/* ========================================================  TXOICR
 * ========================================================= */
/* ========================================================  RXOICR
 * ========================================================= */
/* ========================================================  RXUICR
 * ========================================================= */
/* ========================================================  MSTICR
 * ========================================================= */
/* ==========================================================  ICR
 * ========================================================== */
/* ==========================================================  DR
 * =========================================================== */
/* ========================================================  DR_BYTE
 * ======================================================== */
/* ========================================================  DR_HALF
 * ======================================================== */
/* ========================================================  DR_WORD
 * ======================================================== */
/* ======================================================  USERLENGTH
 * ======================================================= */
#define SPIC_USERLENGTH_RDDUMMLEN_Pos \
	(0UL) /*!< RDDUMMLEN (Bit 0)                                    */
#define SPIC_USERLENGTH_RDDUMMLEN_Msk \
	(0xfffUL) /*!< RDDUMMLEN (Bitfield-Mask: 0xfff) */
#define SPIC_USERLENGTH_CMDLEN_Pos \
	(12UL) /*!< CMDLEN (Bit 12)                                       */
#define SPIC_USERLENGTH_CMDLEN_Msk \
	(0x3000UL) /*!< CMDLEN (Bitfield-Mask: 0x03) */
#define SPIC_USERLENGTH_ADDRLEN_Pos \
	(16UL) /*!< ADDRLEN (Bit 16)                                      */
#define SPIC_USERLENGTH_ADDRLEN_Msk \
	(0xf0000UL) /*!< ADDRLEN (Bitfield-Mask: 0x0f) */
/* =========================================================  FLUSH
 * ========================================================= */
#define SPIC_FLUSH_ALL_Pos \
	(0UL) /*!< ALL (Bit 0)                                           */
#define SPIC_FLUSH_ALL_Msk \
	(0x1UL) /*!< ALL (Bitfield-Mask: 0x01)                             */
#define SPIC_FLUSH_DRFIFO_Pos \
	(1UL) /*!< DRFIFO (Bit 1)                                        */
#define SPIC_FLUSH_DRFIFO_Msk \
	(0x2UL) /*!< DRFIFO (Bitfield-Mask: 0x01)                          */
#define SPIC_FLUSH_STFIFO_Pos \
	(2UL) /*!< STFIFO (Bit 2)                                        */
#define SPIC_FLUSH_STFIFO_Msk \
	(0x4UL) /*!< STFIFO (Bitfield-Mask: 0x01)                          */
/* =========================================================  TXNDF
 * ========================================================= */
#define SPIC_TXNDF_NUM_Pos \
	(0UL) /*!< NUM (Bit 0)                                           */
#define SPIC_TXNDF_NUM_Msk (0xffffffUL) /*!< NUM (Bitfield-Mask: 0xffffff) */

/* ========================================================  FLASHWP
 * ======================================================== */
#define IOPAD_FLASHWP_INDETEN_Pos \
	(0UL) /*!< INDETEN (Bit 0)                                       */
#define IOPAD_FLASHWP_INDETEN_Msk \
	(0x1UL) /*!< INDETEN (Bitfield-Mask: 0x01)                         */
#define IOPAD_FLASHWP_OUTDRV_Pos \
	(1UL) /*!< OUTDRV (Bit 1)                                        */
#define IOPAD_FLASHWP_OUTDRV_Msk \
	(0x2UL) /*!< OUTDRV (Bitfield-Mask: 0x01)                          */
#define IOPAD_FLASHWP_SLEWRATE_Pos \
	(2UL) /*!< SLEWRATE (Bit 2)                                      */
#define IOPAD_FLASHWP_SLEWRATE_Msk \
	(0x4UL) /*!< SLEWRATE (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHWP_PULLDWEN_Pos \
	(3UL) /*!< PULLDWEN (Bit 3)                                      */
#define IOPAD_FLASHWP_PULLDWEN_Msk \
	(0x8UL) /*!< PULLDWEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHWP_PULLUPEN_Pos \
	(4UL) /*!< PULLUPEN (Bit 4)                                      */
#define IOPAD_FLASHWP_PULLUPEN_Msk \
	(0x10UL) /*!< PULLUPEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHWP_SCHEN_Pos \
	(5UL) /*!< SCHEN (Bit 5)                                         */
#define IOPAD_FLASHWP_SCHEN_Msk \
	(0x20UL) /*!< SCHEN (Bitfield-Mask: 0x01)                           */
/* =======================================================  FLASHHOLD
 * ======================================================= */
#define IOPAD_FLASHHOLD_INDETEN_Pos \
	(0UL) /*!< INDETEN (Bit 0)                                       */
#define IOPAD_FLASHHOLD_INDETEN_Msk \
	(0x1UL) /*!< INDETEN (Bitfield-Mask: 0x01)                         */
#define IOPAD_FLASHHOLD_OUTDRV_Pos \
	(1UL) /*!< OUTDRV (Bit 1)                                        */
#define IOPAD_FLASHHOLD_OUTDRV_Msk \
	(0x2UL) /*!< OUTDRV (Bitfield-Mask: 0x01)                          */
#define IOPAD_FLASHHOLD_SLEWRATE_Pos \
	(2UL) /*!< SLEWRATE (Bit 2)                                      */
#define IOPAD_FLASHHOLD_SLEWRATE_Msk \
	(0x4UL) /*!< SLEWRATE (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHHOLD_PULLDWEN_Pos \
	(3UL) /*!< PULLDWEN (Bit 3)                                      */
#define IOPAD_FLASHHOLD_PULLDWEN_Msk \
	(0x8UL) /*!< PULLDWEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHHOLD_PULLUPEN_Pos \
	(4UL) /*!< PULLUPEN (Bit 4)                                      */
#define IOPAD_FLASHHOLD_PULLUPEN_Msk \
	(0x10UL) /*!< PULLUPEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHHOLD_SCHEN_Pos \
	(5UL) /*!< SCHEN (Bit 5)                                         */
#define IOPAD_FLASHHOLD_SCHEN_Msk \
	(0x20UL) /*!< SCHEN (Bitfield-Mask: 0x01)                           */
/* ========================================================  FLASHSI
 * ======================================================== */
#define IOPAD_FLASHSI_INDETEN_Pos \
	(0UL) /*!< INDETEN (Bit 0)                                       */
#define IOPAD_FLASHSI_INDETEN_Msk \
	(0x1UL) /*!< INDETEN (Bitfield-Mask: 0x01)                         */
#define IOPAD_FLASHSI_OUTDRV_Pos \
	(1UL) /*!< OUTDRV (Bit 1)                                        */
#define IOPAD_FLASHSI_OUTDRV_Msk \
	(0x2UL) /*!< OUTDRV (Bitfield-Mask: 0x01)                          */
#define IOPAD_FLASHSI_SLEWRATE_Pos \
	(2UL) /*!< SLEWRATE (Bit 2)                                      */
#define IOPAD_FLASHSI_SLEWRATE_Msk \
	(0x4UL) /*!< SLEWRATE (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHSI_PULLDWEN_Pos \
	(3UL) /*!< PULLDWEN (Bit 3)                                      */
#define IOPAD_FLASHSI_PULLDWEN_Msk \
	(0x8UL) /*!< PULLDWEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHSI_PULLUPEN_Pos \
	(4UL) /*!< PULLUPEN (Bit 4)                                      */
#define IOPAD_FLASHSI_PULLUPEN_Msk \
	(0x10UL) /*!< PULLUPEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHSI_SCHEN_Pos \
	(5UL) /*!< SCHEN (Bit 5)                                         */
#define IOPAD_FLASHSI_SCHEN_Msk \
	(0x20UL) /*!< SCHEN (Bitfield-Mask: 0x01)                           */
/* ========================================================  FLASHSO
 * ======================================================== */
#define IOPAD_FLASHSO_INDETEN_Pos \
	(0UL) /*!< INDETEN (Bit 0)                                       */
#define IOPAD_FLASHSO_INDETEN_Msk \
	(0x1UL) /*!< INDETEN (Bitfield-Mask: 0x01)                         */
#define IOPAD_FLASHSO_OUTDRV_Pos \
	(1UL) /*!< OUTDRV (Bit 1)                                        */
#define IOPAD_FLASHSO_OUTDRV_Msk \
	(0x2UL) /*!< OUTDRV (Bitfield-Mask: 0x01)                          */
#define IOPAD_FLASHSO_SLEWRATE_Pos \
	(2UL) /*!< SLEWRATE (Bit 2)                                      */
#define IOPAD_FLASHSO_SLEWRATE_Msk \
	(0x4UL) /*!< SLEWRATE (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHSO_PULLDWEN_Pos \
	(3UL) /*!< PULLDWEN (Bit 3)                                      */
#define IOPAD_FLASHSO_PULLDWEN_Msk \
	(0x8UL) /*!< PULLDWEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHSO_PULLUPEN_Pos \
	(4UL) /*!< PULLUPEN (Bit 4)                                      */
#define IOPAD_FLASHSO_PULLUPEN_Msk \
	(0x10UL) /*!< PULLUPEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHSO_SCHEN_Pos \
	(5UL) /*!< SCHEN (Bit 5)                                         */
#define IOPAD_FLASHSO_SCHEN_Msk \
	(0x20UL) /*!< SCHEN (Bitfield-Mask: 0x01)                           */
/* ========================================================  FLASHCS
 * ======================================================== */
#define IOPAD_FLASHCS_INDETEN_Pos \
	(0UL) /*!< INDETEN (Bit 0)                                       */
#define IOPAD_FLASHCS_INDETEN_Msk \
	(0x1UL) /*!< INDETEN (Bitfield-Mask: 0x01)                         */
#define IOPAD_FLASHCS_OUTDRV_Pos \
	(1UL) /*!< OUTDRV (Bit 1)                                        */
#define IOPAD_FLASHCS_OUTDRV_Msk \
	(0x2UL) /*!< OUTDRV (Bitfield-Mask: 0x01)                          */
#define IOPAD_FLASHCS_SLEWRATE_Pos \
	(2UL) /*!< SLEWRATE (Bit 2)                                      */
#define IOPAD_FLASHCS_SLEWRATE_Msk \
	(0x4UL) /*!< SLEWRATE (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHCS_PULLDWEN_Pos \
	(3UL) /*!< PULLDWEN (Bit 3)                                      */
#define IOPAD_FLASHCS_PULLDWEN_Msk \
	(0x8UL) /*!< PULLDWEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHCS_PULLUPEN_Pos \
	(4UL) /*!< PULLUPEN (Bit 4)                                      */
#define IOPAD_FLASHCS_PULLUPEN_Msk \
	(0x10UL) /*!< PULLUPEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHCS_SCHEN_Pos \
	(5UL) /*!< SCHEN (Bit 5)                                         */
#define IOPAD_FLASHCS_SCHEN_Msk \
	(0x20UL) /*!< SCHEN (Bitfield-Mask: 0x01)                           */
/* =======================================================  FLASHCLK
 * ======================================================== */
#define IOPAD_FLASHCLK_INDETEN_Pos \
	(0UL) /*!< INDETEN (Bit 0)                                       */
#define IOPAD_FLASHCLK_INDETEN_Msk \
	(0x1UL) /*!< INDETEN (Bitfield-Mask: 0x01)                         */
#define IOPAD_FLASHCLK_OUTDRV_Pos \
	(1UL) /*!< OUTDRV (Bit 1)                                        */
#define IOPAD_FLASHCLK_OUTDRV_Msk \
	(0x2UL) /*!< OUTDRV (Bitfield-Mask: 0x01)                          */
#define IOPAD_FLASHCLK_SLEWRATE_Pos \
	(2UL) /*!< SLEWRATE (Bit 2)                                      */
#define IOPAD_FLASHCLK_SLEWRATE_Msk \
	(0x4UL) /*!< SLEWRATE (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHCLK_PULLDWEN_Pos \
	(3UL) /*!< PULLDWEN (Bit 3)                                      */
#define IOPAD_FLASHCLK_PULLDWEN_Msk \
	(0x8UL) /*!< PULLDWEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHCLK_PULLUPEN_Pos \
	(4UL) /*!< PULLUPEN (Bit 4)                                      */
#define IOPAD_FLASHCLK_PULLUPEN_Msk \
	(0x10UL) /*!< PULLUPEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_FLASHCLK_SCHEN_Pos \
	(5UL) /*!< SCHEN (Bit 5)                                         */
#define IOPAD_FLASHCLK_SCHEN_Msk \
	(0x20UL) /*!< SCHEN (Bitfield-Mask: 0x01)                           */
/* =========================================================  PECI
 * ========================================================== */
#define IOPAD_PECI_INDETEN_Pos \
	(0UL) /*!< INDETEN (Bit 0)                                       */
#define IOPAD_PECI_INDETEN_Msk \
	(0x1UL) /*!< INDETEN (Bitfield-Mask: 0x01)                         */
#define IOPAD_PECI_OUTDRV_Pos \
	(1UL) /*!< OUTDRV (Bit 1)                                        */
#define IOPAD_PECI_OUTDRV_Msk \
	(0x2UL) /*!< OUTDRV (Bitfield-Mask: 0x01)                          */
#define IOPAD_PECI_SLEWRATE_Pos \
	(2UL) /*!< SLEWRATE (Bit 2)                                      */
#define IOPAD_PECI_SLEWRATE_Msk \
	(0x4UL) /*!< SLEWRATE (Bitfield-Mask: 0x01)                        */
#define IOPAD_PECI_PULLDWEN_Pos \
	(3UL) /*!< PULLDWEN (Bit 3)                                      */
#define IOPAD_PECI_PULLDWEN_Msk \
	(0x8UL) /*!< PULLDWEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_PECI_PULLUPEN_Pos \
	(4UL) /*!< PULLUPEN (Bit 4)                                      */
#define IOPAD_PECI_PULLUPEN_Msk \
	(0x10UL) /*!< PULLUPEN (Bitfield-Mask: 0x01)                        */
#define IOPAD_PECI_SCHEN_Pos \
	(5UL) /*!< SCHEN (Bit 5)                                         */
#define IOPAD_PECI_SCHEN_Msk \
	(0x20UL) /*!< SCHEN (Bitfield-Mask: 0x01)                           */

/* ==========================================================  GCR
 * ========================================================== */
#define GPIO_GCR_DIR_Pos \
	(0UL) /*!< DIR (Bit 0)                                           */
#define GPIO_GCR_DIR_Msk \
	(0x1UL) /*!< DIR (Bitfield-Mask: 0x01)                             */
#define GPIO_GCR_INDETEN_Pos \
	(1UL) /*!< INDETEN (Bit 1)                                       */
#define GPIO_GCR_INDETEN_Msk \
	(0x2UL) /*!< INDETEN (Bitfield-Mask: 0x01)                         */
#define GPIO_GCR_INVOLMD_Pos \
	(2UL) /*!< INVOLMD (Bit 2)                                       */
#define GPIO_GCR_INVOLMD_Msk \
	(0x4UL) /*!< INVOLMD (Bitfield-Mask: 0x01)                         */
#define GPIO_GCR_PINSTS_Pos \
	(3UL) /*!< PINSTS (Bit 3)                                        */
#define GPIO_GCR_PINSTS_Msk \
	(0x8UL) /*!< PINSTS (Bitfield-Mask: 0x01)                          */
#define GPIO_GCR_MFCTRL_Pos \
	(8UL) /*!< MFCTRL (Bit 8)                                        */
#define GPIO_GCR_MFCTRL_Msk (0x700UL) /*!< MFCTRL (Bitfield-Mask: 0x07) */
#define GPIO_GCR_OUTDRV_Pos \
	(11UL) /*!< OUTDRV (Bit 11)                                       */
#define GPIO_GCR_OUTDRV_Msk (0x800UL) /*!< OUTDRV (Bitfield-Mask: 0x01) */
#define GPIO_GCR_SLEWRATE_Pos \
	(12UL) /*!< SLEWRATE (Bit 12)                                     */
#define GPIO_GCR_SLEWRATE_Msk                          \
	(0x1000UL) /*!< SLEWRATE (Bitfield-Mask: 0x01) \
		    */
#define GPIO_GCR_PULLDWEN_Pos \
	(13UL) /*!< PULLDWEN (Bit 13)                                     */
#define GPIO_GCR_PULLDWEN_Msk                          \
	(0x2000UL) /*!< PULLDWEN (Bitfield-Mask: 0x01) \
		    */
#define GPIO_GCR_PULLUPEN_Pos \
	(14UL) /*!< PULLUPEN (Bit 14)                                     */
#define GPIO_GCR_PULLUPEN_Msk                          \
	(0x4000UL) /*!< PULLUPEN (Bitfield-Mask: 0x01) \
		    */
#define GPIO_GCR_SCHEN_Pos \
	(15UL) /*!< SCHEN (Bit 15)                                        */
#define GPIO_GCR_SCHEN_Msk (0x8000UL) /*!< SCHEN (Bitfield-Mask: 0x01) */
#define GPIO_GCR_OUTMD_Pos \
	(16UL) /*!< OUTMD (Bit 16)                                        */
#define GPIO_GCR_OUTMD_Msk (0x10000UL) /*!< OUTMD (Bitfield-Mask: 0x01) */
#define GPIO_GCR_OUTCTRL_Pos \
	(17UL) /*!< OUTCTRL (Bit 17)                                      */
#define GPIO_GCR_OUTCTRL_Msk (0x20000UL) /*!< OUTCTRL (Bitfield-Mask: 0x01) */
#define GPIO_GCR_INTCTRL_Pos \
	(24UL) /*!< INTCTRL (Bit 24)                                      */
#define GPIO_GCR_INTCTRL_Msk                             \
	(0x7000000UL) /*!< INTCTRL (Bitfield-Mask: 0x07) \
		       */
#define GPIO_GCR_INTEN_Pos \
	(28UL) /*!< INTEN (Bit 28)                                        */
#define GPIO_GCR_INTEN_Msk (0x10000000UL) /*!< INTEN (Bitfield-Mask: 0x01) */
#define GPIO_GCR_INTSTS_Pos \
	(31UL) /*!< INTSTS (Bit 31)                                       */
#define GPIO_GCR_INTSTS_Msk                              \
	(0x80000000UL) /*!< INTSTS (Bitfield-Mask: 0x01) \
			*/

#endif /* __REG_H__ */