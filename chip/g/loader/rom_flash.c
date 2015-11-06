/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "debug_printf.h"
#include "setup.h"
#include "rom_flash.h"

static int _flash_error(void)
{
	int retval = GREG32(FLASH, FSH_ERROR);

	if (!retval)
		return 0;

	debug_printf("Register FLASH_FSH_ERROR is not zero (found %x).\n"
		     "Will read again to verify FSH_ERROR was cleared "
		     "and then continue...\n", retval);

	retval = GREG32(FLASH, FSH_ERROR);
	if (retval)
		debug_printf("ERROR: Read to FLASH_FSH_ERROR (%x) "
			     "did not clear it\n", retval);

	return retval;
}

/* Verify the flash controller is awake. */
static int _check_flash_is_awake(void)
{
	int retval;

	GREG32(FLASH, FSH_TRANS) = 0xFFFFFFFF;
	retval = GREG32(FLASH, FSH_TRANS);
	GREG32(FLASH, FSH_TRANS) =  0x0;

	if (retval == 0) {
		debug_printf("ERROR:FLASH Controller seems unresponsive. "
			     "Did you make sure to run 'reseth'?\n");
		return E_FL_NOT_AWAKE;
	}

	return 0;
}

/* Send cmd to flash controller. */
static int _flash_cmd(uint32_t fidx, uint32_t cmd) {
	int cnt, retval;

	/* Activate controller. */
	GREG32(FLASH, FSH_PE_EN) = FSH_OP_ENABLE;
	GREG32_ADDR(FLASH, FSH_PE_CONTROL0)[fidx] = cmd;

	/* wait on FSH_PE_EN (means the operation started) */
	cnt = 500;  /* TODO(mschilder): pick sane value. */

	do {
		retval = GREG32(FLASH, FSH_PE_EN);
	} while (retval && cnt--);

	if (retval) {
		debug_printf("ERROR: FLASH_FSH_PE_EN never went to 0, is "
			     "0x%x after timeout\n", retval);
		return E_FL_TIMEOUT;
	}

	/*
	 * wait 100us before checking FSH_PE_CONTROL (means the operation
	 * ended)
	 */
	cnt = 1000000;
	do {
		retval = GREG32_ADDR(FLASH, FSH_PE_CONTROL0)[fidx];
	} while (retval && --cnt);

	if (retval) {
		debug_printf("ERROR: FLASH_FSH_PE_CONTROL%d is 0x%x "
			     "after timeout\n", fidx, retval);
		GREG32_ADDR(FLASH, FSH_PE_CONTROL0)[fidx] = 0;
		return E_FL_TIMEOUT;
	}

	return 0;
}

int flash_info_read(uint32_t offset, uint32_t* dst) {
	int retval;

	/* Make sure flash controller is awake. */
	retval = _check_flash_is_awake();
	if (retval)
		return retval;

	GWRITE_FIELD(FLASH, FSH_TRANS, OFFSET, offset);
	GWRITE_FIELD(FLASH, FSH_TRANS, MAINB, 1);
	GWRITE_FIELD(FLASH, FSH_TRANS, SIZE, 1);

	retval = _flash_cmd(1, FSH_OP_READ);
	if (retval)
		return retval;

	if (_flash_error())
		return E_FL_ERROR;

	if (!retval)
		*dst = GREG32(FLASH, FSH_DOUT_VAL1);

	return retval;
}

#if 0
#ifdef NO_UART
#define ALWAYS_PRINTF(...)
#else
#define ALWAYS_PRINTF(...) VERBOSE(__VA_ARGS__)
#endif

#ifdef DEBUG
#define DEBUG_PRINTF(...) ALWAYS_PRINTF(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif


// Do not test the info blocks in HTOL since we are using those for statistics
STATIC const uint32_t flash_main_strt[] = {FLASH0_BASE, FLASH1_BASE};
STATIC const uint32_t flash_main_end[]  = {0x7FFFF, 0xBFFFF};

