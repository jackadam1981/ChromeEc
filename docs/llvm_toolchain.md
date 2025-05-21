# LLVM Toolchain

[TOC]

## Overview

ChromeOS provides a baremetal LLVM toolchain that can be used to build both EC
and Zephyr. The EC and Zephyr-based [fingerprint firmware] use the LLVM
toolchain by default. This document provides details on how the toolchain is
configured and built.

[fingerprint firmware]: ./fingerprint/fingerprint.md

## Supported Baremetal Toolchains

*   RISC-V: `riscv32-cros-elf`
*   ARMv7-M: `armv7m-cros-eabi`
*   ARMv6-M: `arm-none-eabi`
    *   NOTE: this toolchain will be [renamed in the future][armv6 rename].

[armv6 rename]: https://issuetracker.google.com/231650443

## cros_setup_toolchains

[`cros_setup_toolchains`] is the script used to build the various ChromeOS
toolchains. It's a wrapper around Gentoo's [`crossdev`], which is a script to
configure and build the necessary components for a cross-compiler toolchain. One
area where `cros_setup_toolchains` diverges from `crossdev` is in its handling
of LLVM-based toolchains. `crossdev` has LLVM toolchain support, but
`cros_setup_toolchains` does not use it. Instead, `cros_setup_toolchains` adds
extra LLVM packages to the `crossdev` commandline.

Ultimately, `cros_setup_toolchains` invokes `crossdev` with a set of flags. You
can see the exact `crossdev` command invoked in the output:

```bash
(chroot) sudo `which cros_setup_toolchains` -t riscv32-cros-elf --nousepkg
```

```bash
16:29:01.241: INFO: run: crossdev --stable --show-fail-log --env 'FEATURES=splitdebug' -P --oneshot --overlays '/mnt/host/source/src/third_party/chromiumos-overlay /mnt/host/source/src/third_party/eclass-overlay /mnt/host/source/src/third_party/portage-stable' --ov-output /usr/local/portage/crossdev -t riscv32-cros-elf --ex-pkg sys-libs/compiler-rt --ex-pkg sys-libs/llvm-libunwind --ex-pkg sys-libs/libcxx --binutils '[stable]' --gcc '[stable]' --libc '[stable]' --ex-gdb
```

[`cros_setup_toolchains`]: https://crsrc.org/o/chromite/scripts/cros_setup_toolchains.py
[`crossdev`]: https://github.com/gentoo/crossdev

## Toolchain Overlay

The packages that make up the toolchain are defined in the toolchain overlay. [
`crossdev`] searches for ebuilds in the directory `cross-<target-triple>`, where
`<target-triple>` has [this definition][target triple].

As an example, if you want to build a toolchain with the triple
`riscv32-cros-elf`, you would run:

```bash
(chroot) sudo `which cros_setup_toolchains` -t riscv32-cros-elf --nousepkg
```

which will ultimately result in a call to [`crossdev`], which will search for
ebuilds in [`chromiumos/overlays/toolchains/cross-riscv32-elf`]. That directory
simply contains symlinks to the canonical ChromeOS ebuild for the package. See
https://crrev.com/c/6349977 for an example.

[target triple]: https://clang.llvm.org/docs/CrossCompilation.html#target-triple
[`chromiumos/overlays/toolchains/cross-riscv32-elf`]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/toolchains-overlay/cross-riscv32-cros-elf/

## Components

In the [toolchain overlay] for our baremetal toolchains, you'll see the
following components:

*   `binutils`: [GNU Binutils]
    *   [`binutils` ebuild]
*   `compiler-rt`: [Compiler Runtime libraries] for LLVM and `clang`.
    *   [`compiler-rt` ebuild]
*   `gcc`: should not be needed for an LLVM toolchain, but [`crossdev`] assumes
    that you need a minimal [`stage1` `gcc`] and builds it anyway.
    *   [`gcc` ebuild]
*   `gdb`: The [GNU Project Debugger].
    *   [`gdb` ebuild]
*   `libcxx`: [`libc++` C++ standard library]
    *   [`libcxx` ebuild]
*   `llvm-libunwind`: [`libunwind` LLVM Unwinder]
    *   [`llvm-libunwind` ebuild]
*   `newlib`: The [`newlib` C standard library] is an implementation of the C
    standard library targeting embedded systems.
    *   [`newlib` ebuild]

Once you've successfully built a toolchain with [`cros_setup_toolchains`] or
you're using a chroot that has an SDK with a prebuilt toolchain, you can use
`emerge` to rebuild the individual components as you make changes to the
associated ebuild.

