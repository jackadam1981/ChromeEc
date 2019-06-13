# Fingerprint Firmware (FPMCU)

[TOC]

*** note
NOTE: The build commands assume you are in the `~/trunk/src/platform/ec`
directory inside the chroot.
***

*** note
WARNING: When switching branches in the EC codebase, you probably want to nuke
the `build` directory or at least the board you're working on: `rm -rf
build/<board>` to prevent compilation errors.
***

## Software

The main source code for fingerprint sensor functionality lives in the
[`common/fpsensor`](https://chromium.googlesource.com/chromiumos/platform/ec/+/refs/heads/master/common/fpsensor/)
directory.

## Hardware

The following "boards" (specified by the `BOARD` environment variable when
building the EC code) are for fingerprint:

*   [`nocturne_fp`](https://chromium.googlesource.com/chromiumos/platform/ec/+/refs/heads/master/board/nocturne_fp/)
    (STM32H743)
*   [`nami_fp`](https://chromium.googlesource.com/chromiumos/platform/ec/+/refs/heads/master/board/nami_fp/)
    (ST32H743)
*   [`hatch_fp`](https://chromium.googlesource.com/chromiumos/platform/ec/+/refs/heads/master/board/hatch_fp/)
    (STM32F412)
    * Support for the STM32F412 for the FPMCU is not yet fully complete, but it
    is functional enough for testing.

## Building FPMCU Firmware Locally

### See `Makefile` target options

```bash
(chroot) ~/trunk/src/platform/ec $ make help
```

### Build

Replace `<BOARD_NAME>` in the command below with the fingerprint MCU that you are
targeting (e.g., `nocturne_fp`, `nami_fp`, `hatch_fp`).

```bash
(chroot) ~/trunk/src/platform/ec $ make BOARD=<BOARD_NAME> -j
```

### Verbose Build output

Use `V=1` to see the complete compiler output (all flags).

```bash
(chroot) ~/trunk/src/platform/ec $ make V=1 BOARD=nocturne_fp -j
```

## Building all EC firmware (before "repo upload")

Before uploading a change to Gerrit via `repo upload`, you'll need to build
*all* the boards in the EC codebase to make sure your changes do not break any
others.

*** note
NOTE: If you forget to do this, do not worry. `repo upload` will warn you and
prevent you from uploading.
***

```bash
(chroot) ~/trunk/src/platform/ec $ make buildall -j
```

## Build tests

```bash
(chroot) ~/trunk/src/platform/ec $ make BOARD=nocturne_fp tests-nocturne_fp -j
```

## Build ectool

```bash
(chroot) ~/trunk/src/platform/ec $ make BOARD=nocturne_fp utils-host -j
```

## Build and run the `host_command` fuzz test

```bash
(chroot) ~/trunk/src/platform/ec $ make BOARD=nocturne_fp run-host_command_fuzz
```

# Fingerprint Firmware Updates and Flashing

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
[hardware write protection](../write_protection.md)).

## Factory / RMA / Development Updates

### `flash_fp_mcu`

NOTE: This tool is really just for us to use during development or during the
RMA flow (must go through finalization again in that case). We never update RO
in the field (can’t by design).

[`flash_fp_mcu`](https://chromium.googlesource.com/chromiumos/platform/ec/+/master/board/nocturne_fp/flash_fp_mcu)
enables spidev and toggles some GPIOs to put the FPMCU (STM32) into bootloader
mode. At that point it uses [`stm32mon`](#stm32mon) to rewrite the entire flash
(both RO and RW). This will only work if
[HW write protect](../write_protection.md)
is disabled.

### `stm32mon` {#stm32mon}

*** promo
TODO(tomhughes): Add docs.
***

# Fingerprint Firmware Keys and Signing

## Keys

The `RO` section of the fingerprint firmware contains the public portion of the
key used to sign the RW firmware. It uses the public key to validate the
signature of the RW firmware before jumping to it. It is not possible to
update the public key stored in the RO firmware once a device has been shipped
(i.e., once the hardware write protect is enabled).

*** promo
TODO(tomhughes): Add details about different types of keys (`dev`, `premp`,
`mp`, etc).
***

### Resources

*   https://sites.google.com/a/google.com/chromeos/resources/engineering/releng/signer-documentation
*   https://sites.google.com/a/google.com/chromeos/paygen---payload
*   https://b.corp.google.com/issues/77882970

## Signing

[`futility`](https://chromium.googlesource.com/chromiumos/platform/vboot_reference/+/master/futility/)
is used to sign EC firmware. There’s a wrapper script around it for signing
called
[`sign_official_build.sh`](https://chromium.googlesource.com/chromiumos/platform/vboot_reference/+/master/scripts/image_signing/sign_official_build.sh).

### Key ID

The output of `futility show` will show a `Public Key File` and `Signature`
section, each of which have an `ID` field. This ID lets you match the key to the
signature in case there is more than one.
[It’s just a sha1sum of the public key,](https://chromium.googlesource.com/chromiumos/platform/vboot_reference/+/e7db36856ce418552637d1981c173d22dfe5bf39/firmware/2lib/include/2id.h#5)
so it lets you
[uniquely identify the key being used](https://chromium.googlesource.com/chromiumos/platform/vboot_reference/+/e7db36856ce418552637d1981c173d22dfe5bf39/firmware/2lib/include/2rsa.h#14).

If you have the key (e.g., in PEM format), you can compute the `ID` with the
`futility show` command:

```bash
(chroot) $ futility show ./path/to/key.pem
```

#### Example

If you are building the `hatch_fp` "board" on your local machine (which signs
the resulting `ec.bin` with the `dev` key, you can check the `ID` with:

```bash
(chroot)$ futility show board/hatch_fp/dev_key.pem
```

```
Private Key file:      board/hatch_fp/dev_key.pem
  Key length:          3072
  Key sha1sum:         61382804da86b4156d666cc9a976088f8b647d44
```

```bash
(chroot)$ futility show build/hatch_fp/ec.bin
```

```
Public Key file:       build/hatch_fp/ec.bin
  Vboot API:           2.1
  Desc:                ""
  Signature Algorithm: 7 RSA3072EXP3
  Hash Algorithm:      2 SHA256
  Version:             0x00000001
  ID:                  61382804da86b4156d666cc9a976088f8b647d44
Signature:             build/hatch_fp/ec.bin
  Vboot API:           2.1
  Desc:                ""
  Signature Algorithm: 7 RSA3072EXP3
  Hash Algorithm:      2 SHA256
  Total size:          0x1b8 (440)
  ID:                  61382804da86b4156d666cc9a976088f8b647d44
  Data size:           0x2864c (165452)
Signature verification succeeded.
```

### Showing Key ID (fingerprint) for running FW

[Asked on chromeos-chatty-firmware](https://groups.google.com/a/google.com/d/msg/chromeos-chatty-firmware/ZSg423wsFPg/26UbdGwjFQAJ)
about adding an EC command to show the Key ID (fingerprint) from the RO version.
This would make it a lot easier during both development and testing.
