/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MPU module for Chrome EC */

#include "mpu.h"
#include "console.h"
#include "registers.h"
#include "task.h"
#include "util.h"

/* Region assignment. 7 as the highest, a higher index has a higher priority.
 * For example, using 7 for .iram.text allows us to mark entire RAM XN except
 * .iram.text, which is used for hibernation. */
enum mpu_region {
	REGION_IRAM = 0,          /* For internal RAM */
	REGION_IRAM2 = 1,         /* For internal RAM, second region */
	REGION_FLASH_MEMORY = 2,  /* For flash memory */
	REGION_IRAM_TEXT = 7      /* For *.(iram.text) */
};

/**
 * Update a memory region.
 *
 * region: index of the region to update
 * addr: base address of the region
 * size_bit: size of the region in power of two.
 * attr: attribute bits. Current value will be overwritten if enable is true.
 * enable: enables the region if non zero. Otherwise, disables the region.
 *
 * Based on 3.1.4.1 'Updating an MPU Region' of Stellaris LM4F232H5QC Datasheet
 */
static void mpu_update_region(uint8_t region, uint32_t addr, uint8_t size_bit,
			      uint16_t attr, uint8_t enable, uint8_t srd)
{
	asm volatile("isb; dsb;");

	MPU_NUMBER = region;
	MPU_SIZE &= ~1;					/* Disable */
	if (enable) {
		MPU_BASE = addr;
		MPU_ATTR = attr;
		MPU_SIZE = (srd << 8) | (size_bit - 1) << 1 | 1; /* Enable */
	}

	asm volatile("isb; dsb;");
}

/**
 * Configure a region
 *
 * region: index of the region to update
 * addr: Base address of the region
 * size: Size of the region in bytes
 * attr: Attribute bits. Current value will be overwritten if enable is set.
 * enable: Enables the region if non zero. Otherwise, disables the region.
 *
 * Returns EC_SUCCESS on success or EC_ERROR_INVAL if a parameter is invalid.
 */
static int mpu_config_region(uint8_t region, uint32_t addr, uint32_t size,
			     uint16_t attr, uint8_t enable)
{
	int size_bit = 0;
	uint8_t blocks, srd;

	if (!size)
		return EC_SUCCESS;

	/* Bit position of first '1' in size */
	size_bit = 32 - __builtin_clz(size);
	/* Min. region size is 32 bytes */
	if (size_bit < 5)
		return -EC_ERROR_INVAL;

	/* If size is a power of 2 then represent it with a single MPU region */
	if (POWER_OF_TWO(size)) {
		mpu_update_region(region, addr, size_bit, attr, enable, 0);
		return EC_SUCCESS;
	}

	/* Support multi-region protection for IRAM only */
	if (region != REGION_IRAM)
		return -EC_ERROR_INVAL;

	/* Verify we can represent range with 2 regions */
	if (size & ~(0x3f << (size_bit - 6)))
		return -EC_ERROR_INVAL;
	if (__builtin_ctz(size) < 4)
		return -EC_ERROR_INVAL;

	/*
	 * Round up size of first region to power of 2.
	 * Calculate the number of fully occupied blocks (block size =
	 * region size / 8) in the first region.
	 */
	blocks = size >> (size_bit - 3);
	/* Represent occupied blocks with srd mask (inverted for 0 = occupied */
	srd = (1 << blocks) - 1;
	mpu_update_region(region, addr, size_bit + 1, attr, enable, ~srd);

	/*
	 * Second protection region (if necessary) begins at the first block
	 * we marked unoccupied in the first region.
	 * Size of the second region is the block size of first region.
	 */
	addr += (1 << (size_bit - 3)) * blocks;
	/*
	 * Now represent occupied blocks in the second region. It's possible
	 * that the first region completely represented the occupied area, if
	 * so then no second protection region is required.
	 */
	srd = (1 << ((size >> (size_bit - 6)) & 0x7)) - 1;
	if (srd)
		mpu_update_region(region + 1, addr, size_bit - 2, attr, enable,
				  ~srd);

	return EC_SUCCESS;
}

/**
 * Set a region non-executable and read-write.
 *
 * region: index of the region
 * addr: base address of the region
 * size: size of the region in bytes
 * texscb: TEX and SCB bit field
 */
static int mpu_lock_region(uint8_t region, uint32_t addr, uint32_t size,
			   uint8_t texscb)
{
	return mpu_config_region(region, addr, size,
				 MPU_ATTR_XN | MPU_ATTR_RW_RW | texscb, 1);
}

/**
 * Set a region executable and read-write.
 *
 * region: index of the region
 * addr: base address of the region
 * size: size of the region in bytes
 * texscb: TEX and SCB bit field
 */
static int mpu_unlock_region(uint8_t region, uint32_t addr, uint32_t size,
			     uint8_t texscb)
{
	return mpu_config_region(region, addr, size,
				 MPU_ATTR_RW_RW | texscb, 1);
}

void mpu_enable(void)
{
	MPU_CTRL |= MPU_CTRL_PRIVDEFEN | MPU_CTRL_HFNMIENA | MPU_CTRL_ENABLE;
}

void mpu_disable(void)
{
	MPU_CTRL &= ~(MPU_CTRL_PRIVDEFEN | MPU_CTRL_HFNMIENA | MPU_CTRL_ENABLE);
}

uint32_t mpu_get_type(void)
{
	return MPU_TYPE;
}

int mpu_protect_ram(void)
{
	int ret;
	ret = mpu_lock_region(REGION_IRAM, CONFIG_RAM_BASE,
			      CONFIG_DATA_RAM_SIZE, MPU_ATTR_INTERNAL_SRAM);
	if (ret != EC_SUCCESS)
		return ret;
	ret = mpu_unlock_region(
		REGION_IRAM_TEXT, (uint32_t)&__iram_text_start,
		(uint32_t)(&__iram_text_end - &__iram_text_start),
		MPU_ATTR_INTERNAL_SRAM);
	return ret;
}

int mpu_lock_ro_flash(void)
{
	return mpu_lock_region(REGION_FLASH_MEMORY, CONFIG_RO_MEM_OFF,
			       CONFIG_RO_SIZE, MPU_ATTR_FLASH_MEMORY);
}

int mpu_lock_rw_flash(void)
{
	return mpu_lock_region(REGION_FLASH_MEMORY, CONFIG_RW_MEM_OFF,
			       CONFIG_RW_SIZE, MPU_ATTR_FLASH_MEMORY);
}

int mpu_pre_init(void)
{
	int i;

	if (mpu_get_type() != 0x00000800)
		return EC_ERROR_UNIMPLEMENTED;

	mpu_disable();
	for (i = 0; i < 8; ++i)
		mpu_config_region(i, CONFIG_RAM_BASE, CONFIG_RAM_SIZE, 0, 0);

	return EC_SUCCESS;
}
