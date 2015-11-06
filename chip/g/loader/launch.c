#include "boot_loader.h"

void _jump_to_address (const void*) __attribute__((section(".bootloader")));

STATIC
int unlockedForExecution(void) {
  uint32_t status = read_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_SB_COMP_STATUS_OFFSET);
  return (status & GLOBALSEC_SB_COMP_STATUS_SB_BL_SIG_MATCH_MASK) != 0;
}

STATIC
void _jump_to_address(const void* addr) {
  write_reg(M3_VTOR_ADDR, (unsigned)addr);  // Set vector base.

  __asm__ volatile("ldr sp, [%0]; \
                    ldr pc, [%0, #4];"
                    :: "r"(addr)
                    : "memory");

  __builtin_unreachable();
}

void tryLaunch(uint32_t adr, size_t max_size) {
  const SignedHeader* hdr = (const SignedHeader*)(adr);

  static struct {
    uint32_t img_hash[SHA256_DIGEST_WORDS];
    uint32_t fuses_hash[SHA256_DIGEST_WORDS];
    uint32_t info_hash[SHA256_DIGEST_WORDS];
  } hashes;

  static uint32_t hash[SHA256_DIGEST_WORDS];
  static uint32_t fuses[FUSE_MAX];
  static uint32_t info[INFO_MAX];

  memset(&hashes, 0, sizeof(hashes));

  // Sanity check image header.
  if (hdr->magic != -1) return;
  if (hdr->image_size > max_size) return;

  // Sanity checks that image belongs at adr.
  if (hdr->ro_base < adr) return;
  if (hdr->ro_max > adr + max_size) return;
  if (hdr->rx_base < adr) return;
  if (hdr->rx_max > adr + max_size) return;

  VERBOSE("considering image at 0x%8x\n", hdr);
  VERBOSE("image size 0x%8x\n", hdr->image_size);
  VERBOSE("hashing from 0x%8x to 0x%8x\n",
          &hdr->tag, adr + hdr->image_size);

  // Setup candidate execution region 1 based on header information.
  // TODO: harden against glitching: multi readback, check?
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_CPU0_I_STAGING_REGION1_BASE_ADDR_OFFSET,
            hdr->rx_base);
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_CPU0_I_STAGING_REGION1_SIZE_OFFSET,
            hdr->rx_max - hdr->rx_base - 1);
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_CPU0_I_STAGING_REGION1_CTRL_OFFSET,
            GLOBALSEC_CPU0_I_STAGING_REGION1_CTRL_EN_MASK |
            GLOBALSEC_CPU0_I_STAGING_REGION1_CTRL_RD_EN_MASK);

  MEASURE("sha  ",
           hwSHA256((const uint8_t*)(&hdr->tag),
                    hdr->image_size - offsetof(SignedHeader, tag), hashes.img_hash));

  INFO("img_hash  :%h\n", SHA256_DIGEST_BYTES, hashes.img_hash);

  // Sense fuses into RAM array; hash array.
  // TODO: is this glitch resistant enough? Certainly is simple..
  for (int i = 0; i < FUSE_MAX; ++i) {
    fuses[i] = FUSE_IGNORE;
  }
  for (int i = 0; i < FUSE_MAX; ++i) {
    // For the fuses the header cares about, read their values into the map.
    if (hdr->fusemap[i>>5] & (1 << (i&31))) {
      // BNK0_INTG_CHKSUM is the first fuse and as such the best reference
      // to the base address of the fuse memory map.
      fuses[i] = read_reg(FUSE0_BASE_ADDR+FUSE_BNK0_INTG_CHKSUM_OFFSET+i*4);
    }
  }

  hwSHA256((const uint8_t*)fuses, sizeof(fuses), hashes.fuses_hash);

  INFO("fuses_hash:%h\n", SHA256_DIGEST_BYTES, hashes.fuses_hash);

  // Sense info into RAM array; hash array.
  for (int i = 0; i < INFO_MAX; ++i) {
    info[i] = INFO_IGNORE;
  }
  for (int i = 0; i < INFO_MAX; ++i) {
    if (hdr->infomap[i>>5] & (1 << (i&31))) {
      uint32_t val = 0;
      int retval = flash_info_read(i + INFO_MAX, &val);  // read 2nd bank of info
      info[i] ^= val ^ retval;
    }
  }

  hwSHA256((const uint8_t*)info, sizeof(info), hashes.info_hash);
  INFO("info_hash :%h\n", SHA256_DIGEST_BYTES, hashes.info_hash);

  // Hash our set of hashes to get final hash.
  hwSHA256((const uint8_t*)&hashes, sizeof(hashes), hash);

  // Write measured hash to unlock register to try and unlock execution.
  // This would match when doing warm-boot from suspend, so we can avoid the
  // slow RSA verify.
  for (int i = 0; i < SHA256_DIGEST_WORDS; ++i) {
    write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_SB_BL_SIG0_OFFSET+i*4, hash[i]);
  }

  // Unlock attempt.
  // Value written is irrelevant, as long as something is written.
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_SIG_UNLOCK_OFFSET, 1);

  if (!unlockedForExecution()) {
    // Assume warm-boot failed; do full RSA verify.
    MEASURE("rsa  ", LOADERKEY_verify(hdr->keyid, hdr->signature, hash));

    if (unlockedForExecution()) {
      // PWRDN_SCRATCH* should be write-locked, tied to successful SIG_MATCH.
      // Thus ARM is only able to write this hash if signature was correct.
      for (int i = 0; i < SHA256_DIGEST_WORDS; ++i) {
        // TODO: verify written values as glitch protection?
        write_reg(PMU_BASE_ADDR+PMU_PWRDN_SCRATCH8_OFFSET+i*4, hash[i]);
      }
    }
  }

  if (unlockedForExecution()) {
    cyclecounter_print();

    // Write PMU_PWRDN_SCRATCH_LOCK1_OFFSET to lock against rewrites.
    // TODO: glitch resist
    write_reg(PMU_BASE_ADDR+PMU_PWRDN_SCRATCH_LOCK1_OFFSET, 1);

    // Drop software level to stop SIG_MATCH from future write-unlocks.
    // TODO: glitch detect / verify?
    write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_SOFTWARE_LVL_OFFSET, 0x33);

    // Write hdr->tag, hdr->epoch_ to KDF engine FWR[0..7]
    for (int i = 0; i < ARRAYSIZE(hdr->tag); ++i) {
      write_reg(KEYMGR0_BASE_ADDR+KEYMGR_HKEY_FWR0_OFFSET+i*4, hdr->tag[i]);
    }
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_HKEY_FWR7_OFFSET, hdr->epoch_);


    // Crank keyladder

    if (!(read_reg(FUSE0_BASE_ADDR+FUSE_FLASH_PERSO_PAGE_LOCK_OFFSET) &
          (FUSE_HIK_CREATE_LOCK_VAL_MASK << FUSE_HIK_CREATE_LOCK_VAL_LSB))) {
      INFO("Re-reading INFO0\n");
      // Needed because FUSE_FLASH_PERSO_PAGE_LOCK_OFFSET isn't blown)
      //
      // wipe out the flash secrets saved in keymgr and re-read info0
      write_reg(KEYMGR0_BASE_ADDR+KEYMGR_FLASH_RCV_WIPE_OFFSET, 1);
      write_reg(FLASH0_BASE_ADDR+FLASH_FSH_ENABLE_INFO0_SHADOW_READ_OFFSET, 1);
    }

    // Turn up random stalls for SHA
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_SHA_RAND_STALL_CTL_FREQ_OFFSET, 0);  // 0:50%

    uint32_t major = hdr->major_;

    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_FW_MAJOR_VERSION_OFFSET, major);

    // Lock FWR (NOTE: needs to happen after writing major!)
    // TODO: glitch protect?
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_FWR_VLD_OFFSET, 2);
    write_reg(KEYMGR0_BASE_ADDR+KEYMGR_FWR_LOCK_OFFSET, 1);

    const uint32_t FAKE_rom_hash[8] = {1,2,3,4,5,6,7,8};
    hwKeyLadderStep(40, FAKE_rom_hash);

    // TODO: do cert #40 and lock in ROM?
    write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_HIDE_ROM_OFFSET, 1);

    // Sniff key ladder just as app would
    hwKeyLadderStep(0, NULL);
    hwKeyLadderStep(3, NULL);
    hwKeyLadderStep(4, NULL);
    hwKeyLadderStep(5, NULL);
    hwKeyLadderStep(7, NULL);
    hwKeyLadderStep(13, NULL);
    hwKeyLadderStep(18, NULL);
    for (int i = 0; i < (255 - major - 1); ++i) {
      hwKeyLadderStep(21, NULL);
    }
    hwKeyLadderStep(22, NULL);

    uint32_t value[8];
    for (int i = 0; i < 8; ++i) {
      value[i] = read_reg( KEYMGR0_BASE_ADDR+KEYMGR_HKEY_FRR0_OFFSET + i*4);
    }

    uart_printf("FRK[%3d]: %h\n", major, 32, value);

    // crank manually down to key 0, if we are not there already.
    for (int more = major; more > 0; --more) {
      hwSHA256(value, 32, value);
      uart_printf("FRK[%3d]: %h\n", more - 1, 32, value);
    }

    // TODO: bump runlevel(s) according to signature header

    // Flash write protect entire image area (to guard signed blob)
    // REGION0 protects boot_loader, use REGION1 to protect app
    write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_FLASH_REGION1_BASE_ADDR_OFFSET, adr);
    write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_FLASH_REGION1_SIZE_OFFSET, hdr->image_size - 1);
    write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_FLASH_REGION1_CTRL_OFFSET,
              GLOBALSEC_FLASH_REGION1_CTRL_EN_MASK |
              GLOBALSEC_FLASH_REGION1_CTRL_RD_EN_MASK);

    // TODO: lock FLASH_REGION 1?

    disarmRAMGuards();

#if 0
    // Hard code drop level
    write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_CPU0_S_PERMISSION_OFFSET, 0x3c);

    // TODO review: does this make sense to drop here?
    write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_DDMA0_PERMISSION_OFFSET, 0x3c);
#endif

    INFO("Valid image found at 0x%08x, jumping\n", hdr);
    while (!uart_tx_done())
      ;

    _jump_to_address(&hdr[1]);
  }
}
