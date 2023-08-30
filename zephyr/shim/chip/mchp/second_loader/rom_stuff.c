/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "MCHP_MEC172x.h"

static uint8_t rom_otp8(uint16_t byte_idx);
typedef struct {
	/* OTP_REG_INST Structure */
	union {
		/* OTP Power Down Register. */
		__IO uint8_t OTP_PWR_DN;

		struct {
			/*
			 * This active-low bit directly controls the CEB input
			 * into the OTP IP. 0= OTP is operational. 1= OTP is in
			 * power down. Note: This bit is cleared before
			 * performing OTP operations. The bit should stay
			 * cleared until all planned OTP programming and read
			 * operations have completed.
			 */
			__IO uint8_t PWRDN_N : 1;
		} OTP_PWR_DN_b;
	};
	union {
		/*
		 * Address input. Programs the address targeted by OTP
		 * operation. These bits are directly connected to the Kilopass
		 * IP input A[15:11].
		 */
		__IO uint8_t OTP_ADDR1;
		struct {
			/* OTP Address[15:11] bits. */
			__IO uint8_t OTP_ADDRESS_HI;
		} OTP_ADDR1_byt;

		struct {
			/* OTP Address[15:11] bits. */
			__IO uint8_t OTP_ADDR15_11 : 5;
		} OTP_ADDR1_b;
	};

	union {
		/*
		 * Address input. Programs the address targeted by OTP
		 * operation. These bits are directly connected to the Kilopass
		 * IP input A[15:0].
		 */
		__IO uint8_t OTP_ADDR2;
		struct {
			/* OTP Address[10:3] bits. */
			__IO uint8_t OTP_ADDRESS_LO;
		} OTP_ADDR2_byt;
		struct {
			/* OTP Address[10:3] bits. */
			__IO uint8_t OTP_ADDR10_03 : 8;
		} OTP_ADDR2_b;
	};

	union {
		/*
		 * Address input. Programs the address targeted by OTP
		 * operation. These bits are directly connected to the Kilopass
		 * IP input A[15:0].
		 */
		__IO uint8_t OTP_ADDR3;
		struct {
			/* OTP Address[2:0] bits. */
			__IO uint8_t OTP_ADDRESS_BITs;
		} OTP_ADDR3_byt;
		struct {
			/* OTP Address[2:0] bits. */
			__IO uint8_t OTP_ADDR2_0 : 3;
		} OTP_ADDR3_b;
	};

	union {
		/* OTP Program Data Register. */
		__IO uint8_t OTP_PRGM_DATA;

		struct {
			/* Parallel data to be written to the DATA register. The
			 * DATA register is then transferred to the OTP at the
			 * OTP_ADDR location with a program command.
			 */
			__IO uint8_t OTP_WR_DATA : 8;
		} OTP_PRGM_DATA_b;
	};

	union {
		/* OTP PROGRAM MODE Register */
		__IO uint8_t OTP_PRGM_MODE;

		struct {
			/*
			 * Program Mode Byte. This indicates the units of a
			 * programming operation. 0= Bit programming. 1= Byte
			 * programming. Note: OTP reads are always byte wide.
			 */
			__IO uint8_t PGM_MODE_BYTE : 1;
		} OTP_PRGM_MODE_b;
	};

	union {
		/* OTP READ DATA Register. */
		__I uint8_t OTP_RD_DATA;

		struct {
			/*
			 * This is the data from the OTP at the location pointed
			 * to by TP_ADDR. To get fresh data a READ, PROGRAM, or
			 * PROGRAMVERIFY command operation must be issue before
			 * reading this register. Note: PROGRAM commands update
			 * this register as well as the data bus output from OTP
			 * holds the value stored at the previously programmed
			 * location after the deassertion of PGMEN.
			 */
			__I uint8_t OTP_RD_DAT : 8;
		} OTP_RD_DATA_b;
	};
	__I uint8_t RESERVED;

	union {
		__IO uint8_t OTP_FUNC_CMD;

		struct {
			/*
			 * Read OTP command. Note: This bit clears after command
			 * is accepted by the OTP controller. This is a R/W1C
			 * bit.
			 */
			__IO uint8_t READ : 1;
			/*
			 * Program OTP Command. Note: This bit clears after
			 * command is accepted by the OTP controller. This is a
			 * R/W1C bit.
			 */
			__IO uint8_t PROGRAM : 1;
			/*
			 * RESET Command. Pulses the RSTB input into the OPT for
			 * a length specified in OTP Reset Pulse Width Register
			 * Note: This bit clears after command is accepted by
			 * the OTP controller. This is a R/W1C bit.
			 */
			__IO uint8_t RESET : 1;
		} OTP_FUNC_CMD_b;
	};

	union {
		/* Only a single bit in the OTP Function Command Register or OTP
		 * Test Command Register must be set before initiating the OTP
		 * operation via the GO bit.
		 */
		__IO uint8_t OTP_TEST_CMD;

		struct {
			/*
			 * Blank Check verifies that OTP memory is preloaded
			 * with 0 before actual programming. Blank Check is
			 * performed by reading every bit location at nominal
			 * VDD and nominal VDDIO before any other tests or any
			 * programming is performed on the XPM memory. The
			 * command is enabled by latching test mode word 0x28
			 * into the test command register, Note: This bit self
			 * clears after the command is completed. Note: This bit
			 * clears after command is accepted by the OTP
			 * controller.
			 */
			__IO uint8_t BLANKCHECK : 1;
			/*
			 * TESTDEC is a built-in test mode. It enables an end
			 * user to verify the integrity of wordlines and
			 * bitlines as well as screen out the gross defects in
			 * the peripheral logic. It is performed on an
			 * unprogrammed unit. It is performed by latching test
			 * mode word 0x21 into the test command register and
			 * then making subsequent read operations to successive
			 * addresses till the entire OTP memory is read out. The
			 * expected test pattern is a variation of a
			 * checkerboard pattern. Note: This bit self clears
			 * after the command.
			 */
			__IO uint8_t TESTDEC : 1;
			/*
			 * WRTEST Command (Pre-program Test). WRTEST enables an
			 * end user to screen out gross defects in programming
			 * circuitry before programming of the actual XPM memory
			 * array is done. This is enabled by availability of two
			 * spare rows for programming. It is performed by
			 * latching test mode word 0x04 into the test command
			 * register, programming the spare rows and verifying
			 * that spare rows can be programmed correctly. Note:
			 * This bit self clears after the command is completed.
			 * Note: This bit clears after command is accept.
			 */
			__IO uint8_t WRTEST : 1;
			/*
			 * Program and Verify Command. This command programs the
			 * OTP and confirms that the OTP location has been
			 * successfully written. This command issue a compares
			 * the value on the OTPs D bus with the program data to
			 * verify the command.
			 * Note: This command supports single bit and byte
			 * operations. Note: An OTP location shall not be
			 * considered failing until ten iterations. Note: This
			 * bit clears after command is accepted by the OTP
			 * controller.
			 */
			__IO uint8_t PROGRAMVERIFY : 1;
			/*
			 * 0= Testdec1 selected with default value 0x04.
			 * 1= Testdec2 selected with default value 0x14.
			 * Note: TESTDEC is valid only for un-programmed units.
			 */
			__IO uint8_t TEST_DEC_SEL : 1;
		} OTP_TEST_CMD_b;
	};

	union {
		__IO uint8_t OTP_GO_CMD;

		struct {
			/*
			 * Go Command Bit. Setting this bit initiates the OTP
			 * command defined in either the OTP Function Command
			 * Register or OTP Test Command Register. This is a
			 * R/W1C register. Note: This bit clears after command
			 * is accepted by the OTP controller.
			 */
			__IO uint8_t GO : 1;
		} OTP_GO_CMD_b;
	};

	union {
		__IO uint8_t OTP_PASS_FAIL;

		struct {
			/*
			 * Fail Status Bit. This bit asserts after the
			 * completion of an OTP test command to indicate that
			 * the command failed. This is a R/W1C bit. Note: This
			 * bit automatically clears when the GO bit is set.
			 */
			__IO uint8_t FAIL : 1;
			/*
			 * Pass Status Bit. This bit asserts after the
			 * completion of an OTP test command to indicate that
			 * the command passed. This is a R/W1C bit. Note: This
			 * bit automatically clears when the GO bit is set.
			 */
			__IO uint8_t PASS : 1;
		} OTP_PASS_FAIL_b;
	};

	union {
		__I uint8_t OTP_STATUS;

		struct {
			/*
			 * OTP operation is in progress. This bit clears after
			 * OTP command has completed.
			 */
			__I uint8_t BUSY : 1;
			/*
			 * Charge pump is on. This is a direct read of the
			 * CPUMPEN input to the OTP.
			 */
			__I uint8_t CPUMPEN : 1;
			/*
			 * Programming Enable. This is a direct read of the
			 * PGMEN input to the OTP.
			 */
			__I uint8_t PGMEN : 1;
			/*
			 * OTP Write Enable. This is a direct read of the WEB
			 * input to the OTP.
			 */
			__I uint8_t WEB : 1;
			/*
			 * OTP has been permanently locked to be read only. This
			 * is a direct read of the LOCK OTP output. Note: This
			 * bit should never be asserted. The OTP lock feature is
			 * not supported in this part. Assertion of this bit
			 * indicates a catastrophic failure. Note: The contents
			 * of this bit should only be read after the OTP is
			 * taken out of power down and a reset is issued.
			 */
			__I uint8_t OTP_LOCK : 1;
		} OTP_STATUS_b;
	};

	union {
		__IO uint8_t OTP_MAX_PROG;

		struct {
			/*
			 * Maximum Programming. Specifies the number of attempts
			 * made by the PROGRAMVERIFY command to program an OTP
			 * bit cell.
			 */
			__IO uint8_t MAX_PROG : 5;
		} OTP_MAX_PROG_b;
	};
	__I uint16_t RESERVED1;

	union {
		__IO uint8_t OTP_INTR_STATUS;

		struct {
			/*
			 * This bit is set whenever BUSY transitions from 1 to
			 * 0. A one in this bit causes OTP_READY interrupt to
			 * the MCU. Write a 1 to clear this bit. Writes of 0
			 * have no effect (R/W1C).
			 */
			__IO uint8_t READY_INTR_STATUS : 1;
		} OTP_INTR_STATUS_b;
	};

	union {
		__IO uint8_t OTP_INTR_MASK;

		struct {
			/* When 1, prevents the generation of this interrupt. */
			__IO uint8_t READY_INTR_MASK : 1;
		} OTP_INTR_MASK_b;
	};
	__I uint16_t RESERVED2;

	union {
		/* This is the OTP Reset Pulse Width Register. */
		__IO uint16_t OTP_RSTB_PW;

		struct {
			/*
			 * RSTB Pulse Width in terms of system clocks. This
			 * corresponds to RSTB.
			 */
			__IO uint16_t OTP_RSTW : 16;
		} OTP_RSTB_PW_b;
	};
	__I uint16_t RESERVED3;

	union {
		/* This is the OTP Program Pulse Width Register. */
		__IO uint16_t OTP_PPW_REG;

		struct {
			/*
			 * Programming Pulse Width in terms of system clocks.
			 * The minimum value to be used is 240 (~4 uS). This
			 * corresponds to tPW in Figure 26.2, "OTP Write Verify
			 * Timing"which is the assertion time of WEB while
			 * programming the OTP. Note: Assumes 60 MHz system
			 * clock. Note: OTP programming is not supported when
			 * operating off of the ring oscillator.
			 */
			__IO uint16_t OTP_PPW : 16;
		} OTP_PPW_REG_b;
	};
	__I uint16_t RESERVED4;

	union {
		/* This is the OTP Read Pulse Width Register. */
		__IO uint16_t OTP_RPW_REG;

		struct {
			/*
			 * Read pulse width in terms of system clock. This
			 * corresponds to tRWH. Note: Assumes 60 MHz system
			 * clock.
			 */
			__IO uint16_t OTP_RPW : 16;
		} OTP_RPW_REG_b;
	};
	__I uint16_t RESERVED5;

	union {
		/* This is the OTP CEB Setup Time before RSTB Value Register. */
		__IO uint8_t OTP_TCRST_VAL;

		struct {
			/*
			 * CEB Setup Time before RSTB. This corresponds to
			 * tCRST. Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_CRST : 8;
		} OTP_TCRST_VAL_b;
	};

	union {
		/* This is the OTP RSTB Setup Time before READEN Value Register.
		 */
		__IO uint8_t OTP_TRSRD_VAL;

		struct {
			/*
			 * OTP RSTB Setup Time before READEN.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TRSRD : 8;
		} OTP_TRSRD_VAL_b;
	};

	union {
		/* This is the OTP READEN to Output Delay Value Register. */
		__IO uint8_t OTP_TREADEN_VAL;

		struct {
			/*
			 * OTP RSTB Setup Time before READEN.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TREADEN : 8;
		} OTP_TREADEN_VAL_b;
	};

	union {
		/* This is the OTP DLE Setup Time before WEB Value Register. */
		__IO uint8_t OTP_TDLES_VAL;

		struct {
			/*
			 * DLE Setup Time before WEB.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_DLES : 8;
		} OTP_TDLES_VAL_b;
	};

	union {
		/* This is the OTP Minimum WEB Low Pulse Width Value Register.
		 */
		__IO uint8_t OTP_TWWL_VAL;

		struct {
			/* Minimum WEB Low Pulse Width.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TWWL : 8;
		} OTP_TWWL_VAL_b;
	};

	union {
		/* This is the OTP DLE Hold Time after WEB Value Register. */
		__IO uint8_t OTP_TDLEH_VAL;

		struct {
			/*
			 * DLE Hold Time after WEB.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TDLEH : 8;
		} OTP_TDLEH_VAL_b;
	};

	union {
		/* This is the OTP WEB de-assert to PGMEN assert Delay Value
		 * Register.
		 */
		__IO uint8_t OTP_TWPED_VAL;

		struct {
			/*
			 * WEB de-assert to PGMEN assert Delay.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TWPED : 8;
		} OTP_TWPED_VAL_b;
	};

	union {
		/* This is the OTP PGMEN Setup Time before CPUMPEN Value
		 * Register.
		 */
		__IO uint8_t OTP_TPES_VAL;

		struct {
			/*
			 * PGMEN Setup Time before CPUMPEN.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TPES : 8;
		} OTP_TPES_VAL_b;
	};

	union {
		/* This is the OTP CPUMPEN Setup Time before WEB Value Register.
		 */
		__IO uint8_t OTP_TCPS_VAL;

		struct {
			/*
			 * CPUMPEN Setup Time before WEB.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TCPS : 8;
		} OTP_TCPS_VAL_b;
	};

	union {
		/* This is the OTP CPUMPEN Hold Time after WEB Value Register.
		 */
		__IO uint8_t OTP_TCPH_VAL;

		struct {
			/*
			 * CPUMPEN Hold Time after WEB.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TCPH : 8;
		} OTP_TCPH_VAL_b;
	};
	__I uint8_t RESERVED6;

	union {
		/* This is the OTP PGMEN Hold Time after CPUMPEN Value Register.
		 */
		__IO uint8_t OTP_TPEH_VAL;

		struct {
			/*
			 * PGMEM Hold time after CPUMPEN.
			 * Note: Default value for 24 MHz system clock.
			 */
			__IO uint8_t OTP_TPEH : 8;
		} OTP_TPEH_VAL_b;
	};

	union {
		/* This is the OTP PGMEN Setup Time before RSTB Value Register.
		 */
		__IO uint8_t OTP_TPGRST_VAL;

		struct {
			/*
			 * PGMEN Setup Time before RSTB.
			 * Note: Default value for 24 MHz system clock.
			 */
			__IO uint8_t OTP_TPGRST : 8;
		} OTP_TPGRST_VAL_b;
	};

	union {
		/* This is the OTP CLE Setup Time before WEB Value Register. */
		__IO uint8_t OTP_TCLES_VAL;

		struct {
			/*
			 * CLE Setup Time before WEB.
			 * Note: Default value for 24 MHz system clock.
			 */
			__IO uint8_t OTP_TCLES : 8;
		} OTP_TCLES_VAL_b;
	};

	union {
		/* This is the OTP CLE Hold Time after WEB Value Register. */
		__IO uint8_t OTP_TCLEH_VAL;

		struct {
			/* CLE Hold Time after WEB. Note: Default value for 60
			 * MHz system clock.
			 */
			__IO uint8_t OTP_TCLEH : 8;
		} OTP_TCLEH_VAL_b;
	};

	union {
		/* This is the OTP CPUMPEN Setup Time after WEB Value Register.
		 */
		__IO uint8_t OTP_TCPES_VAL;

		struct {
			/*
			 * CPUMPEN Setup Time after WEB.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TCPES : 8;
		} OTP_TCPES_VAL_b;
	};

	union {
		/* This is the OTP Address to Output Delay Value Register. */
		__IO uint8_t OTP_TBCAAC_VAL;

		struct {
			/*
			 * Address to Output Delay.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TBCAAC : 8;
		} OTP_TBCAAC_VAL_b;
	};

	union {
		/* This is the OTP Blank Check Address to Address Change Value
		 * Register.
		 */
		__IO uint8_t OTP_TAAC_VAL;

		struct {
			/*
			 * Blank Check Address to Address change.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TAAC : 8;
		} OTP_TAAC_VAL_b;
	};

	union {
		/* This is the OTP Address to Test Output Delay Value Register.
		 */
		__IO uint8_t OTP_TACCT_VAL;

		struct {
			/*
			 * Address to Test Output Delay.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TACCT : 8;
		} OTP_TACCT_VAL_b;
	};

	union {
		/*
		 * This is the OTP PWRRDY assertion to Wakeup assertion Delay
		 * Value Register.
		 */
		__IO uint8_t OTP_TPWAD_VAL;

		struct {
			/*
			 * PWRRDY assertion to WAKEUP assertion Delay.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TPWAD : 8;
		} OTP_TPWAD_VAL_b;
	};

	union {
		/* This is the OTP Wakeup assertion to CEB Delay Value Register.
		 */
		__IO uint16_t OTP_TPCAD_VAL;

		struct {
			/*
			 * WAKEUP assertion to CEB assertion Delay.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint16_t OTP_TWCAD : 16;
		} OTP_TPCAD_VAL_b;
	};

	union {
		/* This is the OTP Address Setup Time before WEB Value Register.
		 */
		__IO uint8_t OTP_TAS_VAL;

		struct {
			/*
			 * Address Setup Time before WEB.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TAS : 8;
		} OTP_TAS_VAL_b;
	};

	union {
		/* This is the OTP Data Setup Time before WEB Value Register. */
		__IO uint8_t OTP_TDS_VAL;

		struct {
			/*
			 * Data Setup Time before WEB.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TDS : 8;
		} OTP_TDS_VAL_b;
	};

	union {
		/* This is the OTP READEN Setup Time after PGMEM Value Register.
		 */
		__IO uint8_t OTP_TRDEO_VAL;

		struct {
			/*
			 * READEN setup time after PGMEM Delay.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TRDEO : 8;
		} OTP_TRDEO_VAL_b;
	};

	union {
		/* This is the OTP PGMEM Setup Time before PGMVFY Value
		 * Register.
		 */
		__IO uint8_t OTP_TPGSV_VAL;

		struct {
			/*
			 * PGMEM Setup Time before PGMVFY.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TPGSV : 8;
		} OTP_TPGSV_VAL_b;
	};

	union {
		/* This is the OTP PGMVFY Setup Time before READEN Value
		 * Register.
		 */
		__IO uint8_t OTP_TPVSR_VAL;

		struct {
			/*
			 * PGMVFY Setup Time before READEN Delay.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TPVSR : 8;
		} OTP_TPVSR_VAL_b;
	};

	union {
		/* This is the OTP PGMVFY Hold Time after READEN Value Register.
		 */
		__IO uint8_t OTP_TPVHR_VAL;

		struct {
			/*
			 * PGMVFY Hold Time after READEN Delay.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TPVHR : 8;
		} OTP_TPVHR_VAL_b;
	};

	union {
		/* This is the OTP PGMVFY Setup Time before Address Value
		 * Register.
		 */
		__IO uint8_t OTP_TPVSA_VAL;

		struct {
			/*
			 * PGMVFY Setup Time before Address Delay.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TPVSA : 8;
		} OTP_TPVSA_VAL_b;
	};

	union {
		/* This is the CEB De-assertion to WAKEUP de-assertion Delay
		 * Register.
		 */
		__IO uint8_t OTP_TCWDD_VAL;

		struct {
			/*
			 * CEB De-assertion to WAKEUP de-assertion Delay.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TCWDD : 8;
		} OTP_TCWDD_VAL_b;
	};
	__I uint16_t RESERVED7;

	union {
		/* This is the CPUMPEN Setup Time after WEB Register. */
		__IO uint8_t OTP_TCPRD_VAL;

		struct {
			/*
			 * CPUMPEN Setup Time before read enable.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TCPRD : 8;
		} OTP_TCPRD_VAL_b;
	};

	union {
		/* This is the CPUMPEN Hold Time after READEN Register. */
		__IO uint8_t OTP_TRDCP_VAL;

		struct {
			/*
			 * CPUMPEN Hold Time after READEN.
			 * Note: Default value for 60 MHz system clock.
			 */
			__IO uint8_t OTP_TRDCP : 8;
		} OTP_TRDCP_VAL_b;
	};

	union {
		/* This is the Tst Lock Register. */
		__IO uint8_t OTP_TSTLOCK_VAL;

		struct {
			/*
			 * When this bit is set, WRTEST, BLANKCHECK, and
			 * TESTDEC tests cannot be activated.
			 */
			__IO uint8_t OTP_TSTLOCK : 1;
		} OTP_TSTLOCK_VAL_b;
	};
	__I uint8_t RESERVED8;

	union {
		__IO uint8_t OTP_WRLOCK_VAL[4];
		struct {
			/*
			 * When any of the bits are set, the corresponding
			 * 32byte range in the OTP is not writable.
			 */
			__IO uint32_t OTP_WRLOCK : 32;

		} OTP_WRLOCK_VAL_b;
	};

	union {
		__IO uint8_t OTP_RDLOCK_VAL[4];
		struct {
			/*
			 * When any of the bits are set, the corresponding
			 * 32byte range in the OTP is not readable.
			 */
			__IO uint32_t OTP_RDLOCK : 32;
		} OTP_RDLOCK_VAL_b;
	};
	union {
		__IO uint8_t OTP_WRBYTELOCK_VAL[4];
		struct {
			/*
			 * When any of the bits are set, the corresponding Byte
			 * range in the OTP is not writable from offset 320 -
			 * 351
			 */
			__IO uint32_t OTP_WRBYTLOCK : 32;
		} OTP_WRBYTELOCK_VAL_b;
	};

	union {
		__IO uint8_t OTP_RDBYTELOCK_VAL[4];
		struct {
			/*
			 * When any of the bits are set, the corresponding Byte
			 * range in the OTP is not readable from offset 320 -
			 * 351
			 */
			__IO uint32_t OTP_RDBYTLOCK : 32;
		} OTP_RDBYTELOCK_VAL_b;
	};
} EFUSE_TypeDef;

#define EFUSE ((EFUSE_TypeDef *)EFUSE_BASE)
#define EFUSE_BASE (MEC2016_PERIPH_SPB_BASE + 0x2000UL)
#define MEC2016_PERIPH_BASE (0x40000000UL)

#define MEC2016_PERIPH_SPB_BASE ((MEC2016_PERIPH_BASE + 0x80000UL))

#define OTP_IDX_32K_OSC_TRIM (118ul)

/*
 * 32K oscillator trim
 * Offset 0x14 of VBAT Register bank
 * NOTE: By design PLL will not lock until this register is written!
 */
void rom_prog_32kosc(void)
{
	uint8_t b = rom_otp8(OTP_IDX_32K_OSC_TRIM);

	/*
	 * 32K trim field bits[5:0]
	 * b[6] is PLL inhibit which is set at POR.
	 * we force clearing of this bit by keeping only b[5:0]
	 */
	VBAT_INST->TRIM_CNT_32K = (b & 0xFF);
}

static uint8_t rom_otp8(uint16_t byte_idx)
{
	uint8_t b, sts = 0;

	EFUSE->OTP_PWR_DN = 0;
	EFUSE->OTP_PRGM_MODE = 1;

	/* Program the address bytes to read */
	EFUSE->OTP_ADDR2 = ((uint8_t)(byte_idx & 0xFF));
	EFUSE->OTP_ADDR1 = ((uint8_t)((byte_idx & 0x1F00) >> 8ul));
	/* Set the mode for read command */
	EFUSE->OTP_FUNC_CMD = 1;
	/* Set the Go command to execute */
	EFUSE->OTP_GO_CMD = 1;

	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();
	__NOP();

	do {
		sts = (EFUSE->OTP_FUNC_CMD & 0x01);
	} while (sts);

	do {
		sts = (EFUSE->OTP_STATUS & 0x01);
	} while (sts);

	b = EFUSE->OTP_RD_DATA;
	EFUSE->OTP_INTR_STATUS = 1;
	EFUSE->OTP_PWR_DN = 1;

	return b;
}