// internal task to program the flash(es).
STATIC int program_flash_internal(uint32_t fidx, uint32_t offset,
                                  const uint32_t* data, uint32_t size) {
  int retval = 0;
  uint32_t prog_count = 0;
  uint32_t success = 0;
  uint32_t done = 0;
  uint32_t flash_base;
  uint32_t addr;

  // Make sure flash controller is awake
  retval = _check_flash_is_awake();
  if (retval) return retval;

  // Some error checking
  if (size > 32 || size < 1) {
    DEBUG_PRINTF("ERROR: FLASH program limited to 1-32 words\n");
    return E_FL_BAD_SIZE;
  }
  if (data == 0) {
    DEBUG_PRINTF("ERROR: Bad data pointer\n");
    return E_FL_BAD_PTR;
  }
  if (fidx > num_flashes) {
    DEBUG_PRINTF("ERROR: fidx %1d is greater than the number of flashes\n", fidx);
    return E_FL_BAD_BANK;
  }
  // TODO(mschilder): Add assertion on offset bigger than flash size (find flash size from xml)

  // Calculate the address from the offset and fidx
  flash_base = flash_main_strt[fidx];
  addr = flash_base + offset*4;

  DEBUG_PRINTF("FLASH WRITE START --> ADDR=0x%x FIDX:%d OFFSET:0x%x SIZE:%d FIRST-WORD:0x%x\n",
               addr, fidx, offset, size, data[0]);

  // calculate register values for transaction
  uint32_t size_minus_one = size - 1;

  // Send the register values, send the data
  write_reg(FLASH0_BASE_ADDR + FLASH_FSH_TRANS_OFFSET,
            ((offset         << FLASH_FSH_TRANS_OFFSET_LSB) & FLASH_FSH_TRANS_OFFSET_MASK) |
            ((size_minus_one << FLASH_FSH_TRANS_SIZE_LSB)   & FLASH_FSH_TRANS_SIZE_MASK  ));

  // Latch data
  for (uint32_t didx = 0; didx < size; didx++) {
    write_reg(FLASH0_BASE_ADDR + FLASH_FSH_WR_DATA0_OFFSET + (4*didx), data[didx]);
  }

  for (prog_count = 0; prog_count < _max_prog_cycles; prog_count++) {
    DEBUG_PRINTF("PROGRAM FLASH IS SENDING PROGRAM PULSE #%d\n", prog_count);

    retval = _flash_cmd(fidx, FSH_OP_PROGRAM);
    if (retval) return retval;

    // Read the error register
    if (!_flash_error()) {
      if (success) {
        // we already wrote one extra time so nothing more to do
        done = 1;
        break;
      } else {
        // success but need to do one last iteration
        success = 1;
      }
    }
  }

  // Declare victory or die
  if (success && done) {
    DEBUG_PRINTF("PROGRAM FLASH SUCCEEDED! ADDR:0x%x SIZE:%d FIRST WORD:0x%x TOTAL NUM PULSES:%d\n", addr, size, data[0], prog_count);
  } else if (success) {
    DEBUG_PRINTF("FLASH PROGRAMMING SUCCEEDED ONLY ON LAST ATTEMPT (NO 'EXTRA' PULSE DONE): ADDR:0x%x SIZE:%d FIRST WORD:0x%x MAX PULSES:%d\n", addr, size, data[0], prog_count);
  } else {
    DEBUG_PRINTF("FLASH PROGRAMMING FAILED: ADDR:0x%x SIZE:%d FIRST WORD:0x%x MAX-PULSES REACHED:%d\n", addr, size, data[0], _max_prog_cycles);
    return E_FL_WRITE_FAIL;
  }

  return 0;
}

// task to program the flash(s) if the flash index and main/info are known.
int flash_write(uint32_t fidx, uint32_t offset, const uint32_t* data, uint32_t size) {
  int retval;
  uint32_t start = 0;

  // align the programming to 32 words
  if ((offset % 32) + size >= 32) {
    retval = program_flash_internal(fidx, offset, data+start, 32 - (offset % 32));
    if (retval) return retval;
    // update indices before next iteration
    start = 32 - (offset % 32);
    size = size - (32 - (offset % 32));
    offset = offset + (32 - (offset % 32));
  }

  // break the rest of the array to pieces of 32 elements and write them
  while (size > 32) {
    retval = program_flash_internal(fidx, offset, data+start, 32);
    if (retval) return retval;
    // update indices before next iteration
    start = start + 32;
    size = size - 32;
    offset = offset + 32;
  }

  // Write the last chunk of the array
  if (size > 0) {
    retval = program_flash_internal(fidx, offset, data+start, size);
    if (retval) return retval;
  }

  return 0;
}