If you are making changes to a `-9999` ebuild, you'll need to use `cros-workon
--host`. In addition, for the ebuilds that use the LLVM source, you'll need to
sync the LLVM source by adding the [`toolchain` group], since it's marked as
[`notdefault`] in the `repo` manifest:

```bash
(outside) repo init -g toolchain -g default
```

For example, if you change some compiler flags in the [`compiler-rt` ebuild],
and want to rebuild it for RISC-V, you would do:

```bash
(chroot) cros-workon --host start cross-riscv32-cros-elf/compiler-rt
(chroot) emerge cross-riscv32-cros-elf/compiler-rt
```

The standard Portage [`equery`] commands work on these packages as well, so you
can do things like:

**List the files in a package**:

```bash
(chroot)$ equery files <package>
```

```bash
(chroot) equery files cross-riscv32-cros-elf/compiler-rt
 * Searching for compiler-rt in cross-riscv32-cros-elf ...
 * Contents of cross-riscv32-cros-elf/compiler-rt-20.0_pre547379-r48:
/usr
/usr/lib64
/usr/lib64/clang
/usr/lib64/clang/20
/usr/lib64/clang/20/lib
/usr/lib64/clang/20/lib/baremetal
/usr/lib64/clang/20/lib/baremetal/libclang_rt.builtins-riscv32.a
```

**Find the package that provides a file**:

```bash
(chroot) equery belongs /path/to/file
```

```bash
(chroot) equery belongs /usr/lib64/clang/20/lib/baremetal/libclang_rt.builtins-riscv32.a
cross-riscv32-cros-elf/compiler-rt-20.0_pre547379-r48 (/usr/lib64/clang/20/lib/baremetal/libclang_rt.builtins-riscv32.a)
```

[`stage1` `gcc`]: https://chromium-review.googlesource.com/c/chromiumos/overlays/chromiumos-overlay/+/6300620/comment/62ae7a97_aa07877f/
[`toolchain` group]: https://chrome-internal.googlesource.com/chromeos/manifest-internal/+/refs/heads/main/_toolchain.xml
[`notdefault`]: https://gerrit.googlesource.com/git-repo/+/HEAD/docs/manifest-format.md#:~:text=If%20you%20place%20a%20project%20in%20the%20group%20%E2%80%9Cnotdefault%E2%80%9D%2C%20it%20will%20not%20be%20automatically%20downloaded%20by%20repo
[`equery`]: https://wiki.gentoo.org/wiki/Equery
[toolchain overlay]: #toolchain-overlay
[GNU Binutils]: https://www.gnu.org/software/binutils/
[`binutils` ebuild]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/chromiumos-overlay/sys-devel/binutils/
[Compiler Runtime libraries]: https://compiler-rt.llvm.org/
[`compiler-rt` ebuild]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/chromiumos-overlay/sys-libs/compiler-rt/
[`gcc` ebuild]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/chromiumos-overlay/sys-devel/gcc/
[GNU Project Debugger]: https://sourceware.org/gdb/
[`gdb` ebuild]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/chromiumos-overlay/sys-devel/gdb/
[`libc++` C++ standard library]: https://libcxx.llvm.org/
[`libcxx` ebuild]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/chromiumos-overlay/sys-libs/libcxx/
[`libcxx` ebuild]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/chromiumos-overlay/sys-libs/libcxx/
[`libunwind` LLVM Unwinder]: https://github.com/llvm/llvm-project/blob/main/libunwind/docs/index.rst
[`llvm-libunwind` ebuild]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/chromiumos-overlay/sys-libs/llvm-libunwind/
[`newlib` C standard library]: https://sourceware.org/newlib/
[`newlib` ebuild]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/chromiumos-overlay/sys-libs/newlib/

## Testing Toolchain Changes

When modifying an ebuild for a component in the baremetal toolchain, you'll want
to make sure that the EC and Zephyr builds succeed, in addition to the [Renode]
tests passing. You can do that by adding the following to the commit message:

```
Cq-Include-Trybots: chromeos/cq:firmware-ec-cq,firmware-zephyr-cq,firmware-ec-renode-cq
```

Automating this is tracked in http://b/406868085.

## Adding a new baremetal toolchain

If you want to add a baremetal toolchain for a new target triple, see the
[CLs created for creating the RISC-V toolchain][riscv cls].

The general flow is:

*   [Enable the target in LLVM](https://crrev.com/c/6300619), if not already
    enabled.
*   [Create](https://crrev.com/c/6349977) the [toolchain overlay] for the target
    triple.
*   [Add the target triple](https://crrev.com/c/6300631) in a `toolchain.conf`
    file in an existing overlay.
*   [Enable the LLVM packages](https://crrev.com/c/6302748) in
    `cros_setup_toolchains`.
*   [Add the target triple](https://crrev.com/c/6300601) to `is_baremetal_abi`.
*   [Use `is_baremetal_abi`](https://crrev.com/c/6526916) to configure
    target-specific compilation flags.

[riscv cls]: https://chromium-review.googlesource.com/q/topic:%22riscv-llvm%22
[Renode]: ./sitemap.md#renode
