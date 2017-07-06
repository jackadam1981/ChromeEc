
/*
 * MEC1701H revision B0 ROM API addresses
 */

PROVIDE(_ROM_Log_Size = 0x00000100);
PROVIDE(rom_log_get_ptr = 0x00000d6d);

PROVIDE(rom_qmspi_is_done = 0x00006d31);
PROVIDE(rom_qmspi_start_dma = 0x00006f01);
PROVIDE(rom_qmspi_init = 0x00006f5d);
PROVIDE(rom_qmspi_cfg_read_dma = 0x00006f6d);

PROVIDE(rom_sha256_h0 = 0x0000a37c);
PROVIDE(rom_aes_sha_power = 0x00006f91);
PROVIDE(rom_aes_sha_reset = 0x00006fb5);
PROVIDE(rom_sha_is_done = 0x0000881d);
PROVIDE(rom_sha_start = 0x000087b9);
PROVIDE(rom_hash_busy = 0x000087dd);
PROVIDE(rom_hash_status = 0x000087ed);
PROVIDE(rom_hash_iclr = 0x00008da1);
PROVIDE(rom_sha12_finalize = 0x000089dd);
PROVIDE(rom_sha12_init = 0x00008861);
PROVIDE(rom_sha12_update = 0x0000891d);
PROVIDE(rom_sha256_raw = 0x00008add);
PROVIDE(rom_sha256_raw_init = 0x00008b39);
PROVIDE(rom_sha256_raw_update = 0x00008b5d);
PROVIDE(rom_sha35_init = 0x00008bb5);
PROVIDE(rom_sha35_update = 0x00008bfd);
PROVIDE(rom_sha35_finalize = 0x00008cbd);
PROVIDE(rom_sha_init = 0x00008db5);
PROVIDE(rom_sha_update = 0x00008e71);
PROVIDE(rom_sha_final = 0x00008ef5);

PROVIDE(rom_rng_power = 0x000073f1);
PROVIDE(rom_rng_reset = 0x00007389);
PROVIDE(rom_rng_mode = 0x00007425);
PROVIDE(rom_rng_is_on = 0x00007405);
PROVIDE(rom_rng_start = 0x000073c5);
PROVIDE(rom_rng_stop = 0x000073d5);
PROVIDE(rom_rng_get_fifo_level = 0x000073e5);
PROVIDE(rom_rng_get_bytes = 0x00007441);
PROVIDE(rom_rng_get_words = 0x00007489);