// task to erase one page in the flash when the flash index and main/info are known.
int flash_erase(uint32_t fidx, uint32_t page) {
  int retval = 0;
  uint32_t offset = page * words_per_page;
  uint32_t erase_count;
  uint32_t done = 0;
  uint32_t flash_base = 0;
  uint32_t flash_top = 0;

  // Make sure flash controller is awake
  retval = _check_flash_is_awake();
  if (retval) return retval;

  // Calculate the address from the offset and fidx
  flash_base = flash_main_strt[fidx];
  flash_top = flash_main_end[fidx];
  uint32_t addr = flash_base + offset*4;

  // Error checking
  if (fidx > num_flashes)
    DEBUG_PRINTF("ERROR: Cannot find FLASH index %d\n", fidx);
  if (addr > flash_top)
    DEBUG_PRINTF("ERROR: Cannot find FLASH page %d in flash index %d\n",
                  page, fidx);

  // Send the register values
  write_reg(FLASH0_BASE_ADDR + FLASH_FSH_TRANS_OFFSET,
            ((offset << FLASH_FSH_TRANS_OFFSET_LSB) & FLASH_FSH_TRANS_OFFSET_MASK) |
            ((0x0    << FLASH_FSH_TRANS_SIZE_LSB)   & FLASH_FSH_TRANS_SIZE_MASK  ));

  // Start sending erase pulses
  for (erase_count = 0; erase_count < _max_erase_cycles; erase_count++) {
    DEBUG_PRINTF("ERASE FLASH IS SENDING ERASE PULSE #%d\n", erase_count);

    retval = _flash_cmd(fidx, FSH_OP_ERASE);
    if (retval) return retval;

    // Read the error register
    if (!_flash_error()) {
      done = 1;
      break;
    }
  }

  // Declare victory or die
  if (done) {
    DEBUG_PRINTF("ERASE FLASH SUCCEEDED! FLASH IDX:%d ADDRESS:0x%x NUM PULSES:%d\n", fidx, addr, erase_count);
  } else {
    DEBUG_PRINTF("FLASH ERASE FAILED: FLASH IDX:%d ADDRESS:%d MAX PULSES:%d\n", fidx, addr, _max_erase_cycles);
    return E_FL_ERASE_FAIL;
  }

  return 0;
}

// Task to bulkerase the entire flash
int flash_wipe(uint32_t fidx) {
  int retval = 0;
  uint32_t bulkerase_count = 0;
  uint32_t done = 0;
  uint32_t offset = 0;

  // Make sure flash controller is awake
  retval = _check_flash_is_awake();
  if (retval) return retval;

#ifdef FPGA_SIM
  // See b:/23008572
  // TODO: make sure to not have this go to silicon!
  write_reg(FLASH0_BASE_ADDR + FLASH_FSH_TIMING_BULKERASE_MAS1_LAST_CYC_OFFSET, 0x11170);
  write_reg(FLASH0_BASE_ADDR + FLASH_FSH_TIMING_BULKERASE_TOTAL_CYC_OFFSET, 0x11190);
#endif

  DEBUG_PRINTF("BULKERASE FLASH STARTED: FLASH IDX %d ERASE_MAIN_ONLY %d\n", fidx, erase_main_only);

  // Send the register values
  write_reg(FLASH0_BASE_ADDR + FLASH_FSH_TRANS_OFFSET,
            ((offset << FLASH_FSH_TRANS_OFFSET_LSB) & FLASH_FSH_TRANS_OFFSET_MASK) |
            ((0x0    << FLASH_FSH_TRANS_SIZE_LSB)   & FLASH_FSH_TRANS_SIZE_MASK  ));

  // Start sending erase pulses
  for (bulkerase_count = 0; bulkerase_count < _max_bulkerase_cycles; bulkerase_count++) {
    DEBUG_PRINTF("BULKERASE FLASH IS SENDING BULKERASE PULSE #%d\n", bulkerase_count);

    retval = _flash_cmd(fidx, FSH_OP_BULKERASE);
    if (retval) return retval;

    // Read the error register
    if (!_flash_error()) {
      done = 1;
      break;
    }
  }

  // Declare victory or die
  if (done) {
    DEBUG_PRINTF("BULKERASE FLASH SUCCEEDED! FLASH IDX:%d NUM PULSES:%d\n", fidx, bulkerase_count + 1);
  } else {
    DEBUG_PRINTF("FLASH BULKERASE FAILED: FLASH IDX:%d MAX PULSES:%d\n", fidx, _max_bulkerase_cycles);
    return E_FL_WIPE_FAIL;
  }

  return 0;
}

#endif
