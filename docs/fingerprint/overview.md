# Fingerprint Firmware Overview

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

### nocturne_fp

```bash
(chroot) ~/trunk/src/platform/ec $ make BOARD=nocturne_fp -j
```

### nami_fp

```bash
(chroot) ~/trunk/src/platform/ec $ make BOARD=nami_fp -j
```

### hatch_fp

```bash
(chroot) ~/trunk/src/platform/ec $ make BOARD=hatch_fp -j
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