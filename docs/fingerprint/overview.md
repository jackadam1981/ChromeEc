# Building FPMCU Firmware

[TOC]

*** note
NOTE: The build commands assume you are in the `~/trunk/src/platform/ec`
directory inside the chroot.
***

*** promo
WARNING: When switching branches in the EC codebase, you probably want to nuke
the `build` directory or at least the board you're working on: `rm -rf
build/<board>` to prevent compilation errors.
***

## See `Makefile` target options

```bash
(chroot) ~/trunk/src/platform/ec $ make help
```

## Build the fingerprint EC (FPMCU) firmware only

### Nocturne

```bash
(chroot) ~/trunk/src/platform/ec $ make BOARD=nocturne_fp -j
```

### Nami

```bash
(chroot) ~/trunk/src/platform/ec $ make BOARD=nami_fp -j
```

### Hatch

```bash
(chroot) ~/trunk/src/platform/ec $ make BOARD=hatch_fp -j
```

### Verbose Build output

Use `V=1`

```bash
(chroot) ~/trunk/src/platform/ec $ make V=1 BOARD=nocturne_fp -j
```

## Build all EC firmware (before `repo upload`)

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