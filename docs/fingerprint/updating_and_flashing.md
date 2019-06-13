# Fingerprint Firmware Updates and Flashing

[TOC]

## Production Updates

### `fp_updater.sh`

`fp_updater.sh` is a wrapper around `flashrom` and requires already-functioning
RO firmware running on the FPMCU. It’s meant to be used in production to update the RW
firmware.

It's also possible to use it to update the RO firmware if you disable *both* HW
and SW write protect, which we use for updating development devices that do not
have write protect enabled (dogfood devices, EVT, etc.)

In production, only the RW portion of the firmware can be updated (unless the
user disables
[hardware write protection](./write_protection.md)).

## Factory / RMA / Development Updates

### `flash_fp_mcu`

NOTE: This tool is really just for us to use during development or during the
RMA flow (must go through finalization again in that case). We never update RO
in the field (can’t by design).

[`flash_fp_mcu`](https://chromium.googlesource.com/chromiumos/platform/ec/+/master/board/nocturne_fp/flash_fp_mcu)
enables spidev and toggles some GPIOs to put the FPMCU (STM32) into bootloader
mode. At that point it uses [`stm32mon`](#stm32mon) to rewrite the entire flash
(both RO and RW). This will only work if
[HW write protect](./write_protection.md)
is disabled.

### `stm32mon` {#stm32mon}

*** promo
TODO(tomhughes): Add docs.
***
